// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGRenderTargetData.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGRenderTargetData)

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

void UPCGRenderTargetData::AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const
{
	Super::AddToCrc(Ar, bFullDataCrc);

	// This data does not have a bespoke CRC implementation so just use a global unique data CRC.
	AddUIDToCrc(Ar);
}

UPCGSpatialData* UPCGRenderTargetData::CopyInternal() const
{
	UPCGRenderTargetData* NewRenderTargetData = NewObject<UPCGRenderTargetData>();

	CopyBaseTextureData(NewRenderTargetData);

	NewRenderTargetData->RenderTarget = RenderTarget;

	return NewRenderTargetData;
}
