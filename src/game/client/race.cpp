#include <cctype>
#include <vector>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include "race.h"

void CRaceHelper::Init(const CGameClient *pGameClient)
{
	m_pGameClient = pGameClient;

	m_aFlagIndex[TEAM_RED] = -1;
	m_aFlagIndex[TEAM_BLUE] = -1;

	const CTile *pGameTiles = m_pGameClient->Collision()->GameLayer();
	const int MapSize = m_pGameClient->Collision()->GetWidth() * m_pGameClient->Collision()->GetHeight();
	for(int Index = 0; Index < MapSize; Index++)
	{
		const int EntityIndex = pGameTiles[Index].m_Index - ENTITY_OFFSET;
		if(EntityIndex == ENTITY_FLAGSTAND_RED)
		{
			m_aFlagIndex[TEAM_RED] = Index;
			if(m_aFlagIndex[TEAM_BLUE] != -1)
				break; // Found both flags
		}
		else if(EntityIndex == ENTITY_FLAGSTAND_BLUE)
		{
			m_aFlagIndex[TEAM_BLUE] = Index;
			if(m_aFlagIndex[TEAM_RED] != -1)
				break; // Found both flags
		}
		Index += pGameTiles[Index].m_Skip;
	}
}

int CRaceHelper::TimeFromSecondsStr(const char *pStr)
{
	while(*pStr == ' ') // skip leading spaces
		pStr++;
	if(!isdigit(*pStr))
		return -1;
	int Time = str_toint(pStr) * 1000;
	while(isdigit(*pStr))
		pStr++;
	if(*pStr == '.' || *pStr == ',')
	{
		pStr++;
		static const int s_aMult[3] = {100, 10, 1};
		for(size_t i = 0; i < std::size(s_aMult) && isdigit(pStr[i]); i++)
			Time += (pStr[i] - '0') * s_aMult[i];
	}
	return Time;
}

int CRaceHelper::TimeFromStr(const char *pStr)
{
	static constexpr const char *MINUTES_STR = " minute(s) ";
	static constexpr const char *SECONDS_STR = " second(s)";

	const char *pSeconds = str_find(pStr, SECONDS_STR);
	if(!pSeconds)
		return -1;

	const char *pMinutes = str_find(pStr, MINUTES_STR);
	if(pMinutes)
	{
		while(*pStr == ' ') // skip leading spaces
			pStr++;
		int SecondsTime = TimeFromSecondsStr(pMinutes + str_length(MINUTES_STR));
		if(SecondsTime == -1 || !isdigit(*pStr))
			return -1;
		return str_toint(pStr) * 60 * 1000 + SecondsTime;
	}
	else
		return TimeFromSecondsStr(pStr);
}

int CRaceHelper::TimeFromFinishMessage(const char *pStr, char *pNameBuf, int NameBufSize)
{
	static const char *const s_pFinishedStr = " finished in: ";
	const char *pFinished = str_find(pStr, s_pFinishedStr);
	if(!pFinished)
		return -1;

	int FinishedPos = pFinished - pStr;
	if(FinishedPos == 0 || FinishedPos >= NameBufSize)
		return -1;

	str_copy(pNameBuf, pStr, FinishedPos + 1);

	return TimeFromStr(pFinished + str_length(s_pFinishedStr));
}

bool CRaceHelper::IsStart(vec2 Prev, vec2 Pos) const
{
	if(m_pGameClient->m_GameInfo.m_FlagStartsRace)
	{
		int EnemyTeam = m_pGameClient->m_aClients[m_pGameClient->m_Snap.m_LocalClientId].m_Team ^ 1;
		return m_aFlagIndex[EnemyTeam] != -1 && distance(Pos, m_pGameClient->Collision()->GetPos(m_aFlagIndex[EnemyTeam])) < 32;
	}
	else
	{
		std::vector<int> vIndices = m_pGameClient->Collision()->GetMapIndices(Prev, Pos);
		if(!vIndices.empty())
		{
			for(const int Index : vIndices)
			{
				if(m_pGameClient->Collision()->GetTileIndex(Index) == TILE_START)
					return true;
				if(m_pGameClient->Collision()->GetFrontTileIndex(Index) == TILE_START)
					return true;
			}
		}
		else
		{
			const int Index = m_pGameClient->Collision()->GetPureMapIndex(Pos);
			if(m_pGameClient->Collision()->GetTileIndex(Index) == TILE_START)
				return true;
			if(m_pGameClient->Collision()->GetFrontTileIndex(Index) == TILE_START)
				return true;
		}
	}
	return false;
}

bool CRaceHelper::IsFinish(vec2 Pos1, vec2 Pos2) const
{
	const CCollision *pCollision = m_pGameClient->Collision();
	std::vector<int> vIndices = pCollision->GetMapIndices(Pos2, Pos1);
	if(!vIndices.empty())
		for(int &Indice : vIndices)
		{
			if(pCollision->GetTileIndex(Indice) == TILE_FINISH)
				return true;
			if(pCollision->GetFrontTileIndex(Indice) == TILE_FINISH)
				return true;
		}
	else
	{
		if(pCollision->GetTileIndex(pCollision->GetPureMapIndex(Pos1)) == TILE_FINISH)
			return true;
		if(pCollision->GetFrontTileIndex(pCollision->GetPureMapIndex(Pos1)) == TILE_FINISH)
			return true;
	}
	return false;
}

bool CRaceHelper::IsNearStart(vec2 Pos, int RadiusInTiles) const
{
	const CCollision *pCollision = m_pGameClient->Collision();
	float d = RadiusInTiles * 32.0f;
	vec2 TL = Pos;
	vec2 TR = Pos;
	vec2 BL = Pos;
	vec2 BR = Pos;
	// top left
	TL.x = std::clamp(TL.x - d, 0.0f, (float)(pCollision->GetWidth() * 32));
	TL.y = std::clamp(TL.y - d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// top right
	TR.x = std::clamp(TR.x + d, 0.0f, (float)(pCollision->GetWidth() * 32));
	TR.y = std::clamp(TR.y - d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// bottom left
	BL.x = std::clamp(BL.x - d, 0.0f, (float)(pCollision->GetWidth() * 32));
	BL.y = std::clamp(BL.y + d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// bottom right
	BR.x = std::clamp(BR.x + d, 0.0f, (float)(pCollision->GetWidth() * 32));
	BR.y = std::clamp(BR.y + d, 0.0f, (float)(pCollision->GetHeight() * 32));
	if(IsStart(TL, TR))
		return true;
	if(IsStart(BL, BR))
		return true;
	if(IsStart(TL, BL))
		return true;
	if(IsStart(TR, BR))
		return true;
	return false;
}

bool CRaceHelper::IsNearFinish(vec2 Pos, int RadiusInTiles) const
{
	const CCollision *pCollision = m_pGameClient->Collision();
	float d = RadiusInTiles * 32;
	vec2 TL = Pos;
	vec2 TR = Pos;
	vec2 BL = Pos;
	vec2 BR = Pos;
	// top left
	TL.x = std::clamp(TL.x - d, 0.0f, (float)(pCollision->GetWidth() * 32));
	TL.y = std::clamp(TL.y - d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// top right
	TR.x = std::clamp(TR.x + d, 0.0f, (float)(pCollision->GetWidth() * 32));
	TR.y = std::clamp(TR.y - d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// bottom left
	BL.x = std::clamp(BL.x - d, 0.0f, (float)(pCollision->GetWidth() * 32));
	BL.y = std::clamp(BL.y + d, 0.0f, (float)(pCollision->GetHeight() * 32));
	// bottom right
	BR.x = std::clamp(BR.x + d, 0.0f, (float)(pCollision->GetWidth() * 32));
	BR.y = std::clamp(BR.y + d, 0.0f, (float)(pCollision->GetHeight() * 32));
	if(IsFinish(TL, TR))
		return true;
	if(IsFinish(BL, BR))
		return true;
	if(IsFinish(TL, BL))
		return true;
	if(IsFinish(TR, BR))
		return true;
	return false;
}

bool CRaceHelper::IsClusterRangeFinish(vec2 Pos, int RadiusInTiles) const
{
	for(int x = -RadiusInTiles * 32; x < RadiusInTiles * 32; x += 4 * 32)
		for(int y = -RadiusInTiles * 32; y < RadiusInTiles * 32; y += 4 * 32)
			if(IsNearFinish(vec2(Pos.x + x, Pos.y + y)))
				return true;
	return false;
}

bool CRaceHelper::IsClusterRangeStart(vec2 Pos, int RadiusInTiles) const
{
	for(int x = -RadiusInTiles * 32; x < RadiusInTiles * 32; x += 4 * 32)
		for(int y = -RadiusInTiles * 32; y < RadiusInTiles * 32; y += 4 * 32)
			if(IsNearStart(vec2(Pos.x + x, Pos.y + y)))
				return true;
	return false;
}