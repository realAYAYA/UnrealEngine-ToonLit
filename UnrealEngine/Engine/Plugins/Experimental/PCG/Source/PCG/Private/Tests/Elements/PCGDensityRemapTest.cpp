// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"
#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGHelpers.h"

#include "Data/PCGPointData.h"
#include "Elements/PCGDensityRemapElement.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDensityRemapTest, FPCGTestBaseClass, "pcg.tests.DensityRemap.Basic", PCGTestsCommon::TestFlags)

bool FPCGDensityRemapTest::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGDensityRemapSettings>(TestData);
	UPCGDensityRemapSettings* Settings = CastChecked<UPCGDensityRemapSettings>(TestData.Settings);
	FPCGElementPtr DensityRemapElement = TestData.Settings->GetElement();

	TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreateEmptyPointData();
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

	FRandomStream RandomSource(TestData.Seed);
	const int PointCount = 6;
	for (int I = 0; I < PointCount; ++I)
	{
		FPCGPoint& Point = Points.Emplace_GetRef(FTransform(), 1, I);
		Point.Density = static_cast<float>(I) / (PointCount - 1);
	}

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TaggedData.Data = PointData;

	auto ValidateDensityRemap = [this, &TestData, DensityRemapElement, Settings](TArray<float> CorrectDensities) -> bool
	{
		TUniquePtr<FPCGContext> Context = MakeUnique<FPCGContext>(*DensityRemapElement->Initialize(TestData.InputData, TestData.TestPCGComponent, nullptr));
		Context->NumAvailableTasks = 1;

		while (!DensityRemapElement->Execute(Context.Get()))
		{}

		const TArray<FPCGTaggedData>& Inputs = Context->InputData.GetInputs();
		const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();
		
		if (!TestEqual("Valid number of outputs", Inputs.Num(), Outputs.Num()))
		{
			return false;
		}
	
		bool bTestPassed = true;

		for (int DataIndex = 0; DataIndex < Inputs.Num(); ++DataIndex)
		{
			const FPCGTaggedData& Input = Inputs[DataIndex];
			const FPCGTaggedData& Output = Outputs[DataIndex];

			const UPCGSpatialData* InSpatialData = Cast<UPCGSpatialData>(Input.Data);
			check(InSpatialData);

			const UPCGPointData* InPointData = InSpatialData->ToPointData(Context.Get());
			check(InPointData);

			const UPCGSpatialData* OutSpatialData = Cast<UPCGSpatialData>(Output.Data);

			if (!TestNotNull("Valid output SpatialData", OutSpatialData))
			{
				bTestPassed = false;
				continue;
			}

			const UPCGPointData* OutPointData = OutSpatialData->ToPointData(Context.Get());

			if (!TestNotNull("Valid output PointData", OutPointData))
			{
				bTestPassed = false;
				continue;
			}

			const TArray<FPCGPoint>& InPoints = InPointData->GetPoints();
			const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

			if (!TestEqual("Input and output point counts match", InPoints.Num(), OutPoints.Num()))
			{ 
				bTestPassed = false;
				continue;
			}

			for (int PointIndex = 0; PointIndex < InPoints.Num(); ++PointIndex)
			{
				bTestPassed &= TestEqual("Correct density", OutPoints[PointIndex].Density, CorrectDensities[PointIndex]);
			}
		}

		return bTestPassed;
	};

	bool bTestPassed = true;

	// Test [0-1] -> [0-1]
	{
		Settings->InRangeMin = 0.f;
		Settings->InRangeMax = 1.f;
		Settings->OutRangeMin = 0.f;
		Settings->OutRangeMax = 1.f;
		bTestPassed &= TestTrue("Input and Output are identical when InRange and OutRange are identical", ValidateDensityRemap({ 0.f, 0.2f, 0.4f, 0.6f, 0.8f, 1.f }));
	}

	// Test [0-0.4] -> [0-1]
	{
		Settings->InRangeMin = 0.f;
		Settings->InRangeMax = 0.4f;
		Settings->OutRangeMin = 0.f;
		Settings->OutRangeMax = 1.f;
		bTestPassed &= TestTrue("Valid densities for partial InRange", ValidateDensityRemap({ 0.f, 0.5f, 1.f, 0.6f, 0.8f, 1.f }));

		Settings->InRangeMin = 0.4f;
		Settings->InRangeMax = 0.f;
		Settings->OutRangeMin = 1.f;
		Settings->OutRangeMax = 0.f;
		bTestPassed &= TestTrue("Inverting ranges does not effect output", ValidateDensityRemap({ 0.f, 0.5f, 1.f, 0.6f, 0.8f, 1.f }));
	}

	// Test [0-0.4] -> [0.5-1]
	{
		Settings->InRangeMin = 0.4f;
		Settings->InRangeMax = 1.f;
		Settings->OutRangeMin = 0.5f;
		Settings->OutRangeMax = 1.f;
		bTestPassed &= TestTrue("Valid densities for partial OutRange", ValidateDensityRemap({ 0.f, 0.2f, 3.f / 6.f, 4.f / 6.f, 5.f / 6.f, 1.f }));

		Settings->InRangeMin = 1.f;
		Settings->InRangeMax = 0.4f;
		Settings->OutRangeMin = 1.f;
		Settings->OutRangeMax = 0.5f;
		bTestPassed &= TestTrue("Inverting ranges does not effect output", ValidateDensityRemap({ 0.f, 0.2f, 3.f / 6.f, 4.f / 6.f, 5.f / 6.f, 1.f }));
	}

	// Test disabling Range Exclusion
	{
		Settings->InRangeMin = 0.4f;
		Settings->InRangeMax = 1.f;
		Settings->OutRangeMin = 0.5f;
		Settings->OutRangeMax = 1.f;
		Settings->bExcludeValuesOutsideInputRange = false;

		bTestPassed &= TestTrue("All values are remapped when Range Exclusion is disabled", ValidateDensityRemap({ 1.f / 6.f, 2.f / 6.f, 3.f / 6.f, 4.f / 6.f, 5.f / 6.f, 1.f }));

		Settings->bExcludeValuesOutsideInputRange = true;
	}

	// Test Point to Point, Point to Range, and Range to Point
	{
		Settings->InRangeMin = 0.2f;
		Settings->InRangeMax = 0.2f;
		Settings->OutRangeMin = 0.f;
		Settings->OutRangeMax = 1.f;
		bTestPassed &= TestTrue("Point input to Range output", ValidateDensityRemap({ 0.f, 0.5f, 0.4f, 0.6f, 0.8f, 1.f }));

		Settings->InRangeMin = 0.2f;
		Settings->InRangeMax = 0.2f;
		Settings->OutRangeMin = 0.5f;
		Settings->OutRangeMax = 0.5f;
		bTestPassed &= TestTrue("Point input to Point output", ValidateDensityRemap({ 0.f, 0.5f, 0.4f, 0.6f, 0.8f, 1.f }));

		Settings->InRangeMin = 0.2f;
		Settings->InRangeMax = 1.f;
		Settings->OutRangeMin = 0.5f;
		Settings->OutRangeMax = 0.5f;
		bTestPassed &= TestTrue("Range input to Point output", ValidateDensityRemap({ 0.f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f }));
	}

	return bTestPassed;
}
