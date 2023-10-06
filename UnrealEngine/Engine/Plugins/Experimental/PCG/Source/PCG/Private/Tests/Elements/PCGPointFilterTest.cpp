// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGPointFilter.h"

#include "Math/RandomStream.h"


IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGPointFilterDensity, FPCGTestBaseClass, "pcg.tests.PointFilter.Density", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGPointFilterDensityRange, FPCGTestBaseClass, "pcg.tests.PointFilter.DensityRange", PCGTestsCommon::TestFlags)

namespace PCGPointFilterTest
{
	const FName InsideFilterLabel = TEXT("InsideFilter");
	const FName OutsideFilterLabel = TEXT("OutsideFilter");

	UPCGPointData* GeneratePointDataWithRandomDensity(int32 InNumPoints, int32 InRandomSeed)
	{
		UPCGPointData* PointData = PCGTestsCommon::CreateEmptyPointData();
		TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

		FRandomStream RandomSource(InRandomSeed);

		for (int I = 0; I < InNumPoints; ++I)
		{
			FPCGPoint& Point = Points.Emplace_GetRef(FTransform(), RandomSource.FRand(), I);
		}

		return PointData;
	}
}

bool FPCGPointFilterDensity::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGPointFilterSettings>(TestData);
	UPCGPointFilterSettings* Settings = CastChecked<UPCGPointFilterSettings>(TestData.Settings);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	static const int32 NumPoints = 50;
	static const float DensityThreshold = 0.5f;

	Settings->Operator = EPCGPointFilterOperator::Lesser;
	Settings->TargetAttribute.SetPointProperty(EPCGPointProperties::Density);
	Settings->bUseConstantThreshold = true;
	Settings->AttributeTypes.FloatValue = DensityThreshold;
	Settings->AttributeTypes.Type = EPCGMetadataTypes::Float;

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	TaggedData.Data = PCGPointFilterTest::GeneratePointDataWithRandomDensity(NumPoints, PCGDeterminismTests::Defaults::Seed);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// We should have outputs on both
	TArray<FPCGTaggedData> InFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::InsideFilterLabel);
	TArray<FPCGTaggedData> OutFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::OutsideFilterLabel);

	UTEST_EQUAL(TEXT("InFilter pin has 1 output"), InFilterOutput.Num(), 1);
	UTEST_EQUAL(TEXT("OutFilter pin has 1 output"), OutFilterOutput.Num(), 1);

	// Making sure the output is a PointData
	const UPCGPointData* InFilterPointData = Cast<UPCGPointData>(InFilterOutput[0].Data);
	const UPCGPointData* OutFilterPointData = Cast<UPCGPointData>(OutFilterOutput[0].Data);

	UTEST_NOT_NULL(TEXT("InFilter data is a point data"), InFilterPointData);
	UTEST_NOT_NULL(TEXT("OutFilter data is a point data"), OutFilterPointData);

	// Verifying that all points have the right density
	for (const FPCGPoint& Point : InFilterPointData->GetPoints())
	{
		UTEST_TRUE(*FString::Printf(TEXT("Point has a density (%f) lower than the threshold (%f)"), Point.Density, DensityThreshold), Point.Density < DensityThreshold);
	}

	for (const FPCGPoint& Point : OutFilterPointData->GetPoints())
	{
		UTEST_TRUE(*FString::Printf(TEXT("Point has a density (%f) higher or equal than the threshold (%f)"), Point.Density, DensityThreshold), Point.Density >= DensityThreshold);
	}

	return true;
}

bool FPCGPointFilterDensityRange::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGPointFilterRangeSettings>(TestData);
	UPCGPointFilterRangeSettings* Settings = CastChecked<UPCGPointFilterRangeSettings>(TestData.Settings);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	static const int32 NumPoints = 50;
	static const float DensityMinThreshold = 0.3f;
	static const float DensityMaxThreshold = 0.8f;

	Settings->TargetAttribute.SetPointProperty(EPCGPointProperties::Density);

	Settings->MinThreshold.bUseConstantThreshold = true;
	Settings->MinThreshold.AttributeTypes.FloatValue = DensityMinThreshold;
	Settings->MinThreshold.AttributeTypes.Type = EPCGMetadataTypes::Float;

	Settings->MaxThreshold.bUseConstantThreshold = true;
	Settings->MaxThreshold.AttributeTypes.FloatValue = DensityMaxThreshold;
	Settings->MaxThreshold.AttributeTypes.Type = EPCGMetadataTypes::Float;

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	TaggedData.Data = PCGPointFilterTest::GeneratePointDataWithRandomDensity(NumPoints, PCGDeterminismTests::Defaults::Seed);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// We should have outputs on both
	TArray<FPCGTaggedData> InFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::InsideFilterLabel);
	TArray<FPCGTaggedData> OutFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::OutsideFilterLabel);

	UTEST_EQUAL(TEXT("InFilter pin has 1 output"), InFilterOutput.Num(), 1);
	UTEST_EQUAL(TEXT("OutFilter pin has 1 output"), OutFilterOutput.Num(), 1);

	// Making sure the output is a PointData
	const UPCGPointData* InFilterPointData = Cast<UPCGPointData>(InFilterOutput[0].Data);
	const UPCGPointData* OutFilterPointData = Cast<UPCGPointData>(OutFilterOutput[0].Data);

	UTEST_NOT_NULL(TEXT("InFilter data is a point data"), InFilterPointData);
	UTEST_NOT_NULL(TEXT("OutFilter data is a point data"), OutFilterPointData);

	// Verifying that all points have the right density
	for (const FPCGPoint& Point : InFilterPointData->GetPoints())
	{
		UTEST_TRUE(*FString::Printf(TEXT("Point has a density (%f) within the range ([%f, %f])"), Point.Density, DensityMinThreshold, DensityMaxThreshold), (Point.Density >= DensityMinThreshold) && (Point.Density <= DensityMaxThreshold));
	}

	for (const FPCGPoint& Point : OutFilterPointData->GetPoints())
	{
		UTEST_TRUE(*FString::Printf(TEXT("Point has a density (%f) outside the range ([%f, %f])"), Point.Density, DensityMinThreshold, DensityMaxThreshold), (Point.Density < DensityMinThreshold) || (Point.Density > DensityMaxThreshold));
	}

	return true;
}
