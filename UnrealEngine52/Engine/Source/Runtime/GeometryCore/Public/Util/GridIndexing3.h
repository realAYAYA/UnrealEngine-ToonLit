// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp GridIndexing3

#pragma once

#include "VectorTypes.h"
#include "IntVectorTypes.h"
#include "BoxTypes.h"

namespace UE
{
namespace Geometry
{

using namespace UE::Math;

/**
 * Convert between integer grid coordinates and scaled real-valued coordinates (ie assumes integer grid origin == real origin)
 */
template<typename RealType>
struct TScaleGridIndexer3
{
	/** Real-valued size of an integer grid cell */
	RealType CellSize;

	TScaleGridIndexer3() : CellSize((RealType)1) 
	{
		ensure(CellSize >= TMathUtil<RealType>::ZeroTolerance);
	}
	
	TScaleGridIndexer3(RealType CellSize) : CellSize(CellSize) 
	{
	}

	/** Convert real-valued point to integer grid coordinates */
	inline FVector3i ToGrid(const TVector<RealType>& P) const
	{
		return FVector3i(
			(int)TMathUtil<RealType>::Floor(P.X / CellSize),
			(int)TMathUtil<RealType>::Floor(P.Y / CellSize),
			(int)TMathUtil<RealType>::Floor(P.Z / CellSize));
	}

	/** Convert integer grid coordinates to real-valued point */
	inline TVector<RealType> FromGrid(const FVector3i& GridPoint) const
	{
		return TVector<RealType>(GridPoint.X*CellSize, GridPoint.Y*CellSize, GridPoint.Z*CellSize);
	}
};
typedef TScaleGridIndexer3<float> FScaleGridIndexer3f;
typedef TScaleGridIndexer3<double> FScaleGridIndexer3d;



/**
 * Convert between integer grid coordinates and scaled+translated real-valued coordinates
 */
template<typename RealType>
struct TShiftGridIndexer3
{
	/** Real-valued size of an integer grid cell */
	RealType CellSize;
	/** Real-valued origin of grid, position of integer grid origin */
	TVector<RealType> Origin;

	TShiftGridIndexer3() 
		: CellSize((RealType)1), Origin(TVector<RealType>::Zero())
	{
		ensure(CellSize >= TMathUtil<RealType>::ZeroTolerance);
	}

	TShiftGridIndexer3(const TVector<RealType>& origin, RealType cellSize)
		: CellSize(cellSize), Origin(origin)
	{
		ensure(CellSize >= TMathUtil<RealType>::ZeroTolerance);
	}

	/** Convert real-valued point to integer grid coordinates */
	inline FVector3i ToGrid(const TVector<RealType>& point) const
	{
		return FVector3i(
			(int)TMathUtil<RealType>::Floor((point.X - Origin.X) / CellSize),
			(int)TMathUtil<RealType>::Floor((point.Y - Origin.Y) / CellSize),
			(int)TMathUtil<RealType>::Floor((point.Z - Origin.Z) / CellSize));
	}

	/** Convert integer grid coordinates to real-valued point */
	inline TVector<RealType> FromGrid(const FVector3i& gridpoint) const
	{
		return TVector<RealType>(
			((RealType)gridpoint.X * CellSize) + Origin.X,
			((RealType)gridpoint.Y * CellSize) + Origin.Y,
			((RealType)gridpoint.Z * CellSize) + Origin.Z);
	}

	/** Convert real-valued grid coordinates to real-valued point */
	inline TVector<RealType> FromGrid(const TVector<RealType>& RealGridPoint)  const
	{
		return TVector<RealType>(
			((RealType)RealGridPoint.X * CellSize) + Origin.X,
			((RealType)RealGridPoint.Y * CellSize) + Origin.Y,
			((RealType)RealGridPoint.Z * CellSize) + Origin.Z);
	}
};
typedef TShiftGridIndexer3<float> FShiftGridIndexer3f;
typedef TShiftGridIndexer3<double> FShiftGridIndexer3d;


template<typename RealType>
struct TBoundsGridIndexer3 : public TShiftGridIndexer3<RealType>
{
	using TShiftGridIndexer3<RealType>::CellSize;
	using TShiftGridIndexer3<RealType>::Origin;

	TVector<RealType> BoundsMax;

	TBoundsGridIndexer3(const TAxisAlignedBox3<RealType>& Bounds, RealType CellSize)
		: TShiftGridIndexer3<RealType>(Bounds.Min, CellSize),
		BoundsMax(Bounds.Max)
	{}

	FVector3i GridResolution() const
	{
		const RealType InvCellSize = 1.0 / CellSize;
		const TVector<RealType> Extents = BoundsMax - Origin;
		return CeilInt(Extents * InvCellSize);
	}

	static FVector3i CeilInt(const TVector<RealType>& V)
	{
		return FVector3i{ (int)TMathUtil<RealType>::Ceil(V[0]),
			(int)TMathUtil<RealType>::Ceil(V[1]),
			(int)TMathUtil<RealType>::Ceil(V[2]) };
	}

};

typedef TBoundsGridIndexer3<float> FBoundsGridIndexer3f;
typedef TBoundsGridIndexer3<double> FBoundsGridIndexer3d;

} // end namespace UE::Geometry
} // end namespace UE