// https://github.com/wxj881027/QmClient
#include <base/detect.h>

#ifndef MEDIA_PLAYER_DBUS
#define MEDIA_PLAYER_DBUS 0
#endif

#ifndef MEDIA_PLAYER_PULSEAUDIO
#define MEDIA_PLAYER_PULSEAUDIO 0
#endif

#if MEDIA_PLAYER_DBUS
#include <curl/curl.h>
#include <dbus/dbus.h>
#if MEDIA_PLAYER_PULSEAUDIO
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#endif
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wduplicated-branches"
#endif
#include <engine/external/stb_image.h>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif

#include "media_player_impl.h"

#include <engine/shared/config.h>

#if MEDIA_PLAYER_DBUS
#include <base/log.h>
#include <base/system.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
	void ClearAudioCapture(CMediaViewer::CAudioCapture *pAudioCapture)
	{
		if(!pAudioCapture)
			return;

		std::scoped_lock Lock(pAudioCapture->m_Mutex);
		pAudioCapture->m_aFrequencyBands.fill(0.0f);
		pAudioCapture->m_Active = false;
	}

	std::string UrlDecode(const std::string &Encoded)
	{
		std::string Decoded;
		Decoded.reserve(Encoded.size());

		for(size_t i = 0; i < Encoded.size(); ++i)
		{
			const char c = Encoded[i];
			if(c == '%' && i + 2 < Encoded.size())
			{
				auto HexToInt = [](char h) {
					if(h >= '0' && h <= '9')
						return h - '0';
					if(h >= 'A' && h <= 'F')
						return h - 'A' + 10;
					if(h >= 'a' && h <= 'f')
						return h - 'a' + 10;
					return 0;
				};
				const unsigned char hi = (unsigned char)Encoded[i + 1];
				const unsigned char lo = (unsigned char)Encoded[i + 2];
				if(std::isxdigit(hi) && std::isxdigit(lo))
				{
					Decoded += (char)((HexToInt((char)hi) << 4) | HexToInt((char)lo));
					i += 2;
					continue;
				}
			}
			Decoded += c == '+' ? ' ' : c;
		}

		return Decoded;
	}

	std::string DecodeFileUri(const std::string &Uri)
	{
		const std::string Prefix = "file:";
		if(Uri.rfind(Prefix, 0) != 0)
			return Uri;

		std::string Path = Uri.substr(Prefix.size());
		if(Path.rfind("///", 0) == 0)
			Path = Path.substr(2);
		else if(Path.rfind("//localhost/", 0) == 0)
			Path = Path.substr(11);
		else if(Path.rfind("//", 0) == 0)
			Path = Path.substr(2);

		return UrlDecode(Path);
	}

	size_t CurlWriteCallback(void *pContents, size_t Size, size_t Nmemb, void *pUser)
	{
		const size_t Total = Size * Nmemb;
		if(Total == 0)
			return 0;

		auto *pVec = static_cast<std::vector<uint8_t> *>(pUser);
		const auto *pData = static_cast<uint8_t *>(pContents);
		if(pVec->size() + Total > 10 * 1024 * 1024)
			return 0;
		pVec->insert(pVec->end(), pData, pData + Total);
		return Total;
	}

	bool DownloadUrlToVector(const std::string &Url, std::vector<uint8_t> &Out)
	{
		static std::once_flag s_CurlInitFlag;
		std::call_once(s_CurlInitFlag, []() { curl_global_init(CURL_GLOBAL_DEFAULT); });

		CURL *pEasy = curl_easy_init();
		if(!pEasy)
			return false;
		CURLM *pMulti = curl_multi_init();
		if(!pMulti)
		{
			curl_easy_cleanup(pEasy);
			return false;
		}

		curl_easy_setopt(pEasy, CURLOPT_URL, Url.c_str());
		curl_easy_setopt(pEasy, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(pEasy, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
		curl_easy_setopt(pEasy, CURLOPT_WRITEDATA, &Out);
		curl_easy_setopt(pEasy, CURLOPT_ACCEPT_ENCODING, "");
		curl_easy_setopt(pEasy, CURLOPT_USERAGENT, "DDNetMediaViewer/1.0");
		curl_easy_setopt(pEasy, CURLOPT_TIMEOUT, 10L);
		curl_easy_setopt(pEasy, CURLOPT_MAXFILESIZE, 10 * 1024 * 1024L);

		if(curl_multi_add_handle(pMulti, pEasy) != CURLM_OK)
		{
			curl_multi_cleanup(pMulti);
			curl_easy_cleanup(pEasy);
			return false;
		}

		CURLcode Result = CURLE_FAILED_INIT;
		bool GotResult = false;
		auto ReadResult = [&]() {
			int MessagesLeft = 0;
			while(CURLMsg *pMsg = curl_multi_info_read(pMulti, &MessagesLeft))
			{
				if(pMsg->msg == CURLMSG_DONE && pMsg->easy_handle == pEasy)
				{
					Result = pMsg->data.result;
					GotResult = true;
					return true;
				}
			}
			return false;
		};

		int RunningHandles = 0;
		CURLMcode MultiResult = curl_multi_perform(pMulti, &RunningHandles);
		while(MultiResult == CURLM_OK && !GotResult)
		{
			ReadResult();
			if(GotResult || RunningHandles == 0)
				break;

			int NumFds = 0;
			MultiResult = curl_multi_poll(pMulti, nullptr, 0, 1000, &NumFds);
			if(MultiResult != CURLM_OK)
				break;
			MultiResult = curl_multi_perform(pMulti, &RunningHandles);
		}

		ReadResult();
		curl_multi_remove_handle(pMulti, pEasy);
		curl_multi_cleanup(pMulti);
		curl_easy_cleanup(pEasy);
		return MultiResult == CURLM_OK && GotResult && Result == CURLE_OK && !Out.empty();
	}

	bool DecodeImageFromBuffer(const std::vector<uint8_t> &Data, std::vector<uint8_t> &OutRgba, int &OutWidth, int &OutHeight)
	{
		OutWidth = 0;
		OutHeight = 0;
		OutRgba.clear();

		if(Data.empty())
		{
			log_info("mediaviewer", "Cannot decode empty image buffer");
			return false;
		}

		int Channels = 0;
		int Width = 0;
		int Height = 0;
		uint8_t *pImageData = stbi_load_from_memory(Data.data(), (int)Data.size(), &Width, &Height, &Channels, 4);
		if(!pImageData)
		{
			log_info("mediaviewer", "Failed to load image from buffer: %s", stbi_failure_reason());
			return false;
		}
		if(Width <= 0 || Height <= 0)
		{
			log_info("mediaviewer", "Invalid image dimensions: %dx%d", Width, Height);
			stbi_image_free(pImageData);
			return false;
		}

		const size_t ByteCount = (size_t)Width * (size_t)Height * 4;
		OutRgba.resize(ByteCount);
		memcpy(OutRgba.data(), pImageData, ByteCount);
		OutWidth = Width;
		OutHeight = Height;
		stbi_image_free(pImageData);
		return true;
	}

	bool DecodeAlbumArtUrl(const std::string &Url, std::vector<uint8_t> &OutRgba, int &OutWidth, int &OutHeight)
	{
		OutRgba.clear();
		OutWidth = 0;
		OutHeight = 0;

		std::vector<uint8_t> Data;
		if(Url.rfind("file:", 0) == 0)
		{
			const std::string FilePath = DecodeFileUri(Url);
			FILE *pFile = fopen(FilePath.c_str(), "rb");
			if(!pFile)
			{
				log_info("mediaviewer", "Failed to open local album art file: %s", FilePath.c_str());
				return false;
			}

			fseek(pFile, 0, SEEK_END);
			const long FileSize = ftell(pFile);
			fseek(pFile, 0, SEEK_SET);
			if(FileSize <= 0 || FileSize > 10 * 1024 * 1024)
			{
				log_info("mediaviewer", "Album art file too large or empty: %s", FilePath.c_str());
				fclose(pFile);
				return false;
			}

			Data.resize((size_t)FileSize);
			if(fread(Data.data(), 1, (size_t)FileSize, pFile) != (size_t)FileSize)
			{
				log_info("mediaviewer", "Failed to read album art file: %s", FilePath.c_str());
				fclose(pFile);
				return false;
			}
			fclose(pFile);
		}
		else if(Url.rfind("http://", 0) == 0 || Url.rfind("https://", 0) == 0)
		{
			if(!DownloadUrlToVector(Url, Data))
			{
				log_info("mediaviewer", "Failed to download album art from %s", Url.c_str());
				return false;
			}
		}
		else
		{
			log_info("mediaviewer", "Unsupported album art URL scheme: %s", Url.c_str());
			return false;
		}

		if(!DecodeImageFromBuffer(Data, OutRgba, OutWidth, OutHeight))
		{
			log_info("mediaviewer", "Failed to decode album art image from %s", Url.c_str());
			return false;
		}

		return true;
	}

	bool DBusGetPropertyString(DBusConnection *pConnection, const char *pService, const char *pPath, const char *pInterface, const char *pProperty, std::string &Value)
	{
		DBusError Error;
		dbus_error_init(&Error);

		DBusMessage *pMessage = dbus_message_new_method_call(pService, pPath, "org.freedesktop.DBus.Properties", "Get");
		if(!pMessage)
		{
			dbus_error_free(&Error);
			return false;
		}

		const char *pIface = pInterface;
		const char *pProp = pProperty;
		dbus_message_append_args(pMessage, DBUS_TYPE_STRING, &pIface, DBUS_TYPE_STRING, &pProp, DBUS_TYPE_INVALID);

		DBusMessage *pReply = dbus_connection_send_with_reply_and_block(pConnection, pMessage, 1000, &Error);
		dbus_message_unref(pMessage);
		if(!pReply)
		{
			dbus_error_free(&Error);
			return false;
		}

		DBusMessageIter Iter;
		if(!dbus_message_iter_init(pReply, &Iter) || dbus_message_iter_get_arg_type(&Iter) != DBUS_TYPE_VARIANT)
		{
			dbus_message_unref(pReply);
			return false;
		}

		DBusMessageIter Variant;
		dbus_message_iter_recurse(&Iter, &Variant);
		if(dbus_message_iter_get_arg_type(&Variant) != DBUS_TYPE_STRING)
		{
			dbus_message_unref(pReply);
			return false;
		}

		const char *pValue = nullptr;
		dbus_message_iter_get_basic(&Variant, &pValue);
		if(pValue)
			Value = pValue;
		dbus_message_unref(pReply);
		return true;
	}

	bool DBusGetPropertyBool(DBusConnection *pConnection, const char *pService, const char *pPath, const char *pInterface, const char *pProperty, bool &Value)
	{
		DBusError Error;
		dbus_error_init(&Error);

		DBusMessage *pMessage = dbus_message_new_method_call(pService, pPath, "org.freedesktop.DBus.Properties", "Get");
		if(!pMessage)
		{
			dbus_error_free(&Error);
			return false;
		}

		const char *pIface = pInterface;
		const char *pProp = pProperty;
		dbus_message_append_args(pMessage, DBUS_TYPE_STRING, &pIface, DBUS_TYPE_STRING, &pProp, DBUS_TYPE_INVALID);

		DBusMessage *pReply = dbus_connection_send_with_reply_and_block(pConnection, pMessage, 1000, &Error);
		dbus_message_unref(pMessage);
		if(!pReply)
		{
			dbus_error_free(&Error);
			return false;
		}

		DBusMessageIter Iter;
		if(!dbus_message_iter_init(pReply, &Iter) || dbus_message_iter_get_arg_type(&Iter) != DBUS_TYPE_VARIANT)
		{
			dbus_message_unref(pReply);
			return false;
		}

		DBusMessageIter Variant;
		dbus_message_iter_recurse(&Iter, &Variant);
		if(dbus_message_iter_get_arg_type(&Variant) != DBUS_TYPE_BOOLEAN)
		{
			dbus_message_unref(pReply);
			return false;
		}

		dbus_bool_t RawValue = false;
		dbus_message_iter_get_basic(&Variant, &RawValue);
		Value = RawValue != 0;
		dbus_message_unref(pReply);
		return true;
	}

	bool DBusGetPropertyInt64(DBusConnection *pConnection, const char *pService, const char *pPath, const char *pInterface, const char *pProperty, int64_t &Value)
	{
		DBusError Error;
		dbus_error_init(&Error);

		DBusMessage *pMessage = dbus_message_new_method_call(pService, pPath, "org.freedesktop.DBus.Properties", "Get");
		if(!pMessage)
		{
			dbus_error_free(&Error);
			return false;
		}

		const char *pIface = pInterface;
		const char *pProp = pProperty;
		dbus_message_append_args(pMessage, DBUS_TYPE_STRING, &pIface, DBUS_TYPE_STRING, &pProp, DBUS_TYPE_INVALID);

		DBusMessage *pReply = dbus_connection_send_with_reply_and_block(pConnection, pMessage, 1000, &Error);
		dbus_message_unref(pMessage);
		if(!pReply)
		{
			dbus_error_free(&Error);
			return false;
		}

		DBusMessageIter Iter;
		if(!dbus_message_iter_init(pReply, &Iter) || dbus_message_iter_get_arg_type(&Iter) != DBUS_TYPE_VARIANT)
		{
			dbus_message_unref(pReply);
			return false;
		}

		DBusMessageIter Variant;
		dbus_message_iter_recurse(&Iter, &Variant);
		if(dbus_message_iter_get_arg_type(&Variant) != DBUS_TYPE_INT64)
		{
			dbus_message_unref(pReply);
			return false;
		}

		dbus_int64_t RawValue = 0;
		dbus_message_iter_get_basic(&Variant, &RawValue);
		Value = (int64_t)RawValue;
		dbus_message_unref(pReply);
		return true;
	}

	bool DBusGetNames(DBusConnection *pConnection, std::vector<std::string> &Names)
	{
		DBusError Error;
		dbus_error_init(&Error);

		DBusMessage *pMessage = dbus_message_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
		if(!pMessage)
		{
			dbus_error_free(&Error);
			return false;
		}

		DBusMessage *pReply = dbus_connection_send_with_reply_and_block(pConnection, pMessage, 1000, &Error);
		dbus_message_unref(pMessage);
		if(!pReply)
		{
			dbus_error_free(&Error);
			return false;
		}

		DBusMessageIter Iter;
		if(!dbus_message_iter_init(pReply, &Iter) || dbus_message_iter_get_arg_type(&Iter) != DBUS_TYPE_ARRAY)
		{
			dbus_message_unref(pReply);
			return false;
		}

		DBusMessageIter Array;
		dbus_message_iter_recurse(&Iter, &Array);
		while(dbus_message_iter_get_arg_type(&Array) == DBUS_TYPE_STRING)
		{
			const char *pName = nullptr;
			dbus_message_iter_get_basic(&Array, &pName);
			if(pName)
				Names.emplace_back(pName);
			dbus_message_iter_next(&Array);
		}

		dbus_message_unref(pReply);
		return true;
	}

	bool DBusGetMetadata(DBusConnection *pConnection, const char *pService, CPlainState &State)
	{
		DBusError Error;
		dbus_error_init(&Error);

		DBusMessage *pMessage = dbus_message_new_method_call(pService, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");
		if(!pMessage)
		{
			dbus_error_free(&Error);
			return false;
		}

		const char *pIface = "org.mpris.MediaPlayer2.Player";
		const char *pProp = "Metadata";
		dbus_message_append_args(pMessage, DBUS_TYPE_STRING, &pIface, DBUS_TYPE_STRING, &pProp, DBUS_TYPE_INVALID);

		DBusMessage *pReply = dbus_connection_send_with_reply_and_block(pConnection, pMessage, 1000, &Error);
		dbus_message_unref(pMessage);
		if(!pReply)
		{
			dbus_error_free(&Error);
			return false;
		}

		DBusMessageIter Iter;
		if(!dbus_message_iter_init(pReply, &Iter) || dbus_message_iter_get_arg_type(&Iter) != DBUS_TYPE_VARIANT)
		{
			dbus_message_unref(pReply);
			return false;
		}

		DBusMessageIter Variant;
		dbus_message_iter_recurse(&Iter, &Variant);
		if(dbus_message_iter_get_arg_type(&Variant) != DBUS_TYPE_ARRAY)
		{
			dbus_message_unref(pReply);
			return false;
		}

		DBusMessageIter Dict;
		dbus_message_iter_recurse(&Variant, &Dict);
		while(dbus_message_iter_get_arg_type(&Dict) == DBUS_TYPE_DICT_ENTRY)
		{
			DBusMessageIter Entry;
			dbus_message_iter_recurse(&Dict, &Entry);

			const char *pKey = nullptr;
			dbus_message_iter_get_basic(&Entry, &pKey);
			dbus_message_iter_next(&Entry);

			if(dbus_message_iter_get_arg_type(&Entry) == DBUS_TYPE_VARIANT)
			{
				DBusMessageIter ValueIter;
				dbus_message_iter_recurse(&Entry, &ValueIter);
				const int ArgType = dbus_message_iter_get_arg_type(&ValueIter);

				if(pKey && strcmp(pKey, "xesam:title") == 0 && ArgType == DBUS_TYPE_STRING)
				{
					const char *pString = nullptr;
					dbus_message_iter_get_basic(&ValueIter, &pString);
					if(pString)
						State.m_Title = pString;
				}
				else if(pKey && strcmp(pKey, "xesam:album") == 0 && ArgType == DBUS_TYPE_STRING)
				{
					const char *pString = nullptr;
					dbus_message_iter_get_basic(&ValueIter, &pString);
					if(pString)
						State.m_Album = pString;
				}
				else if(pKey && strcmp(pKey, "xesam:artist") == 0 && ArgType == DBUS_TYPE_ARRAY)
				{
					std::vector<std::string> Artists;
					DBusMessageIter ArrayIter;
					dbus_message_iter_recurse(&ValueIter, &ArrayIter);
					while(dbus_message_iter_get_arg_type(&ArrayIter) == DBUS_TYPE_STRING)
					{
						const char *pString = nullptr;
						dbus_message_iter_get_basic(&ArrayIter, &pString);
						if(pString)
							Artists.emplace_back(pString);
						dbus_message_iter_next(&ArrayIter);
					}
					if(!Artists.empty())
						State.m_Artist = Artists.front();
				}
				else if(pKey && (strcmp(pKey, "mpris:artUrl") == 0 || strcmp(pKey, "xesam:artUrl") == 0) && (ArgType == DBUS_TYPE_STRING || ArgType == DBUS_TYPE_OBJECT_PATH))
				{
					const char *pString = nullptr;
					dbus_message_iter_get_basic(&ValueIter, &pString);
					if(pString)
						State.m_AlbumArtUrl = pString;
				}
				else if(pKey && strcmp(pKey, "mpris:trackid") == 0 && (ArgType == DBUS_TYPE_STRING || ArgType == DBUS_TYPE_OBJECT_PATH))
				{
					const char *pString = nullptr;
					dbus_message_iter_get_basic(&ValueIter, &pString);
					if(pString)
						State.m_TrackId = pString;
				}
				else if(pKey && strcmp(pKey, "mpris:length") == 0 && ArgType == DBUS_TYPE_INT64)
				{
					dbus_int64_t Length = 0;
					dbus_message_iter_get_basic(&ValueIter, &Length);
					if(Length > 0)
						State.m_DurationMs = Length / 1000;
				}
			}

			dbus_message_iter_next(&Dict);
		}

		dbus_message_unref(pReply);
		return true;
	}

	std::string DBusChoosePlayer(DBusConnection *pConnection)
	{
		std::vector<std::string> Names;
		if(!DBusGetNames(pConnection, Names))
			return std::string();

		std::string Best;
		for(const std::string &Name : Names)
		{
			if(Name.rfind("org.mpris.MediaPlayer2.", 0) != 0)
				continue;

			std::string PlaybackStatus;
			if(DBusGetPropertyString(pConnection, Name.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "PlaybackStatus", PlaybackStatus))
			{
				if(PlaybackStatus == "Playing")
					return Name;
				if(Best.empty())
					Best = Name;
			}
		}

		return Best;
	}

	void DBusSendPlayerCommand(DBusConnection *pConnection, const std::string &Service, const char *pCommand)
	{
		DBusError Error;
		dbus_error_init(&Error);

		DBusMessage *pMessage = dbus_message_new_method_call(Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", pCommand);
		if(pMessage)
		{
			dbus_connection_send_with_reply_and_block(pConnection, pMessage, 1000, &Error);
			dbus_message_unref(pMessage);
		}
		dbus_error_free(&Error);
	}

	bool DBusUpdatePlayerState(DBusConnection *pConnection, const std::string &Service, CPlainState &State)
	{
		std::string PlaybackStatus;
		if(!DBusGetPropertyString(pConnection, Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "PlaybackStatus", PlaybackStatus))
			return false;

		State.m_Playing = PlaybackStatus == "Playing";

		std::string Identity;
		if(DBusGetPropertyString(pConnection, Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2", "Identity", Identity) && !Identity.empty())
			State.m_ServiceId = Identity;
		else
			State.m_ServiceId = Service;

		DBusGetPropertyBool(pConnection, Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "CanPlay", State.m_CanPlay);
		DBusGetPropertyBool(pConnection, Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "CanPause", State.m_CanPause);
		DBusGetPropertyBool(pConnection, Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "CanGoPrevious", State.m_CanPrev);
		DBusGetPropertyBool(pConnection, Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "CanGoNext", State.m_CanNext);

		int64_t Position = 0;
		if(DBusGetPropertyInt64(pConnection, Service.c_str(), "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "Position", Position) && Position > 0)
			State.m_PositionMs = Position / 1000;

		DBusGetMetadata(pConnection, Service.c_str(), State);
		return true;
	}

#if MEDIA_PLAYER_PULSEAUDIO
	struct CPulseNameQuery
	{
		pa_threaded_mainloop *m_pMainLoop = nullptr;
		std::string m_Value;
		bool m_Done = false;
		bool m_Success = false;
	};

	struct CPulseStreamData
	{
		CMediaViewer *m_pMediaViewer = nullptr;
		std::vector<float> m_SampleBuffer;
		int m_Channels = 0;
		int m_SampleRate = 0;
	};

	void PulseContextStateCallback(pa_context *pContext, void *pUser)
	{
		(void)pContext;
		pa_threaded_mainloop_signal(static_cast<pa_threaded_mainloop *>(pUser), 0);
	}

	void PulseStreamStateCallback(pa_stream *pStream, void *pUser)
	{
		(void)pStream;
		pa_threaded_mainloop_signal(static_cast<pa_threaded_mainloop *>(pUser), 0);
	}

	bool PulseWaitForContextReady(pa_threaded_mainloop *pMainLoop, pa_context *pContext)
	{
		while(true)
		{
			const pa_context_state_t State = pa_context_get_state(pContext);
			if(State == PA_CONTEXT_READY)
				return true;
			if(!PA_CONTEXT_IS_GOOD(State))
				return false;
			pa_threaded_mainloop_wait(pMainLoop);
		}
	}

	bool PulseWaitForStreamReady(pa_threaded_mainloop *pMainLoop, pa_stream *pStream)
	{
		while(true)
		{
			const pa_stream_state_t State = pa_stream_get_state(pStream);
			if(State == PA_STREAM_READY)
				return true;
			if(!PA_STREAM_IS_GOOD(State))
				return false;
			pa_threaded_mainloop_wait(pMainLoop);
		}
	}

	void PulseServerInfoCallback(pa_context *pContext, const pa_server_info *pInfo, void *pUser)
	{
		(void)pContext;
		auto *pQuery = static_cast<CPulseNameQuery *>(pUser);
		if(pInfo && pInfo->default_sink_name)
		{
			pQuery->m_Value = pInfo->default_sink_name;
			pQuery->m_Success = true;
		}
		pQuery->m_Done = true;
		pa_threaded_mainloop_signal(pQuery->m_pMainLoop, 0);
	}

	void PulseSinkInfoCallback(pa_context *pContext, const pa_sink_info *pInfo, int Eol, void *pUser)
	{
		(void)pContext;
		auto *pQuery = static_cast<CPulseNameQuery *>(pUser);
		if(Eol > 0)
		{
			pQuery->m_Done = true;
			pa_threaded_mainloop_signal(pQuery->m_pMainLoop, 0);
			return;
		}

		if(pInfo && pInfo->monitor_source_name)
		{
			pQuery->m_Value = pInfo->monitor_source_name;
			pQuery->m_Success = true;
			pQuery->m_Done = true;
			pa_threaded_mainloop_signal(pQuery->m_pMainLoop, 0);
		}
	}

	std::string PulseResolveDefaultMonitorSource(pa_threaded_mainloop *pMainLoop, pa_context *pContext)
	{
		CPulseNameQuery SinkQuery;
		SinkQuery.m_pMainLoop = pMainLoop;
		pa_operation *pServerOp = pa_context_get_server_info(pContext, PulseServerInfoCallback, &SinkQuery);
		if(!pServerOp)
			return std::string();

		while(!SinkQuery.m_Done)
			pa_threaded_mainloop_wait(pMainLoop);
		pa_operation_unref(pServerOp);
		if(!SinkQuery.m_Success || SinkQuery.m_Value.empty())
			return std::string();

		CPulseNameQuery MonitorQuery;
		MonitorQuery.m_pMainLoop = pMainLoop;
		pa_operation *pSinkOp = pa_context_get_sink_info_by_name(pContext, SinkQuery.m_Value.c_str(), PulseSinkInfoCallback, &MonitorQuery);
		if(!pSinkOp)
			return std::string();

		while(!MonitorQuery.m_Done)
			pa_threaded_mainloop_wait(pMainLoop);
		pa_operation_unref(pSinkOp);
		if(!MonitorQuery.m_Success || MonitorQuery.m_Value.empty())
			return std::string();

		return MonitorQuery.m_Value;
	}

	void PulseConsumeSamples(CPulseStreamData *pStreamData, const float *pSamples, size_t NumFloats)
	{
		if(!pStreamData || !pStreamData->m_pMediaViewer || !pSamples || pStreamData->m_Channels <= 0)
			return;

		const size_t NumFrames = NumFloats / (size_t)pStreamData->m_Channels;
		for(size_t i = 0; i < NumFrames; ++i)
		{
			float Sample = 0.0f;
			for(int Channel = 0; Channel < pStreamData->m_Channels; ++Channel)
				Sample += pSamples[i * (size_t)pStreamData->m_Channels + (size_t)Channel];
			Sample /= (float)pStreamData->m_Channels;

			pStreamData->m_SampleBuffer.push_back(Sample);
			if(pStreamData->m_SampleBuffer.size() >= (size_t)FFT::FFT_SIZE)
			{
				pStreamData->m_pMediaViewer->ProcessAudioFrame(pStreamData->m_SampleBuffer.data(), (int)pStreamData->m_SampleBuffer.size(), pStreamData->m_SampleRate);
				pStreamData->m_SampleBuffer.clear();
			}
		}
	}

	void PulseStreamReadCallback(pa_stream *pStream, size_t Length, void *pUser)
	{
		(void)Length;
		auto *pStreamData = static_cast<CPulseStreamData *>(pUser);
		if(!pStreamData)
			return;

		while(true)
		{
			size_t ReadableBytes = pa_stream_readable_size(pStream);
			if(ReadableBytes == 0 || ReadableBytes == (size_t)-1)
				break;

			const void *pData = nullptr;
			size_t NumBytes = 0;
			if(pa_stream_peek(pStream, &pData, &NumBytes) < 0)
				break;

			if(pData && NumBytes >= sizeof(float) * (size_t)std::max(1, pStreamData->m_Channels))
				PulseConsumeSamples(pStreamData, static_cast<const float *>(pData), NumBytes / sizeof(float));

			pa_stream_drop(pStream);
		}
	}
#endif
}

void ClearDbusAlbumArtLocal(CMediaViewer::CDbus *pDbus, IGraphics *pGraphics)
{
	if(!pDbus)
		return;

	std::scoped_lock Lock(pDbus->m_Mutex);
	if(pGraphics && pDbus->m_AlbumArt.m_Texture.IsValid())
		pGraphics->UnloadTexture(&pDbus->m_AlbumArt.m_Texture);
	if(pGraphics && pDbus->m_PrevAlbumArt.m_Texture.IsValid())
		pGraphics->UnloadTexture(&pDbus->m_PrevAlbumArt.m_Texture);
	pDbus->m_AlbumArt = CMediaViewer::CAlbumArt{};
	pDbus->m_PrevAlbumArt = CMediaViewer::CAlbumArt{};
	pDbus->m_AlbumArtPendingRgba.clear();
	pDbus->m_AlbumArtPendingWidth = 0;
	pDbus->m_AlbumArtPendingHeight = 0;
}

void ApplyDbusSharedAlbumArt(CMediaViewer::CShared *pShared, CMediaViewer::CDbus *pDbus, IGraphics *pGraphics)
{
	if(!pShared || !pDbus || !pGraphics)
		return;

	int AlbumArtWidth = 0;
	int AlbumArtHeight = 0;
	std::vector<uint8_t> AlbumArtPixels;
	if(ConsumeSharedAlbumArt(pShared, AlbumArtPixels, AlbumArtWidth, AlbumArtHeight))
	{
		if(PrepareAlbumArtPixels(AlbumArtPixels, AlbumArtWidth, AlbumArtHeight))
		{
			std::scoped_lock Lock(pDbus->m_Mutex);
			pDbus->m_AlbumArtPendingRgba = std::move(AlbumArtPixels);
			pDbus->m_AlbumArtPendingWidth = AlbumArtWidth;
			pDbus->m_AlbumArtPendingHeight = AlbumArtHeight;
		}
		else
		{
			ClearDbusAlbumArtLocal(pDbus, pGraphics);
			return;
		}
	}

	std::scoped_lock Lock(pDbus->m_Mutex);
	if(pDbus->m_AlbumArtPendingRgba.empty() || pDbus->m_AlbumArtPendingWidth <= 0 || pDbus->m_AlbumArtPendingHeight <= 0)
		return;

	IGraphics::CTextureHandle NewAlbumArt;
	if(!LoadAlbumArtTexture(pGraphics, pDbus->m_AlbumArtPendingRgba, pDbus->m_AlbumArtPendingWidth, pDbus->m_AlbumArtPendingHeight, "dbus_album_art", NewAlbumArt))
		return;

	if(pDbus->m_PrevAlbumArt.m_Texture.IsValid())
		pGraphics->UnloadTexture(&pDbus->m_PrevAlbumArt.m_Texture);
	pDbus->m_PrevAlbumArt = pDbus->m_AlbumArt;
	pDbus->m_AlbumArt.m_Texture = NewAlbumArt;
	pDbus->m_AlbumArt.m_Width = pDbus->m_AlbumArtPendingWidth;
	pDbus->m_AlbumArt.m_Height = pDbus->m_AlbumArtPendingHeight;
	pDbus->m_AlbumArt.m_Colors = pDbus->m_State.m_AlbumArtColors;
	pDbus->m_AlbumArtPendingRgba.clear();
	pDbus->m_AlbumArtPendingWidth = 0;
	pDbus->m_AlbumArtPendingHeight = 0;
}

void CMediaViewer::ThreadMain()
{
	DBusError Error;
	dbus_error_init(&Error);

	DBusConnection *pConnection = dbus_bus_get(DBUS_BUS_SESSION, &Error);
	if(!pConnection)
	{
		dbus_error_free(&Error);
		return;
	}

	dbus_connection_set_exit_on_disconnect(pConnection, false);

	std::string PrevAlbumArtUrl;
	std::string PrevTrackId;
	CMediaViewer::CAlbumArtColors AlbumArtColors;
	bool AlbumArtLoaded = false;
	auto LastAlbumArtAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(10);

	while(!m_StopThread.load(std::memory_order_relaxed))
	{
		CPlainState State;
		bool HasMedia = false;
		if(g_Config.m_ClMediaIsland == 0)
		{
			PublishSharedState(m_pShared.get(), State, false);
			ClearSharedAlbumArt(m_pShared.get());
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		const std::string Service = DBusChoosePlayer(pConnection);
		if(!Service.empty())
			HasMedia = DBusUpdatePlayerState(pConnection, Service, State);

		const bool TrackChanged = (State.m_TrackId != PrevTrackId) || (State.m_AlbumArtUrl != PrevAlbumArtUrl);
		const auto Now = std::chrono::steady_clock::now();
		const bool RetryAlbumArt = !State.m_AlbumArtUrl.empty() && !AlbumArtLoaded && Now - LastAlbumArtAttempt >= std::chrono::seconds(3);
		if(TrackChanged || RetryAlbumArt)
		{
			if(TrackChanged)
			{
				PrevAlbumArtUrl = State.m_AlbumArtUrl;
				PrevTrackId = State.m_TrackId;
				AlbumArtColors = CMediaViewer::CAlbumArtColors{};
				AlbumArtLoaded = false;
			}

			if(!State.m_AlbumArtUrl.empty())
			{
				LastAlbumArtAttempt = Now;
				std::vector<uint8_t> RgbaPixels;
				int Width = 0;
				int Height = 0;
				if(DecodeAlbumArtUrl(State.m_AlbumArtUrl, RgbaPixels, Width, Height))
				{
					ComputeAlbumArtColors(RgbaPixels, Width, Height, State);
					AlbumArtColors = State.m_AlbumArtColors;
					AlbumArtLoaded = true;
					SetSharedAlbumArt(m_pShared.get(), std::move(RgbaPixels), Width, Height);
				}
				else
				{
					AlbumArtLoaded = false;
					ClearSharedAlbumArt(m_pShared.get());
				}
			}
			else
			{
				AlbumArtLoaded = false;
				ClearSharedAlbumArt(m_pShared.get());
			}
		}
		else
		{
			State.m_AlbumArtColors = AlbumArtColors;
		}

		std::deque<ECommand> Commands;
		PublishSharedState(m_pShared.get(), State, HasMedia, &Commands);

		if(HasMedia && !Service.empty())
		{
			for(const ECommand Command : Commands)
			{
				switch(Command)
				{
				case ECommand::Prev:
					DBusSendPlayerCommand(pConnection, Service, "Previous");
					break;
				case ECommand::PlayPause:
					DBusSendPlayerCommand(pConnection, Service, "PlayPause");
					break;
				case ECommand::Next:
					DBusSendPlayerCommand(pConnection, Service, "Next");
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

void CMediaViewer::AudioThreadMain()
{
#if !MEDIA_PLAYER_PULSEAUDIO
	ClearAudioCapture(m_pAudioCapture.get());
	while(!m_StopAudioThread.load(std::memory_order_relaxed))
	{
		if(g_Config.m_ClMediaIsland == 0)
		{
			ClearAudioCapture(m_pAudioCapture.get());
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
	return;
#else
	ClearAudioCapture(m_pAudioCapture.get());

	while(!m_StopAudioThread.load(std::memory_order_relaxed))
	{
		if(g_Config.m_ClMediaIsland == 0)
		{
			ClearAudioCapture(m_pAudioCapture.get());
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		pa_threaded_mainloop *pMainLoop = nullptr;
		pa_context *pContext = nullptr;
		pa_stream *pStream = nullptr;
		bool MainLoopStarted = false;
		bool MainLoopLocked = false;
		bool Connected = false;

		auto Cleanup = [&]() {
			if(MainLoopLocked)
			{
				pa_threaded_mainloop_unlock(pMainLoop);
				MainLoopLocked = false;
			}

			if(pStream)
			{
				if(MainLoopStarted)
				{
					pa_threaded_mainloop_lock(pMainLoop);
					pa_stream_disconnect(pStream);
					pa_stream_unref(pStream);
					pa_threaded_mainloop_unlock(pMainLoop);
				}
				else
				{
					pa_stream_unref(pStream);
				}
				pStream = nullptr;
			}

			if(pContext)
			{
				if(MainLoopStarted)
				{
					pa_threaded_mainloop_lock(pMainLoop);
					pa_context_disconnect(pContext);
					pa_context_unref(pContext);
					pa_threaded_mainloop_unlock(pMainLoop);
				}
				else
				{
					pa_context_unref(pContext);
				}
				pContext = nullptr;
			}

			if(pMainLoop)
			{
				if(MainLoopStarted)
					pa_threaded_mainloop_stop(pMainLoop);
				pa_threaded_mainloop_free(pMainLoop);
				pMainLoop = nullptr;
			}
		};

		pMainLoop = pa_threaded_mainloop_new();
		if(!pMainLoop)
		{
			log_info("mediaviewer", "Failed to create PulseAudio mainloop");
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		pContext = pa_context_new(pa_threaded_mainloop_get_api(pMainLoop), "DDNetMediaViewer");
		if(!pContext)
		{
			log_info("mediaviewer", "Failed to create PulseAudio context");
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		pa_context_set_state_callback(pContext, PulseContextStateCallback, pMainLoop);
		if(pa_threaded_mainloop_start(pMainLoop) < 0)
		{
			log_info("mediaviewer", "Failed to start PulseAudio mainloop");
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		MainLoopStarted = true;

		pa_threaded_mainloop_lock(pMainLoop);
		MainLoopLocked = true;
		if(pa_context_connect(pContext, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0)
		{
			log_info("mediaviewer", "Failed to connect PulseAudio context: %s", pa_strerror(pa_context_errno(pContext)));
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		if(!PulseWaitForContextReady(pMainLoop, pContext))
		{
			log_info("mediaviewer", "PulseAudio context did not become ready");
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		const std::string MonitorSource = PulseResolveDefaultMonitorSource(pMainLoop, pContext);
		if(MonitorSource.empty())
		{
			log_info("mediaviewer", "Failed to resolve PulseAudio monitor source");
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		pa_sample_spec Spec;
		Spec.format = PA_SAMPLE_FLOAT32NE;
		Spec.rate = 48000;
		Spec.channels = 2;

		pa_buffer_attr Attr;
		Attr.maxlength = static_cast<uint32_t>(-1);
		Attr.tlength = static_cast<uint32_t>(-1);
		Attr.prebuf = static_cast<uint32_t>(-1);
		Attr.minreq = static_cast<uint32_t>(-1);
		Attr.fragsize = (uint32_t)(sizeof(float) * Spec.channels * FFT::FFT_SIZE);

		pStream = pa_stream_new(pContext, "media_visualizer", &Spec, nullptr);
		if(!pStream)
		{
			log_info("mediaviewer", "Failed to create PulseAudio stream: %s", pa_strerror(pa_context_errno(pContext)));
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		CPulseStreamData StreamData;
		StreamData.m_pMediaViewer = this;
		StreamData.m_Channels = Spec.channels;
		StreamData.m_SampleRate = (int)Spec.rate;

		pa_stream_set_state_callback(pStream, PulseStreamStateCallback, pMainLoop);
		pa_stream_set_read_callback(pStream, PulseStreamReadCallback, &StreamData);
		if(pa_stream_connect_record(pStream, MonitorSource.c_str(), &Attr, PA_STREAM_ADJUST_LATENCY) < 0)
		{
			log_info("mediaviewer", "Failed to connect PulseAudio record stream: %s", pa_strerror(pa_context_errno(pContext)));
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		if(!PulseWaitForStreamReady(pMainLoop, pStream))
		{
			log_info("mediaviewer", "PulseAudio record stream did not become ready");
			Cleanup();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		if(const pa_sample_spec *pActualSpec = pa_stream_get_sample_spec(pStream))
		{
			StreamData.m_Channels = pActualSpec->channels;
			StreamData.m_SampleRate = (int)pActualSpec->rate;
		}

		MainLoopLocked = false;
		pa_threaded_mainloop_unlock(pMainLoop);
		Connected = true;

		while(!m_StopAudioThread.load(std::memory_order_relaxed))
		{
			pa_threaded_mainloop_lock(pMainLoop);
			const pa_context_state_t ContextState = pa_context_get_state(pContext);
			const pa_stream_state_t StreamState = pa_stream_get_state(pStream);
			const bool BadContext = !PA_CONTEXT_IS_GOOD(ContextState);
			const bool BadStream = !PA_STREAM_IS_GOOD(StreamState);
			pa_threaded_mainloop_unlock(pMainLoop);

			if(BadContext || BadStream)
			{
				log_info("mediaviewer", "PulseAudio stream disconnected, retrying");
				Connected = false;
				break;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(250));
		}

		Cleanup();
		ClearAudioCapture(m_pAudioCapture.get());
		if(Connected && !m_StopAudioThread.load(std::memory_order_relaxed))
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	ClearAudioCapture(m_pAudioCapture.get());
#endif
}

void CMediaViewer::ProcessAudioFrame(const float *pSamples, int NumSamples, int SampleRate)
{
	if(!m_pAudioCapture || !pSamples || NumSamples <= 1 || SampleRate <= 0)
		return;

	std::vector<FFT::CComplex> FftResult;
	FFT::ComputeFFT(pSamples, NumSamples, FftResult);

	const int NumBands = CVisualizer::NUM_FREQUENCY_BANDS;
	std::array<float, CVisualizer::NUM_FREQUENCY_BANDS> Bands{};

	const int UsableBins = FFT::FFT_SIZE / 2;
	const float FreqPerBin = (float)SampleRate / FFT::FFT_SIZE;
	const float MinFreq = 48000.0f / FFT::FFT_SIZE;
	const float MaxFreq = 20000.0f;
	const float LogMin = std::log10(MinFreq);
	const float LogMax = std::log10(MaxFreq);

	for(int Band = 0; Band < NumBands; ++Band)
	{
		const float t0 = (float)Band / NumBands;
		const float t1 = (float)(Band + 1) / NumBands;
		const float Freq0 = std::pow(10.0f, LogMin + t0 * (LogMax - LogMin));
		const float Freq1 = std::pow(10.0f, LogMin + t1 * (LogMax - LogMin));

		const int Bin0 = std::clamp((int)(Freq0 / FreqPerBin), 0, UsableBins - 1);
		const int Bin1 = std::clamp((int)(Freq1 / FreqPerBin), Bin0 + 1, UsableBins);

		float Sum = 0.0f;
		int Count = 0;
		for(int Bin = Bin0; Bin < Bin1; ++Bin)
		{
			Sum += FftResult[Bin].Magnitude();
			++Count;
		}

		if(Count > 0)
		{
			float Magnitude = Sum / Count;
			Magnitude = std::log10(1.0f + Magnitude * 100.0f) / 2.0f;
			Bands[Band] = std::clamp(Magnitude, 0.0f, 1.0f);
		}
	}

	std::scoped_lock Lock(m_pAudioCapture->m_Mutex);
	const float Smoothing = 0.7f;
	for(int i = 0; i < NumBands; ++i)
	{
		m_pAudioCapture->m_aFrequencyBands[i] =
			m_pAudioCapture->m_aFrequencyBands[i] * Smoothing +
			Bands[i] * (1.0f - Smoothing);
	}
	m_pAudioCapture->m_Active = true;
}
#endif
