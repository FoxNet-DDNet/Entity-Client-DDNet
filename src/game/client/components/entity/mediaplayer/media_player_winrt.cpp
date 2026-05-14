// https://github.com/wxj881027/QmClient
#include <base/detect.h>

#ifndef MEDIA_PLAYER_DBUS
#define MEDIA_PLAYER_DBUS 0
#endif

#if defined(CONF_FAMILY_WINDOWS) && __has_include(<winrt/Windows.Foundation.h>)
#define MEDIA_PLAYER_WINRT 1
#endif

#if MEDIA_PLAYER_WINRT
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef NOGDI
#undef NOGDI
#endif

#include <Windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Audio.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Media.Render.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/base.h>

#ifdef ERROR
#undef ERROR
#endif
#endif

#ifndef MEDIA_PLAYER_WINRT
#define MEDIA_PLAYER_WINRT 0
#endif

#include "media_player_impl.h"

#if MEDIA_PLAYER_WINRT
#define IStorage EngineIStorage
#include <engine/shared/config.h>
#undef IStorage
#include <base/system.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <vector>

using namespace winrt::Windows::Media::Control;

namespace
{
	template<typename TAsyncOp>
	bool WaitForAsync(const TAsyncOp &Operation, const std::atomic_bool &StopFlag)
	{
		using winrt::Windows::Foundation::AsyncStatus;
		while(true)
		{
			const AsyncStatus Status = Operation.Status();
			if(Status == AsyncStatus::Completed)
				return true;
			if(Status == AsyncStatus::Canceled || Status == AsyncStatus::Error)
				return false;
			if(StopFlag.load(std::memory_order_relaxed))
				return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	}

	void ClearAlbumArtLocal(CMediaViewer::CWinrt *pWinrt, IGraphics *pGraphics)
	{
		if(pGraphics && pWinrt->m_State.m_AlbumArt.m_Texture.IsValid())
			pGraphics->UnloadTexture(&pWinrt->m_State.m_AlbumArt.m_Texture);
		if(pGraphics && pWinrt->m_State.m_PrevAlbumArt.m_Texture.IsValid())
			pGraphics->UnloadTexture(&pWinrt->m_State.m_PrevAlbumArt.m_Texture);

		pWinrt->m_State.m_AlbumArt.m_Texture.Invalidate();
		pWinrt->m_State.m_AlbumArt.m_Width = 0;
		pWinrt->m_State.m_AlbumArt.m_Height = 0;
		pWinrt->m_State.m_AlbumArt.m_Colors = CMediaViewer::CAlbumArtColors{};
		pWinrt->m_State.m_PrevAlbumArt.m_Texture.Invalidate();
		pWinrt->m_State.m_PrevAlbumArt.m_Width = 0;
		pWinrt->m_State.m_PrevAlbumArt.m_Height = 0;
		pWinrt->m_State.m_PrevAlbumArt.m_Colors = CMediaViewer::CAlbumArtColors{};
		pWinrt->m_AlbumArtPendingRgba.clear();
		pWinrt->m_AlbumArtPendingWidth = 0;
		pWinrt->m_AlbumArtPendingHeight = 0;
	}

	void ClearMediaText(CPlainState &State)
	{
		State.m_ServiceId.clear();
		State.m_Title.clear();
		State.m_Artist.clear();
		State.m_Album.clear();
	}

	void ClearMediaDetails(CPlainState &State, std::string &AlbumArtKey, CMediaViewer::CShared *pShared)
	{
		ClearMediaText(State);
		ClearAlbumArtColors(State);
		AlbumArtKey.clear();
		ClearSharedAlbumArt(pShared);
	}

	void ResetSharedState(CMediaViewer::CShared *pShared, CPlainState &State, bool &HasMedia, std::string &AlbumArtKey)
	{
		HasMedia = false;
		State = CPlainState{};
		AlbumArtKey.clear();
		ClearSharedAlbumArt(pShared);
		PublishSharedState(pShared, State, false);
	}

	bool UpdateAlbumArtData(CMediaViewer::CShared *pShared, CPlainState &State, const winrt::Windows::Storage::Streams::IRandomAccessStreamReference &Thumbnail, const std::atomic_bool &StopFlag)
	{
		if(!Thumbnail)
		{
			ClearAlbumArtColors(State);
			ClearSharedAlbumArt(pShared);
			return false;
		}

		try
		{
			const auto StreamOp = Thumbnail.OpenReadAsync();
			if(!WaitForAsync(StreamOp, StopFlag))
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}
			const auto Stream = StreamOp.GetResults();
			if(!Stream)
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}

			const auto DecoderOp = winrt::Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(Stream);
			if(!WaitForAsync(DecoderOp, StopFlag))
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}
			const auto Decoder = DecoderOp.GetResults();
			if(!Decoder)
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}

			const uint32_t Width = Decoder.PixelWidth();
			const uint32_t Height = Decoder.PixelHeight();
			if(Width == 0 || Height == 0)
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}

			const auto PixelDataOp = Decoder.GetPixelDataAsync(
				winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Rgba8,
				winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied,
				winrt::Windows::Graphics::Imaging::BitmapTransform(),
				winrt::Windows::Graphics::Imaging::ExifOrientationMode::IgnoreExifOrientation,
				winrt::Windows::Graphics::Imaging::ColorManagementMode::DoNotColorManage);
			if(!WaitForAsync(PixelDataOp, StopFlag))
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}
			const auto PixelData = PixelDataOp.GetResults();
			if(!PixelData)
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}

			const auto Pixels = PixelData.DetachPixelData();
			const size_t ExpectedSize = (size_t)Width * (size_t)Height * 4;
			if(Pixels.size() < ExpectedSize || StopFlag.load(std::memory_order_relaxed))
			{
				ClearAlbumArtColors(State);
				ClearSharedAlbumArt(pShared);
				return false;
			}

			std::vector<uint8_t> Copy(Pixels.begin(), Pixels.begin() + ExpectedSize);
			ComputeAlbumArtColors(Copy, (int)Width, (int)Height, State);
			SetSharedAlbumArt(pShared, std::move(Copy), (int)Width, (int)Height);
			return true;
		}
		catch(const winrt::hresult_error &)
		{
			ClearAlbumArtColors(State);
			ClearSharedAlbumArt(pShared);
			return false;
		}
	}
}

void ClearState(CMediaViewer::CWinrt *pWinrt, IGraphics *pGraphics)
{
	ClearAlbumArtLocal(pWinrt, pGraphics);
	pWinrt->m_State = CMediaViewer::CState{};
	pWinrt->m_HasMedia = false;
}

void ApplySharedAlbumArt(CMediaViewer::CShared *pShared, CMediaViewer::CWinrt *pWinrt, IGraphics *pGraphics)
{
	if(!pShared || !pWinrt || !pGraphics)
		return;

	int AlbumArtWidth = 0;
	int AlbumArtHeight = 0;
	std::vector<uint8_t> AlbumArtPixels;
	if(ConsumeSharedAlbumArt(pShared, AlbumArtPixels, AlbumArtWidth, AlbumArtHeight))
	{
		if(PrepareAlbumArtPixels(AlbumArtPixels, AlbumArtWidth, AlbumArtHeight))
		{
			pWinrt->m_AlbumArtPendingRgba = std::move(AlbumArtPixels);
			pWinrt->m_AlbumArtPendingWidth = AlbumArtWidth;
			pWinrt->m_AlbumArtPendingHeight = AlbumArtHeight;
		}
		else
		{
			ClearAlbumArtLocal(pWinrt, pGraphics);
			return;
		}
	}

	if(pWinrt->m_AlbumArtPendingRgba.empty() || pWinrt->m_AlbumArtPendingWidth <= 0 || pWinrt->m_AlbumArtPendingHeight <= 0)
		return;

	IGraphics::CTextureHandle NewAlbumArt;
	if(!LoadAlbumArtTexture(pGraphics, pWinrt->m_AlbumArtPendingRgba, pWinrt->m_AlbumArtPendingWidth, pWinrt->m_AlbumArtPendingHeight, "smtc_album_art", NewAlbumArt))
		return;

	if(pWinrt->m_State.m_PrevAlbumArt.m_Texture.IsValid())
		pGraphics->UnloadTexture(&pWinrt->m_State.m_PrevAlbumArt.m_Texture);
	pWinrt->m_State.m_PrevAlbumArt = pWinrt->m_State.m_AlbumArt;
	pWinrt->m_State.m_AlbumArt.m_Texture = NewAlbumArt;
	pWinrt->m_State.m_AlbumArt.m_Width = pWinrt->m_AlbumArtPendingWidth;
	pWinrt->m_State.m_AlbumArt.m_Height = pWinrt->m_AlbumArtPendingHeight;
	pWinrt->m_AlbumArtPendingRgba.clear();
	pWinrt->m_AlbumArtPendingWidth = 0;
	pWinrt->m_AlbumArtPendingHeight = 0;
}

void CMediaViewer::ThreadMain()
{
	try
	{
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
	}
	catch(const winrt::hresult_error &)
	{
		return;
	}

	constexpr int NoManagerSleep = 500;
	constexpr int SessionSleep = 150;

	{
		GlobalSystemMediaTransportControlsSessionManager Manager{nullptr};
		GlobalSystemMediaTransportControlsSession Session{nullptr};
		CPlainState State{};
		bool HasMedia = false;
		std::string AlbumArtKey;
		bool AlbumArtLoaded = false;
		auto LastPropsUpdate = std::chrono::steady_clock::now() - std::chrono::seconds(2);
		auto LastAlbumArtAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(10);

		while(!m_StopThread)
		{
			try
			{
				if(g_Config.m_ClMediaIsland == 0)
				{
					if(HasMedia)
					{
						ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
						AlbumArtLoaded = false;
					}
					std::this_thread::sleep_for(std::chrono::seconds(1));
					continue;
				}

				if(!Manager)
				{
					try
					{
						const auto RequestOp = GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
						if(!WaitForAsync(RequestOp, m_StopThread))
						{
							if(m_StopThread.load(std::memory_order_relaxed))
								break;
							Manager = nullptr;
						}
						else
						{
							Manager = RequestOp.GetResults();
						}
					}
					catch(const winrt::hresult_error &)
					{
						Manager = nullptr;
					}
				}

				if(!Manager)
				{
					if(HasMedia)
					{
						ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
						AlbumArtLoaded = false;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(NoManagerSleep));
					continue;
				}

				Session = Manager.GetCurrentSession();
				if(!Session)
				{
					if(HasMedia)
					{
						ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
						AlbumArtLoaded = false;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(SessionSleep));
					continue;
				}

				const auto PlaybackInfo = Session.GetPlaybackInfo();
				if(!PlaybackInfo)
				{
					if(HasMedia)
					{
						ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
						AlbumArtLoaded = false;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(SessionSleep));
					continue;
				}

				const auto Controls = PlaybackInfo.Controls();
				if(!Controls)
				{
					if(HasMedia)
					{
						ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
						AlbumArtLoaded = false;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(SessionSleep));
					continue;
				}

				State.m_CanPlay = Controls.IsPlayEnabled();
				State.m_CanPause = Controls.IsPauseEnabled();
				State.m_CanPrev = Controls.IsPreviousEnabled();
				State.m_CanNext = Controls.IsNextEnabled();
				State.m_Playing = PlaybackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

				const auto Timeline = Session.GetTimelineProperties();
				if(!Timeline)
				{
					if(HasMedia)
					{
						ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
						AlbumArtLoaded = false;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(SessionSleep));
					continue;
				}

				const int64_t Start100ns = Timeline.StartTime().count();
				const int64_t End100ns = Timeline.EndTime().count();
				const int64_t Position100ns = Timeline.Position().count();
				const int64_t Duration100ns = End100ns - Start100ns;
				const int64_t PositionRel100ns = Position100ns - Start100ns;
				State.m_DurationMs = Duration100ns > 0 ? Duration100ns / 10000 : 0;
				State.m_PositionMs = PositionRel100ns > 0 ? PositionRel100ns / 10000 : 0;
				HasMedia = true;

				const auto Now = std::chrono::steady_clock::now();
				if(Now - LastPropsUpdate >= std::chrono::seconds(1))
				{
					LastPropsUpdate = Now;
					try
					{
						const auto MediaPropsOp = Session.TryGetMediaPropertiesAsync();
						if(!WaitForAsync(MediaPropsOp, m_StopThread))
						{
							if(m_StopThread.load(std::memory_order_relaxed))
								break;
							ClearMediaDetails(State, AlbumArtKey, m_pShared.get());
							AlbumArtLoaded = false;
						}
						else
						{
							const auto MediaProps = MediaPropsOp.GetResults();
							if(!MediaProps)
							{
								ClearMediaDetails(State, AlbumArtKey, m_pShared.get());
								AlbumArtLoaded = false;
							}
							else
							{
								const std::string ServiceId = Session ? winrt::to_string(Session.SourceAppUserModelId()) : std::string();
								const std::string Title = winrt::to_string(MediaProps.Title());
								const std::string Artist = winrt::to_string(MediaProps.Artist());
								const std::string Album = winrt::to_string(MediaProps.AlbumTitle());

								State.m_ServiceId = ServiceId;
								State.m_Title = Title;
								State.m_Artist = Artist;
								State.m_Album = Album;

								const bool HasText = !Title.empty() || !Artist.empty() || !Album.empty();
								if(HasText)
								{
									const std::string NewKey = Title + "\n" + Artist + "\n" + Album;
									const bool TrackChanged = NewKey != AlbumArtKey;
									const bool RetryAlbumArt = !AlbumArtLoaded && Now - LastAlbumArtAttempt >= std::chrono::seconds(10);
									if(TrackChanged || RetryAlbumArt)
									{
										AlbumArtKey = NewKey;
										LastAlbumArtAttempt = Now;
										const auto Thumbnail = MediaProps.Thumbnail();
										if(Thumbnail)
											AlbumArtLoaded = UpdateAlbumArtData(m_pShared.get(), State, Thumbnail, m_StopThread);
										else
										{
											ClearAlbumArtColors(State);
											ClearSharedAlbumArt(m_pShared.get());
											AlbumArtLoaded = false;
										}
									}
								}
								else
								{
									ClearMediaDetails(State, AlbumArtKey, m_pShared.get());
									AlbumArtLoaded = false;
								}
							}
						}
					}
					catch(const winrt::hresult_error &)
					{
						ClearMediaDetails(State, AlbumArtKey, m_pShared.get());
						AlbumArtLoaded = false;
					}
				}

				std::deque<ECommand> Commands;
				PublishSharedState(m_pShared.get(), State, HasMedia, &Commands);
				if(Session)
				{
					for(const auto Command : Commands)
					{
						try
						{
							switch(Command)
							{
							case ECommand::Prev:
								Session.TrySkipPreviousAsync();
								break;
							case ECommand::PlayPause:
								Session.TryTogglePlayPauseAsync();
								break;
							case ECommand::Next:
								Session.TrySkipNextAsync();
								break;
							}
						}
						catch(const winrt::hresult_error &)
						{
						}
					}
				}
			}
			catch(const winrt::hresult_error &)
			{
				ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
				AlbumArtLoaded = false;
			}
			catch(...)
			{
				ResetSharedState(m_pShared.get(), State, HasMedia, AlbumArtKey);
				AlbumArtLoaded = false;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(SessionSleep));
		}
	}

	winrt::uninit_apartment();
}

void CMediaViewer::AudioThreadMain()
{
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	IMMDeviceEnumerator *pEnumerator = nullptr;
	IMMDevice *pDevice = nullptr;
	IAudioClient *pAudioClient = nullptr;
	IAudioCaptureClient *pCaptureClient = nullptr;

	HRESULT hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator),
		nullptr,
		CLSCTX_ALL,
		__uuidof(IMMDeviceEnumerator),
		(void **)&pEnumerator);

	if(SUCCEEDED(hr))
		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	if(SUCCEEDED(hr))
		hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)&pAudioClient);

	WAVEFORMATEX *pWaveFormat = nullptr;
	if(SUCCEEDED(hr))
		hr = pAudioClient->GetMixFormat(&pWaveFormat);
	if(SUCCEEDED(hr))
	{
		hr = pAudioClient->Initialize(
			AUDCLNT_SHAREMODE_SHARED,
			AUDCLNT_STREAMFLAGS_LOOPBACK,
			10000000,
			0,
			pWaveFormat,
			nullptr);
	}
	if(SUCCEEDED(hr))
		hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void **)&pCaptureClient);
	if(SUCCEEDED(hr))
		hr = pAudioClient->Start();

	const int SampleRate = pWaveFormat ? pWaveFormat->nSamplesPerSec : 48000;
	const int Channels = pWaveFormat ? pWaveFormat->nChannels : 2;

	std::vector<float> SampleBuffer;
	SampleBuffer.reserve(FFT::FFT_SIZE);

	while(!m_StopAudioThread && SUCCEEDED(hr))
	{
		UINT32 PacketLength = 0;
		hr = pCaptureClient->GetNextPacketSize(&PacketLength);

		while(SUCCEEDED(hr) && PacketLength != 0)
		{
			BYTE *pData = nullptr;
			UINT32 NumFramesAvailable = 0;
			DWORD Flags = 0;

			hr = pCaptureClient->GetBuffer(&pData, &NumFramesAvailable, &Flags, nullptr, nullptr);
			if(SUCCEEDED(hr))
			{
				if(!(Flags & AUDCLNT_BUFFERFLAGS_SILENT) && pData)
				{
					const float *pFloatData = reinterpret_cast<const float *>(pData);
					for(UINT32 i = 0; i < NumFramesAvailable; ++i)
					{
						float Sample = 0.0f;
						for(int ch = 0; ch < Channels; ++ch)
							Sample += pFloatData[i * Channels + ch];
						Sample /= (float)Channels;

						SampleBuffer.push_back(Sample);
						if(SampleBuffer.size() >= FFT::FFT_SIZE)
						{
							ProcessAudioFrame(SampleBuffer.data(), (int)SampleBuffer.size(), SampleRate);
							SampleBuffer.clear();
						}
					}
				}

				pCaptureClient->ReleaseBuffer(NumFramesAvailable);
			}

			hr = pCaptureClient->GetNextPacketSize(&PacketLength);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if(pAudioClient)
		pAudioClient->Stop();
	if(pCaptureClient)
		pCaptureClient->Release();
	if(pAudioClient)
		pAudioClient->Release();
	if(pDevice)
		pDevice->Release();
	if(pEnumerator)
		pEnumerator->Release();
	if(pWaveFormat)
		CoTaskMemFree(pWaveFormat);

	CoUninitialize();
}

void CMediaViewer::ProcessAudioFrame(const float *pSamples, int NumSamples, int SampleRate)
{
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

	bool AllZero = true;
	for(int band = 0; band < NumBands; ++band)
	{
		const float t0 = (float)band / NumBands;
		const float t1 = (float)(band + 1) / NumBands;
		const float freq0 = std::pow(10.0f, LogMin + t0 * (LogMax - LogMin));
		const float freq1 = std::pow(10.0f, LogMin + t1 * (LogMax - LogMin));

		const int bin0 = std::clamp((int)(freq0 / FreqPerBin), 0, UsableBins - 1);
		const int bin1 = std::clamp((int)(freq1 / FreqPerBin), bin0 + 1, UsableBins);

		float Sum = 0.0f;
		int Count = 0;
		for(int bin = bin0; bin < bin1; ++bin)
		{
			Sum += FftResult[bin].Magnitude();
			++Count;
		}

		if(Count > 0)
		{
			float Magnitude = Sum / Count;
			Magnitude = std::log10(1.0f + Magnitude * 100.0f) / 2.0f;
			Bands[band] = std::clamp(Magnitude, 0.0f, 1.0f);
			if(Magnitude > 0.001f)
				AllZero = false;
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
