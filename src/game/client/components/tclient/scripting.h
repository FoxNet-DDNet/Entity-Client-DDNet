#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_SCRIPTING_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_SCRIPTING_H

#include <engine/console.h>

#include <game/client/component.h>

class CScriptRunner;

class CScripting : public CComponent
{
private:
	CScriptRunner *m_pRunner = nullptr;
	static void ConExecScript(IConsole::IResult *pResult, void *pUserData);

public:
	~CScripting() override;
	void ExecScript(const char *pFilename, const char *pArgs);
	void OnConsoleInit() override;
	int Sizeof() const override { return sizeof(*this); }
};

#endif
