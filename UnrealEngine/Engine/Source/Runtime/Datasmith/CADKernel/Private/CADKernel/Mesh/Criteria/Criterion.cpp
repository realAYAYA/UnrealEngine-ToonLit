// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Mesh/Criteria/Criterion.h"

#include "CADKernel/Mesh/Criteria/AngleCriterion.h"
#include "CADKernel/Mesh/Criteria/CriteriaGrid.h"
#include "CADKernel/Mesh/Criteria/CurvatureCriterion.h"
#include "CADKernel/Mesh/Criteria/SagCriterion.h"
#include "CADKernel/Mesh/Criteria/SizeCriterion.h"

#include "CADKernel/Core/KernelParameters.h"
#include "CADKernel/Geo/GeoPoint.h"
#include "CADKernel/Topo/TopologicalEdge.h"

namespace UE::CADKernel
{

void FCriterion::ApplyOnEdgeParameters(FTopologicalEdge& Edge, const TArray<double>& Coordinates, const TArray<FCurvePoint>& Points) const
{
	TArray<double>& DeltaUMaxs = Edge.GetDeltaUMaxs();

	double NumericPrecision = Edge.GetTolerance3D();
	NumericPrecision /= 10;

	if (Edge.Length() <= NumericPrecision)
	{
		return;
	}

	int32 PreviousIndex = 0;
	for (int32 Index = 1; Index < Coordinates.Num(); Index++)
	{
		int32 MiddleIndex = PreviousIndex + Index;
		double ChordLength;
		double Sag = EvaluateSag(Points[2 * PreviousIndex].Point, Points[2 * Index].Point, Points[MiddleIndex].Point, ChordLength);

		double AiAm = Points[2 * PreviousIndex].Point.Distance(Points[MiddleIndex].Point);
		double AmAi2 = Points[2 * Index].Point.Distance(Points[MiddleIndex].Point);
		double AiAi2 = Points[2 * PreviousIndex].Point.Distance(Points[2 * Index].Point);

		double DeltaU = Coordinates[Index] - Coordinates[PreviousIndex];

		if (ChordLength > NumericPrecision && Sag > DOUBLE_SMALL_NUMBER)
		{
			double DeltaUMax = ComputeDeltaU(ChordLength, DeltaU, Sag);
			if (DeltaUMax < DeltaUMaxs[PreviousIndex]) DeltaUMaxs[PreviousIndex] = DeltaUMax;
		}

		Sag = EvaluateSag(Points[2 * Index].Point, Points[2 * PreviousIndex].Point, Points[MiddleIndex].Point, ChordLength);
		if (ChordLength > NumericPrecision && Sag > DOUBLE_SMALL_NUMBER)
		{
			double DeltaUMax = ComputeDeltaU(ChordLength, DeltaU, Sag);
			if (DeltaUMax < DeltaUMaxs[PreviousIndex])
			{
				DeltaUMaxs[PreviousIndex] = DeltaUMax;
			}
		}
		PreviousIndex = Index;
	}
}

TSharedPtr<FCriterion> FCriterion::Deserialize(FCADKernelArchive& Archive)
{
	ECriterion CriterionType = ECriterion::None;
	Archive << CriterionType;

	TSharedPtr<FEntity> Entity;
	switch (CriterionType)
	{
	case ECriterion::MinSize:
		return FEntity::MakeShared<FMinSizeCriterion>(Archive);
	case ECriterion::MaxSize:
		return FEntity::MakeShared<FMaxSizeCriterion>(Archive);
	case ECriterion::CADCurvature:
		return FEntity::MakeShared<FCurvatureCriterion>(Archive);
	case ECriterion::Sag:
		return FEntity::MakeShared<FSagCriterion>(Archive);
	case ECriterion::Angle:
		return FEntity::MakeShared<FAngleCriterion>(Archive);
	default:
		return TSharedPtr<FCriterion>();
	}
}

TSharedPtr<FCriterion> FCriterion::CreateCriterion(ECriterion Type, double Value)
{
	switch (Type)
	{
	case ECriterion::MinSize:
		return FEntity::MakeShared<FMinSizeCriterion>(Value);
	case ECriterion::MaxSize:
		return FEntity::MakeShared<FMaxSizeCriterion>(Value);
	case ECriterion::Sag:
		return FEntity::MakeShared<FSagCriterion>(Value);
	case ECriterion::Angle:
		return FEntity::MakeShared<FAngleCriterion>(Value);
	case ECriterion::CADCurvature:
		return FEntity::MakeShared<FCurvatureCriterion>();
	default:
		ERROR_FUNCTION_CALL_NOT_EXPECTED;
		return TSharedPtr<FCriterion>();
	}
}

double FCriterion::DefaultValue(ECriterion Type)
{
	switch (Type)
	{
	case ECriterion::MinSize:
		return FSizeCriterion::DefaultValue(Type);
	case ECriterion::MaxSize:
		return FSizeCriterion::DefaultValue(Type);
	case ECriterion::Sag:
		return FSagCriterion::DefaultValue();
	case ECriterion::Angle:
		return FAngleCriterion::DefaultValue();
	default:
		ERROR_FUNCTION_CALL_NOT_EXPECTED;
		return 0.0;
	}
}

#ifdef CADKERNEL_DEV
FInfoEntity& FCriterion::GetInfo(FInfoEntity& Info) const
{
	FEntity::GetInfo(Info)
		.Add(TEXT("CriterionType"), CriterionTypeNames[(uint8)GetCriterionType()])
		.Add(TEXT("Value"), Value());
	return Info;
}
#endif

const TCHAR* CriterionTypeNames[] = {
	TEXT("Size"),
	TEXT("MaxSize"),
	TEXT("MinSize"),
	TEXT("Angle"),
	TEXT("Chord Error"),
	TEXT("CAD Curvature"),
	TEXT("")
};

// Defined for Python purpose
const char* CriterionTypeConstNames[] = {
	"CRITERION_SIZE",
	"CRITERION_MAX_SIZE",
	"CRITERION_MIN_SIZE",
	"CRITERION_ANGLE",
	"CRITERION_SAG",
	"CRITERION_CAD_CURVATURE",
	nullptr
};

// Defined for Python purpose
const char* CriterionTypeConstDescHelp[] = {
	": Size of elements (only for check)",
	": maximum size of elements",
	": minimum size of elements",
	": angle between the normal of elements",
	": distance between elements and CAD surfaces/curves",
	": CAD curvature (only for check)",
	nullptr
};
}