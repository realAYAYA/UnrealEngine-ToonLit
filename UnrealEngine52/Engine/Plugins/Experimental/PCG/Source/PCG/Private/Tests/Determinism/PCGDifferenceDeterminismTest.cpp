// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR
#include "Tests/Determinism/PCGDifferenceDeterminismTest.h"

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "Elements/PCGDifferenceElement.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDifferenceDeterminismSingleSameDataTest, FPCGTestBaseClass, "pcg.tests.Difference.Determinism.SingleSameData", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDifferenceDeterminismSingleIdenticalDataTest, FPCGTestBaseClass, "pcg.tests.Difference.Determinism.SingleMultipleData", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDifferenceDeterminismMultipleSameDataTest, FPCGTestBaseClass, "pcg.tests.Difference.Determinism.MultipleSameData", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDifferenceDeterminismMultipleIdenticalDataTest, FPCGTestBaseClass, "pcg.tests.Difference.Determinism.MultipleIdenticalData", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDifferenceDeterminismOrderIndependenceTest, FPCGTestBaseClass, "pcg.tests.Difference.Determinism.OrderIndependence", PCGTestsCommon::TestFlags)

namespace
{
	void SetSettingsModeInferred(PCGTestsCommon::FTestData& TestData)
	{
		UPCGDifferenceSettings* Settings = CastChecked<UPCGDifferenceSettings>(TestData.Settings);
		Settings->Mode = EPCGDifferenceMode::Inferred;
	}

	void SetSettingsModeContinuous(PCGTestsCommon::FTestData& TestData)
	{
		UPCGDifferenceSettings* Settings = CastChecked<UPCGDifferenceSettings>(TestData.Settings);
		Settings->Mode = EPCGDifferenceMode::Continuous;
	}

	void SetSettingsModeDiscrete(PCGTestsCommon::FTestData& TestData)
	{
		UPCGDifferenceSettings* Settings = CastChecked<UPCGDifferenceSettings>(TestData.Settings);
		Settings->Mode = EPCGDifferenceMode::Discrete;
	}

	void DifferenceTestBase(PCGTestsCommon::FTestData& TestData, EPCGDifferenceMode DifferenceMode)
	{
		TFunction<void(PCGTestsCommon::FTestData&)> AdditionalSettingsDelegate;

		switch (DifferenceMode)
		{
		case EPCGDifferenceMode::Inferred: // To PointData if points in source and difference inputs
			AdditionalSettingsDelegate = SetSettingsModeInferred;
			break;
		case EPCGDifferenceMode::Continuous: // Stays DifferenceData
			AdditionalSettingsDelegate = SetSettingsModeContinuous;
			break;
		case EPCGDifferenceMode::Discrete: // To PointData
			AdditionalSettingsDelegate = SetSettingsModeDiscrete;
			break;
		default:
			break;
		}

		PCGDeterminismTests::GenerateSettings<UPCGDifferenceSettings>(TestData, AdditionalSettingsDelegate);
		// Source
		PCGDeterminismTests::AddVolumeInputData(TestData.InputData, FVector::ZeroVector, PCGDeterminismTests::Defaults::LargeVector, PCGDeterminismTests::Defaults::SmallVector, PCGDifferenceConstants::SourceLabel);

		// Difference
		PCGDeterminismTests::AddVolumeInputData(TestData.InputData, FVector::ZeroVector, PCGDeterminismTests::Defaults::MediumVector, PCGDeterminismTests::Defaults::SmallVector, PCGDifferenceConstants::DifferencesLabel);
	}

	void DifferenceTestMultiple(PCGTestsCommon::FTestData& TestData, EPCGDifferenceMode DifferenceMode)
	{
		DifferenceTestBase(TestData, DifferenceMode);

		// Randomized Sources
		PCGDeterminismTests::AddRandomizedVolumeInputData(TestData, PCGDifferenceConstants::SourceLabel);
		PCGDeterminismTests::AddRandomizedMultiplePointInputData(TestData, 20, PCGDifferenceConstants::SourceLabel);

		// Randomized Differences
		PCGDeterminismTests::AddRandomizedVolumeInputData(TestData, PCGDifferenceConstants::DifferencesLabel);
		PCGDeterminismTests::AddRandomizedMultiplePointInputData(TestData, 20, PCGDifferenceConstants::DifferencesLabel);
	}
}

namespace PCGDeterminismTests
{
	namespace DifferenceElement
	{
		bool RunTestSuite()
		{
			FString EmptyParameters;
			FPCGDifferenceDeterminismSingleSameDataTest DifferenceDeterminismSingleSameDataTest(EmptyParameters);
			FPCGDifferenceDeterminismSingleIdenticalDataTest DifferenceDeterminismSingleIdenticalDataTest(EmptyParameters);
			FPCGDifferenceDeterminismMultipleSameDataTest DifferenceDeterminismMultipleSameDataTest(EmptyParameters);
			FPCGDifferenceDeterminismMultipleIdenticalDataTest DifferenceDeterminismMultipleIdenticalDataTest(EmptyParameters);
			FPCGDifferenceDeterminismOrderIndependenceTest DifferenceDeterminismOrderIndependenceTest(EmptyParameters);

			bool bSuccess = true;
			bSuccess &= DifferenceDeterminismSingleSameDataTest.RunPCGTest(EmptyParameters);
			bSuccess &= DifferenceDeterminismSingleIdenticalDataTest.RunPCGTest(EmptyParameters);
			bSuccess &= DifferenceDeterminismMultipleSameDataTest.RunPCGTest(EmptyParameters);
			bSuccess &= DifferenceDeterminismMultipleIdenticalDataTest.RunPCGTest(EmptyParameters);
			bSuccess &= DifferenceDeterminismOrderIndependenceTest.RunPCGTest(EmptyParameters);
			return bSuccess;
		}
	}
}

bool FPCGDifferenceDeterminismSingleSameDataTest::RunTest(const FString& Parameters)
{
	// Test single same data
	PCGTestsCommon::FTestData TestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData TestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData TestDataDiscrete(PCGDeterminismTests::Defaults::Seed);

	DifferenceTestBase(TestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestBase(TestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestBase(TestDataDiscrete, EPCGDifferenceMode::Discrete);

	TestTrue("Same single input, same output, 'Inferred' mode", PCGDeterminismTests::ExecutionIsDeterministicSameData(TestDataInferred));
	TestTrue("Same single input, same output, 'Continuous' mode", PCGDeterminismTests::ExecutionIsDeterministicSameData(TestDataContinuous));
	TestTrue("Same single input, same output, 'Discrete' mode", PCGDeterminismTests::ExecutionIsDeterministicSameData(TestDataDiscrete));

	return true;
}

bool FPCGDifferenceDeterminismSingleIdenticalDataTest::RunTest(const FString& Parameters)
{
	// Test single identical data
	PCGTestsCommon::FTestData FirstTestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData FirstTestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData FirstTestDataDiscrete(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataDiscrete(PCGDeterminismTests::Defaults::Seed);

	DifferenceTestBase(FirstTestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestBase(FirstTestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestBase(FirstTestDataDiscrete, EPCGDifferenceMode::Discrete);
	DifferenceTestBase(SecondTestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestBase(SecondTestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestBase(SecondTestDataDiscrete, EPCGDifferenceMode::Discrete);

	TestTrue("Identical single input, same output, 'Inferred' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataInferred, SecondTestDataInferred));
	TestTrue("Identical single input, same output, 'Continuous' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataContinuous, SecondTestDataContinuous));
	TestTrue("Identical single input, same output, 'Discrete' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataDiscrete, SecondTestDataDiscrete));

	return true;
}

bool FPCGDifferenceDeterminismMultipleSameDataTest::RunTest(const FString& Parameters)
{
	// Test multiple same data
	PCGTestsCommon::FTestData TestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData TestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData TestDataDiscrete(PCGDeterminismTests::Defaults::Seed);

	DifferenceTestMultiple(TestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestMultiple(TestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestMultiple(TestDataDiscrete, EPCGDifferenceMode::Discrete);

	TestTrue("Same single input, same output, 'Inferred' mode", PCGDeterminismTests::ExecutionIsDeterministicSameData(TestDataInferred));
	TestTrue("Same single input, same output, 'Continuous' mode", PCGDeterminismTests::ExecutionIsDeterministicSameData(TestDataContinuous));
	TestTrue("Same single input, same output, 'Discrete' mode", PCGDeterminismTests::ExecutionIsDeterministicSameData(TestDataDiscrete));

	return true;
}

bool FPCGDifferenceDeterminismMultipleIdenticalDataTest::RunTest(const FString& Parameters)
{
	// Test multiple identical data
	PCGTestsCommon::FTestData FirstTestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData FirstTestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData FirstTestDataDiscrete(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataDiscrete(PCGDeterminismTests::Defaults::Seed);

	DifferenceTestMultiple(FirstTestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestMultiple(FirstTestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestMultiple(FirstTestDataDiscrete, EPCGDifferenceMode::Discrete);
	DifferenceTestMultiple(SecondTestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestMultiple(SecondTestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestMultiple(SecondTestDataDiscrete, EPCGDifferenceMode::Discrete);

	TestTrue("Identical single input, same output, 'Inferred' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataInferred, SecondTestDataInferred));
	TestTrue("Identical single input, same output, 'Continuous' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataContinuous, SecondTestDataContinuous));
	TestTrue("Identical single input, same output, 'Discrete' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataDiscrete, SecondTestDataDiscrete));

	return true;
}

bool FPCGDifferenceDeterminismOrderIndependenceTest::RunTest(const FString& Parameters)
{
	// Test multiple identical data, shuffled
	PCGTestsCommon::FTestData FirstTestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData FirstTestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData FirstTestDataDiscrete(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataInferred(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataContinuous(PCGDeterminismTests::Defaults::Seed);
	PCGTestsCommon::FTestData SecondTestDataDiscrete(PCGDeterminismTests::Defaults::Seed);

	DifferenceTestMultiple(FirstTestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestMultiple(FirstTestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestMultiple(FirstTestDataDiscrete, EPCGDifferenceMode::Discrete);
	DifferenceTestMultiple(SecondTestDataInferred, EPCGDifferenceMode::Inferred);
	DifferenceTestMultiple(SecondTestDataContinuous, EPCGDifferenceMode::Continuous);
	DifferenceTestMultiple(SecondTestDataDiscrete, EPCGDifferenceMode::Discrete);

	PCGDeterminismTests::ShuffleInputOrder(SecondTestDataInferred);
	PCGDeterminismTests::ShuffleInputOrder(SecondTestDataContinuous);
	PCGDeterminismTests::ShuffleInputOrder(SecondTestDataDiscrete);

	TestTrue("Shuffled input order, same output, 'Inferred' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataInferred, SecondTestDataInferred));
	TestTrue("Shuffled input order, same output, 'Continuous' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataContinuous, SecondTestDataContinuous));
	TestTrue("Shuffled input order, same output, 'Discrete' mode", PCGDeterminismTests::ExecutionIsDeterministic(FirstTestDataDiscrete, SecondTestDataDiscrete));

	return true;
}

#endif