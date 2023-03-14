// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HeadlessChaosTestUtility.h"
#include "Modules/ModuleManager.h"

namespace ChaosTest {

	/**/
	void SimplexLine();

	void SimplexTriangle();

	void SimplexTetrahedron();

	void GJKSphereSphereTest();

	void GJKSphereBoxTest();

	void GJKSphereCapsuleTest();

	void GJKSphereConvexTest();

	void GJKSphereScaledSphereTest();

	void GJKSphereSphereSweep();

	void GJKSphereBoxSweep();

	void GJKSphereCapsuleSweep();

	void GJKSphereConvexSweep();

	void GJKSphereScaledSphereSweep();

	void GJKSphereTransformedSphereSweep();

	void GJKBoxCapsuleSweep();

	void GJKBoxBoxSweep();

	void GJKCapsuleConvexInitialOverlapSweep();

	void GJKLargeDistanceCapsuleSweep();
}