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

const FPoint& FCriteriaGrid::GetPoint(int32 UIndex, int32 VIndex, bool bIsInternalU, bool bIsInternalV, FVector3f* OutNormal) const
{
	int32 Index = GetIndex(UIndex, VIndex, bIsInternalU, bIsInternalV);
	ensureCADKernel(Grid.Points3D.IsValidIndex(Index));

	if (OutNormal)
	{
		*OutNormal = Grid.Normals[Index];
	}
	return Grid.Points3D[Index];
}

void FCriteriaGrid::Init()
{
	TFunction<void(const TArray<double>&, TArray<double>&)> ComputeMiddlePointsCoordinates = [](const TArray<double>& Tab, TArray<double>& Tab2)
	{
		Tab2.SetNum(Tab.Num() * 2 - 1);
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

FCriteriaGrid::FCriteriaGrid(FTopologicalFace& InFace)
	: Face(InFace)
	, CoordinateGrid(InFace.GetCrossingPointCoordinates())
{
	Face.Presample();

	constexpr int32 MaxGrid = 1000000;
	if (CoordinateGrid[EIso::IsoU].Num() * CoordinateGrid[EIso::IsoV].Num() > MaxGrid)
	{
		Face.SetDeleted();
		return;
	}

	Face.InitDeltaUs();
	Init();
#ifdef DISPLAY_CRITERIA_GRID
	Display();
#endif
}

void FCriteriaGrid::ApplyCriteria(const TArray<TSharedPtr<FCriterion>>& Criteria) const
{
	TArray<double>& DeltaUMaxArray = Face.GetCrossingPointDeltaMaxs(EIso::IsoU);
	TArray<double>& DeltaUMiniArray = Face.GetCrossingPointDeltaMins(EIso::IsoU);
	TArray<double>& DeltaVMaxArray = Face.GetCrossingPointDeltaMaxs(EIso::IsoV);
	TArray<double>& DeltaVMinArray = Face.GetCrossingPointDeltaMins(EIso::IsoV);
	FSurfaceCurvature& SurfaceCurvature = Face.GetCurvatures();

	for (int32 IndexV = 0; IndexV < GetCoordinateCount(EIso::IsoV) - 1; ++IndexV)
	{
		for (int32 IndexU = 0; IndexU < GetCoordinateCount(EIso::IsoU) - 1; ++IndexU)
		{
			const FPoint& Point_U0_V0 = GetPoint(IndexU, IndexV);
			const FPoint& Point_U1_V1 = GetPoint(IndexU + 1, IndexV + 1);
			const FPoint& Point_Um_V0 = GetIntermediateU(IndexU, IndexV);
			const FPoint& Point_Um_V1 = GetIntermediateU(IndexU, IndexV+1);
			const FPoint& Point_U0_Vm = GetIntermediateV(IndexU, IndexV);
			const FPoint& Point_U1_Vm = GetIntermediateV(IndexU+1, IndexV);
			const FPoint& Point_Um_Vm = GetIntermediateUV(IndexU, IndexV);

			// Evaluate Sag
			double LengthU;
			double SagU = FCriterion::EvaluateSag(Point_U0_Vm, Point_U1_Vm, Point_Um_Vm, LengthU);
			double LengthV;
			double SagV = FCriterion::EvaluateSag(Point_Um_V0, Point_Um_V1, Point_Um_Vm, LengthV);
			double LengthUV;
			double SagUV = FCriterion::EvaluateSag(Point_U0_V0, Point_U1_V1, Point_Um_Vm, LengthUV);

			for (const TSharedPtr<FCriterion>& Criterion : Criteria)
			{
				Criterion->UpdateDelta((GetCoordinate(EIso::IsoU, IndexU + 1) - GetCoordinate(EIso::IsoU, IndexU)), SagU, SagUV, SagV, LengthU, LengthUV, DeltaUMaxArray[IndexU], DeltaUMiniArray[IndexU], SurfaceCurvature[EIso::IsoU]);
				Criterion->UpdateDelta((GetCoordinate(EIso::IsoV, IndexV + 1) - GetCoordinate(EIso::IsoV, IndexV)), SagV, SagUV, SagU, LengthV, LengthUV, DeltaVMaxArray[IndexV], DeltaVMinArray[IndexV], SurfaceCurvature[EIso::IsoV]);
			}
		}
	}

	// Delta of the extremities are smooth to avoid big disparity 
	if (DeltaUMaxArray.Num() > 2)
	{
		DeltaUMaxArray[0] = (DeltaUMaxArray[0] + DeltaUMaxArray[1] * 2) * AThird;
		DeltaUMaxArray.Last() = (DeltaUMaxArray.Last() + DeltaUMaxArray[DeltaUMaxArray.Num() - 2] * 2) * AThird;
	}

	if (DeltaVMaxArray.Num() > 2)
	{
		DeltaVMaxArray[0] = (DeltaVMaxArray[0] + DeltaVMaxArray[1] * 2) * AThird;
		DeltaVMaxArray.Last() = (DeltaVMaxArray.Last() + DeltaVMaxArray[DeltaVMaxArray.Num() - 2] * 2) * AThird;
	}
}

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
		F3DDebugSession A(TEXT("Loop 3D"));
		for (const TSharedPtr<FTopologicalLoop>& Loop : Face.GetLoops())
		{
			UE::CADKernel::Display(*Loop);
		}
	}

	{
		F3DDebugSession A(TEXT("Loop 2D"));
		for (const TSharedPtr<FTopologicalLoop>& Loop : Face.GetLoops())
		{
			UE::CADKernel::Display2D(*Loop);
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

} // namespace UE::CADKernel

