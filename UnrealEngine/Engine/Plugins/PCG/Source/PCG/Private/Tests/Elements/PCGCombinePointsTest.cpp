// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"

#include "Elements/PCGCombinePoints.h"

namespace CombinePointsCommonTests
{
	TUniquePtr<FPCGContext> GenerateTestDataAndRun(bool CenterPivot, bool UseSourceTransform, FTransform PtTransform)
	{
		PCGTestsCommon::FTestData TestData;
		PCGTestsCommon::GenerateSettings<UPCGCombinePointsSettings>(TestData);
		UPCGCombinePointsSettings* Settings = CastChecked<UPCGCombinePointsSettings>(TestData.Settings);
		Settings->bCenterPivot = CenterPivot;
		Settings->bUseFirstPointTransform = UseSourceTransform;
		Settings->PointTransform = PtTransform;

		FPCGTaggedData& Inputs = TestData.InputData.TaggedData.Emplace_GetRef();
		Inputs.Pin = PCGPinConstants::DefaultInputLabel;
		UPCGPointData* InData = PCGTestsCommon::CreateRandomPointData(5, 42, false);
		Inputs.Data = InData;

		// Setting the transform explicitly
		UPCGPointData* InputPointData = Cast<UPCGPointData>(InData);
		TArray<FPCGPoint>& InPoints = InputPointData->GetMutablePoints();
		for (int i = 0; i < InPoints.Num(); ++i)
		{
			InPoints[i].Transform.SetLocation(FVector(10.0, 10.0, 10.0) * i);
			InPoints[i].Transform.SetRotation(FQuat::Identity);
			InPoints[i].Transform.SetScale3D(FVector::One() * i + 1);
		}

		FPCGElementPtr TestElement = TestData.Settings->GetElement();

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		while (!TestElement->Execute(Context.Get())) {}

		return Context;
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCombinePointsTest_Default, FPCGTestBaseClass, "Plugins.PCG.CombinePoints.Default", PCGTestsCommon::TestFlags)

bool FPCGCombinePointsTest_Default::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = CombinePointsCommonTests::GenerateTestDataAndRun(true, true, FTransform());

	// Test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("OutputA point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("OutputA point count", OutPoints.Num(), 1);

	/** Point[0] at(0)->bounds(-1, 1)
	 * Point [1] at (10) -> bounds (8, 12)
	 * Point [2] at (20) -> bounds (17, 23)
	 * Point [3] at (30) -> bounds (26, 34)
	 * Point [4] at (40) -> bounds (35, 45)
	 * 45 + (-1) / 2 == 22 <- check center pivot
	 * 45 + |-1| / 2 == 23 <- check extents */

	UTEST_EQUAL("Output Location", OutPoints[0].Transform.GetLocation(), FVector(22.0));
	UTEST_EQUAL("Output Bounds Min", OutPoints[0].BoundsMin, FVector(-23.0));
	UTEST_EQUAL("Output Bounds Max", OutPoints[0].BoundsMax, FVector(23.0));
	
	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCombinePointsTest_SourceTransform, FPCGTestBaseClass, "Plugins.PCG.CombinePoints.SourceTransform", PCGTestsCommon::TestFlags)

bool FPCGCombinePointsTest_SourceTransform::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = CombinePointsCommonTests::GenerateTestDataAndRun(false, true, FTransform());

	// Test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("OutputA point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("OutputA point count", OutPoints.Num(), 1);

	UTEST_EQUAL("Output Location", OutPoints[0].Transform.GetLocation(), FVector(0.0));
	UTEST_EQUAL("Output Bounds Min", OutPoints[0].BoundsMin, FVector(-1.0));
	UTEST_EQUAL("Output Bounds Max", OutPoints[0].BoundsMax, FVector(45.0));

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCombinePointsTest_TransformLocation, FPCGTestBaseClass, "Plugins.PCG.CombinePoints.TransformLocation", PCGTestsCommon::TestFlags)

bool FPCGCombinePointsTest_TransformLocation::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = CombinePointsCommonTests::GenerateTestDataAndRun(false, false, FTransform(FRotator::ZeroRotator, FVector(10.0), FVector::One()));

	// Test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("OutputA point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("OutputA point count", OutPoints.Num(), 1);

	// Has min at -1, max at 45 at origin
	// Bound extents shift by (10) 

	UTEST_EQUAL("Output Location", OutPoints[0].Transform.GetLocation(), FVector(10.0));
	UTEST_EQUAL("Output Bounds Min", OutPoints[0].BoundsMin, FVector(-11.0));
	UTEST_EQUAL("Output Bounds Max", OutPoints[0].BoundsMax, FVector(35.0));

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCombinePointsTest_TransformRotation, FPCGTestBaseClass, "Plugins.PCG.CombinePoints.TransformRotation", PCGTestsCommon::TestFlags)

bool FPCGCombinePointsTest_TransformRotation::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = CombinePointsCommonTests::GenerateTestDataAndRun(false, false, FTransform(FRotator(90.0, 0.0, 0.0), FVector::ZeroVector, FVector::One()));

	// Test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("OutputA point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("OutputA point count", OutPoints.Num(), 1);

	// Rotate a cube around X axis
	UTEST_EQUAL("Output Location", OutPoints[0].Transform.GetLocation(), FVector::ZeroVector);
	UTEST_EQUAL("Output Bounds Min", OutPoints[0].BoundsMin, FVector(-1.0, -1.0, -45.0));
	UTEST_EQUAL("Output Bounds Max", OutPoints[0].BoundsMax, FVector(45.0, 45.0, 1.0));

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCombinePointsTest_TransformScale, FPCGTestBaseClass, "Plugins.PCG.CombinePoints.TransformScale", PCGTestsCommon::TestFlags)

bool FPCGCombinePointsTest_TransformScale::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = CombinePointsCommonTests::GenerateTestDataAndRun(false, false, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(2.0)));

	// Test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("OutputA point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("OutputA point count", OutPoints.Num(), 1);

	UTEST_EQUAL("Output Location", OutPoints[0].Transform.GetLocation(), FVector::ZeroVector);
	UTEST_EQUAL("Output Bounds Min", OutPoints[0].BoundsMin, FVector(-0.5));
	UTEST_EQUAL("Output Bounds Max", OutPoints[0].BoundsMax, FVector(22.5));

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGCombinePointsTest_TransformNegScale, FPCGTestBaseClass, "Plugins.PCG.CombinePoints.TransformNegScale", PCGTestsCommon::TestFlags)

bool FPCGCombinePointsTest_TransformNegScale::RunTest(const FString& Parameters)
{
	TUniquePtr<FPCGContext> Context = CombinePointsCommonTests::GenerateTestDataAndRun(false, false, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(-2.0)));

	// Test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("OutputA point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("OutputA point count", OutPoints.Num(), 1);

	UTEST_EQUAL("Output Location", OutPoints[0].Transform.GetLocation(), FVector::ZeroVector);
	UTEST_EQUAL("Output Bounds Min", OutPoints[0].BoundsMin, FVector(-22.5));
	UTEST_EQUAL("Output Bounds Max", OutPoints[0].BoundsMax, FVector(0.5));

	return true;
}