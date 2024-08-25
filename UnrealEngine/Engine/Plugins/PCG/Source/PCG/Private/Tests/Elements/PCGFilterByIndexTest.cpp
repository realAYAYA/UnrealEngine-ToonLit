// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/PCGTestsCommon.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"

#include "Elements/PCGFilterByIndex.h"

namespace PCGFilterByIndexTestConstants
{
	static constexpr int32 InputNum = 5;
}

namespace PCGFilterByIndexTestPrivate
{
	TArray<FPCGTaggedData> ResetAndExecute(PCGTestsCommon::FTestData& TestData, UPCGFilterByIndexSettings* Settings)
	{
		TestData.Reset(Settings);
		const FPCGElementPtr TestElement = Settings->GetElement();

		for (int Index = 0; Index < PCGFilterByIndexTestConstants::InputNum; ++Index)
		{
			FPCGTaggedData& InputData = TestData.InputData.TaggedData.Emplace_GetRef();
			InputData.Pin = PCGPinConstants::DefaultInputLabel;
			InputData.Data = PCGTestsCommon::CreateRandomPointData(Index + 1, 42);
		}

		TUniquePtr<FPCGContext> Context = TestData.InitializeTestContext();

		while (!TestElement->Execute(Context.Get()))
		{
		}

		return Context->OutputData.GetInputs();
	}

	bool ValidateData(const TArray<FPCGTaggedData>& Outputs)
	{
		check(Outputs.Num() == PCGFilterByIndexTestConstants::InputNum);

		for (int I = 0; I < PCGFilterByIndexTestConstants::InputNum; ++I)
		{
			const UPCGPointData* OutPointData = Cast<UPCGPointData>(Outputs[I].Data);
			if (!OutPointData || (OutPointData->GetPoints().Num() != I + 1))
			{
				return false;
			}
		}

		return true;
	}
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGFilterByIndexTest_Basic, FPCGTestBaseClass, "Plugins.PCG.FilterByIndex.Basic", PCGTestsCommon::TestFlags)

bool FPCGFilterByIndexTest_Basic::RunTest(const FString& Parameters)
{
	using PCGFilterByIndexTestConstants::InputNum;

	PCGTestsCommon::FTestData TestData;
	UPCGFilterByIndexSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGFilterByIndexSettings>(TestData);

	// Empty index input string
	{
		Settings->SelectedIndices.Empty();
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_EQUAL("Output count", Outputs.Num(), InputNum);
		UTEST_TRUE("Output is valid", PCGFilterByIndexTestPrivate::ValidateData(Outputs));

		UTEST_EQUAL("Output 1 filtered exclusively", Outputs[0].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 2 filtered exclusively", Outputs[1].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 3 filtered exclusively", Outputs[2].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 4 filtered exclusively", Outputs[3].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 5 filtered exclusively", Outputs[4].Pin, PCGPinConstants::DefaultOutFilterLabel);
	}

	// Explicit index selection
	{
		Settings->SelectedIndices = FString("0,2,4");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_EQUAL("Output count", Outputs.Num(), InputNum);
		UTEST_TRUE("Output is valid", PCGFilterByIndexTestPrivate::ValidateData(Outputs));

		UTEST_EQUAL("Output 1 filtered inclusively", Outputs[0].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 2 filtered exclusively", Outputs[1].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 3 filtered inclusively", Outputs[2].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 4 filtered exclusively", Outputs[3].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 5 filtered inclusively", Outputs[4].Pin, PCGPinConstants::DefaultInFilterLabel);
	}

	// Negative index selection. 5 - 2 = 3
	{
		Settings->SelectedIndices = FString("-2");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_EQUAL("Output count", Outputs.Num(), InputNum);
		UTEST_TRUE("Output is valid", PCGFilterByIndexTestPrivate::ValidateData(Outputs));

		UTEST_EQUAL("Output 1 filtered exclusively", Outputs[0].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 2 filtered exclusively", Outputs[1].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 3 filtered exclusively", Outputs[2].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 4 filtered inclusively", Outputs[3].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 5 filtered exclusively", Outputs[4].Pin, PCGPinConstants::DefaultOutFilterLabel);
	}

	// Range index selection [1:3) = [1:2] since range slicing doesn't include last index
	{
		Settings->SelectedIndices = FString("1:3");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_EQUAL("Output count", Outputs.Num(), InputNum);
		UTEST_TRUE("Output is valid", PCGFilterByIndexTestPrivate::ValidateData(Outputs));

		UTEST_EQUAL("Output 1 filtered exclusively", Outputs[0].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 2 filtered inclusively", Outputs[1].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 3 filtered inclusively", Outputs[2].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 4 filtered exclusively", Outputs[3].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 5 filtered exclusively", Outputs[4].Pin, PCGPinConstants::DefaultOutFilterLabel);
	}

	// Range index selection (should be [5-3,5-1) = [2,4) = [2:3]
	{
		Settings->SelectedIndices = FString("-3:-1");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_EQUAL("Output count", Outputs.Num(), InputNum);
		UTEST_TRUE("Output is valid", PCGFilterByIndexTestPrivate::ValidateData(Outputs));

		UTEST_EQUAL("Output 1 filtered exclusively", Outputs[0].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 2 filtered exclusively", Outputs[1].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 3 filtered inclusively", Outputs[2].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 4 filtered inclusively", Outputs[3].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 5 filtered exclusively", Outputs[4].Pin, PCGPinConstants::DefaultOutFilterLabel);
	}

	// Combination index selection
	{
		Settings->SelectedIndices = FString("0,2:3,4");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_EQUAL("Output count", Outputs.Num(), InputNum);
		UTEST_TRUE("Output is valid", PCGFilterByIndexTestPrivate::ValidateData(Outputs));

		UTEST_EQUAL("Output 1 filtered inclusively", Outputs[0].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 2 filtered exclusively", Outputs[1].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 3 filtered inclusively", Outputs[2].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 4 filtered exclusively", Outputs[3].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 5 filtered inclusively", Outputs[4].Pin, PCGPinConstants::DefaultInFilterLabel);
	}

	// Combination index selection, with inverted filter
	{
		Settings->bInvertFilter = true;
		Settings->SelectedIndices = FString("0,2:3,4");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_EQUAL("Output count", Outputs.Num(), InputNum);
		UTEST_TRUE("Output is valid", PCGFilterByIndexTestPrivate::ValidateData(Outputs));

		UTEST_EQUAL("Output 1 filtered exclusively", Outputs[0].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 2 filtered inclusively", Outputs[1].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 3 filtered exclusively", Outputs[2].Pin, PCGPinConstants::DefaultOutFilterLabel);
		UTEST_EQUAL("Output 4 filtered inclusively", Outputs[3].Pin, PCGPinConstants::DefaultInFilterLabel);
		UTEST_EQUAL("Output 5 filtered exclusively", Outputs[4].Pin, PCGPinConstants::DefaultOutFilterLabel);
	}

	return true;
}

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGFilterByIndexTest_InvalidSelection, FPCGTestBaseClass, "Plugins.PCG.FilterByIndex.InvalidSelection", PCGTestsCommon::TestFlags)

bool FPCGFilterByIndexTest_InvalidSelection::RunTest(const FString& Parameters)
{
	PCGTestsCommon::FTestData TestData;
	UPCGFilterByIndexSettings* Settings = PCGTestsCommon::GenerateSettings<UPCGFilterByIndexSettings>(TestData);

	AddExpectedError("Invalid expression in index selection string", EAutomationExpectedMessageFlags::Contains, 3);

	// Test inverted range
	{
		Settings->SelectedIndices = FString("4:0");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_TRUE("Output count", Outputs.IsEmpty());
	}

	// Test inverted range with negative indices
	{
		Settings->SelectedIndices = FString("-1:-3");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_TRUE("Output count", Outputs.IsEmpty());
	}

	// Duplicate selection for range
	{
		Settings->SelectedIndices = FString("2:2");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_TRUE("Output count", Outputs.IsEmpty());
	}

	AddExpectedError("Invalid character in index selection string");

	// Test invalid character
	{
		Settings->SelectedIndices = FString("0,2:3,4*");
		const TArray<FPCGTaggedData> Outputs = PCGFilterByIndexTestPrivate::ResetAndExecute(TestData, Settings);

		UTEST_TRUE("Output count", Outputs.IsEmpty());
	}

	return true;
}