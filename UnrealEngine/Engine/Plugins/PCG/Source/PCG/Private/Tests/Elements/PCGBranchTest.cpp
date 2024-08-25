// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Elements/ControlFlow/PCGBranch.h"
#include "Tests/PCGTestsCommon.h"

#if WITH_EDITOR

class FPCGBranchTest : public FPCGTestBaseClass
{
	using FPCGTestBaseClass::FPCGTestBaseClass;

protected:
	bool ExecuteTest(const bool bOutputToB)
	{
		PCGTestsCommon::FTestData TestData;
		UPCGBranchSettings* TestSettings = PCGTestsCommon::GenerateSettings<UPCGBranchSettings>(TestData);
		TestSettings->bOutputToB = bOutputToB;

		const FPCGElementPtr TestElement = TestSettings->GetElement();

		UTEST_TRUE("Test element created and valid", TestElement.IsValid());

		FPCGTaggedData& InputTaggedData = TestData.InputData.TaggedData.Emplace_GetRef();
		InputTaggedData.Data = PCGTestsCommon::CreateEmptyPointData();
		InputTaggedData.Pin = PCGPinConstants::DefaultInputLabel;

		const TUniquePtr<FPCGContext> TestContext = TestData.InitializeTestContext();

		UTEST_TRUE("Test context created and valid", TestContext.IsValid());

		while (!TestElement->Execute(TestContext.Get()))
		{
		}

		const TArray<FPCGTaggedData> FirstOutputData = TestContext->OutputData.GetInputsByPin(FName("Output A"));
		const TArray<FPCGTaggedData> SecondOutputData = TestContext->OutputData.GetInputsByPin(FName("Output B"));

		UTEST_TRUE("Output passed through to the correct pin", bOutputToB ? !SecondOutputData.IsEmpty() : !FirstOutputData.IsEmpty());

		UTEST_TRUE("Output not passed through to the incorrect pin", bOutputToB ? FirstOutputData.IsEmpty() : SecondOutputData.IsEmpty());

		UTEST_EQUAL("Only one output", FirstOutputData.Num() + SecondOutputData.Num(), 1);

		return true;
	}
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FPCGBranchTest_Basic, FPCGBranchTest, "Plugins.PCG.Branch.Basic", PCGTestsCommon::TestFlags)

bool FPCGBranchTest_Basic::RunTest(const FString& Parameters)
{
	TArray<FPCGTaggedData> OutputData;

	UTEST_TRUE("'Output to A' test succeeded", ExecuteTest(false));

	UTEST_TRUE("'Output to B' test succeeded", ExecuteTest(true));

	return true;
}

#endif // WITH_EDITOR