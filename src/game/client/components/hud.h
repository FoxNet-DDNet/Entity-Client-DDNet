/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_H
#define GAME_CLIENT_COMPONENTS_HUD_H
#include "entity/mediaplayer/media_player.h"

#include <base/color.h>
#include <base/vmath.h>

#include <engine/client/enums.h>
#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/component.h>

#include <cstdint>

// EClient
class CArtCropProfile
{
public:
	float m_Left = 0.01f;
	float m_Right = 0.01f;
	float m_Top = 0.01f;
	float m_Bottom = 0.01f;
};

struct SScoreInfo
{
	SScoreInfo()
	{
		Reset();
	}

	void Reset()
	{
		m_TextRankContainerIndex.Reset();
		m_TextScoreContainerIndex.Reset();
		m_RoundRectQuadContainerIndex = -1;
		m_OptionalNameTextContainerIndex.Reset();
		m_aScoreText[0] = 0;
		m_aRankText[0] = 0;
		m_aPlayerNameText[0] = 0;
		m_ScoreTextWidth = 0.f;
		m_Initialized = false;
	}

	STextContainerIndex m_TextRankContainerIndex;
	STextContainerIndex m_TextScoreContainerIndex;
	float m_ScoreTextWidth;
	char m_aScoreText[16];
	char m_aRankText[16];
	char m_aPlayerNameText[MAX_NAME_LENGTH];
	int m_RoundRectQuadContainerIndex;
	STextContainerIndex m_OptionalNameTextContainerIndex;

	bool m_Initialized;
};

class CHud : public CComponent
{
	float m_Width, m_Height;

	int m_HudQuadContainerIndex;
	SScoreInfo m_aScoreInfo[2];
	STextContainerIndex m_FPSTextContainerIndex;
	STextContainerIndex m_DDRaceEffectsTextContainerIndex;
	STextContainerIndex m_PlayerAngleTextContainerIndex;
	float m_PlayerPrevAngle;
	STextContainerIndex m_aPlayerSpeedTextContainers[2];
	float m_aPlayerPrevSpeed[2];
	int m_aPlayerSpeed[2];
	enum class ESpeedChange
	{
		NONE,
		INCREASE,
		DECREASE
	};
	ESpeedChange m_aLastPlayerSpeedChange[2];
	STextContainerIndex m_aPlayerPositionContainers[2];
	float m_aPlayerPrevPosition[2];

	void RenderCursor();

	void RenderTextInfo();
	void RenderConnectionWarning();
	void RenderTeambalanceWarning();

	void PrepareAmmoHealthAndArmorQuads();
	void RenderAmmoHealthAndArmor(const CNetObj_Character *pCharacter);

	void PreparePlayerStateQuads();
	void RenderPlayerState(int ClientId);

	int m_LastSpectatorCountTick;
	void RenderSpectatorCount();
	void RenderDummyActions();
	void RenderMovementInformation();

	void UpdateMovementInformationTextContainer(STextContainerIndex &TextContainer, float FontSize, float Value, float &PrevValue);
	void RenderMovementInformationTextContainer(STextContainerIndex &TextContainer, const ColorRGBA &Color, float X, float Y);

	class CMovementInformation
	{
	public:
		vec2 m_Pos;
		vec2 m_Speed;
		float m_Angle = 0.0f;
	};
	class CMovementInformation GetMovementInformation(int ClientId, int Conn) const;

	float GameTimerWidth(float Size, int Time);
	int GameTimerTime();

	void RenderGameTimer(vec2 Pos, float Size);
	void RenderPauseNotification();
	void RenderSuddenDeath();

	void RenderScoreHud();
	int m_LastLocalClientId = -1;

	void RenderSpectatorHud();
	void RenderWarmupTimer();
	void RenderLocalTime(float x);

	static constexpr float MOVEMENT_INFORMATION_LINE_HEIGHT = 8.0f;

public:
	CHud();
	int Sizeof() const override { return sizeof(*this); }

	void ResetHudContainers();
	void OnWindowResize() override;
	void OnReset() override;
	void OnRender() override;
	void OnInit() override;
	void OnNewSnapshot() override;

	// DDRace

	void OnMessage(int MsgType, void *pRawMsg) override;
	void RenderNinjaBarPos(float x, float y, float Width, float Height, float Progress, float Alpha = 1.0f);

private:
	void RenderRecord();
	void RenderDDRaceEffects();
	float m_TimeCpDiff;
	float m_aPlayerRecord[NUM_DUMMIES];
	float m_FinishTimeDiff;
	int m_DDRaceTime;
	int m_FinishTimeLastReceivedTick;
	int m_TimeCpLastReceivedTick;
	bool m_ShowFinishTime;

	inline float GetMovementInformationBoxHeight();
	inline int GetDigitsIndex(int Value, int Max);

	// Quad Offsets
	int m_aAmmoOffset[NUM_WEAPONS];
	int m_HealthOffset;
	int m_EmptyHealthOffset;
	int m_ArmorOffset;
	int m_EmptyArmorOffset;
	int m_aCursorOffset[NUM_WEAPONS];
	int m_FlagOffset;
	int m_AirjumpOffset;
	int m_AirjumpEmptyOffset;
	int m_aWeaponOffset[NUM_WEAPONS];
	int m_EndlessJumpOffset;
	int m_EndlessHookOffset;
	int m_JetpackOffset;
	int m_TeleportGrenadeOffset;
	int m_TeleportGunOffset;
	int m_TeleportLaserOffset;
	int m_SoloOffset;
	int m_CollisionDisabledOffset;
	int m_HookHitDisabledOffset;
	int m_HammerHitDisabledOffset;
	int m_GunHitDisabledOffset;
	int m_ShotgunHitDisabledOffset;
	int m_GrenadeHitDisabledOffset;
	int m_LaserHitDisabledOffset;
	int m_DeepFrozenOffset;
	int m_LiveFrozenOffset;
	int m_DummyHammerOffset;
	int m_DummyCopyOffset;
	int m_PracticeModeOffset;
	int m_Team0ModeOffset;
	int m_LockModeOffset;

	// EClient
	bool RenderLocalTime() const;

	void FreezeHelpers();

	void RenderIsland();

	void RenderVisualizer(const CMediaViewer::CState &State, ColorRGBA Primary, ColorRGBA Secondary, vec2 Pos, vec2 Size, int NumBands);

	class CMediaIsland
	{
	public:
		class CTextScrollState
		{
		public:
			float m_Offset = 0.0f;
			float m_Overflow = 0.0f;
			float m_Progress = 0.0f;
			float m_HoldTime = 0.0f;
			bool m_Forward = true;

			void Reset()
			{
				m_Offset = 0.0f;
				m_Overflow = 0.0f;
				m_Progress = 0.0f;
				m_HoldTime = 0.0f;
				m_Forward = true;
			}
		};

		class CPosSize
		{
		public:
			vec2 m_Pos = vec2();
			vec2 m_Size = vec2();

			vec2 m_WantedPos = vec2();
			vec2 m_WantedSize = vec2();
			bool m_Initialized = false;
			void Update(float DeltaTime);
		};

		enum class EVisualState
		{
			MINIMIZED,
			EXPANDED,
		};

		EVisualState m_VisualState = EVisualState::MINIMIZED;
		float m_AnimProgress = 0.0f; // 0.0f to 1.0f

		bool m_Changed = false;
		float m_ChangedAnim = 0.0f; // 0.0f to 1.0f, used to animate changes in the media state

		CMediaViewer::CState m_CurState;

		CTextScrollState m_TitleScroll;
		CTextScrollState m_ArtistScroll;

		float m_TitleTextWidth = 0.0f;
		float m_ArtistTextWidth = 0.0f;
		float m_SizeScale = 1.0f;

		bool m_Hovered = false;
		bool m_PrevHovered = false;

		CArtCropProfile m_CropProfile;
		CArtCropProfile m_PrevCropProfile;

		// Position and size of the island rect
		CPosSize m_Rect;
		CPosSize m_AlbumArt;
		CPosSize m_Visualizer;

		bool m_Initialized = false;

		void ResetPosSize()
		{
			m_Rect = CPosSize();
			m_AlbumArt = CPosSize();
			m_Visualizer = CPosSize();
		}

		void Reset()
		{
			m_VisualState = EVisualState::MINIMIZED;
			m_AnimProgress = 0.0f;
			m_TitleScroll.Reset();
			m_ArtistScroll.Reset();

			m_Hovered = false;
			m_PrevHovered = false;

			ResetPosSize();
		}

	} m_Island;
	vec2 m_FPSPos;

public:
	vec2 IslandPos() const { return m_Island.m_Rect.m_Pos; }
	vec2 IslandSize() const { return m_Island.m_Rect.m_Size; }
	vec2 FpsPos() const { return m_FPSPos; }
};

#endif
