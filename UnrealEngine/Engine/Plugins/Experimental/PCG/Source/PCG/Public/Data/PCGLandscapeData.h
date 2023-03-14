// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSurfaceData.h"

#include "PCGLandscapeData.generated.h"

class ALandscapeProxy;
class ULandscapeInfo;
class UPCGLandscapeCache;

/**
* Landscape data access abstraction for PCG. Supports multi-landscape access, but it assumes that they are not overlapping.
*/
UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGLandscapeData : public UPCGSurfaceData
{
	GENERATED_BODY()

public:
	void Initialize(const TArray<TWeakObjectPtr<ALandscapeProxy>>& InLandscapes, const FBox& InBounds, bool bInHeightOnly, bool bInUseMetadata);

	// ~Begin UObject interface
	virtual void PostLoad();
	// ~End UObject interface

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Landscape | Super::GetDataType(); }
	// ~End UPCGData interface

	// ~Begin UPGCSpatialData interface
	virtual FBox GetBounds() const override;
	virtual FBox GetStrictBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual bool HasNonTrivialTransform() const override { return true; }
	// ~End UPGCConcreteData interface

	// ~Begin UPCGSpatialDataWithPointCache interface
	virtual bool SupportsBoundedPointData() const { return true; }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override { return CreatePointData(Context, FBox(EForceInit::ForceInit)); }
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context, const FBox& InBounds) const override;
	// ~End UPCGConcreteDataWithPointCache interface

	// TODO: add on property changed to clear cached data. This is used to populate the LandscapeInfos array.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SourceData)
	TArray<TSoftObjectPtr<ALandscapeProxy>> Landscapes;

	bool IsUsingMetadata() const { return bUseMetadata; }

protected:
	/** Returns the landscape info associated to the first landscape that contains the given position
	* Note that this implicitly removes support for overlapping landscapes, which might be a future TODO
	*/
	const ULandscapeInfo* GetLandscapeInfo(const FVector& InPosition) const;

	UPROPERTY()
	FBox Bounds = FBox(EForceInit::ForceInit);

	UPROPERTY()
	bool bHeightOnly = false;

	UPROPERTY()
	bool bUseMetadata = true;

private:
	// Transient data
	TArray<TPair<FBox, ULandscapeInfo*>> LandscapeInfos;
	UPCGLandscapeCache* LandscapeCache = nullptr;
};
