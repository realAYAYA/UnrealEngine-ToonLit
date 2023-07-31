// Copyright Epic Games, Inc. All Rights Reserved.
#include "CADKernel/Geo/Curves/HyperbolaCurve.h"

namespace UE::CADKernel
{

TSharedPtr<FEntityGeom> FHyperbolaCurve::ApplyMatrix(const FMatrixH& InMatrix) const
{
	FMatrixH NewMatrix = InMatrix * Matrix;
	return FEntity::MakeShared<FHyperbolaCurve>(NewMatrix, SemiMajorAxis, SemiImaginaryAxis, Boundary);
}

void FHyperbolaCurve::Offset(const FPoint& OffsetDirection)
{
	FMatrixH Offset = FMatrixH::MakeTranslationMatrix(OffsetDirection);
	Matrix *= Offset;
}

#ifdef CADKERNEL_DEV
FInfoEntity& FHyperbolaCurve::GetInfo(FInfoEntity& Info) const
{
	return FCurve::GetInfo(Info).Add(TEXT("Matrix"), Matrix)
		.Add(TEXT("semi axis"), SemiMajorAxis)
		.Add(TEXT("semi imag axis"), SemiImaginaryAxis);
}
#endif

}
