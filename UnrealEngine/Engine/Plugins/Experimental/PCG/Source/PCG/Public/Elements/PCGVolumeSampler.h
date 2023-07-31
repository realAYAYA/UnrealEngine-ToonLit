// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"

#include "PCGVolumeSampler.generated.h"

class UPCGPointData;
class UPCGSpatialData;

namespace PCGVolumeSampler
{
	struct FVolumeSamplerSettings
	{
		FVector VoxelSize;
	};

	UPCGPointData* SampleVolume(FPCGContext* Context, const UPCGSpatialData* SpatialData, const FVolumeSamplerSettings& SamplerSettings);
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGVolumeSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data")
	FVector VoxelSize = FVector(100.0, 100.0, 100.0);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("VolumeSamplerNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	
protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface
};

class FPCGVolumeSamplerElement : public FSimplePCGElement
{
protected:
	bool ExecuteInternal(FPCGContext* Context) const override;
};