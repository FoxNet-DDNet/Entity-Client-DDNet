#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_TRANSLATE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_TRANSLATE_H

#include <game/client/component.h>
#include <game/client/components/chat.h>

#include <memory>
#include <optional>
#include <vector>

class CTranslate;

enum class ETranslateBackend
{
	INVALID,
	LIBRETRANSLATE,
	DEEPL_FREE,
	GOOGLE,
	NUM,
};

class ITranslateBackend
{
public:
	virtual bool IsRateLimited() const { return false; }
	virtual ~ITranslateBackend() = default;
	virtual const char *EncodeTarget(const char *pTarget) const;
	virtual bool CompareTargets(const char *pA, const char *pB) const;
	virtual const char *Name() const = 0;
	virtual std::optional<bool> Update(CTranslateResponse &Out) = 0;
};

class CTranslate : public CComponent
{
	class CTranslateJob
	{
	public:
		// Only set while the request is on its way, a job without a backend is
		// waiting for its turn at the backend
		std::unique_ptr<ITranslateBackend> m_pBackend = nullptr;
		ETranslateBackend m_Backend = ETranslateBackend::INVALID;
		// Number of requests sent for this job, retries included
		int m_Attempts = 0;
		// Earliest time the request may be sent
		int64_t m_SendTime = 0;
		// The job is dropped if it cannot be sent before this, the chat message
		// it belongs to is not worth translating forever
		int64_t m_Deadline = 0;
		// For chat translations
		CChat::CLine *m_pLine = nullptr;
		std::shared_ptr<CTranslateResponse> m_pTranslateResponse = nullptr;
	};
	std::vector<CTranslateJob> m_vJobs;

	// Request pacing and rate limit backoff, tracked for every backend on its own
	class CBackendState
	{
	public:
		// Earliest time another request may be sent, keeps a busy chat from
		// bursting into the backend
		int64_t m_NextRequest = 0;
		// Set while the backend answered with a rate limit and wants to be left alone
		int64_t m_BackoffUntil = 0;
		// Consecutive rate limits, decides how long the backoff is
		int m_Strikes = 0;
	};
	CBackendState m_aBackendStates[(int)ETranslateBackend::NUM];

	CBackendState &BackendState(ETranslateBackend Backend) { return m_aBackendStates[(int)Backend]; }
	const CBackendState &BackendState(ETranslateBackend Backend) const { return m_aBackendStates[(int)Backend]; }

	// Time at which the next request may be sent, may be in the past
	int64_t NextRequestTime(ETranslateBackend Backend) const;
	bool SendRequest(CTranslateJob &Job);
	void OnRateLimited(ETranslateBackend Backend);
	void OnRequestSucceeded(ETranslateBackend Backend);

	static void ConTranslate(IConsole::IResult *pResult, void *pUserData);
	static void ConTranslateId(IConsole::IResult *pResult, void *pUserData);
	static void ConTranslateResetLimits(IConsole::IResult *pResult, void *pUserData);

public:
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnRender() override;

	void Translate(int Id, bool Manual);
	void Translate(const char *pName, bool Manual);
	void Translate(CChat::CLine &Line, bool Manual);

	void AutoTranslate(CChat::CLine &Line);
};

#endif
