// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "EnhancedInputModule.h"
#include "InputMappingContext.h"
#include "InputTestFramework.h"
#include "Misc/AutomationTest.h"

// Tests focused on binding logic for both delegates and input devices



constexpr auto BasicBindingTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;	// TODO: Run as Smoke/Client? No world on RunSmokeTests startup...

UControllablePlayer& ABasicBindingTest(FAutomationTestBase* Test, EInputActionValueType ForValueType)
{
	// Initialise
	UWorld* World =
	GIVEN(AnEmptyWorld());

	UControllablePlayer& Data =
	AND(AControllablePlayer(World));
	Test->TestTrue(TEXT("Controllable Player is valid"), Data.IsValid());	// TODO: Can we early out on a failed Test?

	AND(AnInputContextIsAppliedToAPlayer(Data, TestContext, 0));
	UInputAction* Action =
	AND(AnInputAction(Data, TestAction, ForValueType));
	AND(AnActionIsMappedToAKey(Data, TestContext, TestAction, TestKey));

	return Data;
}

// ******************************
// Delegate firing (notification) tests
// ******************************


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputBindingDigitalTest, "Input.Binding.DigitalTrigger", BasicBindingTestFlags)

bool FInputBindingDigitalTest::RunTest(const FString& Parameters)
{
	UControllablePlayer& Data =
	GIVEN(ABasicBindingTest(this, EInputActionValueType::Boolean));

	// Unpressed shouldn't trigger
	WHEN(InputIsTicked(Data));
	THEN(!FInputTestHelper::TestTriggered(Data, TestAction));

	// Pressing
	WHEN(AKeyIsActuated(Data, TestKey));
	AND(InputIsTicked(Data));
	THEN(PressingKeyTriggersAction(Data, TestAction));

	// Holding for multiple ticks
	const int32 HoldTicks = 100;
	for (int32 i = 0; i < HoldTicks; ++i)
	{
		WHEN(InputIsTicked(Data));
		THEN(HoldingKeyTriggersAction(Data, TestAction));
	}

	// Releasing - does not fire canceled!
	WHEN(AKeyIsReleased(Data, TestKey));
	AND(InputIsTicked(Data));
	THEN(ReleasingKeyTriggersCompleted(Data, TestAction));

	WHEN(InputIsTicked(Data));
	THEN(ReleasingKeyDoesNotTrigger(Data, TestAction));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputBindingAnalogTest, "Input.Binding.AnalogTrigger", BasicBindingTestFlags)

bool FInputBindingAnalogTest::RunTest(const FString& Parameters)
{
	UControllablePlayer& Data =
	GIVEN(ABasicBindingTest(this, EInputActionValueType::Axis1D));

	// Unpressed shouldn't trigger
	WHEN(AnActionIsMappedToAKey(Data, TestContext, TestAction, TestAxis));
	AND(InputIsTicked(Data));
	THEN(!FInputTestHelper::TestTriggered(Data, TestAction));

	const TArray<float> TestValues = { 0.25f, 0.5f, 1.f, 1.f, -1.f, 0.75f, 0.5f, -0.1f};
	for(float TestValue : TestValues)
	{
		WHEN(AKeyIsActuated(Data, TestAxis, TestValue));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersAction(Data, TestAction));
		AND(TestEqual(TEXT("Trigger value"), FInputTestHelper::GetTriggered<float>(Data, TestAction), TestValue));
	}

	// Releasing - does not fire canceled!
	WHEN(AKeyIsReleased(Data, TestAxis));
	AND(InputIsTicked(Data));
	THEN(ReleasingKeyTriggersCompleted(Data, TestAction));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputBindingMultipleKeyValuesTest, "Input.Binding.MultipleKeyValues", BasicBindingTestFlags)

bool FInputBindingMultipleKeyValuesTest::RunTest(const FString& Parameters)
{
	const int32 HoldTicks = 10;

	UControllablePlayer& Data =
	GIVEN(ABasicBindingTest(this, EInputActionValueType::Boolean));
	AND(AnActionIsMappedToAKey(Data, TestContext, TestAction, TestKey2));	// Bind second key
	AND(AKeyIsActuated(Data, TestKey));	// Depress first key

	// Holding for multiple ticks
	for (int32 i = 0; i < HoldTicks; ++i)
	{
		WHEN(InputIsTicked(Data));
		THEN(HoldingKeyTriggersAction(Data, TestAction));
		AND(TestEqual(TEXT("Single keypress generates a consistent value over multiple ticks"), FInputTestHelper::GetTriggered<FVector>(Data, TestAction), FVector(1.f, 0.f, 0.f)));
	}

	// Switch keys in a single tick
	WHEN(AKeyIsReleased(Data, TestKey));
	AND(AKeyIsActuated(Data, TestKey2));

	// Holding for multiple ticks
	for (int32 i = 0; i < HoldTicks; ++i)
	{
		WHEN(InputIsTicked(Data));
		THEN(HoldingKeyTriggersAction(Data, TestAction));
		AND(TestEqual(TEXT("Switching keys in a single tick generates a consistent value"), FInputTestHelper::GetTriggered<FVector>(Data, TestAction), FVector(1.f, 0.f, 0.f)));
	}

	// Switch keys over 2 ticks
	WHEN(AKeyIsReleased(Data, TestKey2));
	AND(InputIsTicked(Data));
	AND(AKeyIsActuated(Data, TestKey));

	// Holding for multiple ticks
	for (int32 i = 0; i < HoldTicks; ++i)
	{
		WHEN(InputIsTicked(Data));
		THEN(HoldingKeyTriggersAction(Data, TestAction));
		AND(TestEqual(TEXT("Switching keys over two ticks generates a consistent value"), FInputTestHelper::GetTriggered<FVector>(Data, TestAction), FVector(1.f, 0.f, 0.f)));
	}

	// Release both keys
	WHEN(AKeyIsReleased(Data, TestKey));
	AND(InputIsTicked(Data));
	THEN(ReleasingKeyTriggersCompleted(Data, TestAction));

	// Double key press on a single tick with hold
	WHEN(AKeyIsActuated(Data, TestKey));
	AND(AKeyIsActuated(Data, TestKey2));
	for (int32 i = 0; i < HoldTicks; ++i)
	{
		WHEN(InputIsTicked(Data));
		THEN(HoldingKeyTriggersAction(Data, TestAction));
		AND(TestEqual(TEXT("Multiple key actuations on a single tick generates a consistent value"), FInputTestHelper::GetTriggered<FVector>(Data, TestAction), FVector(1.f, 0.f, 0.f)));
	}

	// Unpressed shouldn't trigger
	WHEN(AKeyIsReleased(Data, TestKey));
	AND(AKeyIsReleased(Data, TestKey2));
	WHEN(InputIsTicked(Data));
	THEN(!FInputTestHelper::TestTriggered(Data, TestAction));

	return true;
}

// TODO: Need to check merging logic for keys generating different levels of "pressedness" activity
// TODO: Merging of multiple input mapping contexts including priority and actions of various bConsumeInput states

