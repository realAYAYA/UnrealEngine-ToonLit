// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Data/PCGPointData.h"

#include "Elements/PCGGather.h"

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGGatherTest_Basic, FPCGTestBaseClass, "Plugins.PCG.Gather.Basic", PCGTestsCommon::TestFlags)

bool FPCGGatherTest_Basic::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	PCGTestsCommon::GenerateSettings<UPCGGatherSettings>(TestData);

	FPCGElementPtr TestElement = TestData.Settings->GetElement();

	{
		FPCGTaggedData& SourcePin = TestData.InputData.TaggedData.Emplace_GetRef();
		SourcePin.Pin = PCGPinConstants::DefaultInputLabel;
		SourcePin.Data = PCGTestsCommon::CreateRandomPointData(100, 42);
	}

	TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

	while (!TestElement->Execute(Context.Get())) {}

	// test our point data
	const TArray<FPCGTaggedData>& Outputs = Context->OutputData.GetInputs();

	UTEST_EQUAL("Output count", Outputs.Num(), 1);

	const UPCGPointData *OutPointData = Cast<UPCGPointData>(Outputs[0].Data);

	UTEST_NOT_NULL("Output point data", OutPointData);

	const TArray<FPCGPoint>& OutPoints = OutPointData->GetPoints();

	UTEST_EQUAL("Output point count", OutPoints.Num(), 100);

	return true;
}

