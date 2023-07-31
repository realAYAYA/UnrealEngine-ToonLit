// Copyright Epic Games, Inc. All Rights Reserved.

#include "MobileFSRViewExtension.h"
#include "MobileFSRUpscaler.h"
#include "PostProcess/PostProcessUpscale.h"

static TAutoConsoleVariable<int32> CVarEnableFSR(
	TEXT("r.Mobile.FSR.Enabled"),
	1,
	TEXT("Enable Mobile FSR for Primary Upscale"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarMobileFSRCASEnabled(
	TEXT("r.Mobile.FSR.RCAS.Enabled"),
	1,
	TEXT("FidelityFX FSR/RCAS : Robust Contrast Adaptive Sharpening Filter"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32>CVarMobileFSRUpsamplingEnabled(
	TEXT("r.Mobile.FSR.Upsampling.Enabled"),
	1,
	TEXT("FidelityFX FSR/RCAS : Robust Contrast Adaptive Sharpening Filter"),
	ECVF_Default);
  
void FMobileFSRViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (InViewFamily.GetFeatureLevel() == ERHIFeatureLevel::ES3_1 && CVarEnableFSR.GetValueOnAnyThread() > 0)
	{
		if (CVarMobileFSRUpsamplingEnabled.GetValueOnAnyThread())
		{
			InViewFamily.SetPrimarySpatialUpscalerInterface(new FMobileFSRUpscaler(true));
		}
		if (CVarMobileFSRCASEnabled.GetValueOnAnyThread())
		{
			InViewFamily.SetSecondarySpatialUpscalerInterface(new FMobileFSRUpscaler(false));
		}
	}
}