// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGPolyLineData.h"
#include "PCGProjectionData.h"
#include "PCGSplineData.generated.h"

class USplineComponent;
class UPCGSurfaceData;

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSplineData : public UPCGPolyLineData
{
	GENERATED_BODY()

public:
	void Initialize(USplineComponent* InSpline);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Spline | Super::GetDataType(); }
	// ~End UPCGData interface

	//~Begin UPCGPolyLineData interface
	virtual int GetNumSegments() const override;
	virtual FVector::FReal GetSegmentLength(int SegmentIndex) const override;
	virtual FVector GetLocationAtDistance(int SegmentIndex, FVector::FReal Distance) const override;
	virtual FTransform GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, FBox* OutBounds = nullptr) const override;
	//~End UPCGPolyLineData interface

	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual UPCGProjectionData* ProjectOn(const UPCGSpatialData* InOther) const override;
	//~End 

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SourceData)
	TSoftObjectPtr<USplineComponent> Spline;

protected:
	UPROPERTY()
	FBox CachedBounds = FBox(EForceInit::ForceInit);
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGSplineProjectionData : public UPCGProjectionData
{
	GENERATED_BODY()
public:
	void Initialize(const UPCGSplineData* InSourceSpline, const UPCGSpatialData* InTargetSurface);

	//~Begin UPCGSpatialData interface
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	//~End UPCGSpatialData interface

	const UPCGSplineData* GetSpline() const;
	const UPCGSpatialData* GetSurface() const;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SpatialData)
	FInterpCurveVector2D ProjectedPosition;

protected:
	FVector2D Project(const FVector& InVector) const;
};
