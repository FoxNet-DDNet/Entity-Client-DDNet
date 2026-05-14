// https://github.com/wxj881027/QmClient
#ifndef GAME_CLIENT_COMPONENTS_ENTITY_MEDIAPLAYER_MEDIA_PLAYER_IMPL_H
#define GAME_CLIENT_COMPONENTS_ENTITY_MEDIAPLAYER_MEDIA_PLAYER_IMPL_H

#include <base/detect.h>

#ifndef MEDIA_PLAYER_DBUS
#define MEDIA_PLAYER_DBUS 0
#endif

#if defined(CONF_FAMILY_WINDOWS) && __has_include(<winrt/Windows.Foundation.h>)
#define MEDIA_PLAYER_WINRT 1
#endif

#ifndef MEDIA_PLAYER_WINRT
#define MEDIA_PLAYER_WINRT 0
#endif

#ifndef MEDIA_PLAYER_PULSEAUDIO
#define MEDIA_PLAYER_PULSEAUDIO 0
#endif

#include "media_player.h"

#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <numbers>
#include <string>
#include <vector>

class CPlainState
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
	CMediaViewer::CAlbumArtColors m_AlbumArtColors;
	std::string m_AlbumArtUrl;
	std::string m_TrackId;
};

enum class ECommand
{
	Prev,
	PlayPause,
	Next,
};

void ClearSharedAlbumArt(CMediaViewer::CShared *pShared);
void SetSharedAlbumArt(CMediaViewer::CShared *pShared, std::vector<uint8_t> &&Pixels, int Width, int Height);
void ClearAlbumArtColors(CPlainState &State);
void CopyPlainStateToState(const CPlainState &Source, CMediaViewer::CState &Dest);
void PublishSharedState(CMediaViewer::CShared *pShared, const CPlainState &State, bool HasMedia, std::deque<ECommand> *pCommands = nullptr);
void QueueSharedCommand(CMediaViewer::CShared *pShared, ECommand Command);
bool ConsumeSharedAlbumArt(CMediaViewer::CShared *pShared, std::vector<uint8_t> &Pixels, int &Width, int &Height);
void ComputeAlbumArtColors(const std::vector<uint8_t> &Pixels, int Width, int Height, CPlainState &State);
bool PrepareAlbumArtPixels(std::vector<uint8_t> &Pixels, int Width, int Height);
bool LoadAlbumArtTexture(IGraphics *pGraphics, const std::vector<uint8_t> &Pixels, int Width, int Height, const char *pTextureName, IGraphics::CTextureHandle &Texture);

#if MEDIA_PLAYER_WINRT
void ClearState(CMediaViewer::CWinrt *pWinrt, IGraphics *pGraphics);
void ApplySharedAlbumArt(CMediaViewer::CShared *pShared, CMediaViewer::CWinrt *pWinrt, IGraphics *pGraphics);
#endif

#if MEDIA_PLAYER_DBUS
void ClearDbusAlbumArtLocal(CMediaViewer::CDbus *pDbus, IGraphics *pGraphics);
void ApplyDbusSharedAlbumArt(CMediaViewer::CShared *pShared, CMediaViewer::CDbus *pDbus, IGraphics *pGraphics);
#endif

class CMediaViewer::CShared
{
public:
	std::mutex m_Mutex;
	CPlainState m_State{};
	bool m_HasMedia = false;
	std::deque<ECommand> m_Commands;
	std::vector<uint8_t> m_AlbumArtRgba;
	int m_AlbumArtWidth = 0;
	int m_AlbumArtHeight = 0;
	bool m_AlbumArtDirty = false;
};

#if MEDIA_PLAYER_WINRT
class CMediaViewer::CWinrt
{
public:
	CMediaViewer::CState m_State = {};
	bool m_HasMedia = false;
	std::vector<uint8_t> m_AlbumArtPendingRgba;
	int m_AlbumArtPendingWidth = 0;
	int m_AlbumArtPendingHeight = 0;
};
#endif

#if MEDIA_PLAYER_WINRT || MEDIA_PLAYER_DBUS
namespace FFT
{
	constexpr int FFT_SIZE = 768;

	class CComplex
	{
	public:
		float Real;
		float Imag;

		CComplex(float r = 0.0f, float i = 0.0f) :
			Real(r), Imag(i) {}

		CComplex operator+(const CComplex &Other) const
		{
			return CComplex(Real + Other.Real, Imag + Other.Imag);
		}

		CComplex operator-(const CComplex &Other) const
		{
			return CComplex(Real - Other.Real, Imag - Other.Imag);
		}

		CComplex operator*(const CComplex &Other) const
		{
			return CComplex(
				Real * Other.Real - Imag * Other.Imag,
				Real * Other.Imag + Imag * Other.Real);
		}

		float Magnitude() const
		{
			return std::sqrt(Real * Real + Imag * Imag);
		}
	};

	inline void FFTRecursive(std::vector<CComplex> &x)
	{
		const int N = (int)x.size();
		if(N <= 1)
			return;

		std::vector<CComplex> Even(N / 2);
		std::vector<CComplex> Odd(N / 2);
		for(int i = 0; i < N / 2; ++i)
		{
			Even[i] = x[i * 2];
			Odd[i] = x[i * 2 + 1];
		}

		FFTRecursive(Even);
		FFTRecursive(Odd);

		for(int k = 0; k < N / 2; ++k)
		{
			const float Angle = -2.0f * std::numbers::pi_v<float> * k / N;
			const CComplex t = CComplex(std::cos(Angle), std::sin(Angle)) * Odd[k];
			x[k] = Even[k] + t;
			x[k + N / 2] = Even[k] - t;
		}
	}

	inline void ComputeFFT(const float *pSamples, int NumSamples, std::vector<CComplex> &Output)
	{
		const int N = std::min(NumSamples, FFT_SIZE);
		Output.resize(FFT_SIZE);

		for(int i = 0; i < N; ++i)
		{
			const float Window = 0.54f - 0.46f * std::cos(2.0f * std::numbers::pi_v<float> * i / (N - 1));
			Output[i] = CComplex(pSamples[i] * Window, 0.0f);
		}

		for(int i = N; i < FFT_SIZE; ++i)
			Output[i] = CComplex(0.0f, 0.0f);

		FFTRecursive(Output);
	}
}

class CMediaViewer::CAudioCapture
{
public:
	std::mutex m_Mutex;
	std::array<float, CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS> m_aFrequencyBands{};
	bool m_Active = false;
};
#endif

#if MEDIA_PLAYER_DBUS
class CMediaViewer::CDbus
{
public:
	std::mutex m_Mutex;
	CPlainState m_State{};
	bool m_HasMedia = false;
	CMediaViewer::CAlbumArt m_AlbumArt;
	CMediaViewer::CAlbumArt m_PrevAlbumArt;
	std::vector<uint8_t> m_AlbumArtPendingRgba;
	int m_AlbumArtPendingWidth = 0;
	int m_AlbumArtPendingHeight = 0;
};
#endif

#endif
