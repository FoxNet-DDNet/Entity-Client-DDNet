#include "translate.h"

#include <base/log.h>

#include <engine/http.h>
#include <engine/shared/json.h>
#include <engine/shared/jsonwriter.h>
#include <engine/shared/protocol.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/localization.h>

#include <algorithm>
#include <cctype>
#include <memory>

// How many requests a single job may send before it gives up on being rate limited
static constexpr int RATE_LIMIT_MAX_ATTEMPTS = 3;
// A backoff never grows past its backend maximum, this only keeps the shift sane
static constexpr int RATE_LIMIT_MAX_STRIKES = 16;
// How long a request may wait for its turn before it is dropped, translating a
// message that scrolled out of the chat long ago helps nobody
static constexpr int64_t QUEUE_MAX_WAIT_SECONDS_AUTO = 5;
static constexpr int64_t QUEUE_MAX_WAIT_SECONDS_MANUAL = 30;

class CTranslateBackendLimits
{
public:
	// Minimum time between two requests, sending faster than this is what gets
	// us rate limited in the first place
	int m_MinIntervalMs;
	// First backoff after a rate limit, doubles for every rate limit that
	// follows until the maximum is reached
	int m_BackoffSeconds;
	int m_BackoffMaxSeconds;
};

static CTranslateBackendLimits TranslateBackendLimits(ETranslateBackend Backend)
{
	switch(Backend)
	{
	case ETranslateBackend::LIBRETRANSLATE:
		// Usually self hosted, no reason to hold back
		return {0, 5, 60};
	case ETranslateBackend::DEEPL:
		return {500, 30, 600};
	case ETranslateBackend::GOOGLE:
		// Punishes bursts hard and stays angry for a long time afterwards
		return {1500, 60, 900};
	default:
		return {0, 5, 60};
	}
}

const char *CTranslate::BackendName(ETranslateBackend Backend)
{
	switch(Backend)
	{
	case ETranslateBackend::LIBRETRANSLATE:
		return "LibreTranslate";
	case ETranslateBackend::DEEPL:
		return "DeepL";
	case ETranslateBackend::GOOGLE:
		return "Google Translate";
	default:
		return "Translate";
	}
}

CTranslate::EBackendRequirement CTranslate::BackendRequirement(ETranslateBackend Backend)
{
	switch(Backend)
	{
	case ETranslateBackend::DEEPL:
		// Every request carries the key as an Authorization header, there is nothing to fall back
		// on without one
		return EBackendRequirement::API_KEY;
	case ETranslateBackend::LIBRETRANSLATE:
		// There is no public instance to aim at, an endpoint that was never set means the requests
		// go to a server on this machine
		return EBackendRequirement::ENDPOINT;
	default:
		return EBackendRequirement::NONE;
	}
}

bool CTranslate::BackendReady(ETranslateBackend Backend)
{
	switch(BackendRequirement(Backend))
	{
	case EBackendRequirement::API_KEY:
		return g_Config.m_EcTranslateKey[0] != '\0';
	case EBackendRequirement::ENDPOINT:
		return g_Config.m_EcTranslateEndpoint[0] != '\0';
	default:
		return true;
	}
}

int CTranslate::NumBackends()
{
	// INVALID is not something anyone can pick
	return (int)ETranslateBackend::NUM - 1;
}

int CTranslate::BackendConfigValue(int Index)
{
	return Index + 1;
}

ETranslateBackend CTranslate::Backend()
{
	const int Value = g_Config.m_EcTranslateBackend;
	// Anything out of range lands on the default rather than on nothing. Zero in particular is what
	// the config parser makes of the backend name this setting used to hold, so a config written
	// before it became a number keeps translating instead of going quiet.
	if(Value <= (int)ETranslateBackend::INVALID || Value >= (int)ETranslateBackend::NUM)
		return ETranslateBackend::GOOGLE;
	return (ETranslateBackend)Value;
}

// Time between requests of a backend, the config overrides the backend default
static int64_t TranslateMinInterval(ETranslateBackend Backend)
{
	const int64_t Milliseconds = g_Config.m_EcTranslateMinInterval >= 0 ? g_Config.m_EcTranslateMinInterval : TranslateBackendLimits(Backend).m_MinIntervalMs;
	return Milliseconds * time_freq() / 1000;
}

// Rounds a duration up to full seconds, for messages shown to the user
static int TimeToSeconds(int64_t Time)
{
	return (int)((Time + time_freq() - 1) / time_freq());
}

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

void CTranslate::NormalizeLanguage(const char *pLanguage, char *pOut, size_t Size)
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

bool CTranslate::IsLanguageInList(const char *pList, const char *pLanguage)
{
	if(!pList || pList[0] == '\0')
		return false;

	char aLanguage[16];
	NormalizeLanguage(pLanguage, aLanguage, sizeof(aLanguage));
	if(aLanguage[0] == '\0')
		return false;

	char aToken[16];
	char aTokenNormalized[16];
	while((pList = str_next_token(pList, ", ", aToken, sizeof(aToken))) != nullptr)
	{
		NormalizeLanguage(aToken, aTokenNormalized, sizeof(aTokenNormalized));
		if(aTokenNormalized[0] != '\0' && str_comp_nocase(aLanguage, aTokenNormalized) == 0)
			return true;
	}

	return false;
}

class CTranslateLanguage
{
public:
	const char *m_pCode;
	const char *m_pName;
};

// Sorted by name, which is the order a picker shows them in. Codes are the short forms
// NormalizeLanguage reduces everything to, so "zh-CN" and "zh" both land on the Chinese entry.
static const CTranslateLanguage gs_aLanguages[] = {
	{"sq", "Albanian"},
	{"ar", "Arabic"},
	{"bn", "Bengali"},
	{"bg", "Bulgarian"},
	{"zh", "Chinese"},
	{"hr", "Croatian"},
	{"cs", "Czech"},
	{"da", "Danish"},
	{"nl", "Dutch"},
	{"en", "English"},
	{"et", "Estonian"},
	{"tl", "Filipino"},
	{"fi", "Finnish"},
	{"fr", "French"},
	{"de", "German"},
	{"el", "Greek"},
	{"he", "Hebrew"},
	{"hi", "Hindi"},
	{"hu", "Hungarian"},
	{"id", "Indonesian"},
	{"it", "Italian"},
	{"ja", "Japanese"},
	{"ko", "Korean"},
	{"lv", "Latvian"},
	{"lt", "Lithuanian"},
	{"mk", "Macedonian"},
	{"ms", "Malay"},
	{"no", "Norwegian"},
	{"fa", "Persian"},
	{"pl", "Polish"},
	{"pt", "Portuguese"},
	{"ro", "Romanian"},
	{"ru", "Russian"},
	{"sr", "Serbian"},
	{"sk", "Slovak"},
	{"sl", "Slovenian"},
	{"es", "Spanish"},
	{"sv", "Swedish"},
	{"th", "Thai"},
	{"tr", "Turkish"},
	{"uk", "Ukrainian"},
	{"ur", "Urdu"},
	{"vi", "Vietnamese"},
};

int CTranslate::NumLanguages()
{
	return (int)std::size(gs_aLanguages);
}

const char *CTranslate::LanguageCode(int Index)
{
	if(Index < 0 || Index >= NumLanguages())
		return "";
	return gs_aLanguages[Index].m_pCode;
}

const char *CTranslate::LanguageName(int Index)
{
	if(Index < 0 || Index >= NumLanguages())
		return "";
	return gs_aLanguages[Index].m_pName;
}

void CTranslate::SetLanguageInList(char *pList, size_t Size, const char *pLanguage, bool Add)
{
	char aLanguage[16];
	NormalizeLanguage(pLanguage, aLanguage, sizeof(aLanguage));
	if(aLanguage[0] == '\0')
		return;

	// The list is rebuilt rather than edited in place, which also drops duplicates and the empty
	// entries a hand written list tends to carry along
	char aResult[256] = "";
	char aToken[16];
	char aTokenNormalized[16];
	const char *pRead = pList;
	while((pRead = str_next_token(pRead, ", ", aToken, sizeof(aToken))) != nullptr)
	{
		NormalizeLanguage(aToken, aTokenNormalized, sizeof(aTokenNormalized));
		if(aTokenNormalized[0] == '\0')
			continue;
		if(str_comp_nocase(aTokenNormalized, aLanguage) == 0)
			continue;
		if(aResult[0] != '\0')
			str_append(aResult, ",");
		str_append(aResult, aTokenNormalized);
	}

	if(Add)
	{
		if(aResult[0] != '\0')
			str_append(aResult, ",");
		str_append(aResult, aLanguage);
	}

	str_copy(pList, aResult, Size);
}

static bool HandleLanguageBlacklist(const char *pLanguage)
{
	return CTranslate::IsLanguageInList(g_Config.m_EcTranslateLanguageBlacklist, pLanguage);
}

static bool HandleLanguageWhitelist(const char *pLanguage)
{
	// An empty list means everything passes, it is not a list that happens to match nothing
	if(!pLanguage || pLanguage[0] == '\0' || g_Config.m_EcTranslateLanguageWhitelist[0] == '\0')
		return true;

	return CTranslate::IsLanguageInList(g_Config.m_EcTranslateLanguageWhitelist, pLanguage);
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
	std::shared_ptr<IHttpRequest> m_pHttpRequest = nullptr;
	virtual bool ParseResponse(CTranslateResponse &Out) = 0;
	virtual bool ParseHttpError() const { return false; }

	void CreateHttpRequest(IHttp &Http, const char *pUrl)
	{
		std::shared_ptr<IHttpRequest> pGet = ::CreateHttpRequest(pUrl);
		pGet->LogProgress(HTTPLOG::FAILURE);
		pGet->FailOnErrorStatus(false);
		pGet->Timeout(CTimeout{10000, 0, 500, 10});

		m_pHttpRequest = pGet;
		Http.Run(pGet);
	}

public:
	bool IsRateLimited() const override
	{
		// StatusCode() may only be read once the request is done
		if(!m_pHttpRequest || m_pHttpRequest->State() != EHttpState::DONE)
			return false;
		const int StatusCode = m_pHttpRequest->StatusCode();
		// 429 is the rate limit itself, 503 is a backend that is out of capacity,
		// both mean the same thing for us: stop asking for a while
		return StatusCode == 429 || StatusCode == 503;
	}

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

// DeepL writes em dashes where a chat message reads better with a comma. The spacing on either
// side goes with it, so "a \xe2\x80\x94 b" comes out as "a, b" and not as "a , b".
static void ReplaceEmDashes(char *pText, size_t Size)
{
	static const char *EM_DASH = "\xe2\x80\x94"; // U+2014
	if(str_find(pText, EM_DASH) == nullptr)
		return;

	std::string Result;
	for(const char *pRead = pText; *pRead != '\0';)
	{
		if(str_startswith(pRead, EM_DASH) == nullptr)
		{
			Result += *pRead;
			++pRead;
			continue;
		}

		// The dash takes the spacing on both of its sides with it
		while(!Result.empty() && Result.back() == ' ')
			Result.pop_back();
		pRead += str_length(EM_DASH);
		while(*pRead == ' ')
			++pRead;

		// With nothing left on one side there is nothing to join, and a dash that lands against a
		// comma only wants the space. Neither leaves a comma hanging on its own.
		if(!Result.empty() && *pRead != '\0')
			Result += Result.back() == ',' ? " " : ", ";
	}

	str_copy(pText, Result.c_str(), Size);
}

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
		return CTranslate::BackendName(ETranslateBackend::LIBRETRANSLATE);
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

class CTranslateBackendDeepl : public ITranslateBackendHttp
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
		ReplaceEmDashes(Out.m_Text, sizeof(Out.m_Text));
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
		return CTranslate::BackendName(ETranslateBackend::DEEPL);
	}
	CTranslateBackendDeepl(IHttp &Http, const char *pText)
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

		// Free and Pro speak the same api, they only differ in where they listen, so the endpoint
		// is all a Pro key needs pointed at api.deepl.com
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
		return CTranslate::BackendName(ETranslateBackend::GOOGLE);
	}

	CTranslateBackendGoogle(IHttp &Http, const char *pText)
	{
		char aBuf[4096];
		str_format(aBuf, sizeof(aBuf), "https://translate.google.com/translate_a/single?client=gtx&sl=auto&tl=%s&dt=t&q=",
			EncodeTarget(g_Config.m_EcTranslateTarget));
		UrlEncode(pText, aBuf + strlen(aBuf), sizeof(aBuf) - strlen(aBuf));
		CreateHttpRequest(Http, aBuf);
	}
};

static std::unique_ptr<ITranslateBackend> CreateTranslateBackend(ETranslateBackend Backend, IHttp &Http, const char *pText)
{
	switch(Backend)
	{
	case ETranslateBackend::LIBRETRANSLATE:
		return std::make_unique<CTranslateBackendLibretranslate>(Http, pText);
	case ETranslateBackend::DEEPL:
		return std::make_unique<CTranslateBackendDeepl>(Http, pText);
	case ETranslateBackend::GOOGLE:
		return std::make_unique<CTranslateBackendGoogle>(Http, pText);
	case ETranslateBackend::INVALID:
		break;
	}
	return nullptr;
}

int64_t CTranslate::NextRequestTime(ETranslateBackend Backend) const
{
	const CBackendState &State = BackendState(Backend);
	return std::max(State.m_NextRequest, State.m_BackoffUntil);
}

bool CTranslate::SendRequest(CTranslateJob &Job)
{
	Job.m_pBackend = CreateTranslateBackend(Job.m_Backend, *Http(), Job.m_pLine->m_aText);
	if(!Job.m_pBackend)
		return false;
	Job.m_Attempts++;
	Job.m_SendTime = 0;
	// Hold the next request back, a backend that is asked once per chat message
	// is what gets us rate limited to begin with
	BackendState(Job.m_Backend).m_NextRequest = time() + TranslateMinInterval(Job.m_Backend);
	return true;
}

void CTranslate::OnRateLimited(ETranslateBackend Backend)
{
	CBackendState &State = BackendState(Backend);
	const int64_t Now = time();
	// Requests that were already in flight when the backoff started must not extend it
	if(Now < State.m_BackoffUntil)
		return;

	const CTranslateBackendLimits Limits = TranslateBackendLimits(Backend);
	State.m_Strikes = std::min(State.m_Strikes + 1, RATE_LIMIT_MAX_STRIKES);
	const int64_t Seconds = std::min<int64_t>((int64_t)Limits.m_BackoffSeconds << std::min(State.m_Strikes - 1, 20), Limits.m_BackoffMaxSeconds);
	State.m_BackoffUntil = Now + Seconds * time_freq();

	// Translations going quiet for minutes looks like the module is broken
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Translation ratelimited, waiting %" PRId64 " seconds", Seconds);
	GameClient()->ClientMessage(aBuf);
}

void CTranslate::OnRequestSucceeded(ETranslateBackend Backend)
{
	// Recover slowly, a single answer that got through does not mean the limit
	// is gone. Dropping straight back to the shortest backoff makes us run into
	// the same wall over and over.
	CBackendState &State = BackendState(Backend);
	State.m_Strikes = std::max(State.m_Strikes - 1, 0);
}

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

void CTranslate::ConTranslateResetLimits(IConsole::IResult *pResult, void *pUserData)
{
	CTranslate *pThis = static_cast<CTranslate *>(pUserData);
	for(CBackendState &State : pThis->m_aBackendStates)
		State = CBackendState();
	pThis->GameClient()->ClientMessage("Translate rate limits cleared");
}

void CTranslate::OnConsoleInit()
{
	Console()->Register("translate", "?r[name]", CFGFLAG_CLIENT, ConTranslate, this, "Translate last message (of a given name)");
	Console()->Register("translate_id", "v[id]", CFGFLAG_CLIENT, ConTranslateId, this, "Translate last message of the person with this id");
	Console()->Register("translate_reset_limits", "", CFGFLAG_CLIENT, ConTranslateResetLimits, this, "Forget the rate limit backoff of all translate backends");
}

void CTranslate::Translate(int Id, bool Manual)
{
	if(Id < 0 || Id > (int)std::size(GameClient()->m_aClients))
	{
		GameClient()->ClientMessage("Not a valid ID");
		return;
	}
	const auto &Player = GameClient()->m_aClients[Id];
	if(!Player.m_Active)
	{
		GameClient()->ClientMessage("ID not connected");
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
				if(!(pLine->m_pTranslateResponse->m_Auto && Manual))
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
		GameClient()->ClientMessage("No message to translate");
		return;
	}

	Translate(*pLineBest, Manual);
}

void CTranslate::Translate(CChat::CLine &Line, bool Manual)
{
	if(m_vJobs.size() > 15)
	{
		if(Manual)
			GameClient()->ClientMessage("Translate queue is full");
		return;
	}

	const ETranslateBackend Backend = CTranslate::Backend();

	const int64_t Now = time();
	const int64_t SendTime = NextRequestTime(Backend);
	const int64_t Wait = std::max<int64_t>(SendTime - Now, 0);
	const int64_t MaxWait = (Manual ? QUEUE_MAX_WAIT_SECONDS_MANUAL : QUEUE_MAX_WAIT_SECONDS_AUTO) * time_freq();
	if(Wait > MaxWait)
	{
		// Backing off for longer than the message stays interesting, queueing it
		// up would only translate it after everybody has moved on
		if(Manual)
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%s is rate limited, try again in %d seconds", CTranslate::BackendName(Backend), TimeToSeconds(Wait));
			GameClient()->ClientMessage(aBuf);
		}
		return;
	}

	CTranslateJob Job;
	Job.m_Backend = Backend;
	Job.m_SendTime = SendTime;
	Job.m_Deadline = Now + MaxWait;
	Job.m_pLine = &Line;
	Job.m_pTranslateResponse = std::make_shared<CTranslateResponse>();
	Job.m_pTranslateResponse->m_Auto = !Manual;
	Job.m_pLine->m_pTranslateResponse = Job.m_pTranslateResponse;

	if(Wait == 0 && !SendRequest(Job))
		return;

	if(Manual)
	{
		if(Wait > 0 && BackendState(Backend).m_BackoffUntil > Now)
		{
			str_format(Job.m_pTranslateResponse->m_Text, sizeof(Job.m_pTranslateResponse->m_Text), Localize("Rate limited, retrying in %d seconds", "translate"), TimeToSeconds(Wait));
			Job.m_pTranslateResponse->m_Error = true;
		}
		else
		{
			str_format(Job.m_pTranslateResponse->m_Text, sizeof(Job.m_pTranslateResponse->m_Text), Localize("%s translating to %s", "translate"), CTranslate::BackendName(Backend), g_Config.m_EcTranslateTarget);
		}
		Job.m_pLine->m_Time = Now;
	}
	else
	{
		Job.m_pTranslateResponse->m_Text[0] = '\0';
	}

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

		bool Success = false;
		bool RateLimited = false;

		if(!Job.m_pBackend)
		{
			// Waiting for the turn of this job at its backend
			const int64_t SendTime = std::max(Job.m_SendTime, NextRequestTime(Job.m_Backend));
			if(SendTime > Job.m_Deadline)
			{
				RateLimited = true; // Waited long enough, give up on this message
			}
			else
			{
				if(Time < SendTime)
				{
					Job.m_SendTime = SendTime;
					return false;
				}
				if(!SendRequest(Job))
					return true;
			}
		}

		if(!RateLimited)
		{
			const std::optional<bool> Done = Job.m_pBackend->Update(*Job.m_pTranslateResponse);
			if(!Done.has_value())
				return false; // Keep ongoing tasks
			Success = *Done;
			RateLimited = Job.m_pBackend->IsRateLimited();
			if(RateLimited)
			{
				Success = false;
				OnRateLimited(Job.m_Backend);
				const int64_t SendTime = NextRequestTime(Job.m_Backend);
				if(Job.m_Attempts < RATE_LIMIT_MAX_ATTEMPTS && SendTime <= Job.m_Deadline)
				{
					// Send it again once the backoff has passed
					Job.m_pBackend = nullptr; // Aborts the finished request and frees it until then
					Job.m_SendTime = SendTime;
					if(!Job.m_pTranslateResponse->m_Auto)
					{
						str_format(Job.m_pTranslateResponse->m_Text, sizeof(Job.m_pTranslateResponse->m_Text), Localize("Rate limited, retrying in %d seconds", "translate"), TimeToSeconds(SendTime - Time));
						Job.m_pTranslateResponse->m_Error = true;
						Job.m_pLine->m_Time = Time;
						GameClient()->m_Chat.RebuildChat();
					}
					return false; // Keep the job around for the retry
				}
			}
			else if(Success)
			{
				OnRequestSucceeded(Job.m_Backend);
			}
		}

		if(RateLimited)
		{
			if(Job.m_pTranslateResponse->m_Auto)
			{
				// Nobody is waiting for these, drop them silently instead of
				// filling the chat with errors that all say the same thing
				Job.m_pTranslateResponse->m_Text[0] = '\0';
				Job.m_pTranslateResponse->m_Language[0] = '\0';
				return true;
			}
			str_copy(Job.m_pTranslateResponse->m_Text, Localize("rate limited", "translate"));
		}

		if(Success)
		{
			// Backends disagree on case, DeepL answers "RU" where the others say "ru". The code is
			// shown on the chat line and offered to the language lists, both of which want one
			// spelling rather than whichever the backend felt like.
			char aLanguage[sizeof(Job.m_pTranslateResponse->m_Language)];
			str_utf8_tolower(Job.m_pTranslateResponse->m_Language, aLanguage, sizeof(aLanguage));
			str_copy(Job.m_pTranslateResponse->m_Language, aLanguage);

			const bool SameTextAsInput = str_comp_nocase(Job.m_pLine->m_aText, Job.m_pTranslateResponse->m_Text) == 0;
			if(SameTextAsInput) // Check for no translation difference
			{
				Job.m_pTranslateResponse->m_Text[0] = '\0';
				Job.m_pTranslateResponse->m_Language[0] = '\0';
			}

			const bool SameLanguageAsTarget = Job.m_pBackend->CompareTargets(Job.m_pTranslateResponse->m_Language, g_Config.m_EcTranslateTarget);
			if(SameLanguageAsTarget && !SameTextAsInput)
			{
				// Keep successful translations even if backend source-language detection is wrong.
				Job.m_pTranslateResponse->m_Language[0] = '\0';
			}

			if(Job.m_pTranslateResponse->m_Auto)
			{
				if(!HandleLanguageWhitelist(Job.m_pTranslateResponse->m_Language))
				{
					Job.m_pTranslateResponse->m_Text[0] = '\0';
					Job.m_pTranslateResponse->m_Language[0] = '\0';
				}

				else if(HandleLanguageBlacklist(Job.m_pTranslateResponse->m_Language))
				{
					Job.m_pTranslateResponse->m_Text[0] = '\0';
					Job.m_pTranslateResponse->m_Language[0] = '\0';
				}
			}
		}
		else
		{
			char aBuf[sizeof(Job.m_pTranslateResponse->m_Text)];
			str_format(aBuf, sizeof(aBuf), Localize("%s to %s failed: %s", "translate"), CTranslate::BackendName(Job.m_Backend), g_Config.m_EcTranslateTarget, Job.m_pTranslateResponse->m_Text);
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
	const ETranslateBackend Backend = CTranslate::Backend();

	if(Backend == ETranslateBackend::DEEPL && g_Config.m_EcTranslateKey[0] == '\0')
		return;

	Translate(Line, false);
	s_LastLines[s_LastLineIndex] = Line;
	s_LastLineIndex = (s_LastLineIndex + 1) % 5;
}
