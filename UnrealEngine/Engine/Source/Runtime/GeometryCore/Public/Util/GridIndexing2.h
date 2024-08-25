// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp GridIndexing2

#pragma once

#include "VectorTypes.h"
#include "IntVectorTypes.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

/**
 * Convert between integer grid coordinates and scaled real-valued coordinates (ie assumes integer grid origin == real origin)
 */
template<typename RealType>
struct TScaleGridIndexer2
{
	/** Real-valued size of an integer grid cell */
	RealType CellSize;

	TScaleGridIndexer2() : CellSize((RealType)1) 
	{
		ensure(CellSize >= TMathUtil<RealType>::ZeroTolerance);
	}

	TScaleGridIndexer2(RealType CellSize) : CellSize(CellSize)
	{
		ensure(CellSize >= TMathUtil<RealType>::ZeroTolerance);
	}

	/** Convert real-valued point to integer grid coordinates */
	inline FVector2i ToGrid(const TVector2<RealType>& P) const
	{
		return FVector2i(
			(int)TMathUtil<RealType>::Floor(P.X / CellSize),
			(int)TMathUtil<RealType>::Floor(P.Y / CellSize));
	}

	/** Convert integer grid coordinates to real-valued point */
	inline TVector2<RealType> FromGrid(const FVector2i& GridPoint) const
	{
		return TVector2<RealType>(GridPoint.X*CellSize, GridPoint.Y*CellSize);
	}
};
typedef TScaleGridIndexer2<float> FScaleGridIndexer2f;
typedef TScaleGridIndexer2<double> FScaleGridIndexer2d;


/**
 * Convert between integer grid coordinates and scaled+translated real-valued coordinates
 */
template<typename RealType>
struct TShiftGridIndexer2
{
	/** Real-valued size of an integer grid cell */
	RealType CellSize;
	/** Real-valued origin of grid, position of integer grid origin */
	TVector2<RealType> Origin;

	TShiftGridIndexer2(const TVector2<RealType>& origin, RealType cellSize)
	{
		Origin = origin;
		CellSize = cellSize;
		ensure(CellSize >= TMathUtil<RealType>::ZeroTolerance);
	}

	/** Convert real-valued point to integer grid coordinates */
	inline FVector2i ToGrid(const TVector2<RealType>& Point) const
	{
		return FVector2i(
			(int)TMathUtil<RealType>::Floor((Point.X - Origin.X) / CellSize),
			(int)TMathUtil<RealType>::Floor((Point.Y - Origin.Y) / CellSize));
	}

	/** Convert integer grid coordinates to real-valued point */
	inline TVector2<RealType> FromGrid(const FVector2i& GridPoint) const
	{
		return TVector2<RealType>(
			((RealType)GridPoint.X * CellSize) + Origin.X,
			((RealType)GridPoint.Y * CellSize) + Origin.Y);
	}

	/** Convert real-valued grid coordinates to real-valued point */
	inline TVector2<RealType> FromGrid(const TVector2<RealType>& RealGridPoint) const
	{
		return TVector2<RealType>(
			((RealType)RealGridPoint.X * CellSize) + Origin.X,
			((RealType)RealGridPoint.Y * CellSize) + Origin.Y);
	}
};
typedef TShiftGridIndexer2<float> FShiftGridIndexer2f;
typedef TShiftGridIndexer2<double> FShiftGridIndexer2d;


} // end namespace UE::Geometry
} // end namespace UE
