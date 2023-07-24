// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/Types.h"
#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Geo/Sampling/SurfacicSampling.h"

namespace UE::CADKernel
{
class FTopologicalFace;

class FCriteriaGrid
{
protected:

	const FCoordinateGrid& CoordinateGrid;
	int32 TrueUcoorindateCount;
	FSurfacicSampling Grid;

	int32 GetIndex(int32 UIndex, int32 VIndex, bool bIsInternalU, bool bIsInternalV) const
	{
		int32 TrueUIndex = UIndex * 2;
		int32 TrueVIndex = VIndex * 2;
		if (bIsInternalU)
		{
			++TrueUIndex;
		}
		if (bIsInternalV)
		{
			++TrueVIndex;
		}
		return TrueVIndex * TrueUcoorindateCount + TrueUIndex;
	}

	const FPoint& GetPoint(int32 UIndex, int32 VIndex, bool bIsInternalU, bool bIsInternalV, FVector3f* OutNormal = nullptr) const
	{
		int32 Index = GetIndex(UIndex, VIndex, bIsInternalU, bIsInternalV);
		ensureCADKernel(Grid.Points3D.IsValidIndex(Index));

		if (OutNormal)
		{
			*OutNormal = Grid.Normals[Index];
		}
		return Grid.Points3D[Index];
	}

	void Init(const FTopologicalFace& Face);

public:

	FCriteriaGrid(const FTopologicalFace& InFace);

	double GetCoordinate(EIso Iso, int32 ind) const
	{
		return CoordinateGrid[Iso][ind];
	}

	constexpr const TArray<double>& GetCoordinates(EIso Iso) const
	{
		return CoordinateGrid[Iso];
	}

	int32 GetCoordinateCount(EIso Iso) const
	{
		return CoordinateGrid.IsoCount(Iso);
	}

	const TArray<FPoint>& GetGridPoints() const
	{
		return Grid.Points3D;
	}

	const TArray<FVector3f>& GetGridNormals() const
	{
		return Grid.Normals;
	}

	const FPoint& GetPoint(int32 iU, int32 iV, FVector3f* normal = nullptr) const
	{
		return GetPoint(iU, iV, false, false, normal);
	}

	const FPoint& GetIntermediateU(int32 iU, int32 iV, FVector3f* normal = nullptr) const
	{
		return GetPoint(iU, iV, true, false, normal);
	}

	const FPoint& GetIntermediateV(int32 iU, int32 iV, FVector3f* normal = nullptr) const
	{
		return GetPoint(iU, iV, false, true, normal);
	}

	const FPoint& GetIntermediateUV(int32 iU, int32 iV, FVector3f* normal = nullptr) const
	{
		return GetPoint(iU, iV, true, true, normal);
	}

#ifdef DISPLAY_CRITERIA_GRID
	void Display() const;
#endif

};
}

