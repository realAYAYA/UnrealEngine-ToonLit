// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"

#include "PCGProjectionData.generated.h"

/**
* Generic projection class (A projected onto B) that intercepts spatial queries
*/
UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGProjectionData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()
public:
	void Initialize(const UPCGSpatialData* InSource, const UPCGSpatialData* InTarget, const FPCGProjectionParams& InProjectionParams);

	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	const FPCGProjectionParams& GetProjectionParams() const { return ProjectionParams; }

	// ~Begin UPCGData interface
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual int GetDimension() const override;
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual FVector GetNormal() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool HasNonTrivialTransform() const override;
	virtual bool RequiresCollapseToSample() const override;
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface

protected:
	void CopyBaseProjectionClass(UPCGProjectionData* NewProjectionData) const;

	FBox ProjectBounds(const FBox& InBounds) const;

	/** Applies data from target point to projected point, conditionally according to the projection params. */
	void ApplyProjectionResult(const FPCGPoint& InTargetPoint, FPCGPoint& InOutProjected) const;

	void GetIncludeExcludeAttributeNames(TSet<FName>& OutAttributeNames) const;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	TObjectPtr<const UPCGSpatialData> Source = nullptr;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = SpatialData)
	TObjectPtr<const UPCGSpatialData> Target = nullptr;

	UPROPERTY()
	FBox CachedBounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	FBox CachedStrictBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadwrite, VisibleAnywhere, Category = SpatialData)
	FPCGProjectionParams ProjectionParams;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "PCGPointData.h"
#endif
