/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#define GAME_CLIENT_COMPONENTS_NAMEPLATES_H
#include <base/vmath.h>

#include <engine/shared/protocol.h>
#include <engine/textrender.h>

#include <game/client/component.h>
#include "tclient/warlist.h"

enum
{
	MAX_LINES = 64,
	MAX_LINE_LENGTH = 256
};

struct CNetObj_Character;
struct CNetObj_PlayerInfo;

class CNamePlates;

class CNamePlate
{
public:
	class CNamePlateName
	{
	public:
		CNamePlateName()
		{
			Reset();
		}
		void Reset()
		{
			m_TextContainerIndex.Reset();
			m_Id = -1;
			m_aName[0] = '\0';
			m_FriendMark = false;
			m_FontSize = -INFINITY;
		}
		void Update(CNamePlates &This, int Id, const char *pName, bool FriendMark, float FontSize);
		STextContainerIndex m_TextContainerIndex;
		char m_aName[MAX_NAME_LENGTH];
		int m_Id;
		bool m_FriendMark;
		float m_FontSize;
	};
	class CNamePlateClan
	{
	public:
		CNamePlateClan()
		{
			Reset();
		}
		void Reset()
		{
			m_TextContainerIndex.Reset();
			m_aClan[0] = '\0';
			m_FontSize = -INFINITY;
		}
		void Update(CNamePlates &This, const char *pClan, float FontSize);
		STextContainerIndex m_TextContainerIndex;
		char m_aClan[MAX_CLAN_LENGTH];
		float m_FontSize;
	};
	class CNamePlateHookWeakStrongId
	{
	public:
		CNamePlateHookWeakStrongId()
		{
			Reset();
		}
		void Reset()
		{
			m_TextContainerIndex.Reset();
			m_Id = -1;
			m_FontSize = -INFINITY;
		}
		void Update(CNamePlates &This, int Id, float FontSize);
		STextContainerIndex m_TextContainerIndex;
		int m_Id;
		float m_FontSize;
	};
	class CNamePlateOldWeakStrong
	{
	public:
		CNamePlateOldWeakStrong()
		{
			Reset();
		}
		void Reset()
		{
			m_TextContainerIndex.Reset();
			m_Id = -1;
			m_FontSize = -INFINITY;
		}
		void Update(CNamePlates &This, int Id, float FontSize);
		STextContainerIndex m_TextContainerIndex;
		int m_Id;
		float m_FontSize;
	};
	void DeleteTextContainers(ITextRender &TextRender)
	{
		TextRender.DeleteTextContainer(m_Name.m_TextContainerIndex);
		TextRender.DeleteTextContainer(m_Clan.m_TextContainerIndex);
		TextRender.DeleteTextContainer(m_WeakStrongId.m_TextContainerIndex);
	}
	CNamePlate()
	{
		Reset();
	}
	void Reset()
	{
		m_Name.Reset();
		m_Clan.Reset();
		m_WeakStrongId.Reset();

		// A-Client
		m_ChatBox.Reset();
		m_OldWeakStrongId.Reset();

		// TClient
		m_Reason.Reset();
	}
	CNamePlateName m_Name;
	CNamePlateClan m_Clan;
	CNamePlateHookWeakStrongId m_WeakStrongId;

	class CNamePlateChatBox
	{
	public:
		CNamePlateChatBox()
		{
			Reset();
		}
		void Reset()
		{
			m_TextContainerIndex.Reset();
			m_aMsg[0] = '\0';
			m_FontSize = -INFINITY;
		}
		void Update(CNamePlates &This, const char *pMsg, float FontSize);
		STextContainerIndex m_TextContainerIndex;
		char m_aMsg[MAX_LINE_LENGTH];
		float m_FontSize;
	};

	// TClient
	class CNamePlateReason
	{
	public:
		CNamePlateReason()
		{
			Reset();
		}
		void Reset()
		{
			m_TextContainerIndex.Reset();
			m_aReason[0] = '\0';
			m_FontSize = -INFINITY;
		}
		void Update(CNamePlates &This, const char *pReason, float FontSize);
		STextContainerIndex m_TextContainerIndex;
		char m_aReason[MAX_WARLIST_REASON_LENGTH];
		float m_FontSize;
	};

	CNamePlateOldWeakStrong m_OldWeakStrongId;
	CNamePlateChatBox m_ChatBox;
	CNamePlateReason m_Reason;
};

class CNamePlates : public CComponent
{
	friend class CNamePlate::CNamePlateName;
	friend class CNamePlate::CNamePlateClan;
	friend class CNamePlate::CNamePlateOldWeakStrong;
	friend class CNamePlate::CNamePlateHookWeakStrongId;
	friend class CNamePlate::CNamePlateChatBox;

	// TClient
	friend class CNamePlate::CNamePlateReason;

	CNamePlate m_aNamePlates[MAX_CLIENTS];

	
public: // A-Client
	void ResetNamePlates();
private:

	int m_DirectionQuadContainerIndex;
	class CRenderNamePlateData
	{
	public:
		vec2 m_Position;
		ColorRGBA m_Color;
		ColorRGBA m_OutlineColor;
		float m_Alpha;
		const char *m_pName;
		bool m_ShowFriendMark;
		int m_ClientId;
		float m_FontSize;
		const char *m_pClan;
		float m_FontSizeClan;
		bool m_ShowDirection;
		bool m_DirLeft;
		bool m_DirJump;
		bool m_DirRight;
		float m_FontSizeDirection;
		bool m_ShowHookWeakStrong;
		TRISTATE m_HookWeakStrong;
		bool m_ShowHookWeakStrongId;
		int m_HookWeakStrongId;
		float m_FontSizeHookWeakStrong;

		int m_OldNameplateId;

		// TClient
		int m_RealClientId;
		bool m_IsGame = false;
		bool m_ShowClanWarInName = false;
		bool m_IsLocal = false;
	};

	void RenderNamePlate(CNamePlate &NamePlate, const CRenderNamePlateData &Data);

public:
	void RenderNamePlateGame(vec2 Position, const CNetObj_PlayerInfo *pPlayerInfo, float Alpha);
	void RenderNamePlatePreview(vec2 Position);
	virtual int Sizeof() const override { return sizeof(*this); }
	virtual void OnWindowResize() override;
	virtual void OnInit() override;
	virtual void OnRender() override;
};

#endif
