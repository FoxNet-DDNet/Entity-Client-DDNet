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

#include <algorithm>
#include <array>
#include <cmath>
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

#if MEDIA_PLAYER_WINRT || MEDIA_PLAYER_DBUS
// Lets the bands fall back towards zero while no audio is arriving, so they do not freeze
// mid-bounce when playback stops.
void DecayAudioBands(CMediaViewer::CAudioCapture *pAudioCapture, float DeltaSeconds);
#endif

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
	// Radix-2 Cooley-Tukey only works on power of two sizes.
	constexpr int FFT_SIZE = 1024;
	static_assert((FFT_SIZE & (FFT_SIZE - 1)) == 0, "FFT_SIZE must be a power of two");
	// Bins above the Nyquist frequency mirror the ones below it and carry no extra information.
	constexpr int FFT_BINS = FFT_SIZE / 2;
	// Transforms overlap: a new one starts every FFT_HOP samples rather than every FFT_SIZE, so
	// the bars update four times per window. This is most of what makes them feel responsive to
	// the beat instead of catching up to it.
	constexpr int FFT_HOP = FFT_SIZE / 4;

	class CComplex
	{
	public:
		float m_Real;
		float m_Imag;

		CComplex(float Real = 0.0f, float Imag = 0.0f) :
			m_Real(Real), m_Imag(Imag) {}

		CComplex operator+(const CComplex &Other) const
		{
			return CComplex(m_Real + Other.m_Real, m_Imag + Other.m_Imag);
		}

		CComplex operator-(const CComplex &Other) const
		{
			return CComplex(m_Real - Other.m_Real, m_Imag - Other.m_Imag);
		}

		CComplex operator*(const CComplex &Other) const
		{
			return CComplex(
				m_Real * Other.m_Real - m_Imag * Other.m_Imag,
				m_Real * Other.m_Imag + m_Imag * Other.m_Real);
		}

		float Magnitude() const
		{
			return std::sqrt(m_Real * m_Real + m_Imag * m_Imag);
		}
	};

	using CBuffer = std::array<CComplex, FFT_SIZE>;

	// Everything the transform needs that only depends on FFT_SIZE, computed once instead of
	// once per audio frame.
	class CTables
	{
	public:
		std::array<int, FFT_SIZE> m_aBitReverse{};
		std::array<CComplex, FFT_BINS> m_aTwiddle{};
		std::array<float, FFT_SIZE> m_aWindow{};
		float m_WindowGain = 0.0f; // Mean of the window, undoes the level the window takes away.

		CTables()
		{
			int NumBits = 0;
			while((1 << NumBits) < FFT_SIZE)
				++NumBits;

			for(int Index = 0; Index < FFT_SIZE; ++Index)
			{
				int Reversed = 0;
				for(int Bit = 0; Bit < NumBits; ++Bit)
					Reversed |= ((Index >> Bit) & 1) << (NumBits - 1 - Bit);
				m_aBitReverse[Index] = Reversed;
			}

			for(int Index = 0; Index < FFT_BINS; ++Index)
			{
				const float Angle = -2.0f * std::numbers::pi_v<float> * Index / FFT_SIZE;
				m_aTwiddle[Index] = CComplex(std::cos(Angle), std::sin(Angle));
			}

			float WindowSum = 0.0f;
			for(int Index = 0; Index < FFT_SIZE; ++Index)
			{
				// Hamming window, keeps a single tone from leaking across the whole spectrum.
				m_aWindow[Index] = 0.54f - 0.46f * std::cos(2.0f * std::numbers::pi_v<float> * Index / (FFT_SIZE - 1));
				WindowSum += m_aWindow[Index];
			}
			m_WindowGain = WindowSum / FFT_SIZE;
		}
	};

	inline const CTables &Tables()
	{
		static const CTables s_Tables;
		return s_Tables;
	}

	// Windows up to FFT_SIZE real samples into Output, zero pads the rest and transforms them in
	// place. Runs without allocating so it is safe to call from an audio callback. Only the first
	// FFT_BINS bins of the result are meaningful for real input.
	inline void ComputeFFT(const float *pSamples, int NumSamples, CBuffer &Output)
	{
		const CTables &Tables = FFT::Tables();
		const int NumUsed = std::min(NumSamples, FFT_SIZE);

		// Scattering the input through the bit reversal permutation up front lets the butterflies
		// below run in place.
		for(int Index = 0; Index < NumUsed; ++Index)
			Output[Tables.m_aBitReverse[Index]] = CComplex(pSamples[Index] * Tables.m_aWindow[Index], 0.0f);
		for(int Index = NumUsed; Index < FFT_SIZE; ++Index)
			Output[Tables.m_aBitReverse[Index]] = CComplex(0.0f, 0.0f);

		for(int Length = 2; Length <= FFT_SIZE; Length <<= 1)
		{
			const int Half = Length / 2;
			const int TwiddleStep = FFT_SIZE / Length;
			for(int Base = 0; Base < FFT_SIZE; Base += Length)
			{
				for(int Offset = 0; Offset < Half; ++Offset)
				{
					const CComplex Odd = Tables.m_aTwiddle[Offset * TwiddleStep] * Output[Base + Offset + Half];
					const CComplex Even = Output[Base + Offset];
					Output[Base + Offset] = Even + Odd;
					Output[Base + Offset + Half] = Even - Odd;
				}
			}
		}
	}
}

class CMediaViewer::CAudioCapture
{
public:
	// Which FFT bins feed each visualizer band, plus the correction applied to it. Depends on the
	// sample rate only, so it is rebuilt when that changes instead of once per audio frame.
	class CBandPlan
	{
	public:
		int m_SampleRate = 0;
		std::array<int, CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS> m_aBinBegin{};
		std::array<int, CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS> m_aBinEnd{};
		std::array<float, CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS> m_aTiltDb{};

		void Update(int SampleRate);
	};

	std::mutex m_Mutex;
	std::array<float, CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS> m_aFrequencyBands{};
	// How fast each bar is currently falling. Paired with m_aFrequencyBands, same lock.
	std::array<float, CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS> m_aFallSpeed{};
	bool m_Active = false;

	// Only ever touched by the audio thread, so these need no locking.
	CBandPlan m_BandPlan;
	FFT::CBuffer m_FftBuffer{};
	// The newest samples, downmixed to mono. Refilled by FFT_HOP between transforms.
	std::array<float, FFT::FFT_SIZE> m_aWindow{};
	int m_WindowCount = 0;
	// The level each band has been sitting at recently, in dB. Movement is measured against this.
	std::array<float, CMediaViewer::CVisualizer::NUM_FREQUENCY_BANDS> m_aAverageDb{};
	bool m_AverageSeeded = false;
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
