// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSpatialData.h"

#include "Math/GenericOctreePublic.h"
#include "Math/GenericOctree.h"

#include "PCGPointData.generated.h"

class AActor;

namespace PCGPointHelpers
{
	void Lerp(const FPCGPoint& A, const FPCGPoint& B, float Ratio, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata);
	void Bilerp(const FPCGPoint& X0Y0, const FPCGPoint& X1Y0, const FPCGPoint& X0Y1, const FPCGPoint& X1Y1, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, float XFactor, float YFactor);
	void BilerpWithSnapping(const FPCGPoint& X0Y0, const FPCGPoint& X1Y0, const FPCGPoint& X0Y1, const FPCGPoint& X1Y1, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, float XFactor, float YFactor);
}

struct PCG_API FPCGPointRef
{
	FPCGPointRef(const FPCGPoint& InPoint);
	FPCGPointRef(const FPCGPointRef& InPointRef);

	const FPCGPoint* Point;
	FBoxSphereBounds Bounds;
};

struct PCG_API FPCGPointRefSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPCGPointRef& InPoint)
	{
		return InPoint.Bounds;
	}

	FORCEINLINE static const bool AreElementsEqual(const FPCGPointRef& A, const FPCGPointRef& B)
	{
		// TODO: verify if that's sufficient
		return A.Point == B.Point;
	}

	FORCEINLINE static void ApplyOffset(FPCGPointRef& InPoint)
	{
		ensureMsgf(false, TEXT("Not implemented"));
	}

	FORCEINLINE static void SetElementId(const FPCGPointRef& Element, FOctreeElementId2 OctreeElementID)
	{
	}
};

// TODO: Split this in "concrete" vs "api" class (needed for views)
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPointData : public UPCGSpatialData
{
	GENERATED_BODY()

public:
	typedef TOctree2<FPCGPointRef, FPCGPointRefSemantics> PointOctree;

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Point | Super::GetDataType(); }
	// ~End UPCGData interface

	// ~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 0; }
	virtual FBox GetBounds() const override;
	virtual const UPCGPointData* ToPointData(FPCGContext* Context, const FBox& InBounds = FBox(EForceInit::ForceInit)) const { return this; }
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	// ~End UPCGSpatialData interface

	void InitializeFromActor(AActor* InActor);

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	const TArray<FPCGPoint>& GetPoints() const { return Points; }

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	FPCGPoint GetPoint(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void SetPoints(const TArray<FPCGPoint>& InPoints);
	
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void CopyPointsFrom(const UPCGPointData* InData, const TArray<int>& InDataIndices);

	TArray<FPCGPoint>& GetMutablePoints();

	const PointOctree& GetOctree() const;

protected:
	void RebuildOctree() const;
	void RecomputeBounds() const;

	UPROPERTY()
	TArray<FPCGPoint> Points;

	mutable FCriticalSection CachedDataLock;
	mutable PointOctree Octree;
	mutable FBox Bounds; // TODO: review if this needs to be threadsafe
	mutable bool bBoundsAreDirty = true;
	mutable bool bOctreeIsDirty = true;
};
