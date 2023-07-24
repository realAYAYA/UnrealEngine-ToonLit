// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGData.h"
#include "PCGPoint.h" // IWYU pragma: keep
#include "Elements/PCGProjectionParams.h"
#include "Metadata/PCGMetadata.h"

#include "PCGSpatialData.generated.h"

class AActor;
struct FPCGContext;
class UPCGPointData;
class UPCGIntersectionData;
class UPCGUnionData;
class UPCGDifferenceData;
class UPCGProjectionData;

/**
* "Concrete" data base class for PCG generation
* This will be the base class for data classes that actually represent
* concrete evidence of spatial data - points, surfaces, splines, etc.
* In opposition to settings/control type of data.
* 
* Conceptually, any concrete data can be decayed into points (potentially through transformations)
* which hold metadata and a transform, and this is the basic currency of the PCG framework.
*/
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSpatialData : public UPCGData
{
	GENERATED_BODY()

public:
	UPCGSpatialData(const FObjectInitializer& ObjectInitializer);

	//~Begin UObject Interface
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~End UObject Interface

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Spatial; }
	// ~End UPCGData interface

	/** Virtual call to allocate a new spacial data object, duplicate this spatial data into
	*   and parent the new metadata with this class metadata (if asked).
	*   Should be way cheaper than DuplicateObject, since we avoid duplicating metadata.
	*   It will not deep copy references.
	*   Some data are marked mutable and therefore are not threadsafe to copy, so they are not copied.
	*   They are mainly cached values (and octree for points).
	*   TODO: If we want to also copy those values (can be an optimization), we need to guard the copy.
	*/
	UPCGSpatialData* DuplicateData(const bool bInitializeMetadata = true) const;

	/** Returns the dimension of the data type, which has nothing to do with the dimension of its points */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual int GetDimension() const PURE_VIRTUAL(UPCGSpatialData::GetDimension, return 0;);

	/** Returns the full bounds (including density fall-off) of the data */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual FBox GetBounds() const PURE_VIRTUAL(UPCGSpatialData::GetBounds, return FBox(EForceInit::ForceInit););

	/** Returns whether a given spatial data is bounded as some data types do not require bounds by themselves */
	virtual bool IsBounded() const { return true; }

	/** Returns the bounds in which the density is always 1 */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual FBox GetStrictBounds() const { return FBox(EForceInit::ForceInit); }

	/** Returns the expected data normal (for surfaces) or eventual projection axis (for volumes) */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual FVector GetNormal() const { return FVector::UnitZ(); }

	/** Computes the density at a given location */
	UFUNCTION(BlueprintCallable, Category = Distribution)
	virtual float GetDensityAtPosition(const FVector& InPosition) const;

	/** Discretizes the data into points */
	UFUNCTION(BlueprintCallable, Category = SpatialData, meta = (DeprecatedFunction, DeprecationMessage = "The To Point Data function is deprecated - use To Point Data With Context instead."))
	const UPCGPointData* ToPointData() const { return ToPointData(nullptr); }

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	const UPCGPointData* ToPointDataWithContext(UPARAM(ref) FPCGContext& Context) const { return ToPointData(&Context); }

	virtual const UPCGPointData* ToPointData(FPCGContext* Context, const FBox& InBounds = FBox(EForceInit::ForceInit)) const PURE_VIRTUAL(UPCGSpatialData::ToPointData, return nullptr;);

	/** Transform a world-space position to a world-space position in relation to the current data. (ex: projection on surface) */
	FVector TransformPosition(const FVector& InPosition) const;

	/** Sample rotation, scale and other attributes from this data at the query position. Returns true if Transform location and Bounds overlaps this data. */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const PURE_VIRTUAL(UPCGSpatialData::SamplePoint, return false;);

	/** Project the query point onto this data, and sample point and metadata information at the projected position. Returns true if successful. */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;

	/** Returns true if the data has a non-trivial transform */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual bool HasNonTrivialTransform() const { return false; }

	/** Returns a specialized data to intersect with another data */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual UPCGIntersectionData* IntersectWith(const UPCGSpatialData* InOther) const;

	/** Returns a specialized data to project this on another data of equal or higher dimension. Returns copy of this data if projection fails. */
	UFUNCTION(BlueprintCallable, Category = SpatialData, meta=(AutoCreateRefTerm="InParams"))
	virtual UPCGSpatialData* ProjectOn(const UPCGSpatialData* InOther, const FPCGProjectionParams& InParams = FPCGProjectionParams()) const;

	/** Returns a specialized data to union this with another data */
	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual UPCGUnionData* UnionWith(const UPCGSpatialData* InOther) const;

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	virtual UPCGDifferenceData* Subtract(const UPCGSpatialData* InOther) const;

	UFUNCTION(BlueprintCallable, Category = Metadata)
	const UPCGMetadata* ConstMetadata() const { return Metadata; }

	UFUNCTION(BlueprintCallable, Category = Metadata)
	UPCGMetadata* MutableMetadata() { return Metadata; }

	UFUNCTION(BlueprintCallable, Category = Metadata, meta=(DeprecatedFunction, DeprecationMessage = "The Create Empty Metadata function is not needed anymore - it can safely be removed"))
	UPCGMetadata* CreateEmptyMetadata();

	UFUNCTION(BlueprintCallable, Category = SpatialData)
	void InitializeFromData(const UPCGSpatialData* InSource, const UPCGMetadata* InMetadataParentOverride = nullptr, bool bInheritMetadata = true, bool bInheritAttributes = true);

	/** True if this operation does not have an inverse and cannot be queried analytically/implicitly, and therefore must be collapsed to an explicit point representation. */
	virtual bool RequiresCollapseToSample() const { return false; }

	/** A call that is made recursively up through the graph to find the best candidate shape for point generation. If InDimension is -1, finds lowest dimensional shape. */
	virtual const UPCGSpatialData* FindShapeFromNetwork(const int InDimension) const { return (InDimension == -1 || GetDimension() == InDimension) ? this : nullptr; }

	/** Find the first concrete (non-composite) shape in the network. Depth first search. */
	virtual const UPCGSpatialData* FindFirstConcreteShapeFromNetwork() const { return !!(GetDataType() & EPCGDataType::Concrete) ? this : nullptr; }

	UPROPERTY(Transient, BlueprintReadWrite, EditAnywhere, Category = Data)
	TWeakObjectPtr<AActor> TargetActor = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, BlueprintReadWrite, EditAnywhere, Category = Debug)
	bool bKeepZeroDensityPoints = false;
#endif

	// Not accessible through blueprint to make sure the constness is preserved
	UPROPERTY(VisibleAnywhere, Category = Metadata)
	TObjectPtr<UPCGMetadata> Metadata = nullptr;

protected:
	virtual UPCGSpatialData* CopyInternal() const PURE_VIRTUAL(UPCGSpatialData::CopyInternal, return nullptr;);
};

UCLASS(Abstract, ClassGroup = (Procedural))
class PCG_API UPCGSpatialDataWithPointCache : public UPCGSpatialData
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~End UObject Interface

	// ~UPCGSpatialData implementation
	virtual const UPCGPointData* ToPointData(FPCGContext* Context, const FBox& InBounds = FBox(EForceInit::ForceInit)) const override;
	// ~End UPCGSpatialData implementation

protected:
	virtual bool SupportsBoundedPointData() const { return false; }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const PURE_VIRTUAL(UPCGSpatialData::CreatePointData, return nullptr;);
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context, const FBox& InBounds) const { return CreatePointData(Context); }

private:
	UPROPERTY(Transient)
	mutable TObjectPtr<const UPCGPointData> CachedPointData;

	UPROPERTY(Transient)
	mutable TArray<FBox> CachedBoundedPointDataBoxes;

	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<const UPCGPointData>> CachedBoundedPointData;

	mutable FCriticalSection CacheLock;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGContext.h"
#endif
