// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGVolumeData.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGIntersectionDataTest, FPCGTestBaseClass, "pcg.tests.Intersection.Data", PCGTestsCommon::TestFlags)

bool FPCGIntersectionDataTest::RunTest(const FString& Parameters)
{
	UPCGPointData* InsidePoint = PCGTestsCommon::CreatePointData();
	check(InsidePoint->GetPoints().Num() == 1);

	UPCGPointData* OutsidePoint = PCGTestsCommon::CreatePointData(FVector::OneVector * 10000);
	check(OutsidePoint->GetPoints().Num() == 1);

	UPCGVolumeData* Volume = PCGTestsCommon::CreateVolumeData(FBox::BuildAABB(FVector::ZeroVector, FVector::OneVector * 100));

	// Create intersections
	UPCGIntersectionData* InsideVolume = InsidePoint->IntersectWith(Volume);
	UPCGIntersectionData* VolumeInside = Volume->IntersectWith(InsidePoint);
	UPCGIntersectionData* OutsideVolume = OutsidePoint->IntersectWith(Volume);
	UPCGIntersectionData* VolumeOutside = Volume->IntersectWith(OutsidePoint);

	auto ValidateInsideIntersection = [this, InsidePoint](UPCGIntersectionData* Intersection)
	{
		// Basic data validations
		TestTrue("Valid intersection", Intersection != nullptr);

		if (!Intersection)
		{
			return;
		}

		TestTrue("Valid dimension", Intersection->GetDimension() == 0);
		TestTrue("Valid bounds", Intersection->GetBounds() == InsidePoint->GetBounds());
		TestTrue("Valid strict bounds", Intersection->GetStrictBounds() == InsidePoint->GetStrictBounds());

		// Validate sample point		
		const FPCGPoint& Point = InsidePoint->GetPoints()[0];

		FPCGPoint SampledPoint;
		TestTrue("Successful point sampling", Intersection->SamplePoint(Point.Transform, Point.GetLocalBounds(), SampledPoint, nullptr));
		TestTrue("Correct sampled point", PCGTestsCommon::PointsAreIdentical(Point, SampledPoint));

		// Validate create point data
		const UPCGPointData* OutputPointData = Intersection->ToPointData(nullptr);
		TestTrue("Successful ToPoint", OutputPointData != nullptr);
		
		if (OutputPointData)
		{
			TestTrue("Valid number of points in ToPoint", OutputPointData->GetPoints().Num() == 1);
			if (OutputPointData->GetPoints().Num() == 1)
			{
				TestTrue("Correct point in ToPoint", PCGTestsCommon::PointsAreIdentical(Point, OutputPointData->GetPoints()[0]));
			}
		}
	};

	ValidateInsideIntersection(InsideVolume);
	ValidateInsideIntersection(VolumeInside);

	auto ValidateOutsideIntersection = [this, OutsidePoint](UPCGIntersectionData* Intersection)
	{
		TestTrue("Valid intersection", Intersection != nullptr);

		if (!Intersection)
		{
			return;
		}

		TestTrue("Valid dimension", Intersection->GetDimension() == 0);
		TestTrue("Null bounds", !Intersection->GetBounds().IsValid);
		TestTrue("Null strict bounds", !Intersection->GetStrictBounds().IsValid);

		// Validate that we're not able to sample a point
		const FPCGPoint& Point = OutsidePoint->GetPoints()[0];

		FPCGPoint SampledPoint;
		TestTrue("Unsuccessful point sampling", !Intersection->SamplePoint(Point.Transform, Point.GetLocalBounds(), SampledPoint, nullptr));

		// Validate empty point data
		const UPCGPointData* OutputPointData = Intersection->ToPointData(nullptr);
		TestTrue("Successful ToPoint", OutputPointData != nullptr);

		if (OutputPointData)
		{
			TestTrue("Empty point data", OutputPointData->GetPoints().Num() == 0);
		}
	};

	ValidateOutsideIntersection(OutsideVolume);
	ValidateOutsideIntersection(VolumeOutside);

	return true;
}

//TOODs:
// Test with one/two data that do not have a trivial transformation (e.g. projection, surfaces, ...)