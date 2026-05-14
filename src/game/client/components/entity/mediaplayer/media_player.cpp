#include "media_player_impl.h"

#include <base/system.h>

#include <engine/image.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numbers>
#include <string>
#include <vector>
#define IStorage EngineIStorage
#include <engine/shared/config.h>
#undef IStorage

void ClearSharedAlbumArt(CMediaViewer::CShared *pShared)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	pShared->m_AlbumArtRgba.clear();
	pShared->m_AlbumArtWidth = 0;
	pShared->m_AlbumArtHeight = 0;
	pShared->m_AlbumArtDirty = true;
}

void SetSharedAlbumArt(CMediaViewer::CShared *pShared, std::vector<uint8_t> &&Pixels, int Width, int Height)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	pShared->m_AlbumArtRgba = std::move(Pixels);
	pShared->m_AlbumArtWidth = Width;
	pShared->m_AlbumArtHeight = Height;
	pShared->m_AlbumArtDirty = true;
}

void ClearAlbumArtColors(CPlainState &State)
{
	State.m_AlbumArtColors = CMediaViewer::CAlbumArtColors{};
}

void CopyPlainStateToState(const CPlainState &Source, CMediaViewer::CState &Dest)
{
	Dest.m_CanPlay = Source.m_CanPlay;
	Dest.m_CanPause = Source.m_CanPause;
	Dest.m_CanPrev = Source.m_CanPrev;
	Dest.m_CanNext = Source.m_CanNext;
	Dest.m_Playing = Source.m_Playing;
	Dest.m_ServiceId = Source.m_ServiceId;
	Dest.m_Title = Source.m_Title;
	Dest.m_Artist = Source.m_Artist;
	Dest.m_Album = Source.m_Album;
	Dest.m_PositionMs = Source.m_PositionMs;
	Dest.m_DurationMs = Source.m_DurationMs;
	Dest.m_AlbumArt.m_Colors = Source.m_AlbumArtColors;
}

void PublishSharedState(CMediaViewer::CShared *pShared, const CPlainState &State, bool HasMedia, std::deque<ECommand> *pCommands)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	pShared->m_State = State;
	pShared->m_HasMedia = HasMedia;
	if(pCommands)
		pCommands->swap(pShared->m_Commands);
}

void QueueSharedCommand(CMediaViewer::CShared *pShared, ECommand Command)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	pShared->m_Commands.push_back(Command);
}

bool ConsumeSharedAlbumArt(CMediaViewer::CShared *pShared, std::vector<uint8_t> &Pixels, int &Width, int &Height)
{
	std::scoped_lock Lock(pShared->m_Mutex);
	if(!pShared->m_AlbumArtDirty)
		return false;

	Width = pShared->m_AlbumArtWidth;
	Height = pShared->m_AlbumArtHeight;
	Pixels = std::move(pShared->m_AlbumArtRgba);
	pShared->m_AlbumArtRgba.clear();
	pShared->m_AlbumArtDirty = false;
	return true;
}

class CColorAccumulator
{
public:
	float m_Weight = 0.0f;
	float m_SumR = 0.0f;
	float m_SumG = 0.0f;
	float m_SumB = 0.0f;

	void Add(const ColorRGBA &Color, float Weight)
	{
		if(Weight <= 0.0f)
			return;

		m_Weight += Weight;
		m_SumR += Color.r * Weight;
		m_SumG += Color.g * Weight;
		m_SumB += Color.b * Weight;
	}

	void Add(const CColorAccumulator &Other)
	{
		m_Weight += Other.m_Weight;
		m_SumR += Other.m_SumR;
		m_SumG += Other.m_SumG;
		m_SumB += Other.m_SumB;
	}

	[[nodiscard]] ColorRGBA Average() const
	{
		if(m_Weight <= 0.0f)
			return ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);

		const float InvWeight = 1.0f / m_Weight;
		return ColorRGBA(
			std::clamp(m_SumR * InvWeight, 0.0f, 1.0f),
			std::clamp(m_SumG * InvWeight, 0.0f, 1.0f),
			std::clamp(m_SumB * InvWeight, 0.0f, 1.0f),
			1.0f);
	}
};

static float ColorDistance(const ColorRGBA &A, const ColorRGBA &B)
{
	const float Dr = A.r - B.r;
	const float Dg = A.g - B.g;
	const float Db = A.b - B.b;
	return std::sqrt(Dr * Dr + Dg * Dg + Db * Db);
}

static int CircularBinDistance(int A, int B, int BinCount)
{
	const int Dist = std::abs(A - B);
	return std::min(Dist, BinCount - Dist);
}

static bool IsColorDistinctEnough(const ColorRGBA &Primary, const ColorRGBA &Candidate)
{
	const ColorHSLA PrimaryHsl = color_cast<ColorHSLA>(Primary);
	const ColorHSLA CandidateHsl = color_cast<ColorHSLA>(Candidate);
	const float HueDistance = std::min(std::abs(PrimaryHsl.h - CandidateHsl.h), 1.0f - std::abs(PrimaryHsl.h - CandidateHsl.h));
	const float LightnessDistance = std::abs(PrimaryHsl.l - CandidateHsl.l);
	const float SaturationDistance = std::abs(PrimaryHsl.s - CandidateHsl.s);
	const float RgbDistance = ColorDistance(Primary, Candidate);

	return HueDistance >= 0.10f ||
	       LightnessDistance >= 0.16f ||
	       SaturationDistance >= 0.22f ||
	       RgbDistance >= 0.25f;
}

void ComputeAlbumArtColors(const std::vector<uint8_t> &Pixels, int Width, int Height, CPlainState &State)
{
	ClearAlbumArtColors(State);

	if(Width <= 0 || Height <= 0 || Pixels.size() < (size_t)Width * (size_t)Height * 4)
		return;

	constexpr int HUE_BIN_COUNT = 24;
	constexpr float MIN_ALPHA = 8.0f / 255.0f;
	constexpr float MIN_SATURATION = 0.10f;
	std::array<CColorAccumulator, HUE_BIN_COUNT> aHueBins{};
	CColorAccumulator NeutralBin;
	float TotalWeight = 0.0f;
	for(size_t Index = 0; Index + 3 < Pixels.size(); Index += 4)
	{
		const float Alpha = Pixels[Index + 3] / 255.0f;
		if(Alpha <= MIN_ALPHA)
			continue;

		float R = Pixels[Index + 0] / 255.0f;
		float G = Pixels[Index + 1] / 255.0f;
		float B = Pixels[Index + 2] / 255.0f;
		if(Alpha < 1.0f)
		{
			const float InvAlpha = 1.0f / Alpha;
			R = std::clamp(R * InvAlpha, 0.0f, 1.0f);
			G = std::clamp(G * InvAlpha, 0.0f, 1.0f);
			B = std::clamp(B * InvAlpha, 0.0f, 1.0f);
		}

		const ColorRGBA Color(R, G, B, 1.0f);
		const ColorHSLA Hsl = color_cast<ColorHSLA>(Color);
		const float Weight = Alpha * (0.30f + 0.70f * Hsl.s);
		if(Weight <= 0.0f)
			continue;

		TotalWeight += Weight;
		if(Hsl.s < MIN_SATURATION)
		{
			NeutralBin.Add(Color, Weight * 0.85f);
			continue;
		}

		const int Bin = std::clamp((int)(Hsl.h * HUE_BIN_COUNT), 0, HUE_BIN_COUNT - 1);
		aHueBins[Bin].Add(Color, Weight);
	}

	if(TotalWeight <= 0.0f)
		return;

	auto CombinedBinScore = [&](int Bin) {
		return aHueBins[Bin].m_Weight +
		       aHueBins[(Bin + HUE_BIN_COUNT - 1) % HUE_BIN_COUNT].m_Weight * 0.65f +
		       aHueBins[(Bin + 1) % HUE_BIN_COUNT].m_Weight * 0.65f;
	};

	auto CombinedAccumulator = [&](int Bin) {
		CColorAccumulator Result;
		Result.Add(aHueBins[(Bin + HUE_BIN_COUNT - 1) % HUE_BIN_COUNT]);
		Result.Add(aHueBins[Bin]);
		Result.Add(aHueBins[(Bin + 1) % HUE_BIN_COUNT]);
		return Result;
	};

	bool PrimaryIsNeutral = NeutralBin.m_Weight > 0.0f;
	int PrimaryBin = -1;
	float BestPrimaryScore = NeutralBin.m_Weight * 0.90f;
	for(int Bin = 0; Bin < HUE_BIN_COUNT; ++Bin)
	{
		const float Score = CombinedBinScore(Bin);
		if(Score > BestPrimaryScore)
		{
			BestPrimaryScore = Score;
			PrimaryBin = Bin;
			PrimaryIsNeutral = false;
		}
	}

	if(PrimaryIsNeutral)
	{
		if(NeutralBin.m_Weight <= 0.0f)
			return;
		State.m_AlbumArtColors.m_Primary = NeutralBin.Average();
	}
	else
	{
		const CColorAccumulator PrimaryAccumulator = CombinedAccumulator(PrimaryBin);
		if(PrimaryAccumulator.m_Weight <= 0.0f)
			return;
		State.m_AlbumArtColors.m_Primary = PrimaryAccumulator.Average();
	}
	State.m_AlbumArtColors.m_HasPrimary = true;

	const ColorRGBA PrimaryColor = State.m_AlbumArtColors.m_Primary;
	float BestSecondaryScore = 0.0f;
	ColorRGBA SecondaryColor = PrimaryColor;

	if(NeutralBin.m_Weight >= TotalWeight * 0.04f)
	{
		const ColorRGBA NeutralColor = NeutralBin.Average();
		if(IsColorDistinctEnough(PrimaryColor, NeutralColor))
		{
			BestSecondaryScore = NeutralBin.m_Weight;
			SecondaryColor = NeutralColor;
		}
	}

	for(int Bin = 0; Bin < HUE_BIN_COUNT; ++Bin)
	{
		if(!PrimaryIsNeutral && CircularBinDistance(Bin, PrimaryBin, HUE_BIN_COUNT) < 3)
			continue;

		const float Score = CombinedBinScore(Bin);
		if(Score < TotalWeight * 0.04f || Score <= BestSecondaryScore)
			continue;

		const CColorAccumulator CandidateAccumulator = CombinedAccumulator(Bin);
		if(CandidateAccumulator.m_Weight <= 0.0f)
			continue;

		const ColorRGBA CandidateColor = CandidateAccumulator.Average();
		if(!IsColorDistinctEnough(PrimaryColor, CandidateColor))
			continue;

		BestSecondaryScore = Score;
		SecondaryColor = CandidateColor;
	}

	if(BestSecondaryScore > 0.0f)
	{
		State.m_AlbumArtColors.m_Secondary = SecondaryColor;
		State.m_AlbumArtColors.m_HasSecondary = true;
	}
}

static void ApplyRoundedMask(std::vector<uint8_t> &Pixels, int Width, int Height, float Radius)
{
	if(Pixels.empty() || Width <= 0 || Height <= 0 || Radius <= 0.0f)
		return;

	const float MaxRadius = 0.5f * (float)std::min(Width, Height);
	const float R = std::min(Radius, MaxRadius);
	if(R <= 0.0f)
		return;

	const float Left = R;
	const float Right = (float)Width - R;
	const float Top = R;
	const float Bottom = (float)Height - R;
	const float OuterR2 = R * R;
	const float InnerR = R - 1.0f;
	const float InnerR2 = InnerR > 0.0f ? InnerR * InnerR : 0.0f;
	const bool UseSoftEdge = InnerR > 0.0f;

	for(int y = 0; y < Height; ++y)
	{
		const float Fy = (float)y + 0.5f;
		for(int x = 0; x < Width; ++x)
		{
			const float Fx = (float)x + 0.5f;
			float Dx = 0.0f;
			float Dy = 0.0f;
			bool Corner = false;

			if(Fx < Left && Fy < Top)
			{
				Dx = Left - Fx;
				Dy = Top - Fy;
				Corner = true;
			}
			else if(Fx > Right && Fy < Top)
			{
				Dx = Fx - Right;
				Dy = Top - Fy;
				Corner = true;
			}
			else if(Fx < Left && Fy > Bottom)
			{
				Dx = Left - Fx;
				Dy = Fy - Bottom;
				Corner = true;
			}
			else if(Fx > Right && Fy > Bottom)
			{
				Dx = Fx - Right;
				Dy = Fy - Bottom;
				Corner = true;
			}

			if(!Corner)
				continue;

			const float Dist2 = Dx * Dx + Dy * Dy;
			if(Dist2 <= (UseSoftEdge ? InnerR2 : OuterR2))
				continue;

			float Alpha = 0.0f;
			if(UseSoftEdge && Dist2 < OuterR2)
			{
				const float Dist = std::sqrt(Dist2);
				Alpha = std::clamp(R - Dist, 0.0f, 1.0f);
			}

			const size_t Index = (size_t)(y * Width + x) * 4;
			if(Alpha <= 0.0f)
			{
				Pixels[Index + 0] = 0;
				Pixels[Index + 1] = 0;
				Pixels[Index + 2] = 0;
				Pixels[Index + 3] = 0;
			}
			else if(Alpha < 1.0f)
			{
				Pixels[Index + 0] = (uint8_t)std::round(Pixels[Index + 0] * Alpha);
				Pixels[Index + 1] = (uint8_t)std::round(Pixels[Index + 1] * Alpha);
				Pixels[Index + 2] = (uint8_t)std::round(Pixels[Index + 2] * Alpha);
				Pixels[Index + 3] = (uint8_t)std::round(Pixels[Index + 3] * Alpha);
			}
		}
	}
}

bool PrepareAlbumArtPixels(std::vector<uint8_t> &Pixels, int Width, int Height)
{
	const size_t ExpectedSize = (size_t)Width * (size_t)Height * 4;
	if(Width <= 0 || Height <= 0 || Pixels.size() < ExpectedSize)
		return false;

	constexpr float RoundingRatio = 2.0f / 14.0f;
	const float Radius = (float)std::min(Width, Height) * RoundingRatio;
	ApplyRoundedMask(Pixels, Width, Height, Radius);
	return true;
}

bool LoadAlbumArtTexture(IGraphics *pGraphics, const std::vector<uint8_t> &Pixels, int Width, int Height, const char *pTextureName, IGraphics::CTextureHandle &Texture)
{
	if(!pGraphics || Width <= 0 || Height <= 0)
		return false;

	const size_t ExpectedSize = (size_t)Width * (size_t)Height * 4;
	if(Pixels.size() < ExpectedSize)
		return false;

	CImageInfo Image;
	Image.m_Width = (size_t)Width;
	Image.m_Height = (size_t)Height;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.m_pData = static_cast<uint8_t *>(malloc(ExpectedSize));
	if(!Image.m_pData)
		return false;

	mem_copy(Image.m_pData, Pixels.data(), ExpectedSize);
	Texture = pGraphics->LoadTextureRawMove(Image, 0, pTextureName);
	if(Image.m_pData)
		Image.Free();
	return Texture.IsValid();
}

CMediaViewer::CMediaViewer() = default;
CMediaViewer::~CMediaViewer() = default;

void CMediaViewer::OnInit()
{
#if MEDIA_PLAYER_WINRT
	m_pWinrt = std::make_unique<CWinrt>();
	m_pShared = std::make_unique<CShared>();
	m_pAudioCapture = std::make_unique<CAudioCapture>();

	m_StopThread.store(false, std::memory_order_relaxed);
	m_StopAudioThread.store(false, std::memory_order_relaxed);

	m_Thread = std::thread(&CMediaViewer::ThreadMain, this);
	m_AudioThread = std::thread(&CMediaViewer::AudioThreadMain, this);
#elif MEDIA_PLAYER_DBUS
	m_pDbus = std::make_unique<CDbus>();
	m_pShared = std::make_unique<CShared>();
	m_pAudioCapture = std::make_unique<CAudioCapture>();
	m_StopThread.store(false, std::memory_order_relaxed);
	m_StopAudioThread.store(false, std::memory_order_relaxed);
	m_Thread = std::thread(&CMediaViewer::ThreadMain, this);
	m_AudioThread = std::thread(&CMediaViewer::AudioThreadMain, this);
#endif
}

void CMediaViewer::OnShutdown()
{
#if MEDIA_PLAYER_WINRT
	m_StopThread.store(true, std::memory_order_relaxed);
	m_StopAudioThread.store(true, std::memory_order_relaxed);

	if(m_Thread.joinable())
		m_Thread.join();
	if(m_AudioThread.joinable())
		m_AudioThread.join();

	ClearState(m_pWinrt.get(), Graphics());

	m_pAudioCapture.reset();
	m_pShared.reset();
	m_pWinrt.reset();
#elif MEDIA_PLAYER_DBUS
	m_StopThread.store(true, std::memory_order_relaxed);
	m_StopAudioThread.store(true, std::memory_order_relaxed);
	if(m_AudioThread.joinable())
		m_AudioThread.join();
	if(m_Thread.joinable())
		m_Thread.join();
	ClearDbusAlbumArtLocal(m_pDbus.get(), Graphics());
	m_pAudioCapture.reset();
	m_pShared.reset();
	m_pDbus.reset();
#endif
}

void CMediaViewer::OnUpdate()
{
#if MEDIA_PLAYER_WINRT
	if(!m_pWinrt || !m_pShared)
		return;

	CPlainState SharedState{};
	bool HasMedia = false;
	{
		std::scoped_lock Lock(m_pShared->m_Mutex);
		SharedState = m_pShared->m_State;
		HasMedia = m_pShared->m_HasMedia;
	}

	if(!HasMedia)
	{
		if(m_pWinrt->m_HasMedia)
			ClearState(m_pWinrt.get(), Graphics());
		m_pWinrt->m_HasMedia = false;
	}
	else
	{
		m_pWinrt->m_HasMedia = true;
		CopyPlainStateToState(SharedState, m_pWinrt->m_State);
	}

	ApplySharedAlbumArt(m_pShared.get(), m_pWinrt.get(), Graphics());
#elif MEDIA_PLAYER_DBUS
	if(!m_pShared || !m_pDbus)
		return;

	CPlainState SharedState{};
	bool HasMedia = false;
	{
		std::scoped_lock Lock(m_pShared->m_Mutex);
		SharedState = m_pShared->m_State;
		HasMedia = m_pShared->m_HasMedia;
	}

	if(!HasMedia)
	{
		if(m_pDbus->m_HasMedia)
		{
			std::scoped_lock Lock(m_pDbus->m_Mutex);
			m_pDbus->m_HasMedia = false;
		}
	}
	else
	{
		std::scoped_lock Lock(m_pDbus->m_Mutex);
		m_pDbus->m_HasMedia = true;
		m_pDbus->m_State = SharedState;
	}

	ApplyDbusSharedAlbumArt(m_pShared.get(), m_pDbus.get(), Graphics());
#endif
}

bool CMediaViewer::GetStateSnapshot(CMediaViewer::CState &State) const
{
	if(!g_Config.m_ClMediaIsland)
	{
		State = CMediaViewer::CState{};
		return false;
	}

#if MEDIA_PLAYER_WINRT
	if(!m_pShared || !m_pWinrt || !m_pAudioCapture)
	{
		State = CMediaViewer::CState{};
		return false;
	}

	if(m_pWinrt->m_HasMedia)
	{
		State = m_pWinrt->m_State;
		{
			std::scoped_lock Lock(m_pAudioCapture->m_Mutex);
			State.m_Visualizer.m_aFrequencyBands = m_pAudioCapture->m_aFrequencyBands;
			State.m_Visualizer.m_Active = m_pAudioCapture->m_Active;
		}
		return true;
	}
#elif MEDIA_PLAYER_DBUS
	if(!m_pDbus || !m_pAudioCapture)
	{
		State = CMediaViewer::CState{};
		return false;
	}

	{
		std::scoped_lock Lock(m_pDbus->m_Mutex);
		if(!m_pDbus->m_HasMedia)
		{
			State = CMediaViewer::CState{};
			return false;
		}

		CopyPlainStateToState(m_pDbus->m_State, State);
		State.m_AlbumArt = m_pDbus->m_AlbumArt;
		State.m_AlbumArt.m_Colors = m_pDbus->m_State.m_AlbumArtColors;
		State.m_PrevAlbumArt = m_pDbus->m_PrevAlbumArt;
	}

	{
		std::scoped_lock Lock(m_pAudioCapture->m_Mutex);
		State.m_Visualizer.m_aFrequencyBands = m_pAudioCapture->m_aFrequencyBands;
		State.m_Visualizer.m_Active = m_pAudioCapture->m_Active;
	}
	return true;
#endif

	State = CMediaViewer::CState{};
	return false;
}

void CMediaViewer::Previous()
{
#if MEDIA_PLAYER_WINRT || MEDIA_PLAYER_DBUS
	if(!m_pShared)
		return;

	QueueSharedCommand(m_pShared.get(), ECommand::Prev);
#endif
}

void CMediaViewer::PlayPause()
{
#if MEDIA_PLAYER_WINRT || MEDIA_PLAYER_DBUS
	if(!m_pShared)
		return;

	QueueSharedCommand(m_pShared.get(), ECommand::PlayPause);
#endif
}

void CMediaViewer::Next()
{
#if MEDIA_PLAYER_WINRT || MEDIA_PLAYER_DBUS
	if(!m_pShared)
		return;

	QueueSharedCommand(m_pShared.get(), ECommand::Next);
#endif
}

void CMediaViewer::CVisualizer::GetBands(float *pOutBands, int NumBands) const
{
	const int CopyCount = std::min(NumBands, NUM_FREQUENCY_BANDS);
	for(int i = 0; i < CopyCount; ++i)
		pOutBands[i] = m_aFrequencyBands[i];
}

float CMediaViewer::CVisualizer::GetAverageBand() const
{
	float Sum = 0.0f;
	for(float Band : m_aFrequencyBands)
		Sum += Band;
	return Sum / NUM_FREQUENCY_BANDS;
}
