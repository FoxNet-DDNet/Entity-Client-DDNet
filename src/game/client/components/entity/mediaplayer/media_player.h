// https://github.com/wxj881027/QmClient
#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MEDIAPLAYER_MEDIA_PLAYER_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MEDIAPLAYER_MEDIA_PLAYER_H

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/component.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

#ifndef MEDIA_PLAYER_DBUS
#define MEDIA_PLAYER_DBUS 0
#endif

#ifndef MEDIA_PLAYER_PULSEAUDIO
#define MEDIA_PLAYER_PULSEAUDIO 0
#endif

#include <base/detect.h>

class CMediaViewer : public CComponent
{
public:
	class CVisualizer
	{
	public:
		static constexpr int NUM_FREQUENCY_BANDS = 64; // Number of frequency bars
		std::array<float, NUM_FREQUENCY_BANDS> m_aFrequencyBands; // 0.0 to 1.0
		bool m_Active = false;

		void GetBands(float *pOutBands, int NumBands) const;
		float GetAverageBand() const;
	};

	class CAlbumArtColors
	{
	public:
		ColorRGBA m_Primary = ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);
		ColorRGBA m_Secondary = ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);
		bool m_HasPrimary = false;
		bool m_HasSecondary = false;

		const ColorRGBA &GetPrimary() const
		{
			return m_Primary;
		}

		const ColorRGBA &GetSecondary() const
		{
			return m_HasSecondary ? m_Secondary : m_Primary;
		}

		bool operator==(const CAlbumArtColors &Other) const
		{
			return m_Primary == Other.m_Primary &&
			       m_Secondary == Other.m_Secondary &&
			       m_HasPrimary == Other.m_HasPrimary &&
			       m_HasSecondary == Other.m_HasSecondary;
		}
	};

	class CAlbumArt
	{
	public:
		IGraphics::CTextureHandle m_Texture;
		int m_Width = 0;
		int m_Height = 0;
		CAlbumArtColors m_Colors;
	};

	class CState
	{
	public:
		bool m_CanPlay = false;
		bool m_CanPause = false;
		bool m_CanPrev = false;
		bool m_CanNext = false;
		bool m_Playing = false;
		std::string m_ServiceId;
		std::string m_Title;
		std::string m_Artist;
		std::string m_Album;
		int64_t m_PositionMs = 0;
		int64_t m_DurationMs = 0;

		CAlbumArt m_AlbumArt;
		CAlbumArt m_PrevAlbumArt; // for media island transition

		CVisualizer m_Visualizer;

		bool operator==(const CState &Other) const
		{
			return m_ServiceId == Other.m_ServiceId &&
			       m_Title == Other.m_Title &&
			       m_Artist == Other.m_Artist &&
			       m_Album == Other.m_Album &&
			       m_AlbumArt.m_Colors == Other.m_AlbumArt.m_Colors;
		}
	};

	class CShared;

#if defined(CONF_FAMILY_WINDOWS) && __has_include(<winrt/Windows.Foundation.h>)
	class CWinrt;
	class CAudioCapture;
#elif MEDIA_PLAYER_DBUS
	class CDbus;
	class CAudioCapture;
#endif

	CMediaViewer();
	~CMediaViewer() override;
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnUpdate() override;

	bool GetStateSnapshot(CState &State) const;
	void Previous();
	void PlayPause();
	void Next();
	void ProcessAudioFrame(const float *pSamples, int NumSamples, int SampleRate);

private:
#if defined(CONF_FAMILY_WINDOWS) && __has_include(<winrt/Windows.Foundation.h>)
	std::unique_ptr<CWinrt> m_pWinrt;
	std::unique_ptr<CShared> m_pShared;
	std::unique_ptr<CAudioCapture> m_pAudioCapture;
	std::thread m_Thread;
	std::thread m_AudioThread;
	std::atomic_bool m_StopThread = false;
	std::atomic_bool m_StopAudioThread = false;

	void ThreadMain();
	void AudioThreadMain();
#elif MEDIA_PLAYER_DBUS
	std::unique_ptr<CDbus> m_pDbus;
	std::unique_ptr<CShared> m_pShared;
	std::unique_ptr<CAudioCapture> m_pAudioCapture;
	std::thread m_Thread;
	std::thread m_AudioThread;
	std::atomic_bool m_StopThread = false;
	std::atomic_bool m_StopAudioThread = false;

	void ThreadMain();
	void AudioThreadMain();
#endif
};

#endif
