// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"
#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGData.h"
#include "PCGHelpers.h"

#include "Data/PCGPointData.h"
#include "Elements/PCGDensityNoise.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDensityNoiseTest, FPCGTestBaseClass, "pcg.tests.DensityNoise.Basic", PCGTestsCommon::TestFlags)

bool FPCGDensityNoiseTest::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGDensityNoiseSettings>(TestData);
	UPCGDensityNoiseSettings* Settings = CastChecked<UPCGDensityNoiseSettings>(TestData.Settings);
	FPCGElementPtr DensityNoiseElement = TestData.Settings->GetElement();

	TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreateEmptyPointData();
	TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

	FRandomStream RandomSource(TestData.Seed);
	const int PointCount = 5;
	for (int I = 0; I < PointCount; ++I)
	{
		FPCGPoint& Point = Points.Emplace_GetRef(FTransform(), 1, I);
		Point.Density = RandomSource.GetFraction();
	}

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TaggedData.Data = PointData;

	auto ValidateDensityNoise = [this, &TestData, DensityNoiseElement, Settings](TArray<float> ExpectedOutput) -> bool
	{
		TUniquePtr<FPCGContext> Context = MakeUnique<FPCGContext>(*DensityNoiseElement->Initialize(TestData.InputData, TestData.TestPCGComponent, nullptr));
		Context->NumAvailableTasks = 1;

		while (!DensityNoiseElement->Execute(Context.Get()))
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

			for (int PointIndex = 0; PointIndex < ExpectedOutput.Num(); ++PointIndex)
			{
				bTestPassed &= TestEqual("Correct density", OutPoints[PointIndex].Density, ExpectedOutput[PointIndex]);
			}
		}

		return bTestPassed;
	};

	bool bTestPassed = true;

	auto ValidateDensityNoiseForAllDensityModes = [this, &bTestPassed, ValidateDensityNoise, Settings]()
	{
		Settings->DensityMode = EPCGDensityNoiseMode::Set;
		bTestPassed &= ValidateDensityNoise({});

		Settings->DensityMode = EPCGDensityNoiseMode::Minimum;
		bTestPassed &= ValidateDensityNoise({});

		Settings->DensityMode = EPCGDensityNoiseMode::Maximum;
		bTestPassed &= ValidateDensityNoise({});

		Settings->DensityMode = EPCGDensityNoiseMode::Add;
		bTestPassed &= ValidateDensityNoise({});

		Settings->DensityMode = EPCGDensityNoiseMode::Multiply;
		bTestPassed &= ValidateDensityNoise({});
	};

	// Test [0-1]
	{
		Settings->DensityNoiseMin = 0.f;
		Settings->DensityNoiseMax = 1.f;
		Settings->bInvertSourceDensity = false;
		ValidateDensityNoiseForAllDensityModes();

		Settings->bInvertSourceDensity = true;
		ValidateDensityNoiseForAllDensityModes();
	}

	// Test [0-0.5]
	{
		Settings->DensityNoiseMin = 0.f;
		Settings->DensityNoiseMax = 0.5f;
		Settings->bInvertSourceDensity = false;
		ValidateDensityNoiseForAllDensityModes();
		
		Settings->bInvertSourceDensity = true;
		ValidateDensityNoiseForAllDensityModes();
	}

	// Test [0.5-1]
	{
		Settings->DensityNoiseMin = 0.5f;
		Settings->DensityNoiseMax = 1.f;
		Settings->bInvertSourceDensity = false;
		ValidateDensityNoiseForAllDensityModes();

		Settings->bInvertSourceDensity = true;
		ValidateDensityNoiseForAllDensityModes();
	}

	// Test [1-0]
	{
		Settings->DensityNoiseMin = 1.f;
		Settings->DensityNoiseMax = 0.f;
		Settings->bInvertSourceDensity = false;
		ValidateDensityNoiseForAllDensityModes();

		Settings->bInvertSourceDensity = true;
		ValidateDensityNoiseForAllDensityModes();
	}

	// Test [0.5-0]
	{
		Settings->DensityNoiseMin = 0.5f;
		Settings->DensityNoiseMax = 0.f;
		Settings->bInvertSourceDensity = false;
		ValidateDensityNoiseForAllDensityModes();
		
		Settings->bInvertSourceDensity = true;
		ValidateDensityNoiseForAllDensityModes();
	}

	// Test [1-0.5]
	{
		Settings->DensityNoiseMin = 1.f;
		Settings->DensityNoiseMax = 0.5f;
		Settings->bInvertSourceDensity = false;
		ValidateDensityNoiseForAllDensityModes();

		Settings->bInvertSourceDensity = true;
		ValidateDensityNoiseForAllDensityModes();
	}

	Settings->bInvertSourceDensity = false;

	// Test expected values for Set
	{
		Settings->DensityNoiseMin = 0.5f;
		Settings->DensityNoiseMax = 0.5f;
		Settings->DensityMode = EPCGDensityNoiseMode::Set;
		bTestPassed &= ValidateDensityNoise({ 0.5f, 0.5f, 0.5f, 0.5f, 0.5f });
	}

	// Test expected values for Minimum
	{
		Settings->DensityNoiseMin = 0.f;
		Settings->DensityNoiseMax = 0.f;
		Settings->DensityMode = EPCGDensityNoiseMode::Minimum;
		bTestPassed &= ValidateDensityNoise({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f });
	}

	// Test expected values for Maximum
	{
		Settings->DensityNoiseMin = 1.f;
		Settings->DensityNoiseMax = 1.f;
		Settings->DensityMode = EPCGDensityNoiseMode::Maximum;
		bTestPassed &= ValidateDensityNoise({ 1.f, 1.f, 1.f, 1.f, 1.f });
	}

	// Test expected values for Add
	{
		Settings->DensityNoiseMin = 0.f;
		Settings->DensityNoiseMax = 0.f;
		Settings->DensityMode = EPCGDensityNoiseMode::Add;
		bTestPassed &= ValidateDensityNoise({ 
			Points[0].Density,
			Points[1].Density,
			Points[2].Density,
			Points[3].Density,
			Points[4].Density,
		});
	}

	// Test expected values for Multiply
	{
		Settings->DensityNoiseMin = 0.f;
		Settings->DensityNoiseMax = 0.f;
		Settings->DensityMode = EPCGDensityNoiseMode::Multiply;
		bTestPassed &= ValidateDensityNoise({ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f });
	}

	return bTestPassed;
}
