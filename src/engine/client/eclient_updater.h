#ifndef ENGINE_CLIENT_ECLIENT_UPDATER_H
#define ENGINE_CLIENT_ECLIENT_UPDATER_H

#include <engine/kernel.h>

#include <memory>
#include <string>

class IClient;
class IHttp;
class IHttpRequest;
class IStorage;

class IEClientUpdater : public IInterface
{
	MACRO_INTERFACE("eclient_updater")

public:
	enum class EState
	{
		IDLE,
		CHECKING,
		AVAILABLE,
		DOWNLOADING,
		READY,
		FAILED,
	};

	virtual EState State() const = 0;
	virtual const char *LatestVersion() const = 0;
	virtual int Progress() const = 0;
	virtual void Check() = 0;
	virtual void Download() = 0;
};

class CEClientUpdater : public IEClientUpdater
{
	IClient *m_pClient = nullptr;
	IStorage *m_pStorage = nullptr;
	IHttp *m_pHttp = nullptr;
	std::shared_ptr<IHttpRequest> m_pRequest;
	EState m_State = EState::IDLE;
	char m_aLatestVersion[32] = "";
	std::string m_AssetUrl;
	std::string m_AssetDigest;
	int m_Progress = 0;

	void FinishCheck();
	void FinishDownload();

public:
	void Init();
	void Update();
	EState State() const override { return m_State; }
	const char *LatestVersion() const override { return m_aLatestVersion; }
	int Progress() const override { return m_Progress; }
	void Check() override;
	void Download() override;
	bool HasPendingInstall() const { return m_State == EState::READY; }
	bool LaunchInstaller();
};

#endif
