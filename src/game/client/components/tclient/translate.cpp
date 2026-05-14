#include "translate.h"

#include <base/log.h>

#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/shared/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/localization.h>

#include <algorithm>
#include <cctype>
#include <memory>

static void UrlEncode(const char *pText, char *pOut, size_t Length)
{
	if(Length == 0)
		return;
	size_t OutPos = 0;
	for(const char *p = pText; *p && OutPos < Length - 1; ++p)
	{
		unsigned char c = *(const unsigned char *)p;
		if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			if(OutPos >= Length - 1)
				break;
			pOut[OutPos++] = c;
		}
		else
		{
			if(OutPos + 3 >= Length)
				break;
			snprintf(pOut + OutPos, 4, "%%%02X", c);
			OutPos += 3;
		}
	}
	pOut[OutPos] = '\0';
}

static std::string UrlDecode(std::string_view Encoded)
{
	std::string Decoded;
	Decoded.reserve(Encoded.size());
	for(size_t i = 0; i < Encoded.size(); ++i)
	{
		const char c = Encoded[i];
		if(c == '%' && i + 2 < Encoded.size())
		{
			auto HexToInt = [](char Hex) -> int {
				if(Hex >= '0' && Hex <= '9')
					return Hex - '0';
				if(Hex >= 'a' && Hex <= 'f')
					return 10 + (Hex - 'a');
				if(Hex >= 'A' && Hex <= 'F')
					return 10 + (Hex - 'A');
				return -1;
			};
			const int High = HexToInt(Encoded[i + 1]);
			const int Low = HexToInt(Encoded[i + 2]);
			if(High >= 0 && Low >= 0)
			{
				Decoded.push_back((char)((High << 4) | Low));
				i += 2;
				continue;
			}
		}
		else if(c == '+')
		{
			Decoded.push_back(' ');
			continue;
		}
		Decoded.push_back(c);
	}
	return Decoded;
}

static void NormalizeTranslatedText(char *pText, size_t Size)
{
	if(!pText || Size == 0 || pText[0] == '\0')
		return;

	int EscapedSequenceCount = 0;
	for(const char *p = pText; p[0] != '\0' && p[1] != '\0' && p[2] != '\0'; ++p)
	{
		if(p[0] == '%' && std::isxdigit((unsigned char)p[1]) && std::isxdigit((unsigned char)p[2]))
			++EscapedSequenceCount;
	}

	if(EscapedSequenceCount == 0)
		return;

	const bool LooksEncoded = EscapedSequenceCount >= 2 || str_find(pText, "%20") || str_find(pText, "%3D") || str_find(pText, "%2F") || str_find(pText, "%3A");
	if(!LooksEncoded)
		return;

	const std::string Decoded = UrlDecode(pText);
	if(Decoded.empty() || str_comp(Decoded.c_str(), pText) == 0)
		return;

	str_copy(pText, Decoded.c_str(), Size);
}

static void NormalizeLanguageCode(const char *pLanguage, char *pOut, size_t Size)
{
	if(Size == 0)
		return;

	pOut[0] = '\0';
	if(!pLanguage || pLanguage[0] == '\0')
		return;

	str_utf8_tolower(pLanguage, pOut, Size);

	for(char *p = pOut; *p; ++p)
	{
		if(*p == '-' || *p == '_' || *p == ' ')
		{
			*p = '\0';
			break;
		}
	}
}

static bool HandleLanguageBlacklist(const char *pLanguage)
{
	if(!pLanguage || pLanguage[0] == '\0' || g_Config.m_EcTranslateLanguageBlacklist[0] == '\0')
		return false;

	char aLanguage[16];
	NormalizeLanguageCode(pLanguage, aLanguage, sizeof(aLanguage));
	if(aLanguage[0] == '\0')
		return false;

	char aToken[16];
	char aTokenNormalized[16];
	const char *pList = g_Config.m_EcTranslateLanguageBlacklist;
	while((pList = str_next_token(pList, ", ", aToken, sizeof(aToken))) != nullptr)
	{
		NormalizeLanguageCode(aToken, aTokenNormalized, sizeof(aTokenNormalized));
		if(aTokenNormalized[0] != '\0' && str_comp_nocase(aLanguage, aTokenNormalized) == 0)
			return true;
	}

	return false;
}

static bool HandleLanguageWhitelist(const char *pLanguage)
{
	if(!pLanguage || pLanguage[0] == '\0' || g_Config.m_EcTranslateLanguageWhitelist[0] == '\0')
		return true;

	char aLanguage[16];
	NormalizeLanguageCode(pLanguage, aLanguage, sizeof(aLanguage));
	if(aLanguage[0] == '\0')
		return true;

	char aToken[16];
	char aTokenNormalized[16];
	const char *pList = g_Config.m_EcTranslateLanguageWhitelist;

	while((pList = str_next_token(pList, ", ", aToken, sizeof(aToken))) != nullptr)
	{
		NormalizeLanguageCode(aToken, aTokenNormalized, sizeof(aTokenNormalized));
		if(aTokenNormalized[0] != '\0' && str_comp_nocase(aLanguage, aTokenNormalized) == 0)
			return true;
	}
	return false;
}

const char *ITranslateBackend::EncodeTarget(const char *pTarget) const
{
	if(!pTarget || pTarget[0] == '\0')
		return DefaultConfig::EcTranslateTarget;
	return pTarget;
}

bool ITranslateBackend::CompareTargets(const char *pA, const char *pB) const
{
	if(pA == pB) // if(!pA && !pB)
		return true;
	if(!pA || !pB)
		return false;
	if(str_comp_nocase(EncodeTarget(pA), EncodeTarget(pB)) == 0)
		return true;
	return false;
}

class ITranslateBackendHttp : public ITranslateBackend
{
protected:
	std::shared_ptr<CHttpRequest> m_pHttpRequest = nullptr;
	virtual bool ParseResponse(CTranslateResponse &Out) = 0;
	virtual bool ParseHttpError() const { return false; }

	void CreateHttpRequest(IHttp &Http, const char *pUrl)
	{
		auto pGet = std::make_shared<CHttpRequest>(pUrl);
		pGet->LogProgress(HTTPLOG::FAILURE);
		pGet->FailOnErrorStatus(false);
		pGet->Timeout(CTimeout{10000, 0, 500, 10});

		m_pHttpRequest = pGet;
		Http.Run(pGet);
	}

public:
	std::optional<bool> Update(CTranslateResponse &Out) override
	{
		dbg_assert(m_pHttpRequest != nullptr, "m_pHttpRequest is nullptr");
		if(m_pHttpRequest->State() == EHttpState::RUNNING || m_pHttpRequest->State() == EHttpState::QUEUED)
			return std::nullopt;
		if(m_pHttpRequest->State() == EHttpState::ABORTED)
		{
			str_copy(Out.m_Text, "Aborted");
			return false;
		}
		if(m_pHttpRequest->State() != EHttpState::DONE)
		{
			str_copy(Out.m_Text, "Curl error");
			return false;
		}
		if(m_pHttpRequest->StatusCode() != 200 && !ParseHttpError())
		{
			str_format(Out.m_Text, sizeof(Out.m_Text), "Got http code %d", m_pHttpRequest->StatusCode());
			return false;
		}
		return ParseResponse(Out);
	}
	~ITranslateBackendHttp() override
	{
		if(m_pHttpRequest)
			m_pHttpRequest->Abort();
	}
};

class CTranslateBackendLibretranslate : public ITranslateBackendHttp
{
private:
	bool ParseResponseJson(const json_value *pObj, CTranslateResponse &Out)
	{
		if(!pObj)
		{
			str_copy(Out.m_Text, "Response is not JSON");
			return false;
		}

		if(pObj->type != json_object)
		{
			str_copy(Out.m_Text, "Response is not object");
			return false;
		}

		const json_value *pError = json_object_get(pObj, "error");
		if(pError != &json_value_none)
		{
			if(pError->type != json_string)
				str_copy(Out.m_Text, "Error is not string");
			else
				str_copy(Out.m_Text, pError->u.string.ptr);
			return false;
		}

		const json_value *pTranslatedText = json_object_get(pObj, "translatedText");
		if(pTranslatedText == &json_value_none)
		{
			str_copy(Out.m_Text, "No translatedText");
			return false;
		}
		if(pTranslatedText->type != json_string)
		{
			str_copy(Out.m_Text, "translatedText is not string");
			return false;
		}

		const json_value *pDetectedLanguage = json_object_get(pObj, "detectedLanguage");
		if(pDetectedLanguage == &json_value_none)
		{
			str_copy(Out.m_Text, "No pDetectedLanguage");
			return false;
		}
		if(pDetectedLanguage->type != json_object)
		{
			str_copy(Out.m_Text, "pDetectedLanguage is not object");
			return false;
		}

		const json_value *pConfidence = json_object_get(pDetectedLanguage, "confidence");
		if(pConfidence == &json_value_none || ((pConfidence->type == json_double && pConfidence->u.dbl == 0.0f) ||
							      (pConfidence->type == json_integer && pConfidence->u.integer == 0)))
		{
			str_copy(Out.m_Text, "Unknown language");
			return false;
		}

		const json_value *pLanguage = json_object_get(pDetectedLanguage, "language");
		if(pLanguage == &json_value_none)
		{
			str_copy(Out.m_Text, "No language");
			return false;
		}
		if(pLanguage->type != json_string)
		{
			str_copy(Out.m_Text, "language is not string");
			return false;
		}

		str_copy(Out.m_Text, pTranslatedText->u.string.ptr);
		str_copy(Out.m_Language, pLanguage->u.string.ptr);

		return true;
	}

protected:
	bool ParseResponse(CTranslateResponse &Out) override
	{
		json_value *pObj = m_pHttpRequest->ResultJson();
		bool Res = ParseResponseJson(pObj, Out);
		json_value_free(pObj);
		return Res;
	}
	bool ParseHttpError() const override { return true; }

public:
	const char *Name() const override
	{
		return "LibreTranslate";
	}
	CTranslateBackendLibretranslate(IHttp &Http, const char *pText)
	{
		CJsonStringWriter Json = CJsonStringWriter();
		Json.BeginObject();
		Json.WriteAttribute("q");
		Json.WriteStrValue(pText);
		Json.WriteAttribute("source");
		Json.WriteStrValue("auto");
		Json.WriteAttribute("target");
		Json.WriteStrValue(EncodeTarget(g_Config.m_EcTranslateTarget));
		Json.WriteAttribute("format");
		Json.WriteStrValue("text");
		if(g_Config.m_EcTranslateKey[0] != '\0')
		{
			Json.WriteAttribute("api_key");
			Json.WriteStrValue(g_Config.m_EcTranslateKey);
		}
		Json.EndObject();
		CreateHttpRequest(Http, g_Config.m_EcTranslateEndpoint[0] == '\0' ? "localhost:5000/translate" : g_Config.m_EcTranslateEndpoint);
		const char *pJson = Json.GetOutputString().c_str();
		m_pHttpRequest->PostJson(pJson);
	}
};

class CTranslateBackendFtapi : public ITranslateBackendHttp
{
private:
	bool ParseResponseJson(const json_value *pObj, CTranslateResponse &Out)
	{
		if(!pObj)
		{
			str_copy(Out.m_Text, "Response is not JSON");
			return false;
		}

		if(pObj->type != json_object)
		{
			str_copy(Out.m_Text, "Response is not object");
			return false;
		}

		const json_value *pTranslatedText = json_object_get(pObj, "destination-text");
		if(pTranslatedText == &json_value_none)
		{
			str_copy(Out.m_Text, "No destination-text");
			return false;
		}
		if(pTranslatedText->type != json_string)
		{
			str_copy(Out.m_Text, "destination-text is not string");
			return false;
		}

		const json_value *pDetectedLanguage = json_object_get(pObj, "source-language");
		if(pDetectedLanguage == &json_value_none)
		{
			str_copy(Out.m_Text, "No source-language");
			return false;
		}
		if(pDetectedLanguage->type != json_string)
		{
			str_copy(Out.m_Text, "source-language is not string");
			return false;
		}

		str_copy(Out.m_Text, pTranslatedText->u.string.ptr);
		str_copy(Out.m_Language, pDetectedLanguage->u.string.ptr);

		return true;
	}

protected:
	bool ParseResponse(CTranslateResponse &Out) override
	{
		json_value *pObj = m_pHttpRequest->ResultJson();
		bool Res = ParseResponseJson(pObj, Out);
		json_value_free(pObj);
		return Res;
	}

public:
	const char *EncodeTarget(const char *pTarget) const override
	{
		if(!pTarget || pTarget[0] == '\0')
			return DefaultConfig::EcTranslateTarget;
		if(str_comp_nocase(pTarget, "zh") == 0)
			return "zh-cn";
		return pTarget;
	}
	const char *Name() const override
	{
		return "FreeTranslateAPI";
	}
	CTranslateBackendFtapi(IHttp &Http, const char *pText)
	{
		char aBuf[4096];
		str_format(aBuf, sizeof(aBuf), "%s/translate?dl=%s&text=",
			g_Config.m_EcTranslateEndpoint[0] != '\0' ? g_Config.m_EcTranslateEndpoint : "https://ftapi.pythonanywhere.com",
			EncodeTarget(g_Config.m_EcTranslateTarget));

		UrlEncode(pText, aBuf + strlen(aBuf), sizeof(aBuf) - strlen(aBuf));

		CreateHttpRequest(Http, aBuf);
	}
};

class CTranslateBackendDeeplFree : public ITranslateBackendHttp
{
private:
	bool ParseResponseJson(const json_value *pObj, CTranslateResponse &Out)
	{
		if(!pObj)
		{
			str_copy(Out.m_Text, "Response is not JSON");
			return false;
		}

		if(pObj->type != json_object)
		{
			str_copy(Out.m_Text, "Response is not object");
			return false;
		}

		const json_value *pMessage = json_object_get(pObj, "message");
		if(pMessage != &json_value_none)
		{
			if(pMessage->type == json_string)
				str_copy(Out.m_Text, pMessage->u.string.ptr);
			else
				str_copy(Out.m_Text, "DeepL error");
			return false;
		}

		const json_value *pTranslations = json_object_get(pObj, "translations");
		if(pTranslations == &json_value_none)
		{
			str_copy(Out.m_Text, "No translations");
			return false;
		}
		if(pTranslations->type != json_array || json_array_length(pTranslations) <= 0)
		{
			str_copy(Out.m_Text, "translations is invalid");
			return false;
		}

		const json_value *pTranslation = json_array_get(pTranslations, 0);
		if(pTranslation == &json_value_none || pTranslation->type != json_object)
		{
			str_copy(Out.m_Text, "translation is invalid");
			return false;
		}

		const json_value *pTranslatedText = json_object_get(pTranslation, "text");
		if(pTranslatedText == &json_value_none)
		{
			str_copy(Out.m_Text, "No text");
			return false;
		}
		if(pTranslatedText->type != json_string)
		{
			str_copy(Out.m_Text, "text is not string");
			return false;
		}

		const json_value *pDetectedLanguage = json_object_get(pTranslation, "detected_source_language");
		if(pDetectedLanguage != &json_value_none && pDetectedLanguage->type != json_string)
		{
			str_copy(Out.m_Text, "detected_source_language is not string");
			return false;
		}

		str_copy(Out.m_Text, pTranslatedText->u.string.ptr);
		if(pDetectedLanguage != &json_value_none)
			str_copy(Out.m_Language, pDetectedLanguage->u.string.ptr);
		else
			Out.m_Language[0] = '\0';

		return true;
	}

protected:
	bool ParseResponse(CTranslateResponse &Out) override
	{
		json_value *pObj = m_pHttpRequest->ResultJson();
		bool Res = ParseResponseJson(pObj, Out);
		json_value_free(pObj);
		return Res;
	}
	bool ParseHttpError() const override { return true; }

public:
	const char *EncodeTarget(const char *pTarget) const override
	{
		if(!pTarget || pTarget[0] == '\0')
			return DefaultConfig::EcTranslateTarget;
		if(str_comp_nocase(pTarget, "zh") == 0)
			return "ZH";
		if(str_comp_nocase(pTarget, "en") == 0)
			return "EN";
		if(str_comp_nocase(pTarget, "pt") == 0)
			return "PT-PT";
		static char s_aTarget[16];
		str_copy(s_aTarget, pTarget);
		for(char *p = s_aTarget; *p; ++p)
			*p = str_uppercase(*p);
		return s_aTarget;
	}
	const char *Name() const override
	{
		return "DeepL Free";
	}
	CTranslateBackendDeeplFree(IHttp &Http, const char *pText)
	{
		CJsonStringWriter Json;
		Json.BeginObject();
		Json.WriteAttribute("text");
		Json.BeginArray();
		Json.WriteStrValue(pText);
		Json.EndArray();
		Json.WriteAttribute("target_lang");
		Json.WriteStrValue(EncodeTarget(g_Config.m_EcTranslateTarget));
		Json.EndObject();

		CreateHttpRequest(Http, g_Config.m_EcTranslateEndpoint[0] == '\0' ? "https://api-free.deepl.com/v2/translate" : g_Config.m_EcTranslateEndpoint);
		char aAuth[320];
		str_format(aAuth, sizeof(aAuth), "DeepL-Auth-Key %s", g_Config.m_EcTranslateKey);
		m_pHttpRequest->HeaderString("Authorization", aAuth);
		const char *pJson = Json.GetOutputString().c_str();
		m_pHttpRequest->PostJson(pJson);
	}
};

class CTranslateBackendGoogle : public ITranslateBackendHttp
{
private:
	bool ParseResponseJson(const json_value *pObj, CTranslateResponse &Out)
	{
		if(!pObj)
		{
			str_copy(Out.m_Text, "Response is not JSON");
			return false;
		}

		if(pObj->type != json_array)
		{
			str_copy(Out.m_Text, "Response is not array");
			return false;
		}

		const json_value *pSentences = json_array_get(pObj, 0);
		if(!pSentences || pSentences->type != json_array)
		{
			str_copy(Out.m_Text, "Missing translation entries");
			return false;
		}

		std::string Result;
		for(int i = 0; i < json_array_length(pSentences); ++i)
		{
			const json_value *pSentence = json_array_get(pSentences, i);
			if(!pSentence || pSentence->type != json_array)
				continue;

			const json_value *pTranslated = json_array_get(pSentence, 0);
			if(!pTranslated || pTranslated->type != json_string)
				continue;

			Result += pTranslated->u.string.ptr;
		}

		if(Result.empty())
		{
			str_copy(Out.m_Text, "Translation empty");
			return false;
		}

		str_copy(Out.m_Text, Result.c_str(), sizeof(Out.m_Text));
		NormalizeTranslatedText(Out.m_Text, sizeof(Out.m_Text));

		const json_value *pDetectedLanguage = json_array_get(pObj, 2);
		if(pDetectedLanguage && pDetectedLanguage->type == json_string)
			str_copy(Out.m_Language, pDetectedLanguage->u.string.ptr, sizeof(Out.m_Language));
		else
			Out.m_Language[0] = '\0';

		return true;
	}

protected:
	bool ParseResponse(CTranslateResponse &Out) override
	{
		json_value *pObj = m_pHttpRequest->ResultJson();
		bool Res = ParseResponseJson(pObj, Out);
		json_value_free(pObj);
		return Res;
	}

public:
	const char *Name() const override
	{
		return "Google Translate";
	}

	CTranslateBackendGoogle(IHttp &Http, const char *pText)
	{
		char aBuf[4096];
		str_format(aBuf, sizeof(aBuf), "https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=%s&dt=t&q=",
			EncodeTarget(g_Config.m_EcTranslateTarget));
		UrlEncode(pText, aBuf + strlen(aBuf), sizeof(aBuf) - strlen(aBuf));
		CreateHttpRequest(Http, aBuf);
	}
};

void CTranslate::ConTranslate(IConsole::IResult *pResult, void *pUserData)
{
	const char *pName;
	if(pResult->NumArguments() == 0)
		pName = nullptr;
	else
		pName = pResult->GetString(0);

	CTranslate *pThis = static_cast<CTranslate *>(pUserData);
	pThis->Translate(pName, true);
}

void CTranslate::ConTranslateId(IConsole::IResult *pResult, void *pUserData)
{
	CTranslate *pThis = static_cast<CTranslate *>(pUserData);
	pThis->Translate(pResult->GetInteger(0), true);
}

void CTranslate::OnConsoleInit()
{
	Console()->Register("translate", "?r[name]", CFGFLAG_CLIENT, ConTranslate, this, "Translate last message (of a given name)");
	Console()->Register("translate_id", "v[id]", CFGFLAG_CLIENT, ConTranslateId, this, "Translate last message of the person with this id");
}

void CTranslate::Translate(int Id, bool Manual)
{
	if(Id < 0 || Id > (int)std::size(GameClient()->m_aClients))
	{
		GameClient()->m_Chat.Echo("Not a valid ID");
		return;
	}
	const auto &Player = GameClient()->m_aClients[Id];
	if(!Player.m_Active)
	{
		GameClient()->m_Chat.Echo("ID not connected");
		return;
	}
	Translate(Player.m_aName, Manual);
}

void CTranslate::Translate(const char *pName, bool Manual)
{
	CChat::CLine *pLineBest = nullptr;
	if(GameClient()->m_Chat.m_CurrentLine > 0)
	{
		int ScoreBest = -1;
		for(int i = 0; i < MAX_LINES; i++)
		{
			CChat::CLine *pLine = &GameClient()->m_Chat.m_aLines[((GameClient()->m_Chat.m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
			if(pLine->m_pTranslateResponse)
			{
				if(pLine->m_pTranslateResponse->m_Auto && Manual)
				{
					pLine->m_pTranslateResponse->m_Auto = false;
				}
				else
					continue;
			}
			if(pLine->m_ClientId == CChat::CLIENT_MSG)
				continue;
			if(pLine->m_ClientId == CChat::ECLIENT_MSG)
				continue;
			if(pLine->m_ClientId == CChat::SILENT_MSG)
				continue;
			for(int Id : GameClient()->m_aLocalIds)
				if(pLine->m_ClientId == Id)
					continue;
			int Score = 0;
			if(pName)
			{
				if(pLine->m_ClientId == CChat::SERVER_MSG)
					continue;
				if(str_comp(pLine->m_aName, pName) == 0)
					Score = 2;
				else if(str_comp_nocase(pLine->m_aName, pName) == 0)
					Score = 1;
				else
					continue;
			}
			if(Score > ScoreBest)
			{
				ScoreBest = Score;
				pLineBest = pLine;
			}
		}
	}
	if(!pLineBest || pLineBest->m_aText[0] == '\0')
	{
		GameClient()->m_Chat.Echo("No message to translate");
		return;
	}

	Translate(*pLineBest, Manual);
}

void CTranslate::Translate(CChat::CLine &Line, bool Manual)
{
	if(m_vJobs.size() > 15)
	{
		return;
	}

	CTranslateJob Job;
	Job.m_pLine = &Line;
	Job.m_pTranslateResponse = std::make_shared<CTranslateResponse>();
	Job.m_pLine->m_pTranslateResponse = Job.m_pTranslateResponse;

	if(str_comp_nocase(g_Config.m_EcTranslateBackend, "libretranslate") == 0)
		Job.m_pBackend = std::make_unique<CTranslateBackendLibretranslate>(*Http(), Job.m_pLine->m_aText);
	else if(str_comp_nocase(g_Config.m_EcTranslateBackend, "ftapi") == 0)
		Job.m_pBackend = std::make_unique<CTranslateBackendFtapi>(*Http(), Job.m_pLine->m_aText);
	else if(str_comp_nocase(g_Config.m_EcTranslateBackend, "deeplfree") == 0 || str_comp_nocase(g_Config.m_EcTranslateBackend, "deepl") == 0)
		Job.m_pBackend = std::make_unique<CTranslateBackendDeeplFree>(*Http(), Job.m_pLine->m_aText);
	else if(str_comp_nocase(g_Config.m_EcTranslateBackend, "google") == 0)
		Job.m_pBackend = std::make_unique<CTranslateBackendGoogle>(*Http(), Job.m_pLine->m_aText);
	else
	{
		GameClient()->m_Chat.Echo("Invalid translate backend");
		return;
	}

	if(Manual)
	{
		str_format(Job.m_pTranslateResponse->m_Text, sizeof(Job.m_pTranslateResponse->m_Text), Localize("%s translating to %s", "translate"), Job.m_pBackend->Name(), g_Config.m_EcTranslateTarget);
		Job.m_pLine->m_Time = time();
	}
	else
	{
		Job.m_pTranslateResponse->m_Text[0] = '\0';
	}

	Job.m_pTranslateResponse->m_Auto = !Manual;

	m_vJobs.emplace_back(std::move(Job));

	if(Manual)
		GameClient()->m_Chat.RebuildChat();
}

void CTranslate::OnRender()
{
	if(m_vJobs.empty())
		return;

	const auto Time = time();
	auto ForEach = [&](CTranslateJob &Job) {
		if(Job.m_pLine->m_pTranslateResponse != Job.m_pTranslateResponse)
			return true; // Not the same line anymore
		const std::optional<bool> Done = Job.m_pBackend->Update(*Job.m_pTranslateResponse);
		if(!Done.has_value())
			return false; // Keep ongoing tasks
		if(*Done)
		{
			if(Job.m_pTranslateResponse->m_Auto)
			{
				if(!HandleLanguageWhitelist(Job.m_pTranslateResponse->m_Language))
				{
					Job.m_pTranslateResponse->m_Text[0] = '\0';
				}

				else if(HandleLanguageBlacklist(Job.m_pTranslateResponse->m_Language))
				{
					Job.m_pTranslateResponse->m_Text[0] = '\0';
				}
			}
			if(Job.m_pBackend->CompareTargets(Job.m_pTranslateResponse->m_Language, g_Config.m_EcTranslateTarget))
			{
				Job.m_pTranslateResponse->m_Text[0] = '\0';
			}
			else if(str_comp_nocase(Job.m_pLine->m_aText, Job.m_pTranslateResponse->m_Text) == 0) // Check for no translation difference
			{
				Job.m_pTranslateResponse->m_Text[0] = '\0';
				Job.m_pTranslateResponse->m_Language[0] = '\0';
			}
		}
		else
		{
			char aBuf[sizeof(Job.m_pTranslateResponse->m_Text)];
			str_format(aBuf, sizeof(aBuf), Localize("%s to %s failed: %s", "translate"), Job.m_pBackend->Name(), g_Config.m_EcTranslateTarget, Job.m_pTranslateResponse->m_Text);
			Job.m_pTranslateResponse->m_Error = true;
			str_copy(Job.m_pTranslateResponse->m_Text, aBuf);
		}
		Job.m_pLine->m_Time = Time;
		GameClient()->m_Chat.RebuildChat();
		return true;
	};
	m_vJobs.erase(std::remove_if(m_vJobs.begin(), m_vJobs.end(), ForEach), m_vJobs.end());
}

void CTranslate::AutoTranslate(CChat::CLine &Line)
{
	static CChat::CLine s_LastLines[5] = {CChat::CLine()};
	static int s_LastLineIndex = 0;

	for(CChat::CLine &LastLine : s_LastLines)
	{
		if(LastLine.m_ClientId == Line.m_ClientId && Line.m_Time <= LastLine.m_Time + time_freq() * 0.01f)
			return;
	}

	if(!g_Config.m_EcTranslateAuto)
		return;
	if(Line.m_ClientId <= CChat::SERVER_MSG)
		return;
	for(const int Id : GameClient()->m_aLocalIds)
	{
		if(Id >= 0 && Id == Line.m_ClientId)
			return;
	}
	if(str_comp(g_Config.m_EcTranslateBackend, "ftapi") == 0)
	{
		// FTAPI quickly gets overloaded, please do not disable this
		// It may shut down if we spam it too hard
		return;
	}
	if((str_comp(g_Config.m_EcTranslateBackend, "deeplfree") == 0 || str_comp(g_Config.m_EcTranslateBackend, "deepl") == 0) && g_Config.m_EcTranslateKey[0] == '\0')
		return;

	Translate(Line, false);
	s_LastLines[s_LastLineIndex] = Line;
	s_LastLineIndex = (s_LastLineIndex + 1) % 5;
}
