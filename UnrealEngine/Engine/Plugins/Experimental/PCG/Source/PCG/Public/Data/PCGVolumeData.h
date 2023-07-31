// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSpatialData.h"

#include "PCGVolumeData.generated.h"

class AVolume;

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGVolumeData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()

public:
	void Initialize(AVolume* InVolume, AActor* InTargetActor = nullptr);
	void Initialize(const FBox& InBounds, AActor* InTargetActor);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Volume | Super::GetDataType(); }
	// ~End UPCGData interface

	// ~Begin UPGCSpatialData interface
	virtual int GetDimension() const override { return 3; }
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	// ~End UPGCSpatialData interface

	// ~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	// ~End UPCGSpatialDataWithPointCache interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data")
	FVector VoxelSize = FVector(100.0, 100.0, 100.0);

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SourceData)
	TObjectPtr<AVolume> Volume = nullptr;

	UPROPERTY()
	FBox Bounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	FBox StrictBounds = FBox(EForceInit::ForceInit);
};
