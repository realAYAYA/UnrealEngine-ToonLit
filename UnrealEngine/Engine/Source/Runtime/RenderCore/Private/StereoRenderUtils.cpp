// Copyright Epic Games, Inc. All Rights Reserved.

#include "StereoRenderUtils.h"
#include "Misc/App.h"
#include "ShaderPlatformCachedIniValue.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHI.h"
#include "RenderUtils.h"

// enable this to printf-debug stereo rendering aspects on a device
#define UE_DEBUG_STEREO_ASPECTS			(!UE_BUILD_SHIPPING && !UE_BUILD_TEST && !UE_EDITOR)

#if UE_DEBUG_STEREO_ASPECTS
	#define UE_DEBUG_SSA_LOG_INIT(Platform) \
	bool bSSALogEnable = false; \
	{ \
		static EShaderPlatform LastLogged[2] = { SP_NumPlatforms, SP_NumPlatforms };	\
		if (UNLIKELY(LastLogged[0] != Platform && LastLogged[1] != Platform)) \
		{ \
			LastLogged[1] = LastLogged[0]; \
			LastLogged[0] = Platform; \
			bSSALogEnable = FApp::CanEverRender(); \
		} \
	}

	#define UE_DEBUG_SSA_LOG(Verbosity, Format, ...) \
	{ \
		if (UNLIKELY(bSSALogEnable)) \
		{ \
			UE_LOG(LogInit, Verbosity, TEXT("FStereoShaderAspects: %s"), *FString::Printf(Format, ##__VA_ARGS__)); \
		} \
	}
#else
	#define UE_DEBUG_SSA_LOG_INIT(Platform)
	#define UE_DEBUG_SSA_LOG(Verbosity, Format, ...)
#endif

#define UE_DEBUG_SSA_LOG_BOOL(Boolean) \
{ \
	UE_DEBUG_SSA_LOG(Log, TEXT(#Boolean) TEXT(" = %d"), Boolean); \
}

namespace UE::StereoRenderUtils
{
	
RENDERCORE_API FStereoShaderAspects::FStereoShaderAspects(EShaderPlatform Platform) :
	bInstancedStereoEnabled(false)
	, bMobileMultiViewEnabled(false)
	, bInstancedMultiViewportEnabled(false)
	, bInstancedStereoNative(false)
	, bMobileMultiViewNative(false)
	, bMobileMultiViewFallback(false)
{
	check(Platform < EShaderPlatform::SP_NumPlatforms);
	UE_DEBUG_SSA_LOG_INIT(Platform);
	
	// Would be nice to use URendererSettings, but not accessible in RenderCore
	static FShaderPlatformCachedIniValue<bool> CVarInstancedStereo(TEXT("vr.InstancedStereo"));
	static FShaderPlatformCachedIniValue<bool> CVarMobileMultiView(TEXT("vr.MobileMultiView"));
	
	const bool bInstancedStereo = CVarInstancedStereo.Get(Platform);

	const bool bMobilePlatform = IsMobilePlatform(Platform);
	const bool bMobilePostprocessing = IsMobileHDR();
	const bool bMobileMultiView = CVarMobileMultiView.Get(Platform);
	// If we're in a non-rendering run (cooker, DDC commandlet, anything with -nullrhi), don't check GRHI* setting, as it reflects runtime RHI capabilities.
	const bool bMultiViewportCapable = (GRHISupportsArrayIndexFromAnyShader || !FApp::CanEverRender()) && RHISupportsMultiViewport(Platform);

	bInstancedStereoNative = !bMobilePlatform && bInstancedStereo && RHISupportsInstancedStereo(Platform);

	UE_DEBUG_SSA_LOG(Log, TEXT("--- StereoAspects begin ---"));
	UE_DEBUG_SSA_LOG(Log, TEXT("Platform=%s (%d)"), *LexToString(Platform), static_cast<int32>(Platform));
	UE_DEBUG_SSA_LOG_BOOL(bInstancedStereo);
	UE_DEBUG_SSA_LOG_BOOL(bMobilePlatform);
	UE_DEBUG_SSA_LOG_BOOL(bMobilePostprocessing);
	UE_DEBUG_SSA_LOG_BOOL(bMobileMultiView);
	UE_DEBUG_SSA_LOG_BOOL(bMultiViewportCapable);
	UE_DEBUG_SSA_LOG_BOOL(bInstancedStereoNative);
	UE_DEBUG_SSA_LOG(Log, TEXT("---"));

	const bool bMobileMultiViewCoreSupport = bMobilePlatform && bMobileMultiView; 
	if (bMobileMultiViewCoreSupport)
	{
		UE_DEBUG_SSA_LOG(Log, TEXT("RHISupportsMobileMultiView(%s) = %d."), *LexToString(Platform), RHISupportsMobileMultiView(Platform));
		UE_DEBUG_SSA_LOG(Log, TEXT("RHISupportsInstancedStereo(%s) = %d."), *LexToString(Platform), RHISupportsInstancedStereo(Platform));

		if (RHISupportsMobileMultiView(Platform))
		{
			bMobileMultiViewNative = true;
		}
		else if (RHISupportsInstancedStereo(Platform))
		{
			// ISR in mobile shaders is achieved via layered RTs and eye-dependent layer index, so unlike on desktop, it does not depend on multi-viewport
			// If we're in a non-rendering run (cooker, DDC commandlet, anything with -nullrhi), don't check GRHI* setting, as it reflects runtime RHI capabilities.
			const bool bVertexShaderLayerCapable = (GRHISupportsArrayIndexFromAnyShader || !FApp::CanEverRender()) && RHISupportsVertexShaderLayer(Platform);
			UE_DEBUG_SSA_LOG_BOOL(bVertexShaderLayerCapable);

			if (bVertexShaderLayerCapable)
			{
				bMobileMultiViewFallback = true;
			}
		}
	}

	UE_DEBUG_SSA_LOG_BOOL(bMobileMultiViewCoreSupport);
	UE_DEBUG_SSA_LOG_BOOL(bMobileMultiViewNative);
	UE_DEBUG_SSA_LOG_BOOL(bMobileMultiViewFallback);
	UE_DEBUG_SSA_LOG(Log, TEXT("---"));

	// "instanced stereo" is confusingly used to refer to two two modes:
	// 1) regular aka "native" ISR, where the views are selected via SV_ViewportArrayIndex - uses non-mobile shaders
	// 2) "mobile multiview fallback" ISR, which writes to a texture layer via SV_RenderTargetArrayIndex - uses mobile shaders
	// IsInstancedStereoEnabled() will be true in both cases

	bInstancedMultiViewportEnabled = bInstancedStereoNative && bMultiViewportCapable;
	// Since instanced stereo now relies on multi-viewport capability, it cannot be separately enabled from it.
	bInstancedStereoEnabled = bInstancedStereoNative || bMobileMultiViewFallback;
	bMobileMultiViewEnabled = bMobileMultiViewNative || bMobileMultiViewFallback;

	UE_DEBUG_SSA_LOG_BOOL(bInstancedMultiViewportEnabled);
	UE_DEBUG_SSA_LOG_BOOL(bInstancedStereoEnabled);
	UE_DEBUG_SSA_LOG_BOOL(bMobileMultiViewEnabled);
	UE_DEBUG_SSA_LOG(Log, TEXT("--- StereoAspects end ---"));

	// check the following invariants
	checkf(!bMobileMultiViewNative || !bInstancedStereoEnabled, TEXT("When a platform supports MMV natively, ISR should not be enabled."));
	checkf(!(bMobileMultiViewEnabled && bInstancedStereoEnabled) || bMobileMultiViewFallback, TEXT("If a platform uses MMV fallback, both MMV and ISR should be enabled."));
	checkf(!bInstancedStereoEnabled || (bInstancedMultiViewportEnabled || bMobileMultiViewFallback), TEXT("If ISR is enabled, we need either multi-viewport (since we no longer support clip-distance method) or MMV fallback (which uses vertex layer)."));
}
	

RENDERCORE_API void LogISRInit(const UE::StereoRenderUtils::FStereoShaderAspects& Aspects)
{
	UE_LOG(LogInit, Log, TEXT("XR: Instanced Stereo Rendering is %s"), (Aspects.IsInstancedStereoEnabled() ? TEXT("Enabled") : TEXT("Disabled")));
	UE_LOG(LogInit, Log, TEXT("XR: MultiViewport is %s"), (Aspects.IsInstancedMultiViewportEnabled() ? TEXT("Enabled") : TEXT("Disabled")));
	UE_LOG(LogInit, Log, TEXT("XR: Mobile Multiview is %s"), (Aspects.IsMobileMultiViewEnabled() ? TEXT("Enabled") : TEXT("Disabled")));
}

RENDERCORE_API void VerifyISRConfig(const UE::StereoRenderUtils::FStereoShaderAspects& Aspects, EShaderPlatform ShaderPlatform)
{
	// If instanced stereo is enabled, we should also have either multiviewport enabled, or mmv fallback enabled.
	// Otherwise, exit gracefully with a message box
	if (Aspects.IsInstancedStereoEnabled() && !Aspects.IsInstancedMultiViewportEnabled() && !Aspects.IsMobileMultiViewEnabled())
	{
		UE_LOG(LogInit, Log, TEXT("ShaderPlatform=%d RHISupportsInstancedStereo()=%d GRHISupportsArrayIndexFromAnyShader=%d"),
			ShaderPlatform, RHISupportsMultiViewport(ShaderPlatform), GRHISupportsArrayIndexFromAnyShader);

		// MessageBoxExt may not yet handle unattended runs itself (see UE-165694), so special-case here to avoid getting stuck in a commandlet with rendering enabled
		const FText MessageText = NSLOCTEXT("InstancedStereo", "UnableToUseInstancedStereoRenderingText", "Cannot render an Instanced Stereo-enabled project due to a missing functionality on the system. Please check log files for more info.");
		if (!FApp::IsUnattended())
		{
			FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *MessageText.ToString(),
				*NSLOCTEXT("InstancedStereo", "UnableToUseInstancedStereoRendering", "Unable to use Instanced Stereo Rendering.").ToString());
		}
		else
		{
			UE_LOG(LogInit, Error, TEXT("%s"), *MessageText.ToString());
		}
		FPlatformMisc::RequestExitWithStatus(true, 1);
		// unreachable
	}
}

} // namespace UE::StereoRenderUtils
