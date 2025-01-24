#include "auto_kill.h"

#include <base/system.h>
#include <base/vmath.h>
#include <engine/client.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>
#include <game/client/components/chat.h>
#include <game/client/gameclient.h>
#include <game/generated/protocol.h>

void CAutoKill::OnRender()
{
	const int LocalCID = m_pClient->m_Snap.m_LocalClientId;
	const float CurrentRaceTime = GameClient()->CurrentRaceTime();

	if(CurrentRaceTime <= 0.0f)
		m_SentAutoKill = false;

	if(!g_Config.m_ClAutoKill || m_SentAutoKill)
		return;

	// Local player pointer
	CCharacterCore *pLocalCharacter = &m_pClient->m_aClients[LocalCID].m_Predicted;
	const bool LocalIsInFreeze = pLocalCharacter->m_IsInFreeze;

	// If "AutoKill only on Multeasymap" is enabled, check map name once before iterating players.
	if(g_Config.m_ClAutoKillMultOnly)
	{
		bool IsOnMulteasymap = (str_comp(Client()->GetCurrentMap(), "Multeasymap") == 0);
		if(!IsOnMulteasymap)
		{
			if(g_Config.m_ClAutoKillDebug)
			{
				TextRender()->Text(100.0f, 50.0f, 15.0f, "Not on Mult");
			}
			return; // Stop if not on Multeasymap.
		}
		else if(g_Config.m_ClAutoKillDebug)
		{
			TextRender()->Text(100.0f, 50.0f, 15.0f, "On Mult");
		}
	}

	// Access local character snapshots.
	const CNetObj_Character &LocalPrevChar = m_pClient->m_Snap.m_aCharacters[LocalCID].m_Prev;
	const CNetObj_Character &LocalCurChar = m_pClient->m_Snap.m_aCharacters[LocalCID].m_Cur;

	const float IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);

	// Compute local player's interpolated position.
	const vec2 LocalPos = mix(
				      vec2(LocalPrevChar.m_X, LocalPrevChar.m_Y),
				      vec2(LocalCurChar.m_X, LocalCurChar.m_Y),
				      IntraTick) /
			      32.0f;

	const float RangeX = g_Config.m_ClAutoKillRangeX / 100.0f;
	const float RangeY = g_Config.m_ClAutoKillRangeY / 100.0f;

	// Loop through all clients
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		// Skip if it's the local player's slot.
		if(i == LocalCID)
			continue;

		CCharacterCore *pOtherCharacter = &m_pClient->m_aClients[i].m_Predicted;
		const bool EnemyFrozen = pOtherCharacter->m_IsInFreeze;

		const bool IsWar = m_pClient->m_aClients[i].m_IsWar && (!m_pClient->m_aClients[i].m_IsTeam || !m_pClient->m_aClients[i].m_IsHelper);

		// Fetch the other character's previous/current snapshots.
		const CNetObj_Character &OtherPrevChar = m_pClient->m_Snap.m_aCharacters[i].m_Prev;
		const CNetObj_Character &OtherCurChar = m_pClient->m_Snap.m_aCharacters[i].m_Cur;

		// Interpolate enemy position and velocity.
		const vec2 EnemyPos = vec2(OtherPrevChar.m_X, OtherPrevChar.m_Y) / 32.0f;
		const vec2 EnemyVel = mix(
					      vec2(OtherPrevChar.m_VelX, OtherPrevChar.m_VelY),
					      vec2(OtherCurChar.m_VelX, OtherCurChar.m_VelY),
					      IntraTick) /
				      (1000.0f * 1.5f);

		// Debug output if enabled.
		if(g_Config.m_ClAutoKillDebug)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Local X: %d", round_to_int(LocalPos.x));
			TextRender()->Text(100.0f, 100.0f, 15.0f, aBuf);

			char bBuf[64];
			str_format(bBuf, sizeof(bBuf), "Enemy X: %d", round_to_int(EnemyPos.x));
			TextRender()->Text(100.0f, 120.0f, 15.0f, bBuf);
		}

		// Check if local is in freeze
		const bool LocalFreezeCheck =
			(LocalIsInFreeze && !g_Config.m_ClAutoKillWarOnly) ||
			(g_Config.m_ClAutoKillWarOnly && LocalIsInFreeze && IsWar);

		if(LocalFreezeCheck)
		{
			// Check if the other player is above us within a Y range,
			// and horizontally within range of our position.
			// Combine both to confirm the enemy is "on top" or extremely close above in freeze.
			if(!EnemyFrozen)
			{
				const bool InVerticalRange =
					(EnemyPos.y < LocalPos.y) &&
					(EnemyPos.y > (LocalPos.y - 1.0f - RangeY - EnemyVel.y));

				const bool InHorizontalRange =
					(EnemyPos.x <= LocalPos.x + RangeX) &&
					((EnemyPos.x + RangeX) >= LocalPos.x);

				if(InVerticalRange && InHorizontalRange)
				{
					// Decide whether to ignore the server kill protection
					const bool IgnoreKillProt = g_Config.m_ClFreezeKillIgnoreKillProt;
					const float RequiredTimeBeforeProt = 60.0f * g_Config.m_SvKillProtection;

					// Switch kill between /kill and client->sendkill
					if(CurrentRaceTime > RequiredTimeBeforeProt && IgnoreKillProt)
					{
						m_pClient->m_Chat.SendChat(0, "/kill");
					}
					else
					{
						GameClient()->SendKill(LocalCID);
					}

					m_SentAutoKill = true;
					return; // Immediately stop once a kill command has been executed.
				}
			}
		}
	}
}
