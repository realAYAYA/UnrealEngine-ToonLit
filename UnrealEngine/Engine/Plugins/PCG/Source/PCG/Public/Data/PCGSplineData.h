// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/InterpCurve.h"
#include "PCGPolyLineData.h"
#include "PCGProjectionData.h"
#include "PCGSplineStruct.h"
#include "Elements/PCGProjectionParams.h"

#include "PCGSplineData.generated.h"

class UPCGSpatialData;

class USplineComponent;
class UPCGSurfaceData;

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSplineData : public UPCGPolyLineData
{
	GENERATED_BODY()

public:
	void Initialize(const USplineComponent* InSpline);
	void Initialize(const TArray<FSplinePoint>& InSplinePoints, bool bInClosedLoop, const FTransform& InTransform);
	void Initialize(const FPCGSplineStruct& InSplineStruct);
	void ApplyTo(USplineComponent* InSpline);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Spline; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	//~Begin UPCGPolyLineData interface
	virtual FTransform GetTransform() const override;
	virtual int GetNumSegments() const override;
	virtual FVector::FReal GetSegmentLength(int SegmentIndex) const override;
	virtual FVector GetLocationAtDistance(int SegmentIndex, FVector::FReal Distance, bool bWorldSpace = true) const override;
	virtual FTransform GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, bool bWorldSpace = true, FBox* OutBounds = nullptr) const override;
	virtual FVector::FReal GetCurvatureAtDistance(int SegmentIndex, FVector::FReal Distance) const override;
	virtual float GetInputKeyAtDistance(int SegmentIndex, FVector::FReal Distance) const override;
	virtual void GetTangentsAtSegmentStart(int SegmentIndex, FVector& OutArriveTangent, FVector& OutLeaveTangent) const override;
	virtual FVector::FReal GetDistanceAtSegmentStart(int SegmentIndex) const override;
	virtual bool IsClosed() const override { return SplineStruct.bClosedLoop; }
	//~End UPCGPolyLineData interface

	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	virtual UPCGSpatialData* ProjectOn(const UPCGSpatialData* InOther, const FPCGProjectionParams& InParams = FPCGProjectionParams()) const override;
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

	virtual void CopySplineData(UPCGSplineData* InCopy) const;

public:
	// Minimal data needed to replicate the behavior from USplineComponent
	UPROPERTY()
	FPCGSplineStruct SplineStruct;

protected:
	UPROPERTY()
	FBox CachedBounds = FBox(EForceInit::ForceInit);
};

/* The projection of a spline onto a surface. */
UCLASS(MinimalAPI, BlueprintType, ClassGroup=(Procedural))
class UPCGSplineProjectionData : public UPCGProjectionData
{
	GENERATED_BODY()

public:
	PCG_API void Initialize(const UPCGSplineData* InSourceSpline, const UPCGSpatialData* InTargetSurface, const FPCGProjectionParams& InParams);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Spline; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	const UPCGSplineData* GetSpline() const;
	const UPCGSpatialData* GetSurface() const;

	//~Begin UPCGSpatialData interface
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	//~End UPCGSpatialData interface

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SpatialData)
	FInterpCurveVector2D ProjectedPosition;

protected:
	FVector2D Project(const FVector& InVector) const;

	//~Begin UPCGSpatialData interface
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
