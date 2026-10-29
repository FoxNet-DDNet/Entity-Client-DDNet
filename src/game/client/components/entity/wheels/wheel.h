#ifndef GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_WHEEL_H
#define GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_WHEEL_H

#include <base/vmath.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/ui.h>

#include <array>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class CGameClient;

class CWheelItem
{
public:
	float m_AnimationTime = 0.0f;
	std::string m_Name;
	std::string m_Id;
	std::function<void(CGameClient &)> m_Execute;

	CWheelItem() = default;

	CWheelItem(std::string Name, std::string Id, std::function<void(CGameClient &)> Execute) :
		m_Name(std::move(Name)),
		m_Id(std::move(Id)),
		m_Execute(std::move(Execute))
	{
	}

	bool operator==(const CWheelItem &Other) const
	{
		return m_Name == Other.m_Name && m_Id == Other.m_Id;
	}
};

class CWheel
{
public:
	std::array<float, 3> m_aAnimationPhase;

	bool m_Active = false;
	bool m_WasActive = false;
	bool m_TouchPressedOutside;

	float m_AnimationTime = 0.0f;
	int m_SelectedIndex;
	vec2 m_SelectorMouse;

	std::vector<CWheelItem> m_vItems;
	CUi::CTouchState m_TouchState;

	size_t m_MaxItems;
	CGameClient *m_pGameClient = nullptr;

	class CGameClient *GameClient() const { return m_pGameClient; }
	class IClient *Client() const;
	class IGraphics *Graphics() const;
	class ITextRender *TextRender() const;
	class CUi *Ui() const;

	void Reset();
	~CWheel();
	CWheel();

	void SetData(CGameClient *pGameClient, int MaxItems);

	void AddItem(const char *pName, const char *pId, std::function<void(class CGameClient &)> Execute);
	void RemoveItem(const char *pName, const char *pId);
	void RemoveItem(int Index);
	void RemoveAllItems();

	void ExecuteHoveredItem();
	void ExecuteItem(int Index);

	// returns `true` if anything rendered
	bool Render(bool ResetWheelMouse);
};

#endif // GAME_CLIENT_COMPONENTS_ENTITY_WHEELS_WHEEL_H
