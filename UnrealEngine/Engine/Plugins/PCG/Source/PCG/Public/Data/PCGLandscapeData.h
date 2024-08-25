// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSurfaceData.h"

#include "PCGLandscapeData.generated.h"

class UPCGSpatialData;
struct FPCGProjectionParams;

class ALandscapeProxy;
class ULandscapeInfo;
class UPCGLandscapeCache;

USTRUCT(BlueprintType)
struct FPCGLandscapeDataProps
{
	GENERATED_BODY()

	/** Controls whether the points projected on the landscape will return the normal/tangent (if false) or only the position (if true) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bGetHeightOnly = false;

	/** Controls whether data from landscape layers will be retrieved (turning it off is an optimization if that data is not needed) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bGetLayerWeights = true;

	/** Controls whether the points from this landscape will return the actor from which they originate (e.g. which Landscape Proxy) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bGetActorReference = false;

	/** Controls whether the points from the landscape will have their physical material added as the "PhysicalMaterial" attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bGetPhysicalMaterial = false;

	/** Controls whether the component coordinates will be added the point as attributes ('CoordinateX', 'CoordinateY') */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bGetComponentCoordinates = false;
};

/**
* Landscape data access abstraction for PCG. Supports multi-landscape access, but it assumes that they are not overlapping.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGLandscapeData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	void Initialize(const TArray<TWeakObjectPtr<ALandscapeProxy>>& InLandscapes, const FBox& InBounds, const FPCGLandscapeDataProps& InDataProps);

	// ~Begin UObject interface
	virtual void PostLoad();
	// ~End UObject interface

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Landscape; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	// ~Begin UPGCSpatialData interface
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual void SamplePoints(const TArrayView<const TPair<FTransform, FBox>>& Samples, const TArrayView<FPCGPoint>& OutPoints, UPCGMetadata* OutMetadata) const override;
	virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool HasNonTrivialTransform() const override { return true; }
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

public:
	// ~Begin UPCGSpatialDataWithPointCache interface
	virtual bool SupportsBoundedPointData() const { return true; }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override { return CreatePointData(Context, FBox(EForceInit::ForceInit)); }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context, const FBox& InBounds) const override;
	// ~End UPCGSpatialDataWithPointCache interface

	// TODO: add on property changed to clear cached data. This is used to populate the LandscapeInfos array.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SourceData)
	TArray<TSoftObjectPtr<ALandscapeProxy>> Landscapes;

	bool IsUsingMetadata() const { return DataProps.bGetLayerWeights; }

protected:
	/** Returns the landscape info associated to the first landscape that contains the given position
	* Note that this implicitly removes support for overlapping landscapes, which might be a future TODO
	*/
	const ULandscapeInfo* GetLandscapeInfo(const FVector& InPosition) const;

	UPROPERTY()
	FBox Bounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	FPCGLandscapeDataProps DataProps;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bHeightOnly_DEPRECATED = false;

	UPROPERTY()
	bool bUseMetadata_DEPRECATED = true;
#endif // WITH_EDITORONLY_DATA

private:
	// Transient data
	TArray<TPair<FBox, ULandscapeInfo*>> BoundsToLandscapeInfos;
	TArray<ULandscapeInfo*> LandscapeInfos;
	UPCGLandscapeCache* LandscapeCache = nullptr;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
