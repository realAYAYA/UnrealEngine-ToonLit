// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGPolyLineData.h"
#include "Data/PCGSpatialData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPolyLineData)

FBox UPCGPolyLineData::GetBounds() const
{
	FBox Bounds(EForceInit::ForceInit);

	const int NumSegments = GetNumSegments();

	for (int SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		Bounds += GetLocationAtDistance(SegmentIndex, 0);
		Bounds += GetLocationAtDistance(SegmentIndex, GetSegmentLength(SegmentIndex));
	}
	
	return Bounds;
}

FVector::FReal UPCGPolyLineData::GetLength() const
{
	FVector::FReal Length = 0.0;

	for (int SegmentIndex = 0; SegmentIndex < GetNumSegments(); ++SegmentIndex)
	{
		Length += GetSegmentLength(SegmentIndex);
	}

	return Length;
}

void UPCGPolyLineData::GetTangentsAtSegmentStart(int SegmentIndex, FVector& OutArriveTangent, FVector& OutLeaveTangent) const
{
	OutArriveTangent = FVector::Zero();
	OutLeaveTangent = FVector::Zero();
}

float UPCGPolyLineData::GetAlphaAtDistance(int SegmentIndex, FVector::FReal Distance) const
{
	const int32 NumSegments = GetNumSegments();
	const FVector::FReal SegmentLength = GetSegmentLength(SegmentIndex);
	return SegmentIndex / static_cast<float>(NumSegments) + (Distance / SegmentLength / NumSegments);
}
