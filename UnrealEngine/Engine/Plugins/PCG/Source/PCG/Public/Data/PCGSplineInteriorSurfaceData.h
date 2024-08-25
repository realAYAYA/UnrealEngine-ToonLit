// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Data/PCGSurfaceData.h"
#include "Data/PCGSplineStruct.h"

#include "PCGSplineInteriorSurfaceData.generated.h"

class UPCGSpatialData;
class UPCGSplineData;
struct FPCGProjectionParams;

/**
* Represents a surface implicitly using the top-down 2D projection of a closed spline.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSplineInteriorSurfaceData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	PCG_API void Initialize(const UPCGSplineData* InSplineData);

	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	// ~Begin UPCGData interface
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override { return CachedBounds; }
	virtual FBox GetStrictBounds() const override { return CachedBounds; }
	virtual bool IsBounded() const override { return !!CachedBounds.IsValid; }
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool HasNonTrivialTransform() const override { return true; }
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	// ~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	// ~End UPCGConcreteDataWithPointCache interface

protected:
	/** Recompute cached data used for sampling. */
	void CacheData();

	/** True if the given location falls in the top-down projection of the polygon given by our cached spline points. */
	bool PointInsidePolygon(const FTransform& InTransform, const FBox& InBounds) const;

protected:
	/** Minimal data needed to replicate the behavior from USplineComponent. */
	UPROPERTY()
	FPCGSplineStruct SplineStruct;

	UPROPERTY(Transient)
	FBox CachedBounds = FBox(EForceInit::ForceInit);

	/** Cached list of points to describe the polygon given by the spline. */
	UPROPERTY(Transient)
	TArray<FVector> CachedSplinePoints;

	/** Cached list of 2D points to describe the polygon given by the spline. */
	UPROPERTY(Transient)
	TArray<FVector2D> CachedSplinePoints2D;

#if WITH_EDITORONLY_DATA
	/** Flag that indicates when we need to recompute our cached information. Preferable to serializing data that can be recreated on demand. */
	UPROPERTY(Transient)
	bool bNeedsToCache = true;
#endif
};
