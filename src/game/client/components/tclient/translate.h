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
	FTAPI,
	DEEPL_FREE,
	GOOGLE,
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
		std::unique_ptr<ITranslateBackend> m_pBackend = nullptr;
		ETranslateBackend m_Backend = ETranslateBackend::INVALID;
		// Number of requests sent for this job, retries included
		int m_Attempts = 0;
		// While non zero the job waits for the rate limit backoff to pass before it is (re)sent
		int64_t m_RetryTime = 0;
		// For chat translations
		CChat::CLine *m_pLine = nullptr;
		std::shared_ptr<CTranslateResponse> m_pTranslateResponse = nullptr;
	};
	std::vector<CTranslateJob> m_vJobs;

	// Backoff shared by every job of the backend that answered with a rate limit
	ETranslateBackend m_RateLimitBackend = ETranslateBackend::INVALID;
	int64_t m_RateLimitUntil = 0;
	int m_RateLimitStrikes = 0;

	// Time left before the backend may be used again, zero if it is not rate limited
	int64_t RateLimitRemaining(ETranslateBackend Backend) const;
	void RateLimitHit(ETranslateBackend Backend);
	void RateLimitPassed(ETranslateBackend Backend);

	static void ConTranslate(IConsole::IResult *pResult, void *pUserData);
	static void ConTranslateId(IConsole::IResult *pResult, void *pUserData);

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
