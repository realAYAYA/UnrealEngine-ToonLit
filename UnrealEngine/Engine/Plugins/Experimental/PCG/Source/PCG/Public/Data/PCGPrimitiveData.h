// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"

#include "PCGPrimitiveData.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPrimitiveData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()

public:
	void Initialize(UPrimitiveComponent* InPrim);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Primitive; }
	// ~End UPCGData interface

	// ~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 3; }
	virtual FBox GetBounds() const override { return CachedBounds; }
	virtual FBox GetStrictBounds() const override { return CachedStrictBounds; }
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	// TODO needs an implementation to support projection
	//virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	// ~Begin UPCGSpatialDataWithPointCache implementation
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	// ~End UPCGSpatialDataWithPointCache implementation

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data")
	FVector VoxelSize = FVector(100.0, 100.0, 100.0);

protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Data")
	TWeakObjectPtr<UPrimitiveComponent> Primitive = nullptr;

	UPROPERTY()
	FBox CachedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	FBox CachedStrictBounds = FBox(EForceInit::ForceInit);
};