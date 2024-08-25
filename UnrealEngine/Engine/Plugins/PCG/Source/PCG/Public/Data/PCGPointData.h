// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"

#include "Math/GenericOctreePublic.h"
#include "Math/GenericOctree.h"

#include "PCGPointData.generated.h"

struct FPCGProjectionParams;

class AActor;

namespace PCGPointHelpers
{
	void Lerp(const FPCGPoint& A, const FPCGPoint& B, float Ratio, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata);
	void Bilerp(const FPCGPoint& X0Y0, const FPCGPoint& X1Y0, const FPCGPoint& X0Y1, const FPCGPoint& X1Y1, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, float XFactor, float YFactor);
	void BilerpWithSnapping(const FPCGPoint& X0Y0, const FPCGPoint& X1Y0, const FPCGPoint& X0Y1, const FPCGPoint& X1Y1, const UPCGMetadata* SourceMetadata, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, float XFactor, float YFactor);
}

namespace PCGPointDataConstants
{
	const FName ActorReferenceAttribute = TEXT("ActorReference");
}

struct PCG_API FPCGPointRef
{
	FPCGPointRef(const FPCGPoint& InPoint);

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

	//~Begin UObject Interface
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~End UObject Interface

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Point; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	// ~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 0; }
	virtual FBox GetBounds() const override;
	virtual const UPCGPointData* ToPointData(FPCGContext* Context, const FBox& InBounds = FBox(EForceInit::ForceInit)) const { return this; }
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;

protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	// ~End UPCGSpatialData interface

public:
	/** Initializes a single point based on the given actor */
	void InitializeFromActor(AActor* InActor, bool* bOutOptionalSanitizedTagAttributeName = nullptr);

	/** Adds a single point based on the given actor */
	void AddSinglePointFromActor(AActor* InActor, bool* bOutOptionalSanitizedTagAttributeName = nullptr);

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	const TArray<FPCGPoint>& GetPoints() const { return Points; }

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = SpatialData)
	TArray<FPCGPoint> GetPointsCopy() const { return Points; }

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	FPCGPoint GetPoint(int32 Index) const;

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void SetPoints(const TArray<FPCGPoint>& InPoints);
	
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void CopyPointsFrom(const UPCGPointData* InData, const TArray<int>& InDataIndices);

	/** Make a pass on Metadata to flatten parenting and only keep entries used by points. */
	virtual void Flatten() override;

	TArray<FPCGPoint>& GetMutablePoints();

	const PointOctree& GetOctree() const;

	bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, bool bUseBounds) const;

	/** Get the dirty status of the Octree. Note that the Octree can be rebuilt from another thread, so this info can be invalidated at anytime. */
	bool IsOctreeDirty() const { return bOctreeIsDirty; }

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
