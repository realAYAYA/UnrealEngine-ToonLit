// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Mesh/Criteria/CriteriaGrid.h"

#include "CADKernel/Mesh/Criteria/Criterion.h"

#include "CADKernel/Topo/TopologicalFace.h"
#include "CADKernel/Geo/Surfaces/Surface.h"
#include "CADKernel/UI/Display.h"

#ifdef CADKERNEL_DEV
#include "CADKernel/Mesh/Meshers/IsoTriangulator/DefineForDebug.h"
#endif

namespace UE::CADKernel
{

void FCriteriaGrid::Init(const FTopologicalFace& Face)
{
	TFunction<void(const TArray<double>&, TArray<double>&)> ComputeMiddlePointsCoordinates = [](const TArray<double>& Tab, TArray<double>& Tab2)
	{
		const int32 TabSize = Tab.Num();
		ensure(TabSize);

		Tab2.SetNum(TabSize * 2 - 1);

		Tab2[0] = Tab[0];
		for (int32 Index = 1; Index < Tab.Num(); Index++)
		{
			Tab2[2 * Index - 1] = (Tab[Index - 1] + Tab[Index]) / 2.0;
			Tab2[2 * Index] = Tab[Index];
		}
	};

	FCoordinateGrid CoordinateGrid2;
	ComputeMiddlePointsCoordinates(Face.GetCrossingPointCoordinates(EIso::IsoU), CoordinateGrid2[EIso::IsoU]);
	ComputeMiddlePointsCoordinates(Face.GetCrossingPointCoordinates(EIso::IsoV), CoordinateGrid2[EIso::IsoV]);

	Face.GetCarrierSurface()->EvaluatePointGrid(CoordinateGrid2, Grid, true);
	TrueUcoorindateCount = CoordinateGrid2[EIso::IsoU].Num();
}

FCriteriaGrid::FCriteriaGrid(const FTopologicalFace& InFace)
	: CoordinateGrid(InFace.GetCrossingPointCoordinates())
{
	Init(InFace);
#ifdef DISPLAY_CRITERIA_GRID
	Display();
#endif
}

#ifdef DISPLAY_CRITERIA_GRID
void FCriteriaGrid::Display() const
{
	F3DDebugSession _(TEXT("Grid"));

	{
		F3DDebugSession A(TEXT("CriteriaGrid Point 3d"));
		for (int32 VIndex = 0; VIndex < GetCoordinateCount(EIso::IsoV); ++VIndex)
		{
			for (int32 UIndex = 0; UIndex < GetCoordinateCount(EIso::IsoU); ++UIndex)
			{
				UE::CADKernel::DisplayPoint(GetPoint(UIndex, VIndex), UIndex);
			}
		}
	}

	{
		F3DDebugSession A(TEXT("CriteriaGrid IntermediateU"));
		for (int32 VIndex = 0; VIndex < GetCoordinateCount(EIso::IsoV); ++VIndex)
		{
			for (int32 UIndex = 0; UIndex < GetCoordinateCount(EIso::IsoU) - 1; ++UIndex)
			{
				UE::CADKernel::DisplayPoint(GetIntermediateU(UIndex, VIndex), EVisuProperty::PurplePoint);
			}
		}
	}

	{
		F3DDebugSession A(TEXT("CriteriaGrid IntermediateV"));
		for (int32 VIndex = 0; VIndex < GetCoordinateCount(EIso::IsoV) - 1; ++VIndex)
		{
			for (int32 UIndex = 0; UIndex < GetCoordinateCount(EIso::IsoU); ++UIndex)
			{
				UE::CADKernel::DisplayPoint(GetIntermediateV(UIndex, VIndex), EVisuProperty::PurplePoint);
			}
		}
	}

	{
		F3DDebugSession A(TEXT("CriteriaGrid IntermediateUV"));
		for (int32 VIndex = 0; VIndex < GetCoordinateCount(EIso::IsoV) - 1; ++VIndex)
		{
			for (int32 UIndex = 0; UIndex < GetCoordinateCount(EIso::IsoU) - 1; ++UIndex)
			{
				UE::CADKernel::DisplayPoint(GetIntermediateUV(UIndex, VIndex), EVisuProperty::PurplePoint);
			}
		}
	}

	{
		F3DDebugSession A(TEXT("CriteriaGrid Point 2D"));
		for (int32 VIndex = 0; VIndex < GetCoordinateCount(EIso::IsoV); ++VIndex)
		{
			for (int32 UIndex = 0; UIndex < GetCoordinateCount(EIso::IsoU); ++UIndex)
			{
				UE::CADKernel::DisplayPoint(FPoint2D(GetCoordinate(EIso::IsoU, UIndex), GetCoordinate(EIso::IsoV, VIndex)));
			}
		}
	}

	{
		F3DDebugSession A(TEXT("CriteriaGrid Point 2D Intermediate"));
		for (int32 VIndex = 0; VIndex < GetCoordinateCount(EIso::IsoV); ++VIndex)
		{
			for (int32 UIndex = 1; UIndex < GetCoordinateCount(EIso::IsoU); ++UIndex)
			{
				UE::CADKernel::DisplayPoint(FPoint2D((GetCoordinate(EIso::IsoU, UIndex) + GetCoordinate(EIso::IsoU, UIndex - 1)) * 0.5, GetCoordinate(EIso::IsoV, VIndex)), EVisuProperty::PurplePoint);
			}
		}
		for (int32 VIndex = 1; VIndex < GetCoordinateCount(EIso::IsoV); ++VIndex)
		{
			for (int32 UIndex = 0; UIndex < GetCoordinateCount(EIso::IsoU); ++UIndex)
			{
				UE::CADKernel::DisplayPoint(FPoint2D(GetCoordinate(EIso::IsoU, UIndex), (GetCoordinate(EIso::IsoV, VIndex) + GetCoordinate(EIso::IsoV, VIndex - 1)) * 0.5), EVisuProperty::PurplePoint);
			}
		}
		for (int32 VIndex = 1; VIndex < GetCoordinateCount(EIso::IsoV); ++VIndex)
		{
			for (int32 UIndex = 1; UIndex < GetCoordinateCount(EIso::IsoU); ++UIndex)
			{
				UE::CADKernel::DisplayPoint(FPoint2D((GetCoordinate(EIso::IsoU, UIndex) + GetCoordinate(EIso::IsoU, UIndex - 1)) * 0.5, (GetCoordinate(EIso::IsoV, VIndex) + GetCoordinate(EIso::IsoV, VIndex - 1)) * 0.5), EVisuProperty::PurplePoint);
			}
		}
	}

	Wait();
}
#endif

} // namespace UE::CADKernel

