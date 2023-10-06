// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaos.h"
#include "HeadlessChaosTestEvolution.h"
#include "Chaos/Collision/SimSweep.h"

namespace ChaosTest
{
	using namespace Chaos;

	GTEST_TEST(SimOverlapTests, TestBoxBoxOverlap)
	{
		const FVec3 FloorSize = FVec3(10000, 10000, 100);
		const FVec3 BoxPos = FVec3(0, 0, 200);
		const FRotation3 BoxRot = FRotation3::FromIdentity();
		const FVec3 BoxSize = FVec3(100);
		const FReal BoxMass = 100;

		FEvolutionTest Test;

		FGeometryParticleHandle* Floor = Test.AddParticleFloor(FloorSize);
		FPBDRigidParticleHandle* Box = Test.AddParticleBox(BoxPos, BoxRot, BoxSize, BoxMass);

		Test.Tick();

		TArray<Private::FSimOverlapParticleShape> Overlaps;
		const FAABB3 QueryBounds = FAABB3(FVec3(-1000), FVec3(1000));
		Private::SimOverlapBoundsAll(Test.GetEvolution().GetSpatialAcceleration(), QueryBounds, Overlaps);

		EXPECT_EQ(Overlaps.Num(), 2);
		if (Overlaps.Num() == 2)
		{
			EXPECT_TRUE((Overlaps[0].HitParticle == Floor) || (Overlaps[1].HitParticle == Floor));
			EXPECT_TRUE((Overlaps[0].HitParticle == Box) || (Overlaps[1].HitParticle == Box));
		}
	}
}