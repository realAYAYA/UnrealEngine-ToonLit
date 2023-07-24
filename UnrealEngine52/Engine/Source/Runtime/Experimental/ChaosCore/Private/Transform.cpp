// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Transform.h"
#include "Chaos/Real.h"

namespace Chaos
{
	PMatrix<FRealSingle, 4, 4> TRigidTransform<FRealSingle, 3>::operator*(const PMatrix<FRealSingle, 4, 4>& Matrix) const
	{
		// LWC_TODO: Perf pessimization
		return ToMatrixNoScale() * static_cast<const UE::Math::TMatrix<FRealSingle>&>(Matrix);
	}

	PMatrix<FRealDouble, 4, 4> TRigidTransform<FRealDouble, 3>::operator*(const PMatrix<FRealDouble, 4, 4>& Matrix) const
	{
		// LWC_TODO: Perf pessimization
		return ToMatrixNoScale() * static_cast<const UE::Math::TMatrix<FRealDouble>&>(Matrix);
	}
}
