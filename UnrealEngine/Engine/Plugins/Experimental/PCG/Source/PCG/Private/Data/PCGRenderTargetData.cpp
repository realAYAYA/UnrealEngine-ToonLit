// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGRenderTargetData.h"
#include "Kismet/KismetRenderingLibrary.h"

void UPCGRenderTargetData::Initialize(UTextureRenderTarget2D* InRenderTarget, const FTransform& InTransform)
{
	RenderTarget = InRenderTarget;
	Transform = InTransform;

	ColorData.Reset();

	if (RenderTarget)
	{
		if (FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGRenderTargetData::Initialize::ReadData);
			ETextureRenderTargetFormat Format = RenderTarget->RenderTargetFormat;
			if ((Format == RTF_RGBA16f) || (Format == RTF_RGBA32f) || (Format == RTF_RGBA8) || (Format == RTF_R8) || (Format == RTF_RGB10A2))
			{
				const FIntRect Rect = FIntRect(0, 0, RenderTarget->SizeX, RenderTarget->SizeY);
				const FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
				RTResource->ReadLinearColorPixels(ColorData, ReadPixelFlags, Rect);
			}
		}

		Width = RenderTarget->SizeX;
		Height = RenderTarget->SizeY;
	}
	else
	{
		Width = 0;
		Height = 0;
	}

	Bounds = FBox(EForceInit::ForceInit);
	Bounds += FVector(-1.0f, -1.0f, 0.0f);
	Bounds += FVector(1.0f, 1.0f, 0.0f);
	Bounds = Bounds.TransformBy(Transform);
}