// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "Data/PCGSpatialData.h"
#include "PCGComponent.h"

#include "Data/PCGPointData.h"
#include "Elements/PCGNormalToDensity.h"
#include "PCGContext.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Set, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Set", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Set::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Set;

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];

		Point.Transform.SetRotation(FQuat::MakeFromEuler(FVector(45.0, 0, 0)));
		Point.Density = 0.5f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.707f, 0.001f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_CustomNormal, FPCGTestBaseClass, "pcg.tests.NormalToDensity.CustomNormal", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_CustomNormal::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Set;
	Settings->Normal = FVector(0.707, 0, 0.707);

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];
		Point.Density = 0.5f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.707f, 0.001f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Strength, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Strength", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Strength::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Set;
	Settings->Strength = 2;
	Settings->Normal = FVector(0.866, 0, 0.5); // so the dot with up is 0.5

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];
		Point.Density = 0.5f;
		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.707f, 0.001f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Minimum, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Minimum", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Minimum::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Minimum;

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];

		Point.Transform.SetRotation(FQuat::MakeFromEuler(FVector(45.0, 0, 0)));
		Point.Density = 0.5f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.5f, 0.001f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Maximum, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Maximum", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Maximum::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Maximum;

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];

		Point.Transform.SetRotation(FQuat::MakeFromEuler(FVector(45.0, 0, 0)));
		Point.Density = 0.5f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.707f, 0.001f);

	return true;
}


IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Add, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Add", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Add::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Add;

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];

		Point.Transform.SetRotation(FQuat::MakeFromEuler(FVector(45.0, 0, 0)));
		Point.Density = 0.1f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.807f, 0.001f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Subtract, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Subtract", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Subtract::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Subtract;

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];

		Point.Transform.SetRotation(FQuat::MakeFromEuler(FVector(45.0, 0, 0)));
		Point.Density = 0.9f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.9f-0.707f, 0.001f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Multiply, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Multiply", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Multiply::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Multiply;

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];

		Point.Transform.SetRotation(FQuat::MakeFromEuler(FVector(45.0, 0, 0)));
		Point.Density = 0.5f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.5f*0.707f, 0.001f);

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGNormalToDensityTest_Divide, FPCGTestBaseClass, "pcg.tests.NormalToDensity.Divide", PCGTestsCommon::TestFlags)

bool FPCGNormalToDensityTest_Divide::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData(PCGDeterminismTests::Defaults::Seed);
	PCGDeterminismTests::GenerateSettings<UPCGNormalToDensitySettings>(TestData);
	UPCGNormalToDensitySettings* Settings = CastChecked<UPCGNormalToDensitySettings>(TestData.Settings);
	FPCGElementPtr Element = TestData.Settings->GetElement();

	Settings->DensityMode = PCGNormalToDensityMode::Divide;

	{
		TObjectPtr<UPCGPointData> PointData = PCGTestsCommon::CreatePointData();
		FPCGPoint& Point = PointData->GetMutablePoints()[0];

		Point.Transform.SetRotation(FQuat::MakeFromEuler(FVector(45.0, 0, 0)));
		Point.Density = 0.5f;

		FPCGTaggedData& TaggedData = TestData.InputData.TaggedData.Emplace_GetRef(FPCGTaggedData());
		TaggedData.Data = PointData;
		TaggedData.Pin = PCGPinConstants::DefaultInputLabel;
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!Element->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	UPCGSpatialData* OutputData = Cast<UPCGSpatialData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output data", OutputData);

	const UPCGPointData* OutputPointData = OutputData->ToPointData(Context.Get());
	UTEST_NOT_NULL("Output point data", OutputPointData);

	const TArray<FPCGPoint>& OutputPoints = OutputPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutputPoints.Num(), 1);

	UTEST_EQUAL_TOLERANCE("Output point density", OutputPoints[0].Density, 0.5f/0.707f, 0.001f);

	return true;
}

