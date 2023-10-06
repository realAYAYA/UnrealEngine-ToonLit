// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "HeadlessChaosTestUtility.h"

namespace ChaosTest {

	void SphereRaycast();

	void PlaneRaycast();

	void CylinderRaycast();

	void TaperedCylinderRaycast();

	void CapsuleRaycast();

	void CapsuleRaycastFastLargeDistance();

	void CapsuleRaycastMissWithEndPointOnBounds();

	void TriangleRaycast();

	void TriangleRaycastDenegerated();

	void BoxRaycast();

	void VectorizedAABBRaycast();

	void ScaledRaycast();

	void TransformedRaycast();

	void UnionRaycast();

	void IntersectionRaycast();
}