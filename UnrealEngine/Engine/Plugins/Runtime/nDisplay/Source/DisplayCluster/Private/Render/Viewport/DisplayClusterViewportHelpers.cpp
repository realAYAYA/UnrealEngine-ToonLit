// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportHelpers.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Engine/TextureRenderTarget2D.h"

////////////////////////////////////////////////////////////////////////////////
int32 GDisplayClusterOverrideMaxTextureDimension = 8192;
static FAutoConsoleVariableRef CVarDisplayClusterOverrideMaxTextureDimension(
	TEXT("DC.OverrideMaxTextureDimension"),
	GDisplayClusterOverrideMaxTextureDimension,
	TEXT("Override max texture dimension for nDisplay rendering (-1 == disabled, positive values == override)"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterOverrideMinTextureDimension = -1;
static FAutoConsoleVariableRef CVarDisplayClusterOverrideMinTextureDimension(
	TEXT("DC.OverrideMinTextureDimension"),
	GDisplayClusterOverrideMinTextureDimension,
	TEXT("Override min texture dimension for nDisplay rendering (-1 == disabled, positive values == override)"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterDefaultPreviewPixelFormat = 1;
static FAutoConsoleVariableRef CVarDisplayClusterDefaultPreviewPixelFormat(
	TEXT("DC.Preview.DefaultPixelFormat"),
	GDisplayClusterDefaultPreviewPixelFormat,
	TEXT("Defines the default preview RTT pixel format.\n")
	TEXT(" 0: 8bit fixed point RGBA\n")
	TEXT(" 1: 16bit Float RGBA\n")
	TEXT(" 2: 10bit fixed point RGB and 2bit Alpha\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterMaxNumMips = -1;
static FAutoConsoleVariableRef CVarDisplayClusterMaxNumMips(
	TEXT("DC.TextureMaxNumMips"),
	GDisplayClusterMaxNumMips,
	TEXT("Maximum number of mips for viewport texture.\n")
	TEXT(" '0' - disable mips generation.\n")
	TEXT("'-1' - disable this limit.\n")
	TEXT("positive value - set the limit.\n"),
	ECVF_RenderThreadSafe
);

namespace PreviewPixelFormat
{
	static ETextureRenderTargetFormat ImplGetRenderTargetFormatFromInt(const int32 InDefaultPreviewPixelFormat)
	{
		const int32 ValidIndex = FMath::Clamp(InDefaultPreviewPixelFormat, 0, 2);
		static const ETextureRenderTargetFormat SPixelFormat[] = { RTF_RGBA8, RTF_RGBA16f, RTF_RGB10A2 };

		return SPixelFormat[ValidIndex];
	}

	static EPixelFormat GetPreviewPixelFormat()
	{
		const ETextureRenderTargetFormat RenderTargetFormat = ImplGetRenderTargetFormatFromInt(GDisplayClusterDefaultPreviewPixelFormat);
		
		return GetPixelFormatFromRenderTargetFormat(RenderTargetFormat);
	}
};

////////////////////////////////////////////////////////////////////////////////
int32 DisplayClusterViewportHelpers::GetMaxTextureNumMips(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const int32 InNumMips)
{
	int32 NumMips = InNumMips;

#if WITH_EDITOR
	switch (InRenderFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		if (GDisplayClusterDefaultPreviewPixelFormat != 0)
		{
			//@todo: now UE support mips generation only for fixed point textures (8bit RGBA)
			// Remove this hack latter
			// Disable preview mips generation in case of unsupported RTT texture format.
			NumMips = 0;
		}
		break;
	default:
		break;
	}
#endif

	if (GDisplayClusterMaxNumMips >= 0)
	{
		return FMath::Min(GDisplayClusterMaxNumMips, NumMips);
	}

	return NumMips;
}

int32 DisplayClusterViewportHelpers::GetMaxTextureDimension()
{
	// The target always needs be within GMaxTextureDimensions, larger dimensions are not supported by the engine
	static const int32 MaxTextureDimension = 1 << (GMaxTextureMipCount - 1);

	// Use overridden value
	if (GDisplayClusterOverrideMaxTextureDimension > 0)
	{
		// Protect vs wrong CVar values
		if (GDisplayClusterOverrideMinTextureDimension < 0 || GDisplayClusterOverrideMaxTextureDimension > GDisplayClusterOverrideMinTextureDimension)
		{
			return GDisplayClusterOverrideMaxTextureDimension;
		}
	}

	return MaxTextureDimension;
}

int32 DisplayClusterViewportHelpers::GetMinTextureDimension()
{
	static const int32 MinTextureDimension = 16;

	// Use overridden value
	if (GDisplayClusterOverrideMinTextureDimension > 0)
	{
		// Protect vs wrong CVar values
		if (GDisplayClusterOverrideMaxTextureDimension < 0 || GDisplayClusterOverrideMaxTextureDimension > GDisplayClusterOverrideMinTextureDimension)
		{
			return GDisplayClusterOverrideMinTextureDimension;
		}
	}

	return MinTextureDimension;
}

void DisplayClusterViewportHelpers::FreezeRenderingOfViewportTextureResources(TArray<FDisplayClusterViewportTextureResource*>& TextureResources)
{
	for (FDisplayClusterViewportTextureResource* TextureResource : TextureResources)
	{
		if (TextureResource != nullptr)
		{
			TextureResource->RaiseViewportResourceState(EDisplayClusterViewportResourceState::DisableReallocate);
		}
	}
}

bool DisplayClusterViewportHelpers::IsValidTextureSize(const FIntPoint& InSize)
{
	return InSize.GetMin() >= GetMinTextureDimension() && InSize.GetMax() <= GetMaxTextureDimension();
}

// Return size less than max texture dimension. Aspect ratio not changed
FIntPoint DisplayClusterViewportHelpers::GetTextureSizeLessThanMax(const FIntPoint& InSize, const int32 InMaxTextureDimension)
{
	if (InSize.GetMax() > InMaxTextureDimension)
	{
		const float DownscaleMult =  double(InMaxTextureDimension) / double(InSize.GetMax());

		return ScaleTextureSize(InSize, DownscaleMult);
	}

	return InSize;
}

FIntPoint DisplayClusterViewportHelpers::ScaleTextureSize(const FIntPoint& InSize, float InMult)
{
	const int32 ScaledX = FMath::CeilToInt(InSize.X * InMult);
	const int32 ScaledY = FMath::CeilToInt(InSize.Y * InMult);

	return FIntPoint(ScaledX, ScaledY);
}

float DisplayClusterViewportHelpers::GetValidSizeMultiplier(const FIntPoint& InSize, const float InSizeMult, const float InBaseSizeMult)
{
	// find best possible size mult in range 1..InSizeMult
	if (InSizeMult > 1.f)
	{
		const FIntPoint ScaledSize = ScaleTextureSize(InSize, FMath::Max(InSizeMult * InBaseSizeMult, 0.f));
		if (DisplayClusterViewportHelpers::IsValidTextureSize(ScaledSize) == false)
		{
			// Try change 'RenderTargetAdaptRatio' to min possible value
			const float BaseMult = FMath::Max(InBaseSizeMult, 0.f);
			const FIntPoint MinScaledSize = ScaleTextureSize(InSize, BaseMult);

			if (DisplayClusterViewportHelpers::IsValidTextureSize(MinScaledSize) == false)
			{
				// BaseSizeMult to big. Disable size mult
				return 1.f;
			}
			else
			{
				const int32 MinDimension = MinScaledSize.GetMax();
				const int32 MaxDimension = GetMaxTextureDimension();

				// Get the maximum allowed multiplier value
				const float OutMult = float(double(MaxDimension) / double(MinDimension));

#if UE_BUILD_DEBUG
				// debug purpose
				const FIntPoint FinalSize = ScaleTextureSize(InSize, FMath::Max(OutMult * InBaseSizeMult, 0.f));
				check(DisplayClusterViewportHelpers::IsValidTextureSize(FinalSize));
#endif

				return OutMult;
			}
		}
	}

	return InSizeMult;
}

#if WITH_EDITOR
void DisplayClusterViewportHelpers::GetPreviewRenderTargetDesc_Editor(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, EPixelFormat& OutPixelFormat, float& OutDisplayGamma, bool& bOutSRGB)
{
	OutPixelFormat = PreviewPixelFormat::GetPreviewPixelFormat();

	// Default gamma value
	OutDisplayGamma = 2.2f;

	//!
	bOutSRGB = true;

	switch (InRenderFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		if (InRenderFrameSettings.bPreviewEnablePostProcess == false)
		{
			// Disable postprocess for preview. Use Gamma 1.f
			OutDisplayGamma = 1.f;
		}
		break;
	default:
		break;
	}
}
#endif

