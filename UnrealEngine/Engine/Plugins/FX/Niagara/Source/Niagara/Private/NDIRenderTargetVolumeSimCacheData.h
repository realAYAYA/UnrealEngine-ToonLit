// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Serialization/BulkData.h"
#include "PixelFormat.h"

#include "NDIRenderTargetVolumeSimCacheData.generated.h"

USTRUCT()
struct FNDIRenderTargetVolumeSimCacheFrame
{
	GENERATED_BODY();

	~FNDIRenderTargetVolumeSimCacheFrame()
	{
		ReleasePixelData();
	}

	uint8* GetPixelData()
	{
		if (PixelData == nullptr && UncompressedSize > 0)
		{
			BulkData.GetCopy(reinterpret_cast<void**>(&PixelData), true);
		}
		return PixelData;
	}

	void ReleasePixelData()
	{
		if (PixelData != nullptr)
		{
			FMemory::Free(PixelData);
			PixelData = nullptr;
		}
	}

	UPROPERTY()
	FIntVector Size = FIntVector(EForceInit::ForceInitToZero);

	UPROPERTY()
	TEnumAsByte<EPixelFormat> Format = EPixelFormat::PF_A16B16G16R16;

	UPROPERTY()
	int32 UncompressedSize = 0;

	UPROPERTY()
	int32 CompressedSize = 0;

	FByteBulkData BulkData;
	uint8* PixelData = nullptr;
};

UCLASS()
class UNDIRenderTargetVolumeSimCacheData : public UObject
{
	GENERATED_BODY()

public:
	// Begin UObject Interface
	virtual void BeginDestroy() override
	{
		Super::BeginDestroy();

		for (FNDIRenderTargetVolumeSimCacheFrame& Frame : Frames)
		{
			Frame.ReleasePixelData();
		}
		Frames.Empty();
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		for (FNDIRenderTargetVolumeSimCacheFrame& Frame : Frames)
		{
			if (Frame.UncompressedSize == 0)
			{
				continue;
			}

			Frame.BulkData.Serialize(Ar, this);
		}
	}

	UPROPERTY()
	FName CompressionType;

	UPROPERTY()
	TArray<FNDIRenderTargetVolumeSimCacheFrame> Frames;
};
