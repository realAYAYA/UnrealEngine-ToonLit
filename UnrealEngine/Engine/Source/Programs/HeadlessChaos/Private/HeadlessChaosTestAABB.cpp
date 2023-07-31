// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"
#include "Chaos/Matrix.h"
#include "Chaos/Utilities.h"
#include "Chaos/AABB.h"

namespace ChaosTest
{
	using namespace Chaos;

	// A test that was failing for 8-wide ISPC architectures
	GTEST_TEST(AABBTests, TestTransformAABB_ISPC8WideFail)
	{
		const FAABB3 AABB(FVec3(-51.1019897, -51.1019936, -51.1019974), FVec3(51.1019897, 51.1020012, 51.1019974));
		const FRigidTransform3 Transform(FVec3(1.16415322e-10, 5.82076609e-11, -0.0681053028), FRotation3::FromElements(0.579225421, -0.644822955, 0.0343584977, 0.497514546));

		const FAABB3 ExpectedAABB(FVec3(-79.1592407, -84.8343811, -87.7618637), FVec3(79.1592407, 84.8343811, 87.6256561));

		const FAABB3 ResultAABB = AABB.TransformedAABB(Transform);
		EXPECT_NEAR(ResultAABB.Min().X, ExpectedAABB.Min().X, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(ResultAABB.Min().Y, ExpectedAABB.Min().Y, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(ResultAABB.Min().Z, ExpectedAABB.Min().Z, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(ResultAABB.Max().X, ExpectedAABB.Max().X, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(ResultAABB.Max().Y, ExpectedAABB.Max().Y, KINDA_SMALL_NUMBER);
		EXPECT_NEAR(ResultAABB.Max().Z, ExpectedAABB.Max().Z, KINDA_SMALL_NUMBER);
	}


}
