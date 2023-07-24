// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/AutomationTest.h"
#include "IO/IoDispatcher.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FIoStatusTestType
{
	FIoStatusTestType() { }
	FIoStatusTestType(const FIoStatusTestType& Other)
		: Text(Other.Text) { }
	FIoStatusTestType(FIoStatusTestType&&) = default;

	FIoStatusTestType(const FString& InText)
		: Text(InText) { }
	FIoStatusTestType(FString&& InText)
		: Text(MoveTemp(InText)) { }

	FIoStatusTestType& operator=(const FIoStatusTestType& Other) = default;
	FIoStatusTestType& operator=(FIoStatusTestType&& Other) = default;
	FIoStatusTestType& operator=(const FString& OtherText)
	{
		Text = OtherText;
		return *this;
	}

	FString Text;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIoStatusOrTest, "System.Core.IO.IoStatusOr", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

void TestConstruct(FAutomationTestBase& Test)
{
	{
		TIoStatusOr<FIoStatusTestType> Result;
		Test.TestEqual("Default IoStatus is Unknown", Result.Status(), FIoStatus::Unknown);
	}

	{
		const TIoStatusOr<FIoStatusTestType> Other;
		TIoStatusOr<FIoStatusTestType> Result(Other);
		Test.TestEqual("Copy construct", Result.Status(), FIoStatus::Unknown);
	}

	{
		const FIoStatus IoStatus(EIoErrorCode::InvalidCode);
		TIoStatusOr<FIoStatusTestType> Result(IoStatus);
		Test.TestEqual("Construct with status", Result.Status().GetErrorCode(), EIoErrorCode::InvalidCode);
	}

	{
		const FString ExpectedText("Unreal");
		const FIoStatusTestType Type(ExpectedText);
		TIoStatusOr<FIoStatusTestType> Result(Type);
		Test.TestEqual("Construct with value", Result.ValueOrDie().Text, ExpectedText);
	}

	{
		const FString ExpectedText("Unreal");
		TIoStatusOr<FIoStatusTestType> Result(FIoStatusTestType("Unreal"));
		Test.TestEqual("Construct with temporary value", Result.ValueOrDie().Text, ExpectedText);
	}

	{
		TIoStatusOr<FIoStatusTestType> Result(FString("Unreal"));
		Test.TestEqual("Construct with value arguments", Result.ValueOrDie().Text, FString("Unreal"));
	}
}

void TestAssignment(FAutomationTestBase& Test)
{
	{
		const EIoErrorCode ExpectedErrorCode = EIoErrorCode::InvalidCode;
		TIoStatusOr<FIoStatusTestType> Other = FIoStatus(ExpectedErrorCode);
		TIoStatusOr<FIoStatusTestType> Result;
		Result = Other;
		Test.TestEqual("Assign IoStatusOr with status", Result.Status().GetErrorCode(), ExpectedErrorCode);
	}

	{
		const EIoErrorCode ExpectedErrorCode = EIoErrorCode::InvalidCode;
		TIoStatusOr<FIoStatusTestType> Result;
		Result = TIoStatusOr<FIoStatusTestType>(FIoStatus(ExpectedErrorCode));
		Test.TestEqual("Assign temporary IoStatusOr with status", Result.Status().GetErrorCode(), ExpectedErrorCode);
	}

	{
		const FString ExpectedText("Unreal");
		TIoStatusOr<FIoStatusTestType> Other = FIoStatusTestType(ExpectedText);
		TIoStatusOr<FIoStatusTestType> Result;
		Result = Other;
		Test.TestEqual("Assign IoStatusOr with value", Result.ValueOrDie().Text, ExpectedText);
	}

	{
		const FString ExpectedText("Unreal");
		TIoStatusOr<FIoStatusTestType> Result;
		Result = TIoStatusOr<FIoStatusTestType>(ExpectedText);
		Test.TestEqual("Assign temporary IoStatusOr with value", Result.ValueOrDie().Text, ExpectedText);
	}

	{
		const EIoErrorCode ExpectedErrorCode = EIoErrorCode::InvalidCode;
		const FIoStatus IoStatus(ExpectedErrorCode);
		TIoStatusOr<FIoStatusTestType> Result;
		Result = IoStatus; 
		Test.TestEqual("Assign status", Result.Status().GetErrorCode(), ExpectedErrorCode);
	}

	{
		const EIoErrorCode ExpectedErrorCode = EIoErrorCode::InvalidCode;
		TIoStatusOr<FIoStatusTestType> Result;
		Result = FIoStatus(ExpectedErrorCode);
		Test.TestEqual("Assign temporary status", Result.Status().GetErrorCode(), ExpectedErrorCode);
	}

	{
		const FString ExpectedText("Unreal");
		const FIoStatusTestType Value(ExpectedText);
		TIoStatusOr<FIoStatusTestType> Result;
		Result = Value;
		Test.TestEqual("Assign value", Result.ValueOrDie().Text, ExpectedText);
	}

	{
		const FString ExpectedText("Unreal");
		TIoStatusOr<FIoStatusTestType> Result;
		Result = FIoStatusTestType(ExpectedText);
		Test.TestEqual("Assign temporary value", Result.ValueOrDie().Text, ExpectedText);
	}
}

void TestConsumeValue(FAutomationTestBase& Test)
{
	const FString ExpectedText("Unreal");
	TIoStatusOr<FIoStatusTestType> Result = FIoStatusTestType(ExpectedText);
	FIoStatusTestType Value = Result.ConsumeValueOrDie();
	Test.TestEqual("Consume value or die with valid value", Value.Text, ExpectedText);
}

bool FIoStatusOrTest::RunTest(const FString& Parameters)
{
	TestConstruct(*this);
	TestAssignment(*this);
	TestConsumeValue(*this);

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
