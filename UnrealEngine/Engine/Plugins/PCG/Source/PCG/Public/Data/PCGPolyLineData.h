// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSpatialData.h"

#include "PCGPolyLineData.generated.h"

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGPolyLineData : public UPCGSpatialDataWithPointCache
{
	GENERATED_BODY()

public:
	// ~Begin UPCGData interface
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::PolyLine; }
	// ~End UPCGData interface

	//~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 1; }
	virtual FBox GetBounds() const override;
	//~End UPCGSpatialData interface

	/** Get the world-space transform of the entire line. */
	virtual FTransform GetTransform() const { return FTransform::Identity; }

	/** Get the number of segments in this line. If the line is closed, this will be the same as the number of control points in the line. */
	virtual int GetNumSegments() const PURE_VIRTUAL(UPCGPolyLineData::GetNumSegments, return 0;);

	/** Get the length of a specific segment of the line. */
	virtual FVector::FReal GetSegmentLength(int SegmentIndex) const PURE_VIRTUAL(UPCGPolyLineData::GetSegmentLength, return 0;);

	/** Get the total length of the line. */
	virtual FVector::FReal GetLength() const;

	/** Get the location at a distance along the line. */
	virtual FTransform GetTransformAtDistance(int SegmentIndex, FVector::FReal Distance, bool bWorldSpace = true, FBox* OutBounds = nullptr) const PURE_VIRTUAL(UPCGPolyLine::GetTransformAtDistance, return FTransform(););

	/** Get the location at a distance along the line. */
	virtual FVector GetLocationAtDistance(int SegmentIndex, FVector::FReal Distance, bool bWorldSpace = true) const { return GetTransformAtDistance(SegmentIndex, Distance, bWorldSpace).GetLocation(); }

	/** Get the curvature at a distance along the line. */
	virtual FVector::FReal GetCurvatureAtDistance(int SegmentIndex, FVector::FReal Distance) const { return 0; }

	/**
	 * Get a value [0,1] representing how far along the point is to the end of the line. Each segment on the line represents a same-size interval.
	 * For example, if there are three segments, each segment will take up 0.333... of the interval.
	 */
	virtual float GetAlphaAtDistance(int SegmentIndex, FVector::FReal Distance) const;

	/** Get the input key at a distance along the line. InputKey is a float value in [0, N], where N is the number of control points. Each range [i, i+1] represents an interpolation from 0 to 1 across spline segment i. */
	virtual float GetInputKeyAtDistance(int SegmentIndex, FVector::FReal Distance) const { return 0; }

	/** Get the arrive and leave tangents for a control point via its segment index. */
	virtual void GetTangentsAtSegmentStart(int SegmentIndex, FVector& OutArriveTangent, FVector& OutLeaveTangent) const;

	/** Get the cumulative distance along the line to the start of a segment. */
	virtual FVector::FReal GetDistanceAtSegmentStart(int SegmentIndex) const { return 0; }

	/** True if the line is a closed loop. */
	virtual bool IsClosed() const { return false; }

	/** If a PolyLine subtype has custom metadata, it can use this virtual call to write that into the PCG Metadata per-point. */
	virtual void WriteMetadataToPoint(float InputKey, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const {}

	/** True if the line holds custom metadata. */
	virtual bool HasCustomMetadata() const { return false; }
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
