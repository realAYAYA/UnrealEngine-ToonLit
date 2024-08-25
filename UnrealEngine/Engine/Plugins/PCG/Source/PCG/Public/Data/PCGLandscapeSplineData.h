// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPolyLineData.h"

#include "PCGLandscapeSplineData.generated.h"

class UPCGSpatialData;

class ULandscapeSplinesComponent;

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGLandscapeSplineData : public UPCGPolyLineData
{
	GENERATED_BODY()
public:
	void Initialize(ULandscapeSplinesComponent* InSplineComponent);

	// ~Begin UObject interface
	virtual void PostLoad() override;
	// ~End UObject interface

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::LandscapeSpline; }
	virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;
	// ~End UPCGData interface

	//~Begin UPCGPolyLineData interface
	virtual FTransform GetTransform() const override;
	virtual int GetNumSegments() const override;
	virtual FVector::FReal GetSegmentLength(int SegmentIndex) const override;
	virtual FTransform GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, bool bWorldSpace = true, FBox* OutBounds = nullptr) const override;
	virtual FVector::FReal GetCurvatureAtDistance(int SegmentIndex, FVector::FReal Distance) const override;
	virtual float GetInputKeyAtDistance(int SegmentIndex, FVector::FReal Distance) const override;
	virtual void GetTangentsAtSegmentStart(int SegmentIndex, FVector& OutArriveTangent, FVector& OutLeaveTangent) const override;
	virtual FVector::FReal GetDistanceAtSegmentStart(int SegmentIndex) const override;
	//~End UPCGPolyLineData interface

	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
protected:
	virtual UPCGSpatialData* CopyInternal() const override;
	//~End UPCGSpatialData interface

	/** Recompute the reparameterization of the spline by distance. */
	void UpdateReparamTable();

	/** Get the index of the first interp point before a given distance. Also computes the Alpha describing how far (in [0, 1]) the sample is between the left and right points. */
	void GetInterpPointAtDistance(int SegmentIndex, FVector::FReal Distance, int32& OutPointIndex, bool bComputeAlpha, FVector::FReal& OutAlpha) const;

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SourceData)
	TWeakObjectPtr<ULandscapeSplinesComponent> Spline;

	/** Reparameterization of the spline by distance. Useful to query the InputKey at arbitrary distance. */
	FInterpCurveFloat ReparamTable;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "PCGProjectionData.h"
#endif
