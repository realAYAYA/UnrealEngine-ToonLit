// Copyright Epic Games, Inc. All Rights Reserved.

#include "CorrectivesRBFInterpolator.h"

// Just to be sure, also added this in Eigen.Build.cs
#ifndef EIGEN_MPL2_ONLY
#define EIGEN_MPL2_ONLY
#endif

#if defined(_MSC_VER) && USING_CODE_ANALYSIS
#pragma warning(push)
#pragma warning(disable:6294) // Ill-defined for-loop:  initial condition does not satisfy test.  Loop body not executed.
#endif
PRAGMA_DEFAULT_VISIBILITY_START
THIRD_PARTY_INCLUDES_START
#include <Eigen/LU>
THIRD_PARTY_INCLUDES_END
PRAGMA_DEFAULT_VISIBILITY_END
#if defined(_MSC_VER) && USING_CODE_ANALYSIS
#pragma warning(pop)
#endif


bool FCorrectivesRBFInterpolatorBase::SetFullKernel(
	const TArrayView<float>& FullKernel,
	int32 Size
)
{
	using namespace Eigen;

	MatrixXf FullKernelMatrix;

	// Construct the full kernel from the upper half calculated in the templated
	// portion.
	FullKernelMatrix = MatrixXf::Identity(Size, Size);

	for (int32 c = 0, i = 0; i < Size; i++)
	{
		for (int32 j = 0; j < Size; j++)
		{
			FullKernelMatrix(i, j) = FullKernel[c++];
		}
	}

	// If the matrix is non-invertible, return now and bail out.
	float Det = FullKernelMatrix.determinant();
	if (FMath::IsNearlyZero(Det))
		return false;

	Coeffs.SetNumZeroed(Size * Size);
	Map<MatrixXf> Result(Coeffs.GetData(), Size, Size);
	Result = FullKernelMatrix.inverse();	
	return true;
}