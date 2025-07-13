#include <game/client/gameclient.h>

#include <engine/config.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include "skinprofiles.h"

static void EscapeParam(char *pDst, const char *pSrc, int Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

CProfile::CProfile(int BodyColor, int FeetColor, int CountryFlag, int Emote, const char *pSkinName, const char *pName, const char *pClan)
{
	m_BodyColor = BodyColor;
	m_FeetColor = FeetColor;
	m_CountryFlag = CountryFlag;
	m_Emote = Emote;
	str_copy(m_SkinName, pSkinName);
	str_copy(m_Name, pName);
	str_copy(m_Clan, pClan);
}

void CSkinProfiles::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterECallback(ConfigSaveCallback, this);

	Console()->Register("add_profile", "i[body] i[feet] i[flag] i[emote] s[skin] s[name] s[clan]", CFGFLAG_CLIENT, ConAddProfile, this, "Add a profile");

	m_pStorage = Kernel()->RequestInterface<IStorage>();
	IOHANDLE File = m_pStorage->OpenFile(PROFILES_FILE, IOFLAG_READ, IStorage::TYPE_ALL);
	if(File)
	{
		io_close(File);
		Console()->ExecuteFile(PROFILES_FILE);
	}
}

void CSkinProfiles::ConAddProfile(IConsole::IResult *pResult, void *pUserData)
{
	CSkinProfiles *pSelf = (CSkinProfiles *)pUserData;
	pSelf->AddProfile(pResult->GetInteger(0), pResult->GetInteger(1), pResult->GetInteger(2), pResult->GetInteger(3), pResult->GetString(4), pResult->GetString(5), pResult->GetString(6));
}

void CSkinProfiles::AddProfile(int BodyColor, int FeetColor, int CountryFlag, int Emote, const char *pSkinName, const char *pName, const char *pClan)
{
	CProfile Profile = CProfile(BodyColor, FeetColor, CountryFlag, Emote, pSkinName, pName, pClan);
	m_Profiles.push_back(Profile);
}

void CSkinProfiles::ApplyProfile(int Dummy, const CProfile &Profile)
{
	char aCommand[2048] = "";
	auto FAddPart = [&](const char *pName, const char *pValue) {
		str_append(aCommand, Dummy ? "dummy" : "player");
		str_append(aCommand, "_");
		str_append(aCommand, pName);
		str_append(aCommand, " \"");
		char *pDst = aCommand + str_length(aCommand);
		str_escape(&pDst, pValue, aCommand + sizeof(aCommand) - 1); // 1 extra for end quote
		str_append(aCommand, "\";");
	};
	auto FAddPartNumber = [&](const char *pName, int Value) {
		str_append(aCommand, Dummy ? "dummy" : "player");
		str_append(aCommand, "_");
		str_append(aCommand, pName);
		str_append(aCommand, " ");
		int Length = str_length(aCommand);
		str_format(aCommand + Length, sizeof(aCommand) - Length, "%d", Value);
		str_append(aCommand, ";");
	};
	if(g_Config.m_ClProfileSkin && strlen(Profile.m_SkinName) != 0)
		FAddPart("skin", Profile.m_SkinName);
	if(g_Config.m_ClProfileColors && Profile.m_BodyColor != -1 && Profile.m_FeetColor != -1)
	{
		FAddPartNumber("color_body", Profile.m_BodyColor);
		FAddPartNumber("color_feet", Profile.m_FeetColor);
	}
	if(g_Config.m_ClProfileEmote && Profile.m_Emote != -1)
		FAddPartNumber("default_eyes", Profile.m_Emote);
	if(g_Config.m_ClProfileName && strlen(Profile.m_Name) != 0)
		FAddPart("name", Profile.m_Name);
	if(g_Config.m_ClProfileClan && (strlen(Profile.m_Clan) != 0 || g_Config.m_ClProfileOverwriteClanWithEmpty))
		FAddPart("clan", Profile.m_Clan);
	if(g_Config.m_ClProfileFlag && Profile.m_CountryFlag != -2)
		FAddPartNumber("country", Profile.m_CountryFlag);
	Console()->ExecuteLine(aCommand);
}
void CSkinProfiles::WriteLine(const char *pLine)
{
	if(!m_ProfilesFile || io_write(m_ProfilesFile, pLine, str_length(pLine)) != static_cast<unsigned>(str_length(pLine)) || !io_write_newline(m_ProfilesFile))
		return;
}
void CSkinProfiles::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CSkinProfiles *pThis = (CSkinProfiles *)pUserData;

	pThis->m_ProfilesFile = pThis->m_pStorage->OpenFile(PROFILES_FILE, IOFLAG_WRITE, IStorage::TYPE_SAVE);

	if(!pThis->m_ProfilesFile)
	{
		dbg_msg("config", "ERROR: opening %s failed", PROFILES_FILE);
		return;
	}

	char aBuf[256];
	char aBufTemp[128];
	char aEscapeBuf[256];

	for(const CProfile &Profile : pThis->m_Profiles)
	{
		str_copy(aBuf, "add_profile ", sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.m_BodyColor);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.m_FeetColor);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.m_CountryFlag);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		str_format(aBufTemp, sizeof(aBufTemp), "%d ", Profile.m_Emote);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, Profile.m_SkinName, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, Profile.m_Name, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		EscapeParam(aEscapeBuf, Profile.m_Clan, sizeof(aEscapeBuf));
		str_format(aBufTemp, sizeof(aBufTemp), "\"%s\"", aEscapeBuf);
		str_append(aBuf, aBufTemp, sizeof(aBuf));

		pThis->WriteLine(aBuf);
	}
	bool Failed = false;

	if(io_sync(pThis->m_ProfilesFile) != 0)
		Failed = true;
	if(io_close(pThis->m_ProfilesFile) != 0)
		Failed = true;
	pThis->m_ProfilesFile = {};
	if(Failed)
		dbg_msg("config", "ERROR: writing to %s failed", PROFILES_FILE);
}
