// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"

#include "PCGVolumeData.generated.h"

class AVolume;
struct FBodyInstance;

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGVolumeData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()

public:
	~UPCGVolumeData();
	void Initialize(AVolume* InVolume);
	void Initialize(const FBox& InBounds);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Volume; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	// ~Begin UPGCSpatialData interface
	virtual int GetDimension() const override { return 3; }
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	// TODO what should this do - closest point on volume?
	//virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	// ~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	// ~End UPCGSpatialDataWithPointCache interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data")
	FVector VoxelSize = FVector(100.0, 100.0, 100.0);

protected:
	void CopyBaseVolumeData(UPCGVolumeData* NewVolumeData) const;
	void ReleaseInternalBodyInstance();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SourceData)
	TWeakObjectPtr<AVolume> Volume = nullptr;

	UPROPERTY()
	FBox Bounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	FBox StrictBounds = FBox(EForceInit::ForceInit);

	// Internal body instance to perform queries faster, used in static cases only
	FBodyInstance* VolumeBodyInstance = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
