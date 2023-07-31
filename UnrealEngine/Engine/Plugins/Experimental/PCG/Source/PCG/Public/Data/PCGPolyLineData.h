// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGSpatialData.h"

#include "PCGPolyLineData.generated.h"

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPolyLineData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::PolyLine | Super::GetDataType(); }
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 1; }
	virtual FBox GetBounds() const override;
	//~End UPCGSpatialData interface

	virtual int GetNumSegments() const PURE_VIRTUAL(UPCGPolyLineData::GetNumSegments, return 0;);
	virtual FVector::FReal GetSegmentLength(int SegmentIndex) const PURE_VIRTUAL(UPCGPolyLineData::GetSegmentLength, return 0.0f;);
	virtual FTransform GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, FBox* OutBounds = nullptr) const PURE_VIRTUAL(UPCGPolyLine::GetTransformAtDistance, return FTransform(););

	virtual FVector GetLocationAtDistance(int SegmentIndex, FVector::FReal Distance) const { return GetTransformAtDistance(SegmentIndex, Distance).GetLocation(); }
};
