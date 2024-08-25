// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PathTracingDenoiser.h"
#include "RenderGraphBuilder.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RHIResources.h"
#include "RHITypes.h"
#include "SceneView.h"

#include "OpenImageDenoise/oidn.hpp"

BEGIN_SHADER_PARAMETER_STRUCT(FDenoiseTextureParameters, )
	RDG_TEXTURE_ACCESS(InputTexture, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(InputAlbedo, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(InputNormal, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(OutputTexture, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

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

enum class EDenoiseMode
{
	OFF = 0,
	DEFAULT = 1,   // denoise using albedo/normal as provided
	CLEAN_AUX = 2, // denoise albedo/normal first and pass cleanAux (haven't found a scene where this is clearly better, and it runs much slower)
	NO_AOVS = 3,   // denoise beauty only
};

struct OIDNState
{
	// scratch CPU memory for running the OIDN filter
	TArray<FLinearColor> RawPixels;
	TArray<FLinearColor> RawAlbedo;
	TArray<FLinearColor> RawNormal;

	// re-useable filters
	oidn::FilterRef AlbedoFilter;
	oidn::FilterRef NormalFilter;
	oidn::FilterRef PixelsFilter;
	oidn::DeviceRef OIDNDevice;

	EDenoiseMode CurrentMode = EDenoiseMode::OFF;
	FIntPoint CurrentSize = FIntPoint(0, 0);

	void UpdateFilter(FIntPoint Size, EDenoiseMode DenoiserMode)
	{
		int NewSize = Size.X * Size.Y;
		if (RawPixels.Num() != NewSize)
		{
			RawPixels.SetNumUninitialized(NewSize);
			RawAlbedo.SetNumUninitialized(NewSize);
			RawNormal.SetNumUninitialized(NewSize);
			// reset filters so we recreate them to point to the new memory allocation
			AlbedoFilter = oidn::FilterRef();
			NormalFilter = oidn::FilterRef();
			PixelsFilter = oidn::FilterRef();
		}
		if (DenoiserMode == EDenoiseMode::OFF)
		{
			OIDNDevice = oidn::DeviceRef();
			return;
		}
		if (!OIDNDevice)
		{
			OIDNDevice = oidn::newDevice();
			OIDNDevice.commit();
		}
		if (!PixelsFilter || CurrentMode != DenoiserMode || CurrentSize != Size)
		{
			CurrentMode = DenoiserMode;
			CurrentSize = Size;
			if (CurrentMode == EDenoiseMode::CLEAN_AUX)
			{
				AlbedoFilter = OIDNDevice.newFilter("RT");
				AlbedoFilter.setImage("albedo", RawAlbedo.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
				AlbedoFilter.setImage("output", RawAlbedo.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
				AlbedoFilter.commit();
				NormalFilter = OIDNDevice.newFilter("RT");
				NormalFilter.setImage("normal", RawNormal.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
				NormalFilter.setImage("output", RawNormal.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
				NormalFilter.commit();
			}
			else
			{
				AlbedoFilter = oidn::FilterRef();
				NormalFilter = oidn::FilterRef();
			}
			PixelsFilter = OIDNDevice.newFilter("RT");
			// TODO: find a way to denoise the alpha channel? OIDN does not support this yet
			PixelsFilter.setImage("color" , RawPixels.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
			PixelsFilter.setImage("output", RawPixels.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
			if (CurrentMode == EDenoiseMode::DEFAULT || CurrentMode == EDenoiseMode::CLEAN_AUX)
			{
				// default behavior, use the albedo/normal buffers to improve quality
				// TODO: switch these buffers to half precision? (requires OIDN 1.4.2+)
				PixelsFilter.setImage("albedo", RawAlbedo.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
				PixelsFilter.setImage("normal", RawNormal.GetData(), oidn::Format::Float3, Size.X, Size.Y, 0, sizeof(FLinearColor), sizeof(FLinearColor) * Size.X);
			}
			if (CurrentMode == EDenoiseMode::CLEAN_AUX)
			{
				// +cleanAux
				PixelsFilter.set("cleanAux", true);
			}
			PixelsFilter.set("hdr", true);
			PixelsFilter.commit();
		}
	}
	
	void Reset()
	{
		UpdateFilter(FIntPoint(0, 0), EDenoiseMode::OFF);
	}
};

static OIDNState DenoiserState;

template <typename PixelType>
static void CopyTextureFromGPUToCPU(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FIntPoint Size, TArray<PixelType>& DstArray)
{
	FRHIGPUTextureReadback Readback(TEXT("DenoiserReadback"));
	Readback.EnqueueCopy(RHICmdList, SrcTexture, FIntVector::ZeroValue, 0, FIntVector(Size.X, Size.Y, 1));
	RHICmdList.BlockUntilGPUIdle();

	int32_t SrcStride = 0;
	const PixelType* SrcBuffer = static_cast<PixelType*>(Readback.Lock(SrcStride, nullptr));

	PixelType* DstBuffer = DstArray.GetData();
	for (int Y = 0; Y < Size.Y; Y++, DstBuffer += Size.X, SrcBuffer += SrcStride)
	{
		FPlatformMemory::Memcpy(DstBuffer, SrcBuffer, Size.X * sizeof(PixelType));

	}
	Readback.Unlock();
}

template <typename PixelType>
static void CopyTextureFromCPUToGPU(FRHICommandListImmediate& RHICmdList, const TArray<PixelType>& SrcArray, FIntPoint Size, FRHITexture* DstTexture)
{
	uint32_t DestStride;
	FLinearColor* DstBuffer = static_cast<PixelType*>(RHICmdList.LockTexture2D(DstTexture, 0, RLM_WriteOnly, DestStride, false));
	DestStride /= sizeof(PixelType);
	const FLinearColor* SrcBuffer = SrcArray.GetData();
	for (int Y = 0; Y < Size.Y; Y++, SrcBuffer += Size.X, DstBuffer += DestStride)
	{
		FPlatformMemory::Memcpy(DstBuffer, SrcBuffer, Size.X * sizeof(PixelType));
	}
	RHICmdList.UnlockTexture2D(DstTexture, 0, false);
}

static void Denoise(FRHICommandListImmediate& RHICmdList, FRHITexture* ColorTex, FRHITexture* AlbedoTex, FRHITexture* NormalTex, FRHITexture* OutputTex, FRHIGPUMask GPUMask)
{
	static IConsoleVariable* DenoiseModeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing.Denoiser"));

	const int DenoiseModeCVarValue = DenoiseModeCVar ? DenoiseModeCVar->GetInt() : -1;
	const EDenoiseMode DenoiseMode = DenoiseModeCVarValue >= 0 ? EDenoiseMode(DenoiseModeCVarValue) : EDenoiseMode::DEFAULT;

#if WITH_EDITOR
	// NOTE: the time will include the transfer from GPU to CPU which will include waiting for the GPU pipeline to complete
	uint64 FilterExecuteTime = 0;
	FilterExecuteTime -= FPlatformTime::Cycles64();
#endif

	FIntPoint Size = ColorTex->GetSizeXY();
	FIntRect Rect = FIntRect(0, 0, Size.X, Size.Y);

	DenoiserState.UpdateFilter(Size, DenoiseMode);
	CopyTextureFromGPUToCPU(RHICmdList, ColorTex, Size, DenoiserState.RawPixels);
	if (DenoiseMode == EDenoiseMode::DEFAULT || DenoiseMode == EDenoiseMode::CLEAN_AUX)
	{
		CopyTextureFromGPUToCPU(RHICmdList, AlbedoTex, Size, DenoiserState.RawAlbedo);
		CopyTextureFromGPUToCPU(RHICmdList, NormalTex, Size, DenoiserState.RawNormal);
	}
	check(DenoiserState.RawPixels.Num() == Size.X * Size.Y);

	if (DenoiseMode == EDenoiseMode::CLEAN_AUX)
	{
		check(DenoiserState.AlbedoFilter);
		check(DenoiserState.NormalFilter);
		DenoiserState.AlbedoFilter.execute();
		DenoiserState.NormalFilter.execute();
	}

	check(DenoiserState.PixelsFilter);
	DenoiserState.PixelsFilter.execute();
	// copy pixels back to GPU (including alpha channel which was hopefully untouched by OIDN)
	CopyTextureFromCPUToGPU(RHICmdList, DenoiserState.RawPixels, Size, OutputTex);

#if WITH_EDITOR
	const char* errorMessage;
	if (DenoiserState.OIDNDevice.getError(errorMessage) != oidn::Error::None)
	{
		UE_LOG(LogOpenImageDenoise, Warning, TEXT("Denoiser failed: %s"), *FString(errorMessage));
		return;
	}

	FilterExecuteTime += FPlatformTime::Cycles64();
	const double FilterExecuteTimeMS = 1000.0 * FPlatformTime::ToSeconds64(FilterExecuteTime);
	UE_LOG(LogOpenImageDenoise, Log, TEXT("Denoised %d x %d pixels in %.2f ms"), Size.X, Size.Y, FilterExecuteTimeMS);
#endif
}

using namespace UE::Renderer::Private;

class FOIDNDenoiser : public IPathTracingDenoiser
{
public:
	~FOIDNDenoiser() {}

	void AddPasses(FRDGBuilder& GraphBuilder, const FSceneView& View, const FInputs& Inputs) const override
	{
		FDenoiseTextureParameters* DenoiseParameters = GraphBuilder.AllocParameters<FDenoiseTextureParameters>();
		DenoiseParameters->InputTexture = Inputs.ColorTex;
		DenoiseParameters->InputAlbedo = Inputs.AlbedoTex;
		DenoiseParameters->InputNormal = Inputs.NormalTex;
		DenoiseParameters->OutputTexture = Inputs.OutputTex;

		// Need to read GPU mask outside Pass function, as the value is not refreshed inside the pass
		GraphBuilder.AddPass(RDG_EVENT_NAME("OIDN Denoiser Plugin"), DenoiseParameters, ERDGPassFlags::Readback,
			[DenoiseParameters, GPUMask = View.GPUMask](FRHICommandListImmediate& RHICmdList)
		{
			Denoise(RHICmdList,
				DenoiseParameters->InputTexture->GetRHI()->GetTexture2D(),
				DenoiseParameters->InputAlbedo->GetRHI()->GetTexture2D(),
				DenoiseParameters->InputNormal->GetRHI()->GetTexture2D(),
				DenoiseParameters->OutputTexture->GetRHI()->GetTexture2D(),
				GPUMask);
		});
	}
};

void FOpenImageDenoiseModule::StartupModule()
{
#if WITH_EDITOR
	UE_LOG(LogOpenImageDenoise, Log, TEXT("OIDN starting up"));
#endif

	GPathTracingDenoiserPlugin = MakeUnique<FOIDNDenoiser>();
}

void FOpenImageDenoiseModule::ShutdownModule()
{
#if WITH_EDITOR
	UE_LOG(LogOpenImageDenoise, Log, TEXT("OIDN shutting down"));
#endif

	// Release scratch memory and destroy the OIDN device and filters
	DenoiserState.Reset();
	GPathTracingDenoiserPlugin.Reset();
}
