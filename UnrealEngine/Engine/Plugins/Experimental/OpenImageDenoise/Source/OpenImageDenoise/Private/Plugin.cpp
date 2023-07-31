// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PathTracingDenoiser.h"
#include "RHI.h"
#include "RHIResources.h"

#include "OpenImageDenoise/oidn.hpp"

class FOpenImageDenoiseModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#if WITH_EDITOR
DECLARE_LOG_CATEGORY_EXTERN(LogOpenImageDenoise, Log, All);
DEFINE_LOG_CATEGORY(LogOpenImageDenoise);
#endif

IMPLEMENT_MODULE(FOpenImageDenoiseModule, OpenImageDenoise)

enum class EDenoiseMode {
	OFF = 0,
	DEFAULT = 1,   // denoise using albedo/normal as provided
	CLEAN_AUX = 2, // denoise albedo/normal first and pass cleanAux (haven't found a scene where this is clearly better, and it runs much slower)
	NO_AOVS = 3,   // denoise beauty only
};

static void Denoise(FRHICommandListImmediate& RHICmdList, FRHITexture* ColorTex, FRHITexture* AlbedoTex, FRHITexture* NormalTex, FRHITexture* OutputTex, FRHIGPUMask GPUMask)
{
	static IConsoleVariable* DenoiseModeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.Denoiser"));

	const int DenoiseModeCVarValue = DenoiseModeCVar ? DenoiseModeCVar->GetInt() : -1;
	const EDenoiseMode DenoiseMode = DenoiseModeCVarValue >= 0 ? EDenoiseMode(DenoiseModeCVarValue) : EDenoiseMode::DEFAULT;

#if WITH_EDITOR
	uint64 FilterExecuteTime = 0;
	FilterExecuteTime -= FPlatformTime::Cycles64();
#endif

	FIntPoint Size = ColorTex->GetSizeXY();
	FIntRect Rect = FIntRect(0, 0, Size.X, Size.Y);
	TArray<FLinearColor> RawPixels;
	TArray<FLinearColor> RawAlbedo;
	TArray<FLinearColor> RawNormal;
	FReadSurfaceDataFlags ReadDataFlags(ERangeCompressionMode::RCM_MinMax);
	ReadDataFlags.SetLinearToGamma(false);
	ReadDataFlags.SetGPUIndex(GPUMask.GetFirstIndex());
	RHICmdList.ReadSurfaceData(ColorTex, Rect, RawPixels, ReadDataFlags);
	if (DenoiseMode == EDenoiseMode::DEFAULT || DenoiseMode == EDenoiseMode::CLEAN_AUX)
	{
		RHICmdList.ReadSurfaceData(AlbedoTex, Rect, RawAlbedo, ReadDataFlags);
		RHICmdList.ReadSurfaceData(NormalTex, Rect, RawNormal, ReadDataFlags);
	}

	check(RawPixels.Num() == Size.X * Size.Y);

	// create device only once?
	oidn::DeviceRef OIDNDevice = oidn::newDevice();
	OIDNDevice.commit();

	if (DenoiseMode == EDenoiseMode::CLEAN_AUX)
	{
		oidn::FilterRef OIDNFilter = OIDNDevice.newFilter("RT");
		OIDNFilter.setImage("albedo", RawAlbedo.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
		OIDNFilter.setImage("output", RawAlbedo.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
		OIDNFilter.commit();
		OIDNFilter.execute();
	}

	if (DenoiseMode == EDenoiseMode::CLEAN_AUX)
	{
		oidn::FilterRef OIDNFilter = OIDNDevice.newFilter("RT");
		OIDNFilter.setImage("normal", RawNormal.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
		OIDNFilter.setImage("output", RawNormal.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
		OIDNFilter.commit();
		OIDNFilter.execute();
	}

	oidn::FilterRef OIDNFilter = OIDNDevice.newFilter("RT");
	OIDNFilter.setImage("color", RawPixels.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
	if (DenoiseMode == EDenoiseMode::DEFAULT || DenoiseMode == EDenoiseMode::CLEAN_AUX)
	{
		// default behavior
		OIDNFilter.setImage("albedo", RawAlbedo.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
		OIDNFilter.setImage("normal", RawNormal.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
	}
	if (DenoiseMode == EDenoiseMode::CLEAN_AUX)
	{
		// +cleanAux
		OIDNFilter.set("cleanAux", true);
	}

	uint32_t DestStride;
	FLinearColor* DestBuffer = (FLinearColor*)RHICmdList.LockTexture2D(OutputTex, 0, RLM_WriteOnly, DestStride, false);

	OIDNFilter.setImage("output", DestBuffer, oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), DestStride);
	OIDNFilter.set("hdr", true);
	OIDNFilter.commit();

	OIDNFilter.execute();

	// copy alpha channel (TODO: find a way to denoise it as well?)
	for (int Y = 0, OutIndex = 0, Index = 0; Y < Size.Y; Y++)
	{
		for (int X = 0; X < Size.X; X++, OutIndex++, Index++)
		{
			DestBuffer[OutIndex].A = RawPixels[Index].A;
		}
		OutIndex += DestStride / sizeof(FLinearColor) - Size.X;
	}

	RHICmdList.UnlockTexture2D(OutputTex, 0, false);

#if WITH_EDITOR
	const char* errorMessage;
	if (OIDNDevice.getError(errorMessage) != oidn::Error::None)
	{
		UE_LOG(LogOpenImageDenoise, Warning, TEXT("Denoiser failed: %s"), *FString(errorMessage));
		return;
	}

	FilterExecuteTime += FPlatformTime::Cycles64();
	const double FilterExecuteTimeMS = 1000.0 * FPlatformTime::ToSeconds64(FilterExecuteTime);
	UE_LOG(LogOpenImageDenoise, Log, TEXT("Denoised %d x %d pixels in %.2f ms"), Size.X, Size.Y, FilterExecuteTimeMS);
#endif
}


void FOpenImageDenoiseModule::StartupModule()
{
#if WITH_EDITOR
	UE_LOG(LogOpenImageDenoise, Log, TEXT("OIDN starting up"));
#endif
	GPathTracingDenoiserFunc = &Denoise;
}

void FOpenImageDenoiseModule::ShutdownModule()
{
#if WITH_EDITOR
	UE_LOG(LogOpenImageDenoise, Log, TEXT("OIDN shutting down"));
#endif
	GPathTracingDenoiserFunc = nullptr;
}
