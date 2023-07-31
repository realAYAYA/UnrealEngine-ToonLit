// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSpatialData.h"

#include "PCGUnionData.generated.h"

UENUM()
enum class EPCGUnionType : uint8
{
	LeftToRightPriority,
	RightToLeftPriority,
	KeepAll,
};

UENUM()
enum class EPCGUnionDensityFunction : uint8
{
	Maximum,
	ClampedAddition,
	Binary
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGUnionData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void Initialize(const UPCGSpatialData* InA, const UPCGSpatialData* InB);

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void AddData(const UPCGSpatialData* InData);

	void SetType(EPCGUnionType InUnionType) { UnionType = InUnionType; }
	void SetDensityFunction(EPCGUnionDensityFunction InDensityFunction) { DensityFunction = InDensityFunction; }

	//~Begin UPCGSpatialData interface
	virtual int GetDimension() const override;
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool HasNonTrivialTransform() const override;
	//~End UPCGSpatialData interface

	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface

protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	TArray<TObjectPtr<const UPCGSpatialData>> Data;

	UPROPERTY()
	TObjectPtr<const UPCGSpatialData> FirstNonTrivialTransformData = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGUnionType UnionType = EPCGUnionType::LeftToRightPriority;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGUnionDensityFunction DensityFunction = EPCGUnionDensityFunction::Maximum;

	UPROPERTY()
	FBox CachedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	FBox CachedStrictBounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	int CachedDimension = 0;

private:
	void CreateSequentialPointData(FPCGContext* Context, UPCGPointData* PointData, bool bLeftToRight) const;
};
