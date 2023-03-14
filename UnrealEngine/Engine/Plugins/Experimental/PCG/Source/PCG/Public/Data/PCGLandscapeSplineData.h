// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGPolyLineData.h"

#include "PCGLandscapeSplineData.generated.h"

class ULandscapeSplinesComponent;

UCLASS(BlueprintType, ClassGroup=(Procedural))
class PCG_API UPCGLandscapeSplineData : public UPCGPolyLineData
{
	GENERATED_BODY()
public:
	void Initialize(ULandscapeSplinesComponent* InSplineComponent);

	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::LandscapeSpline | Super::GetDataType(); }
	// ~End UPCGData interface

	//~Begin UPCGPolyLineData interface
	virtual int GetNumSegments() const override;
	virtual FVector::FReal GetSegmentLength(int SegmentIndex) const override;
	virtual FTransform GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, FBox* OutBounds = nullptr) const override;
	//~End UPCGPolyLineData interface

	//~Begin UPCGSpatialDataWithPointCache interface
	virtual const UPCGPointData* CreatePointData(FPCGContext* Context) const override;
	//~End UPCGSpatialDataWithPointCache interface

	//~Begin UPCGSpatialData interface
	virtual FBox GetBounds() const override;
	virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	//~End

protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = SourceData)
	TWeakObjectPtr<ULandscapeSplinesComponent> Spline;
};
