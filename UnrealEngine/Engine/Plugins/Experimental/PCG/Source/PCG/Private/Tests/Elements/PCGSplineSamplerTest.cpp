// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"
#include "Tests/Determinism/PCGDeterminismTestsCommon.h"
#include "Elements/PCGSplineSampler.h"

#if WITH_EDITOR

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGSplineSamplerTest, FPCGTestBaseClass, "pcg.tests.SplineSampler.PointInPolygon", PCGTestsCommon::TestFlags)

bool FPCGSplineSamplerTest::RunTest(const FString& Parameters)
{	
	/* Clock-wise torch shape, to test edge cases
	 * |\/ \/|
	 * |_   _|
	 *   |_|
	 */

	const TArray<FVector2D> PolygonPoints =
	{
		FVector2D(0, 0),
		FVector2D(0, 1),
		FVector2D(0, 2),
		FVector2D(1, 1),
		FVector2D(1.5, 2),
		FVector2D(1.5, 2), // Some duplicate points
		FVector2D(2, 1),
		FVector2D(3, 2),
		FVector2D(3, 1),
		FVector2D(3, 0),
		FVector2D(3, 0),
		FVector2D(2.5, 0),
		FVector2D(2, 0),
		FVector2D(2, -1),
		FVector2D(2, -1),
		FVector2D(1, -1),
		FVector2D(1, 0),
		FVector2D(0.5, 0),
	};

	constexpr FVector::FReal MaxDistance = 1000.f;
	bool bTestPassed = true;

	bTestPassed &= TestTrue("Interior point lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(0.5, 0.5), MaxDistance));
	bTestPassed &= TestTrue("Exterior point lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(10.5, 0.5), MaxDistance));

	// Y = -1
	{
		bTestPassed &= TestTrue("(1-e, -1) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(1 - UE_KINDA_SMALL_NUMBER, -1), MaxDistance));
		bTestPassed &= TestTrue("(2+e, -1) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(2 + UE_KINDA_SMALL_NUMBER, -1), MaxDistance));
	}

	// Y = 0
	{
		bTestPassed &= TestTrue("(0-e, 0) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(0 - UE_KINDA_SMALL_NUMBER, 0), MaxDistance));
		bTestPassed &= TestTrue("(1.5, 0) lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(1.5, 0), MaxDistance));
		bTestPassed &= TestTrue("(3+e, 0) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(3 + UE_KINDA_SMALL_NUMBER, 0), MaxDistance));
	}

	// Y = 1
	{
		// Test around (0, 1)
		bTestPassed &= TestTrue("(0-e, 1) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(0 - UE_KINDA_SMALL_NUMBER, 1), MaxDistance));
		bTestPassed &= TestTrue("(0+e, 1) lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(0 + UE_KINDA_SMALL_NUMBER, 1), MaxDistance));

		// Test around (1, 1)
		bTestPassed &= TestTrue("(1-e, 1) lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(1 - UE_KINDA_SMALL_NUMBER, 1), MaxDistance));
		bTestPassed &= TestTrue("(1+e, 1) lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(1 + UE_KINDA_SMALL_NUMBER, 1), MaxDistance));

		// Test around (2, 1)
		bTestPassed &= TestTrue("(2-e, 1) lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(2 - UE_KINDA_SMALL_NUMBER, 1), MaxDistance));
		bTestPassed &= TestTrue("(2+e, 1) lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(2 + UE_KINDA_SMALL_NUMBER, 1), MaxDistance));

		// Test around (3, 1)
		bTestPassed &= TestTrue("(3-e, 1) lies inside polygon", PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(3 - UE_KINDA_SMALL_NUMBER, 1), MaxDistance));
		bTestPassed &= TestTrue("(3+e, 1) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(3 + UE_KINDA_SMALL_NUMBER, 1), MaxDistance));
	}

	// Y = 2
	{
		// Test around (0, 2)
		bTestPassed &= TestTrue("(0-e, 2) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(0 - UE_KINDA_SMALL_NUMBER, 2), MaxDistance));
		bTestPassed &= TestTrue("(0+e, 2) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(0 + UE_KINDA_SMALL_NUMBER, 2), MaxDistance));

		// Test around (1.5, 2)
		bTestPassed &= TestTrue("(1.5-e, 2) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(1.5 - UE_KINDA_SMALL_NUMBER, 2), MaxDistance));
		bTestPassed &= TestTrue("(1.5+e, 2) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(1.5 + UE_KINDA_SMALL_NUMBER, 2), MaxDistance));

		// Test around (3, 2)
		bTestPassed &= TestTrue("(3-e, 2) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(3 - UE_KINDA_SMALL_NUMBER, 2), MaxDistance));
		bTestPassed &= TestTrue("(3+e, 2) lies outside polygon", !PCGSplineSamplerHelpers::PointInsidePolygon2D(PolygonPoints, FVector2D(3 + UE_KINDA_SMALL_NUMBER, 2), MaxDistance));
	}

	return bTestPassed;
}

#endif // WITH_EDITOR
