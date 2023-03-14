// Copyright Epic Games, Inc. All Rights Reserved.

#include "LegacyScreenPercentageDriver.h"
#include "UnrealEngine.h"
#include "Misc/ConfigCacheIni.h"
#include "DynamicResolutionState.h"


static TAutoConsoleVariable<int32> CVarScreenPercentageMode(
	TEXT("r.ScreenPercentage.Mode"), 0,
	TEXT("Selects mode to control the screen percentage.\n")
	TEXT(" 0: Controls the view's screen percentage based on r.ScreenPercentage\n")
	TEXT(" 1: Controls the view's screen percentage based on displayed resolution with r.ScreenPercentage.Auto.* (default)\n"),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<float> CVarScreenPercentage(
	TEXT("r.ScreenPercentage"), 100.0f,
	TEXT("To render in lower resolution and upscale for better performance (combined up with the blenable post process setting).\n")
	TEXT("70 is a good value for low aliasing and performance, can be verified with 'show TestImage'\n")
	TEXT("in percent, >0 and <=100, larger numbers are possible (supersampling) but the downsampling quality is improvable.")
	TEXT("<0 is treated like 100."),
	ECVF_Scalability | ECVF_Default);

static TAutoConsoleVariable<float> CVarAutoPixelCountMultiplier(
	TEXT("r.ScreenPercentage.Auto.PixelCountMultiplier"), 1.0f,
	TEXT(""),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarScreenPercentageMinResolution(
	TEXT("r.ScreenPercentage.MinResolution"),
	0.0f,
	TEXT("Controls the absolute minimum number of rendered pixel."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarScreenPercentageMaxResolution(
	TEXT("r.ScreenPercentage.MaxResolution"),
	0.0f,
	TEXT("Controls the absolute maximum number of rendered pixel before any upscaling such that doesn't go higher than the specified 16:9 resolution ")
	TEXT("of this variable. For instance set this value to 1440 so that you are not rendering more than 2560x1440 = 3.6M pixels. ")
	TEXT("This is useful to set this on PC in your project's DefaultEditor.ini so you are not rendering more pixel on PC in PIE that you would ")
	TEXT("in average on console with your project specific dynamic resolution settings."),
	ECVF_Default);

#if !UE_BUILD_SHIPPING

void OnScreenPercentageChange(IConsoleVariable* Var)
{
	float SP = Var->GetFloat();

	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}

#if WITH_EDITOR
	if (FStaticResolutionFractionHeuristic::FUserSettings::EditorOverridePIESettings())
	{
		UE_LOG(LogEngine, Display, TEXT("r.ScreenPercentage=%f is ignored because overriden by editor settings (r.Editor.Viewport.OverridePIEScreenPercentage). Change this behavior in Edit -> Editor Preferences -> Performance"), SP);
		return;
	}
#endif

	// Check whether dynamic resolution is overriding r.ScreenPercentage.
	{
		FDynamicResolutionStateInfos DynResInfo;
		GEngine->GetDynamicResolutionCurrentStateInfos(/* out */ DynResInfo);

		if (DynResInfo.Status == EDynamicResolutionStatus::Enabled)
		{
			UE_LOG(LogEngine, Display, TEXT("r.ScreenPercentage=%f is ignored because overriden by dynamic resolution settings (r.DynamicRes.OperationMode)."), SP);
			return;
		}
		else if (DynResInfo.Status == EDynamicResolutionStatus::DebugForceEnabled)
		{
			UE_LOG(LogEngine, Display, TEXT("r.ScreenPercentage=%f is ignored because overriden by dynamic resolution settings (r.DynamicRes.TestScreenPercentage)."), SP);
			return;
		}
	}

	// Check whether the screen percentage mode honor r.ScreenPercentage
	int32 SPMode = CVarScreenPercentageMode.GetValueOnGameThread();
	if (SPMode != 0)
	{
		UE_LOG(LogEngine, Display, TEXT("r.ScreenPercentage=%f is ignored because overriden by the screen percentage mode (r.ScreenPercentage.Mode=%d)."), SP, SPMode);
		return;
	}
}

#endif // !UE_BUILD_SHIPPING

void InitScreenPercentage()
{
#if !UE_BUILD_SHIPPING
	CVarScreenPercentage.AsVariable()->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&OnScreenPercentageChange));
#endif
}

static float GetRenderingPixelCount(float Rendering169Height)
{
	return Rendering169Height * FMath::Abs(Rendering169Height) * (1920.0f / 1080.0f);
}

static float GetResolutionFraction(float ScreenPercentage)
{
	return ScreenPercentage / 100.0f;
}

FLegacyScreenPercentageDriver::FLegacyScreenPercentageDriver(
	const FSceneViewFamily& InViewFamily,
	float InGlobalResolutionFraction,
	float InGlobalResolutionFractionUpperBound)
	: ViewFamily(InViewFamily)
	, GlobalResolutionFraction(InGlobalResolutionFraction)
	, GlobalResolutionFractionUpperBound(InGlobalResolutionFractionUpperBound)
{
	if (GlobalResolutionFraction != 1.0f)
	{
		check(ViewFamily.EngineShowFlags.ScreenPercentage);
	}
}

DynamicRenderScaling::TMap<float> FLegacyScreenPercentageDriver::GetResolutionFractionsUpperBound() const
{
	DynamicRenderScaling::TMap<float> UpperBounds;
	UpperBounds.SetAll(1.0f);

	if (ViewFamily.EngineShowFlags.ScreenPercentage)
	{
		UpperBounds[GDynamicPrimaryResolutionFraction] = FMath::Clamp(
			GlobalResolutionFractionUpperBound,
			ISceneViewFamilyScreenPercentage::kMinResolutionFraction,
			ISceneViewFamilyScreenPercentage::kMaxResolutionFraction);
	}

	return UpperBounds;
}

DynamicRenderScaling::TMap<float> FLegacyScreenPercentageDriver::GetResolutionFractions_RenderThread() const
{
	check(IsInRenderingThread());

	DynamicRenderScaling::TMap<float> ResolutionFractions;
	ResolutionFractions.SetAll(1.0f);

	if (ViewFamily.EngineShowFlags.ScreenPercentage)
	{
		ResolutionFractions[GDynamicPrimaryResolutionFraction] = FMath::Clamp(
			GlobalResolutionFraction,
			ISceneViewFamilyScreenPercentage::kMinResolutionFraction,
			ISceneViewFamilyScreenPercentage::kMaxResolutionFraction);
	}

	return ResolutionFractions;
}

ISceneViewFamilyScreenPercentage* FLegacyScreenPercentageDriver::Fork_GameThread(
	const FSceneViewFamily& ForkedViewFamily) const
{
	check(IsInGameThread());

	return new FLegacyScreenPercentageDriver(
		ForkedViewFamily, GlobalResolutionFraction, GlobalResolutionFractionUpperBound);
}

// static
float FLegacyScreenPercentageDriver::GetCVarResolutionFraction()
{
	check(IsInGameThread());
	static const auto ScreenPercentageCVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));

	float GlobalFraction = GetResolutionFraction(CVarScreenPercentage.GetValueOnGameThread());
	if (GlobalFraction <= 0.0)
	{
		GlobalFraction = 1.0f;
	}

	return GlobalFraction;
}

FStaticResolutionFractionHeuristic::FStaticResolutionFractionHeuristic(const FEngineShowFlags& EngineShowFlags)
{
	Settings.bAllowDisplayBasedScreenPercentageMode = (EngineShowFlags.StereoRendering == 0) && (EngineShowFlags.VREditing == 0);
}

#if WITH_EDITOR
// static
bool FStaticResolutionFractionHeuristic::FUserSettings::EditorOverridePIESettings()
{
	if (!GIsEditor)
	{
		return false;
	}

	static auto CVarEditorViewportOverrideGameScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.OverridePIEScreenPercentage"));
	if (!CVarEditorViewportOverrideGameScreenPercentage)
	{
		return false;
	}

	return CVarEditorViewportOverrideGameScreenPercentage->GetInt() != 0;
}
#endif

void FStaticResolutionFractionHeuristic::FUserSettings::PullRunTimeRenderingSettings()
{
	Mode = EScreenPercentageMode(FMath::Clamp(CVarScreenPercentageMode.GetValueOnGameThread(), 0, 2));
	GlobalResolutionFraction = FLegacyScreenPercentageDriver::GetCVarResolutionFraction();
	MinRenderingResolution = CVarScreenPercentageMinResolution.GetValueOnGameThread();
	MaxRenderingResolution = CVarScreenPercentageMaxResolution.GetValueOnGameThread();
	AutoPixelCountMultiplier = CVarAutoPixelCountMultiplier.GetValueOnGameThread();
}

void FStaticResolutionFractionHeuristic::FUserSettings::PullEditorRenderingSettings(bool bIsRealTime, bool bIsPathTraced)
#if WITH_EDITOR
{
	static auto CVarEditorViewportDefaultScreenPercentageRealTimeMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.RealTime"));
	static auto CVarEditorViewportDefaultScreenPercentagePathTracerMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.PathTracer"));
	static auto CVarEditorViewportDefaultScreenPercentageMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentageMode.NonRealTime"));
	static auto CVarEditorViewportDefaultScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.ScreenPercentage"));
	static auto CVarEditorViewportDefaultMinRenderingResolution = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.MinRenderingResolution"));
	static auto CVarEditorViewportDefaultMaxRenderingResolution = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Editor.Viewport.MaxRenderingResolution"));

	if (bIsPathTraced)
	{
		Mode = EScreenPercentageMode(FMath::Clamp(CVarEditorViewportDefaultScreenPercentagePathTracerMode->GetInt(), 0, 2));
	}
	else if (bIsRealTime)
	{
		Mode = EScreenPercentageMode(FMath::Clamp(CVarEditorViewportDefaultScreenPercentageRealTimeMode->GetInt(), 0, 2));
	}
	else
	{
		Mode = EScreenPercentageMode(FMath::Clamp(CVarEditorViewportDefaultScreenPercentageMode->GetInt(), 0, 2));
	}
	GlobalResolutionFraction = GetResolutionFraction(CVarEditorViewportDefaultScreenPercentage->GetInt());
	MinRenderingResolution = CVarEditorViewportDefaultMinRenderingResolution->GetInt();
	MaxRenderingResolution = CVarEditorViewportDefaultMaxRenderingResolution->GetInt();
}
#else
{
	unimplemented();
}
#endif

void FStaticResolutionFractionHeuristic::PullViewFamilyRenderingSettings(const FSceneViewFamily& ViewFamily)
{
	check(ViewFamily.Views.Num() > 0);

	SecondaryViewFraction = ViewFamily.SecondaryViewFraction;

	TotalDisplayedPixelCount = 0;
	for (const FSceneView* View : ViewFamily.Views)
	{
		// Number of pixel drawn in ViewFamily.RenderTarget
		int32 DisplayPixelCount = View->CameraConstrainedViewRect.Area();
		
		TotalDisplayedPixelCount += DisplayPixelCount;
	}
}

float FStaticResolutionFractionHeuristic::ResolveResolutionFraction() const
{
	// Compute number of pixel being rendered.
	float LocalTotalDisplayedPixelCount = FMath::Max(TotalDisplayedPixelCount, 1) * SecondaryViewFraction * SecondaryViewFraction;

	float GlobalResolutionFraction = 1.0f;

	const EScreenPercentageMode EffectiveScreenPercentageMode = Settings.bAllowDisplayBasedScreenPercentageMode ? Settings.Mode : EScreenPercentageMode::Manual;

	if (EffectiveScreenPercentageMode == EScreenPercentageMode::BasedOnDisplayResolution)
	{
		static bool bInitPixelCount = false;
		static float AutoMinDisplayResolution;
		static float AutoMinRenderingResolution;
		static float AutoMidDisplayResolution;
		static float AutoMidRenderingResolution;
		static float AutoMaxDisplayResolution;
		static float AutoMaxRenderingResolution;

		if (!bInitPixelCount)
		{
			bool bSuccess = true;
			bSuccess = bSuccess && GConfig->GetFloat(TEXT("Rendering.AutoScreenPercentage"), TEXT("MinDisplayResolution"), AutoMinDisplayResolution, GEngineIni);
			bSuccess = bSuccess && GConfig->GetFloat(TEXT("Rendering.AutoScreenPercentage"), TEXT("MidDisplayResolution"), AutoMidDisplayResolution, GEngineIni);
			bSuccess = bSuccess && GConfig->GetFloat(TEXT("Rendering.AutoScreenPercentage"), TEXT("MaxDisplayResolution"), AutoMaxDisplayResolution, GEngineIni);
			bSuccess = bSuccess && GConfig->GetFloat(TEXT("Rendering.AutoScreenPercentage"), TEXT("MinRenderingResolution"), AutoMinRenderingResolution, GEngineIni);
			bSuccess = bSuccess && GConfig->GetFloat(TEXT("Rendering.AutoScreenPercentage"), TEXT("MidRenderingResolution"), AutoMidRenderingResolution, GEngineIni);
			bSuccess = bSuccess && GConfig->GetFloat(TEXT("Rendering.AutoScreenPercentage"), TEXT("MaxRenderingResolution"), AutoMaxRenderingResolution, GEngineIni);
			if (!bSuccess)
			{
				UE_LOG(LogEngine, Fatal, TEXT("Failed to load Rendering.AutoScreenPercentage ini section."));
			}
			bInitPixelCount = true;
		}

		float LerpedRenderingPixelCount = 0.0f;
		if (LocalTotalDisplayedPixelCount < GetRenderingPixelCount(AutoMinDisplayResolution))
		{
			LerpedRenderingPixelCount = LocalTotalDisplayedPixelCount * GetRenderingPixelCount(AutoMinRenderingResolution) / GetRenderingPixelCount(AutoMinDisplayResolution);
		}
		else if (LocalTotalDisplayedPixelCount > GetRenderingPixelCount(AutoMaxDisplayResolution))
		{
			LerpedRenderingPixelCount = LocalTotalDisplayedPixelCount * GetRenderingPixelCount(AutoMaxRenderingResolution) / GetRenderingPixelCount(AutoMaxDisplayResolution);
		}
		else
		{
			float MinDisplayPixelCount   = GetRenderingPixelCount(AutoMinDisplayResolution);
			float MaxDisplayPixelCount   = GetRenderingPixelCount(AutoMidDisplayResolution);
			float MinRenderingPixelCount = GetRenderingPixelCount(AutoMinRenderingResolution);
			float MaxRenderingPixelCount = GetRenderingPixelCount(AutoMidRenderingResolution);
			if (LocalTotalDisplayedPixelCount > MaxDisplayPixelCount)
			{
				MinDisplayPixelCount   = GetRenderingPixelCount(AutoMidDisplayResolution);
				MaxDisplayPixelCount   = GetRenderingPixelCount(AutoMaxDisplayResolution);
				MinRenderingPixelCount = GetRenderingPixelCount(AutoMidRenderingResolution);
				MaxRenderingPixelCount = GetRenderingPixelCount(AutoMaxRenderingResolution);
			}

			float ReslutionFractionLerp = FMath::Clamp((LocalTotalDisplayedPixelCount - MinDisplayPixelCount) / (MaxDisplayPixelCount - MinDisplayPixelCount), 0.0f, 1.0f);
			LerpedRenderingPixelCount = FMath::Lerp(MinRenderingPixelCount, MaxRenderingPixelCount, ReslutionFractionLerp);
		}

		GlobalResolutionFraction = FMath::Sqrt(Settings.AutoPixelCountMultiplier * LerpedRenderingPixelCount / LocalTotalDisplayedPixelCount);
	}
	else if (EffectiveScreenPercentageMode == EScreenPercentageMode::BasedOnDPIScale)
	{
		GlobalResolutionFraction = 1.0f / DPIScale;
	}
	else
	{
		ensure(EffectiveScreenPercentageMode == EScreenPercentageMode::Manual);
		GlobalResolutionFraction = Settings.GlobalResolutionFraction;
	}

	// Max the rendering resolution to average target resolution used on platform that have dynamic resolution.
	const float MaxRenderingPixelCount = GetRenderingPixelCount(Settings.MaxRenderingResolution);
	if (MaxRenderingPixelCount > 0.0f)
	{
		// Compute max resolution fraction such that the total number of pixel doesn't go over the CVarScreenPercentageMaxResolution.
		float MaxGlobalResolutionFraction = FMath::Sqrt(float(MaxRenderingPixelCount) / float(LocalTotalDisplayedPixelCount));

		GlobalResolutionFraction = FMath::Min(GlobalResolutionFraction, MaxGlobalResolutionFraction);
	}

	// Min the rendering resolution to avoid any upscaling at very low resolutions.
	const float MinRenderingPixelCount = GetRenderingPixelCount(Settings.MinRenderingResolution);
	if (MinRenderingPixelCount > 0.0f)
	{
		float MinGlobalResolutionFraction = FMath::Min(FMath::Sqrt(float(MinRenderingPixelCount) / float(LocalTotalDisplayedPixelCount)), 1.0f);

		GlobalResolutionFraction = FMath::Max(GlobalResolutionFraction, MinGlobalResolutionFraction);
	}

	return GlobalResolutionFraction;
}
