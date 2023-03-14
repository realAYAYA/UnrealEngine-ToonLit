// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGPolyLineData.h"

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