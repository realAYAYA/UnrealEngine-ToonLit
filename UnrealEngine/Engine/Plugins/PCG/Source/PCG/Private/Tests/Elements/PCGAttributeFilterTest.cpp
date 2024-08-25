// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGPoint.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGAttributeFilter.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Math/RandomStream.h"


IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGPointFilterDensity, FPCGTestBaseClass, "Plugins.PCG.AttributeFilter.Points.Density", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGPointFilterDensityRange, FPCGTestBaseClass, "Plugins.PCG.AttributeFilter.Points.DensityRange", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributeFilterInt, FPCGTestBaseClass, "Plugins.PCG.AttributeFilter.Params.Int", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributeFilterIntRange, FPCGTestBaseClass, "Plugins.PCG.AttributeFilter.Params.IntRange", PCGTestsCommon::TestFlags)
IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGAttributeFilterSkipTestBug, FPCGTestBaseClass, "Plugins.PCG.AttributeFilter.SkipTestBug", PCGTestsCommon::TestFlags)

namespace PCGPointFilterTest
{
	const FName InsideFilterLabel = TEXT("InsideFilter");
	const FName OutsideFilterLabel = TEXT("OutsideFilter");
	const FName FilterLabel = TEXT("Filter");

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

	static const FName IntAttributeName = TEXT("Int");

	UPCGParamData* GenerateAttributeSetWithRandomInt(int32 InNumEntries, int32 InRandomSeed)
	{
		UPCGParamData* ParamData = NewObject<UPCGParamData>();
		check(ParamData && ParamData->Metadata);
		FPCGMetadataAttribute<int32>* Attribute = ParamData->Metadata->CreateAttribute<int32>(IntAttributeName, 0, true, false);
		check(Attribute)

		FRandomStream RandomSource(InRandomSeed);

		for (int I = 0; I < InNumEntries; ++I)
		{
			PCGMetadataEntryKey Key = ParamData->Metadata->AddEntry();
			Attribute->SetValue(Key, RandomSource.GetUnsignedInt());
		}

		return ParamData;
	}
}

bool FPCGPointFilterDensity::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGAttributeFilteringSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGAttributeFilteringSettings>(TestData);
	check(Settings);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	static const int32 NumPoints = 50;
	static const float DensityThreshold = 0.5f;

	Settings->Operator = EPCGAttributeFilterOperator::Lesser;
	Settings->TargetAttribute.SetPointProperty(EPCGPointProperties::Density);
	Settings->bUseConstantThreshold = true;
	Settings->AttributeTypes.FloatValue = DensityThreshold;
	Settings->AttributeTypes.Type = EPCGMetadataTypes::Float;

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	TaggedData.Data = PCGPointFilterTest::GeneratePointDataWithRandomDensity(NumPoints, TestData.Seed);

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
	PCGTestsCommon::FTestData TestData;
	UPCGAttributeFilteringRangeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGAttributeFilteringRangeSettings>(TestData);
	check(Settings);

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
	TaggedData.Data = PCGPointFilterTest::GeneratePointDataWithRandomDensity(NumPoints, TestData.Seed);

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

bool FPCGAttributeFilterInt::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGAttributeFilteringSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGAttributeFilteringSettings>(TestData);
	check(Settings);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	static const int32 NumEntries = 50;
	static const int64 IntThreshold = 999999;

	Settings->Operator = EPCGAttributeFilterOperator::Lesser;
	Settings->TargetAttribute.SetAttributeName(PCGPointFilterTest::IntAttributeName);
	Settings->bUseConstantThreshold = true;
	Settings->AttributeTypes.IntValue = IntThreshold;
	Settings->AttributeTypes.Type = EPCGMetadataTypes::Integer64;

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	TaggedData.Data = PCGPointFilterTest::GenerateAttributeSetWithRandomInt(NumEntries, TestData.Seed);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// We should have outputs on both
	TArray<FPCGTaggedData> InFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::InsideFilterLabel);
	TArray<FPCGTaggedData> OutFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::OutsideFilterLabel);

	UTEST_EQUAL(TEXT("InFilter pin has 1 output"), InFilterOutput.Num(), 1);
	UTEST_EQUAL(TEXT("OutFilter pin has 1 output"), OutFilterOutput.Num(), 1);

	// Making sure the output is a ParamData, and has its attribute
	const UPCGParamData* InFilterParamData = Cast<UPCGParamData>(InFilterOutput[0].Data);
	const UPCGParamData* OutFilterParamData = Cast<UPCGParamData>(OutFilterOutput[0].Data);

	UTEST_NOT_NULL(TEXT("InFilter data is a param data"), InFilterParamData);
	UTEST_NOT_NULL(TEXT("OutFilter data is a param data"), OutFilterParamData);

	const FPCGMetadataAttribute<int32>* InFilterAttribute = InFilterParamData->Metadata->GetConstTypedAttribute<int32>(PCGPointFilterTest::IntAttributeName);
	const FPCGMetadataAttribute<int32>* OutFilterAttribute = OutFilterParamData->Metadata->GetConstTypedAttribute<int32>(PCGPointFilterTest::IntAttributeName);

	UTEST_NOT_NULL(TEXT("InFilter metadata has the int attribute"), InFilterAttribute);
	UTEST_NOT_NULL(TEXT("OutFilter metadata has the int attribute"), OutFilterAttribute);

	// Verifying that all points have the right int value
	for (int32 Key = 0; Key < InFilterParamData->Metadata->GetLocalItemCount(); ++Key)
	{
		int32 Value = InFilterAttribute->GetValueFromItemKey(Key);
		UTEST_TRUE(*FString::Printf(TEXT("Attribute has a value (%d) lower than the threshold (%d)"), Value, (int32)IntThreshold), Value < (int32)IntThreshold);
	}

	for (int32 Key = 0; Key < OutFilterParamData->Metadata->GetLocalItemCount(); ++Key)
	{
		int32 Value = OutFilterAttribute->GetValueFromItemKey(Key);
		UTEST_TRUE(*FString::Printf(TEXT("Attribute has a value (%d) higher than the threshold (%d)"), Value, (int32)IntThreshold), Value >= (int32)IntThreshold);
	}

	return true;
}

bool FPCGAttributeFilterIntRange::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGAttributeFilteringRangeSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGAttributeFilteringRangeSettings>(TestData);
	check(Settings);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	static const int32 NumEntries = 50;
	static const int64 IntMinThreshold = 0;
	static const int64 IntMaxThreshold = 1999999999;

	Settings->TargetAttribute.SetAttributeName(PCGPointFilterTest::IntAttributeName);

	Settings->MinThreshold.bUseConstantThreshold = true;
	Settings->MinThreshold.AttributeTypes.IntValue = IntMinThreshold;
	Settings->MinThreshold.AttributeTypes.Type = EPCGMetadataTypes::Integer64;

	Settings->MaxThreshold.bUseConstantThreshold = true;
	Settings->MaxThreshold.AttributeTypes.IntValue = IntMaxThreshold;
	Settings->MaxThreshold.AttributeTypes.Type = EPCGMetadataTypes::Integer64;

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
	TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	TaggedData.Data = PCGPointFilterTest::GenerateAttributeSetWithRandomInt(NumEntries, TestData.Seed);

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// We should have outputs on both
	TArray<FPCGTaggedData> InFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::InsideFilterLabel);
	TArray<FPCGTaggedData> OutFilterOutput = Context->OutputData.GetInputsByPin(PCGPointFilterTest::OutsideFilterLabel);

	UTEST_EQUAL(TEXT("InFilter pin has 1 output"), InFilterOutput.Num(), 1);
	UTEST_EQUAL(TEXT("OutFilter pin has 1 output"), OutFilterOutput.Num(), 1);

	// Making sure the output is a ParamData, and has its attribute
	const UPCGParamData* InFilterParamData = Cast<UPCGParamData>(InFilterOutput[0].Data);
	const UPCGParamData* OutFilterParamData = Cast<UPCGParamData>(OutFilterOutput[0].Data);

	UTEST_NOT_NULL(TEXT("InFilter data is a param data"), InFilterParamData);
	UTEST_NOT_NULL(TEXT("OutFilter data is a param data"), OutFilterParamData);

	const FPCGMetadataAttribute<int32>* InFilterAttribute = InFilterParamData->Metadata->GetConstTypedAttribute<int32>(PCGPointFilterTest::IntAttributeName);
	const FPCGMetadataAttribute<int32>* OutFilterAttribute = OutFilterParamData->Metadata->GetConstTypedAttribute<int32>(PCGPointFilterTest::IntAttributeName);

	UTEST_NOT_NULL(TEXT("InFilter metadata has the int attribute"), InFilterAttribute);
	UTEST_NOT_NULL(TEXT("OutFilter metadata has the int attribute"), OutFilterAttribute);

	// Verifying that all points have the right int value
	for (int32 Key = 0; Key < InFilterParamData->Metadata->GetLocalItemCount(); ++Key)
	{
		int32 Value = InFilterAttribute->GetValueFromItemKey(Key);
		UTEST_TRUE(*FString::Printf(TEXT("Attribute has a value (%d) within the range [%d, %d]"), Value, (int32)IntMinThreshold, (int32)IntMaxThreshold), Value >= (int32)IntMinThreshold && Value <= (int32)IntMaxThreshold);
	}

	for (int32 Key = 0; Key < OutFilterParamData->Metadata->GetLocalItemCount(); ++Key)
	{
		int32 Value = OutFilterAttribute->GetValueFromItemKey(Key);
		UTEST_TRUE(*FString::Printf(TEXT("Attribute has a value (%d) outside the range [%d, %d]"), Value, (int32)IntMinThreshold, (int32)IntMaxThreshold), Value < (int32)IntMinThreshold || Value > (int32)IntMaxThreshold);
	}

	return true;
}

// We do not reset SkipTests in the case of point sampling, resulting to accepting points that should have not been accepted.
// This fails before fixed CL of UE-201595.
bool FPCGAttributeFilterSkipTestBug::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGAttributeFilteringSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGAttributeFilteringSettings>(TestData);
	check(Settings);

	Settings->TargetAttribute.SetPointProperty(EPCGPointProperties::Density);
	Settings->ThresholdAttribute.SetPointProperty(EPCGPointProperties::Density);
	Settings->Operator = EPCGAttributeFilterOperator::Equal;
	Settings->bUseSpatialQuery = true;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();
	UPCGPointData* InputPointData = NewObject<UPCGPointData>();
	UPCGPointData* ThresholdPointData = NewObject<UPCGPointData>();

	TArray<FPCGPoint>& InputPoints = InputPointData->GetMutablePoints();
	TArray<FPCGPoint>& ThresholdPoints = ThresholdPointData->GetMutablePoints();

	// Take a big number to make sure we go over the 256 default chunk size
	constexpr int32 NumPoints = 2048;
	constexpr int32 HalfNumPoints = NumPoints / 2;
	InputPoints.Reserve(NumPoints);
	ThresholdPoints.Reserve(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		// First half are very different so the sampling should fail, second half are the same the sampling to succeed but the filtering to fail.
		if (i >= HalfNumPoints)
		{
			FPCGPoint& Point = InputPoints.Emplace_GetRef();
			Point.Transform.SetLocation(FVector(10 * i, 10 * i, 10 * i));
			FPCGPoint& ThresholdPoint = ThresholdPoints.Add_GetRef(Point);
			ThresholdPoint.Density = 0.5f;
		}
		else
		{
			FPCGPoint& Point = InputPoints.Emplace_GetRef();
			Point.Transform.SetLocation(FVector(i, i, i));
			FPCGPoint& ThresholdPoint = ThresholdPoints.Emplace_GetRef();
			ThresholdPoint.Transform.SetLocation(FVector(-10 * i - 1000, -10 * i - 1000, -10 * i - 1000));
		}
	}

	FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	TaggedData.Data = InputPointData;

	FPCGTaggedData& SecondTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
	SecondTaggedData.Pin = PCGPointFilterTest::FilterLabel;
	SecondTaggedData.Data = ThresholdPointData;

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

	// We should have the inside filter with half the points and the outside filter to the rest
	UTEST_EQUAL(TEXT("InFilter data has the right number of points"), InFilterPointData->GetPoints().Num(), HalfNumPoints);
	UTEST_EQUAL(TEXT("OutFilter data has the right number of points"), OutFilterPointData->GetPoints().Num(), HalfNumPoints);

	return true;
}