// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"

#include "Elements/PCGDistance.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDistanceTest_PointToPoint, FPCGTestBaseClass, "pcg.tests.Distance.PointToPoint", PCGTestsCommon::TestFlags)

bool FPCGDistanceTest_PointToPoint::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGDistanceSettings>(TestData);
	UPCGDistanceSettings* Settings = CastChecked<UPCGDistanceSettings>(TestData.Settings);

	Settings->SourceShape = PCGDistanceShape::Center;
	Settings->TargetShape = PCGDistanceShape::Center;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	{
		UPCGPointData *SourceData = PCGTestsCommon::CreateRandomPointData(2, 42);

		TArray<FPCGPoint>& SourcePoints = SourceData->GetMutablePoints();
		SourcePoints[0].Transform.SetTranslation(FVector(100, 0, 0));
		SourcePoints[1].Transform.SetTranslation(FVector(0, 50, 0));

		FPCGTaggedData& SourcePin = TestData.InputData.TaggedData.Emplace_GetRef();
		SourcePin.Pin = PCGDistance::SourceLabel;
		SourcePin.Data = SourceData;
	}

	{
		FPCGTaggedData& TargetPin = TestData.InputData.TaggedData.Emplace_GetRef();
		TargetPin.Pin = PCGDistance::TargetLabel;
		TargetPin.Data = PCGTestsCommon::CreatePointData(FVector(0, 0, 0));		
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);

	UTEST_EQUAL("Output count", Outputs.Num(), 2);

	UPCGPointData *OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutPoints.Num(), 2);

	const FPCGMetadataAttribute<float> *DistanceAttribute = OutPointData->Metadata->GetConstTypedAttribute<float>(Settings->AttributeName);
	UTEST_NOT_NULL("Distance attribute", DistanceAttribute);

	UTEST_EQUAL_TOLERANCE("Point 0 distance", DistanceAttribute->GetValue(OutPoints[0].MetadataEntry), 100.f, 0.01f);
	UTEST_EQUAL_TOLERANCE("Point 1 distance", DistanceAttribute->GetValue(OutPoints[1].MetadataEntry), 50.f, 0.01f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDistanceTest_SetDensity, FPCGTestBaseClass, "pcg.tests.Distance.SetDensity", PCGTestsCommon::TestFlags)

bool FPCGDistanceTest_SetDensity::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGDistanceSettings>(TestData);
	UPCGDistanceSettings* Settings = CastChecked<UPCGDistanceSettings>(TestData.Settings);

	Settings->SourceShape = PCGDistanceShape::Center;
	Settings->TargetShape = PCGDistanceShape::Center;
	Settings->AttributeName = NAME_None;
	Settings->bSetDensity = true;
	Settings->MaximumDistance = 200.f;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	{
		UPCGPointData *SourceData = PCGTestsCommon::CreateRandomPointData(2, 42);

		TArray<FPCGPoint>& SourcePoints = SourceData->GetMutablePoints();
		SourcePoints[0].Transform.SetTranslation(FVector(100, 0, 0));
		SourcePoints[1].Transform.SetTranslation(FVector(0, 50, 0));

		FPCGTaggedData& SourcePin = TestData.InputData.TaggedData.Emplace_GetRef();
		SourcePin.Pin = PCGDistance::SourceLabel;
		SourcePin.Data = SourceData;
	}

	{
		FPCGTaggedData& TargetPin = TestData.InputData.TaggedData.Emplace_GetRef();
		TargetPin.Pin = PCGDistance::TargetLabel;
		TargetPin.Data = PCGTestsCommon::CreatePointData(FVector(0, 0, 0));		
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);

	UTEST_EQUAL("Output count", Outputs.Num(), 2);

	UPCGPointData *OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutPoints.Num(), 2);

	UTEST_EQUAL_TOLERANCE("Point 0 distance", OutPoints[0].Density, 0.5f, 0.01f);
	UTEST_EQUAL_TOLERANCE("Point 1 distance", OutPoints[1].Density, 0.25f, 0.01f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDistanceTest_PointToSphere, FPCGTestBaseClass, "pcg.tests.Distance.PointToSphere", PCGTestsCommon::TestFlags)

bool FPCGDistanceTest_PointToSphere::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGDistanceSettings>(TestData);
	UPCGDistanceSettings* Settings = CastChecked<UPCGDistanceSettings>(TestData.Settings);

	Settings->SourceShape = PCGDistanceShape::Center;
	Settings->TargetShape = PCGDistanceShape::SphereBounds;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	{
		UPCGPointData *SourceData = PCGTestsCommon::CreateRandomPointData(2, 42);

		TArray<FPCGPoint>& SourcePoints = SourceData->GetMutablePoints();
		SourcePoints[0].Transform.SetTranslation(FVector(100, 0, 0));
		SourcePoints[1].Transform.SetTranslation(FVector(0, 50, 0));

		FPCGTaggedData& SourcePin = TestData.InputData.TaggedData.Emplace_GetRef();
		SourcePin.Pin = PCGDistance::SourceLabel;
		SourcePin.Data = SourceData;
	}

	const FVector TargetExtents(10.0f);
	const float TargetPointRadius = TargetExtents.Length();

	{
		FPCGTaggedData& TargetPin = TestData.InputData.TaggedData.Emplace_GetRef();
		TargetPin.Pin = PCGDistance::TargetLabel;

		UPCGPointData *TargetData = PCGTestsCommon::CreatePointData(FVector(0, 0, 0));
		TArray<FPCGPoint>& TargetPoints = TargetData->GetMutablePoints();
		TargetPoints[0].SetExtents(TargetExtents);

		TargetPin.Data = TargetData;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);

	UTEST_EQUAL("Output count", Outputs.Num(), 2);

	UPCGPointData *OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutPoints.Num(), 2);

	const FPCGMetadataAttribute<float> *DistanceAttribute = OutPointData->Metadata->GetConstTypedAttribute<float>(Settings->AttributeName);
	UTEST_NOT_NULL("Distance attribute", DistanceAttribute);

	UTEST_EQUAL_TOLERANCE("Point 0 distance", DistanceAttribute->GetValue(OutPoints[0].MetadataEntry), 100.f-TargetPointRadius, 0.01f);
	UTEST_EQUAL_TOLERANCE("Point 1 distance", DistanceAttribute->GetValue(OutPoints[1].MetadataEntry), 50.f-TargetPointRadius, 0.01f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGDistanceTest_PointToBox, FPCGTestBaseClass, "pcg.tests.Distance.PointToBox", PCGTestsCommon::TestFlags)

bool FPCGDistanceTest_PointToBox::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGDistanceSettings>(TestData);
	UPCGDistanceSettings* Settings = CastChecked<UPCGDistanceSettings>(TestData.Settings);

	Settings->SourceShape = PCGDistanceShape::Center;
	Settings->TargetShape = PCGDistanceShape::BoxBounds;

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	{
		UPCGPointData *SourceData = PCGTestsCommon::CreateRandomPointData(2, 42);

		TArray<FPCGPoint>& SourcePoints = SourceData->GetMutablePoints();
		SourcePoints[0].Transform.SetTranslation(FVector(100, 0, 0));
		SourcePoints[1].Transform.SetTranslation(FVector(0, 50, 0));

		FPCGTaggedData& SourcePin = TestData.InputData.TaggedData.Emplace_GetRef();
		SourcePin.Pin = PCGDistance::SourceLabel;
		SourcePin.Data = SourceData;
	}

	// should put the box 10 units away from each point
	const FVector TargetExtents(90.0f, 40.f, 10.f);

	{
		FPCGTaggedData& TargetPin = TestData.InputData.TaggedData.Emplace_GetRef();
		TargetPin.Pin = PCGDistance::TargetLabel;

		UPCGPointData *TargetData = PCGTestsCommon::CreatePointData(FVector(0, 0, 0));
		TArray<FPCGPoint>& TargetPoints = TargetData->GetMutablePoints();
		TargetPoints[0].SetExtents(TargetExtents);

		TargetPin.Data = TargetData;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputsByPin(PCGPinConstants::DefaultOutputLabel);

	UTEST_EQUAL("Output count", Outputs.Num(), 2);

	UPCGPointData *OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutPoints.Num(), 2);

	const FPCGMetadataAttribute<float> *DistanceAttribute = OutPointData->Metadata->GetConstTypedAttribute<float>(Settings->AttributeName);
	UTEST_NOT_NULL("Distance attribute", DistanceAttribute);

	UTEST_EQUAL_TOLERANCE("Point 0 distance", DistanceAttribute->GetValue(OutPoints[0].MetadataEntry), 10.f, 0.01f);
	UTEST_EQUAL_TOLERANCE("Point 1 distance", DistanceAttribute->GetValue(OutPoints[1].MetadataEntry), 10.f, 0.01f);

	return true;
}
