// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoxTypes.h"
#include "CoreMinimal.h"
#include "SegmentTypes.h"

namespace UE
{
namespace Geometry
{
template <typename RealType> struct TAxisAlignedBox2;
template <typename T> struct TSegment2;

// Return true if Segment and Box intersect and false otherwise
template<typename Real>
bool TestIntersection(const TSegment2<Real>& Segment, const TAxisAlignedBox2<Real>& Box);
		
} // namespace UE::Geometry
} // namespace UE
