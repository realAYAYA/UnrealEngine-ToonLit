// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGVolumeData.h"
#include "PCGSurfaceData.h"

#include "Engine/EngineTypes.h"

#include "PCGWorldData.generated.h"

class UPCGSpatialData;

class UWorld;
class UPCGMetadata;
class UPCGComponent;

UENUM()
enum class EPCGWorldQueryFilterByTag
{
	NoTagFilter,
	IncludeTagged,
	ExcludeTagged
};

namespace PCGWorldRayHitConstants
{
	const FName PhysicalMaterialReferenceAttribute = TEXT("PhysicalMaterial");
}

USTRUCT(BlueprintType)
struct FPCGWorldCommonQueryParams
{
	GENERATED_BODY()

	/** If true, will ignore hits/overlaps on content created from PCG. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable))
	bool bIgnorePCGHits = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable))
	bool bIgnoreSelfHits = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data|Advanced", meta = (PCG_Overridable))
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldStatic;

	/** Queries against complex collision if enabled, performance warning */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data|Advanced", meta = (PCG_Overridable))
	bool bTraceComplex = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data|Filtering", meta = (PCG_Overridable))
	EPCGWorldQueryFilterByTag ActorTagFilter = EPCGWorldQueryFilterByTag::NoTagFilter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data|Filtering", meta = (PCG_Overridable, EditCondition = "ActorTagFilter != EPCGWorldQueryFilterByTag::NoTagFilter"))
	FString ActorTagsList;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Data|Filtering", meta = (PCG_Overridable))
	bool bIgnoreLandscapeHits = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable))
	bool bGetReferenceToActorHit = false;

	// Not exposed, will be filled in when initializing this
	UPROPERTY()
	TSet<FName> ParsedActorTagsList;

protected:
	/** Sets up the data we need to efficiently perform the queries */
	void Initialize();
};

USTRUCT(BlueprintType)
struct FPCGWorldVolumetricQueryParams : public FPCGWorldCommonQueryParams
{
	GENERATED_BODY()

	void Initialize();

	/** Controls whether we are trying to find an overlap with physical objects (true) or to find empty spaces that do not contain anything (false) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable))
	bool bSearchForOverlap = true;
};

/** Queries volume for presence of world collision or not. Can be used to voxelize environment. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGWorldVolumetricData : public UPCGVolumeData
{
	GENERATED_BODY()

public:
	PCG_API void Initialize(UWorld* InWorld, const FBox& InBounds = FBox(EForceInit::ForceInit));

	//~Begin UPCGSpatialData interface
	virtual bool IsBounded() const override { return !!Bounds.IsValid; }
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	// TODO not sure what this would mean. Without a direction, this means perhaps finding closest point on any collision surface? Should we implement this disabled?
	//virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	//~Begin UPCGSpatialDataWithPointCache
	virtual bool SupportsBoundedPointData() const { return true; }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override { return CreatePointData(Context, FBox(EForceInit::ForceInit)); }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context, const FBox& InBounds) const override;
	//~End UPCGSpatialDataWithPointCache

	UPROPERTY()
	TWeakObjectPtr<UWorld> World = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UPCGComponent> OriginatingComponent = nullptr;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (ShowOnlyInnerProperties))
	FPCGWorldVolumetricQueryParams QueryParams;
};

USTRUCT(BlueprintType)
struct FPCGWorldRayHitQueryParams : public FPCGWorldCommonQueryParams
{
	GENERATED_BODY()

	void Initialize();

	/** Set ray parameters including origin, direction and length explicitly rather than deriving these from the generating actor bounds. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (DisplayName = "Set Ray Parameters", PCG_Overridable))
	bool bOverrideDefaultParams = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable, EditCondition = "bOverrideDefaultParams", EditConditionHides))
	FVector RayOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable, EditCondition = "bOverrideDefaultParams", EditConditionHides))
	FVector RayDirection = FVector(0.0, 0.0, -1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable, EditCondition = "bOverrideDefaultParams", EditConditionHides))
	double RayLength = 1.0e+5; // 100m

	// TODO: see in FCollisionQueryParams if there are some flags we want to expose
	// examples: bReturnFaceIndex, bReturnPhysicalMaterial, some ignore patterns

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable))
	bool bApplyMetadataFromLandscape = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (PCG_Overridable))
	bool bGetReferenceToPhysicalMaterial = false;
};

/** Executes collision queries against world collision. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGWorldRayHitData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	PCG_API void Initialize(UWorld* InWorld, const FBox& InBounds = FBox(EForceInit::ForceInit));

	// ~Begin UPCGData interface
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override { return Bounds; }
	virtual FBox GetStrictBounds() const override { return Bounds; }
	virtual bool IsBounded() const override { return !!Bounds.IsValid; }
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool HasNonTrivialTransform() const override { return true; }
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	// ~Begin UPCGSpatialDataWithPointCache interface
	virtual bool SupportsBoundedPointData() const { return true; }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override { return CreatePointData(Context, FBox(EForceInit::ForceInit)); }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context, const FBox& InBounds) const override;
	// ~End UPCGConcreteDataWithPointCache interface

	UPROPERTY()
	TWeakObjectPtr<UWorld> World = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UPCGComponent> OriginatingComponent = nullptr;

	UPROPERTY()
	FBox Bounds = FBox(EForceInit::ForceInit);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Data, meta = (ShowOnlyInnerProperties))
	FPCGWorldRayHitQueryParams QueryParams;
};
