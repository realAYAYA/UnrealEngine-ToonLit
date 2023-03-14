// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "InputTestFramework.h"
#include "InputTriggers.h"
#include "InputModifiers.h"
#include "Misc/AutomationTest.h"

// Tests focused on individual triggers



constexpr auto BasicTriggerTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;	// TODO: Run as Smoke/Client? No world on RunSmokeTests startup...
constexpr auto DisabledBasicTriggerTestFlags = BasicTriggerTestFlags | EAutomationTestFlags::Disabled;

// Dumping ground for local trigger tests
static UInputTrigger* TestTrigger = nullptr;
static ETriggerState LastTestTriggerState = ETriggerState::None;

// This will be cleared out by GC as soon as it ticks
template<typename T>
T* ATrigger()
{
	return Cast<T>(TestTrigger = NewObject<T>());
}

void TriggerGetsValue(FInputActionValue Value, float DeltaTime = 0.f)
{
	LastTestTriggerState = ETriggerState::None;

	if (TestTrigger)
	{
		// TODO: Provide an EnhancedPlayerInput
		LastTestTriggerState = TestTrigger->UpdateState(nullptr, Value, DeltaTime);
		TestTrigger->LastValue = Value;
	}
}

// Must declare one of these around a subtest to use TriggerStateIs
#define TRIGGER_SUBTEST(DESC) \
	for(FString ScopedSubTestDescription = TEXT(DESC);ScopedSubTestDescription.Len();ScopedSubTestDescription = "")	// Bodge to create a scoped test description. Usage: TRIGGER_SUBTEST("My Test Description") { TestCode... TriggerStateIs(ETriggerState::Triggered); }

// Forced to true to stop multiple errors from the THEN() TestTrueExpr wrapper
#define TriggerStateIs(STATE) \
	(TestEqual(ScopedSubTestDescription, *UEnum::GetValueAsString(TEXT("EnhancedInput.ETriggerState"), LastTestTriggerState), *UEnum::GetValueAsString(TEXT("EnhancedInput.ETriggerState"), STATE)) || true)


// ******************************
// Delegate firing (notification) tests for device (FKey) based triggers
// ******************************

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerPressedTest, "Input.Triggers.Pressed", BasicTriggerTestFlags)

bool FInputTriggerPressedTest::RunTest(const FString& Parameters)
{
	TRIGGER_SUBTEST("1 - Instant trigger on press")
	{
		GIVEN(ATrigger<UInputTriggerPressed>());
		WHEN(TriggerGetsValue(true));
		THEN(TriggerStateIs(ETriggerState::Triggered));
	}

	TRIGGER_SUBTEST("2 - Trigger stops on release")
	{
		GIVEN(ATrigger<UInputTriggerPressed>());
		WHEN(TriggerGetsValue(true));
		AND(TriggerGetsValue(false));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("3 - Trigger stops on hold")
	{
		GIVEN(ATrigger<UInputTriggerPressed>());
		WHEN(TriggerGetsValue(true));
		AND(TriggerGetsValue(true));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerDownTest, "Input.Triggers.Down", BasicTriggerTestFlags)

bool FInputTriggerDownTest::RunTest(const FString& Parameters)
{
	TRIGGER_SUBTEST("Instant trigger on press")
	{
		GIVEN(ATrigger<UInputTriggerDown>());
		WHEN(TriggerGetsValue(true));
		THEN(TriggerStateIs(ETriggerState::Triggered));
	}

	TRIGGER_SUBTEST("Trigger stops on release")
	{
		GIVEN(ATrigger<UInputTriggerDown>());
		WHEN(TriggerGetsValue(true));
		AND(TriggerGetsValue(false));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("Trigger retained on hold")
	{
		GIVEN(ATrigger<UInputTriggerDown>());
		WHEN(TriggerGetsValue(true));
		AND(TriggerGetsValue(true));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Then lost on release 
		WHEN(TriggerGetsValue(false));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerReleasedTest, "Input.Triggers.Released", BasicTriggerTestFlags)

bool FInputTriggerReleasedTest::RunTest(const FString& Parameters)
{
	TRIGGER_SUBTEST("No trigger on press")
	{
		GIVEN(ATrigger<UInputTriggerReleased>());
		WHEN(TriggerGetsValue(true));
		THEN(TriggerStateIs(ETriggerState::Ongoing));
	}

	TRIGGER_SUBTEST("No trigger on hold")
	{
		WHEN(TriggerGetsValue(true));
		THEN(TriggerStateIs(ETriggerState::Ongoing));
	}

	TRIGGER_SUBTEST("Trigger on release")
	{
		WHEN(TriggerGetsValue(false));
		THEN(TriggerStateIs(ETriggerState::Triggered));
		// But only once
		WHEN(TriggerGetsValue(false));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("No trigger for no input")
	{
		GIVEN(ATrigger<UInputTriggerReleased>());
		WHEN(TriggerGetsValue(false));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	return true;
}

// TODO: Provide a player input pointer to run the Timed Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerHoldTest, "Input.Triggers.Hold", DisabledBasicTriggerTestFlags)

bool FInputTriggerHoldTest::RunTest(const FString& Parameters)
{
	const float FrameTime = 1.f / 60.f;
	const int HoldFrames = 30;	// Half second hold

	TRIGGER_SUBTEST("Release before threshold frame cancels")
	{
		GIVEN(ATrigger<UInputTriggerHold>())->HoldTimeThreshold = FrameTime * HoldFrames;
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Ongoing));
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}


	TRIGGER_SUBTEST("Holding to threshold fires trigger")
	{
		GIVEN(ATrigger<UInputTriggerHold>())->HoldTimeThreshold = FrameTime * HoldFrames;
		WHEN(TriggerGetsValue(true, FrameTime));
		for (int HoldFrame = 1; HoldFrame < HoldFrames - 1; ++HoldFrame)
		{
			AND(TriggerGetsValue(true, FrameTime));
			THEN(TriggerStateIs(ETriggerState::Ongoing));
		}
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Continues to fire
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Release stops fire
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("One shot trigger")
	{
		UInputTriggerHold* Trigger =
		GIVEN(ATrigger<UInputTriggerHold>());
		Trigger->HoldTimeThreshold = FrameTime * HoldFrames;
		Trigger->bIsOneShot = true;
		for (int HoldFrame = 0; HoldFrame < HoldFrames - 1; ++HoldFrame)
		{
			AND(TriggerGetsValue(true, FrameTime));
		}
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Stops firing
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	return true;
}

// TODO: Provide a player input pointer to run the Timed Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerHoldAndReleaseTest, "Input.Triggers.HoldAndRelease", DisabledBasicTriggerTestFlags)

bool FInputTriggerHoldAndReleaseTest::RunTest(const FString& Parameters)
{
	const float FrameTime = 1.f / 60.f;
	const int HoldFrames = 30;	// Half second hold

	TRIGGER_SUBTEST("Release before threshold frame does not trigger")
	{
		GIVEN(ATrigger<UInputTriggerHoldAndRelease>())->HoldTimeThreshold = FrameTime * HoldFrames;
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Ongoing));
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("Holding to threshold frame triggers")
	{
		// Hold to frame 29, release frame 30

		GIVEN(ATrigger<UInputTriggerHoldAndRelease>())->HoldTimeThreshold = FrameTime * HoldFrames;
		for (int HoldFrame = 0; HoldFrame < HoldFrames - 1; ++HoldFrame)
		{
			WHEN(TriggerGetsValue(true, FrameTime));
			THEN(TriggerStateIs(ETriggerState::Ongoing));
		}
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Ticking a further frame resets the trigger
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}


	TRIGGER_SUBTEST("Holding beyond threshold frame triggers")
	{
		// Hold to frame 30, release frame 31.

		GIVEN(ATrigger<UInputTriggerHoldAndRelease>())->HoldTimeThreshold = FrameTime * HoldFrames;
		for (int HoldFrame = 0; HoldFrame < HoldFrames; ++HoldFrame)
		{
			WHEN(TriggerGetsValue(true, FrameTime));
			THEN(TriggerStateIs(ETriggerState::Ongoing));
		}
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Ticking a further frame resets the trigger
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	return true;
}

// TODO: Provide a player input pointer to run the Timed Tests
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerTapTest, "Input.Triggers.Tap", DisabledBasicTriggerTestFlags)

bool FInputTriggerTapTest::RunTest(const FString& Parameters)
{
	const float FrameTime = 1.f / 60.f;
	const int MaxTapFrames = 10;

	TRIGGER_SUBTEST("Releasing on first frame fires trigger")
	{
		GIVEN(ATrigger<UInputTriggerTap>()->TapReleaseTimeThreshold = FrameTime * MaxTapFrames);

		// Pressing
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Ongoing));

		// Releasing immediately
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Ticking a further frame resets the trigger
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("Releasing on final frame fires trigger")
	{
		// Hold to frame 9, release on frame 10 = trigger.

		GIVEN(ATrigger<UInputTriggerTap>()->TapReleaseTimeThreshold = FrameTime * MaxTapFrames);
		// Holding until last trigger frame
		for (int HoldFrame = 0; HoldFrame < MaxTapFrames - 1; ++HoldFrame)
		{
			WHEN(TriggerGetsValue(true, FrameTime));
			THEN(TriggerStateIs(ETriggerState::Ongoing));
		}
		// Releasing
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::Triggered));

		// Ticking a further frame resets the trigger
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("Holding beyond final frame cancels trigger")
	{
		//Hold to frame 9, canceled on frame 10 as still actuated.

		GIVEN(ATrigger<UInputTriggerTap>()->TapReleaseTimeThreshold = FrameTime * MaxTapFrames);
		// Holding until last trigger frame
		for (int HoldFrame = 0; HoldFrame < MaxTapFrames - 1; ++HoldFrame)
		{
			WHEN(TriggerGetsValue(true, FrameTime));
			THEN(TriggerStateIs(ETriggerState::Ongoing));
		}
		// Holding past threshold
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));

		// Doesn't transition back to Ongoing
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));

		// Releasing doesn't trigger
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	TRIGGER_SUBTEST("Releasing immediately after final frame doesn't tick")
	{
		//Hold to frame 10, release frame 11.

		GIVEN(ATrigger<UInputTriggerTap>()->TapReleaseTimeThreshold = FrameTime * MaxTapFrames);
		// Holding until last trigger frame
		for (int HoldFrame = 0; HoldFrame < MaxTapFrames - 1 ; ++HoldFrame)
		{
			WHEN(TriggerGetsValue(true, FrameTime));
			THEN(TriggerStateIs(ETriggerState::Ongoing));
		}

		// Holding past threshold
		WHEN(TriggerGetsValue(true, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));

		// Releasing doesn't trigger
		WHEN(TriggerGetsValue(false, FrameTime));
		THEN(TriggerStateIs(ETriggerState::None));
	}

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerChordsMultiContextTest, "Input.Triggers.Chords.MultiContext", BasicTriggerTestFlags)

bool FInputTriggerChordsMultiContextTest::RunTest(const FString& Parameters)
{
	// Test chords work when the chord action is in a higher priority context
	FKey ChordKey = TestKey2;
	FName BaseAction = TEXT("BaseAction");				// Base action
	FName ChordedAction = TEXT("ChordedAction");			// Chord triggered action
	FName ChordingAction = TEXT("ChordingAction");		// Chording action driving special case e.g. ShiftDown/AcrobaticModifier

	UWorld* World =
	GIVEN(AnEmptyWorld());

	// Initialise
	UControllablePlayer& Data =
	AND(AControllablePlayer(World));

	FName BaseContext = TEXT("BaseContext"), ChordContext = TEXT("ChordContext");
	AND(AnInputContextIsAppliedToAPlayer(Data, BaseContext, 1));
	AND(AnInputContextIsAppliedToAPlayer(Data, ChordContext, 100));

	// Set up action
	AND(AnInputAction(Data, BaseAction, EInputActionValueType::Axis1D));
	AND(AnInputAction(Data, ChordedAction, EInputActionValueType::Axis1D));

	// Set up the chording action (modifier key action)
	AND(UInputAction * ChordingActionPtr = AnInputAction(Data, ChordingAction, EInputActionValueType::Boolean));

	// Bind the chording modifier in the high priority context
	AND(AnActionIsMappedToAKey(Data, ChordContext, ChordingAction, ChordKey));

	// Bind the action to the same key in both contexts
	AND(AnActionIsMappedToAKey(Data, BaseContext, BaseAction, TestAxis));
	AND(AnActionIsMappedToAKey(Data, ChordContext, ChordedAction, TestAxis));

	// But the chorded version inverts the result
	AND(AModifierIsAppliedToAnActionMapping(Data, NewObject<UInputModifierNegate>(), ChordContext, ChordedAction, TestAxis));

	// Apply a chord action trigger to the chorded mapping
	UInputTriggerChordAction* Trigger = NewObject<UInputTriggerChordAction>();
	Trigger->ChordAction = ChordingActionPtr;
	AND(ATriggerIsAppliedToAnActionMapping(Data, Trigger, ChordContext, ChordedAction, TestAxis));


	TRIGGER_SUBTEST("With chord key pressed neither main action triggers, but chording action does")
	{
		WHEN(AKeyIsActuated(Data, ChordKey));
		AND(InputIsTicked(Data));
		THEN(PressingKeyDoesNotTrigger(Data, BaseAction));
		ANDALSO(PressingKeyDoesNotTrigger(Data, ChordedAction));
		ANDALSO(PressingKeyTriggersAction(Data, ChordingAction));
	}

	const float AxisValue = 0.5f;

	TRIGGER_SUBTEST("Switching to test key the action supplies the unmodified value")
	{
		WHEN(AKeyIsReleased(Data, ChordKey));
		AND(AKeyIsActuated(Data, TestAxis, AxisValue));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersAction(Data, BaseAction));
		ANDALSO(ReleasingKeyTriggersCompleted(Data, ChordingAction));
		AND(TestEqual(TEXT("BaseAction"), FInputTestHelper::GetTriggered<float>(Data, BaseAction), AxisValue));
	}

	TRIGGER_SUBTEST("Depressing chord key triggers chorded action modified value")
	{
		WHEN(AKeyIsActuated(Data, ChordKey));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersAction(Data, ChordedAction));
		ANDALSO(ReleasingKeyTriggersCompleted(Data, BaseAction));
		AND(TestEqual(TEXT("BaseAction"), FInputTestHelper::GetTriggered<float>(Data, ChordedAction), -AxisValue));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerChordsSingleContextTest, "Input.Triggers.Chords.SingleContext", BasicTriggerTestFlags)

bool FInputTriggerChordsSingleContextTest::RunTest(const FString& Parameters)
{
	// Test chords work when all base, chorded, and chording actions are in the same context (they are processed in the correct order)
	FKey ChordKey = TestKey2;
	FName BaseAction = TEXT("BaseAction");				// Base action
	FName ChordedAction = TEXT("ChordedAction");			// Chord triggered action
	FName ChordingAction = TEXT("ChordingAction");		// Chording action driving special case e.g. ShiftDown/AcrobaticModifier

	UWorld* World =
		GIVEN(AnEmptyWorld());

	// Initialise
	UControllablePlayer& Data =
		AND(AControllablePlayer(World));

	FName SingleContext = TEXT("Context");
	AND(AnInputContextIsAppliedToAPlayer(Data, SingleContext, 1));

	// Set up action
	AND(AnInputAction(Data, BaseAction, EInputActionValueType::Axis1D));
	AND(AnInputAction(Data, ChordedAction, EInputActionValueType::Axis1D));

	// Set up the chording action (modifier key action)
	AND(UInputAction * ChordingActionPtr = AnInputAction(Data, ChordingAction, EInputActionValueType::Boolean));

	// Bind the chording modifier
	AND(AnActionIsMappedToAKey(Data, SingleContext, ChordingAction, ChordKey));

	// Bind the actions to the same key
	AND(AnActionIsMappedToAKey(Data, SingleContext, BaseAction, TestAxis));
	AND(AnActionIsMappedToAKey(Data, SingleContext, ChordedAction, TestAxis));

	// But the chorded version inverts the result
	AND(AModifierIsAppliedToAnActionMapping(Data, NewObject<UInputModifierNegate>(), SingleContext, ChordedAction, TestAxis));

	// Apply a chord action trigger to the chorded mapping
	UInputTriggerChordAction* Trigger = NewObject<UInputTriggerChordAction>();
	Trigger->ChordAction = ChordingActionPtr;
	AND(ATriggerIsAppliedToAnActionMapping(Data, Trigger, SingleContext, ChordedAction, TestAxis));


	TRIGGER_SUBTEST("With chord key pressed neither main action triggers, but chording action does")
	{
		WHEN(AKeyIsActuated(Data, ChordKey));
		AND(InputIsTicked(Data));
		THEN(PressingKeyDoesNotTrigger(Data, BaseAction));
		ANDALSO(PressingKeyDoesNotTrigger(Data, ChordedAction));
		ANDALSO(PressingKeyTriggersAction(Data, ChordingAction));
	}

	const float AxisValue = 0.5f;

	TRIGGER_SUBTEST("Switching to test key the action supplies the unmodified value")
	{
		WHEN(AKeyIsReleased(Data, ChordKey));
		AND(AKeyIsActuated(Data, TestAxis, AxisValue));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersAction(Data, BaseAction));
		ANDALSO(ReleasingKeyTriggersCompleted(Data, ChordingAction));
		AND(TestEqual(TEXT("BaseAction"), FInputTestHelper::GetTriggered<float>(Data, BaseAction), AxisValue));
	}

	TRIGGER_SUBTEST("Depressing chord key triggers chorded action modified value")
	{
		WHEN(AKeyIsActuated(Data, ChordKey));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersAction(Data, ChordedAction));
		ANDALSO(ReleasingKeyTriggersCompleted(Data, BaseAction));
		AND(TestEqual(TEXT("BaseAction"), FInputTestHelper::GetTriggered<float>(Data, ChordedAction), -AxisValue));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInputTriggerChordsPressedTriggersTest, "Input.Triggers.Chords.WithPressedTriggers", BasicTriggerTestFlags)

bool FInputTriggerChordsPressedTriggersTest::RunTest(const FString& Parameters)
{
	// Test chord behavior with pressed triggers
	// Expected: Main key trigger state should be retained by both base and chorded action, across any chord key state transitions.
	// Pressing or releasing the chord key shouldn't cause any action to trigger by itself (Note: triggering would continue to occur for a down trigger).

	FKey ChordKey = TestKey2;
	FName BaseAction = TEXT("BaseAction");				// Base action
	FName ChordedAction = TEXT("ChordedAction");			// Chord triggered action
	FName ChordingAction = TEXT("ChordingAction");		// Chording action driving special case e.g. ShiftDown/AcrobaticModifier

	UWorld* World =
		GIVEN(AnEmptyWorld());

	// Initialise
	UControllablePlayer& Data =
		AND(AControllablePlayer(World));

	FName BaseContext = TEXT("BaseContext"), ChordContext = TEXT("ChordContext");
	AND(AnInputContextIsAppliedToAPlayer(Data, BaseContext, 1));
	AND(AnInputContextIsAppliedToAPlayer(Data, ChordContext, 100));

	// Set up action
	AND(AnInputAction(Data, BaseAction, EInputActionValueType::Boolean));
	AND(AnInputAction(Data, ChordedAction, EInputActionValueType::Boolean));

	// Set up the chording action (modifier key action)
	AND(UInputAction * ChordingActionPtr = AnInputAction(Data, ChordingAction, EInputActionValueType::Boolean));

	// Bind the chording modifier in the high priority context
	AND(AnActionIsMappedToAKey(Data, ChordContext, ChordingAction, ChordKey));

	// Bind the actions to the same key in both contexts
	AND(AnActionIsMappedToAKey(Data, BaseContext, BaseAction, TestKey));
	AND(AnActionIsMappedToAKey(Data, ChordContext, ChordedAction, TestKey));

	// Apply pressed triggers to both actions
	AND(ATriggerIsAppliedToAnAction(Data, NewObject<UInputTriggerPressed>(), BaseAction));
	AND(ATriggerIsAppliedToAnAction(Data, NewObject<UInputTriggerPressed>(), ChordedAction));

	// Apply a chord action trigger to the chorded mapping
	UInputTriggerChordAction* Trigger = NewObject<UInputTriggerChordAction>();
	Trigger->ChordAction = ChordingActionPtr;
	AND(ATriggerIsAppliedToAnAction(Data, Trigger, ChordedAction));

	TRIGGER_SUBTEST("Pressing key triggers base action")
	{
		WHEN(AKeyIsActuated(Data, TestKey));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersAction(Data, BaseAction));
		ANDALSO(PressingKeyDoesNotTrigger(Data, ChordedAction));
	}

	TRIGGER_SUBTEST("Pressing chord key does not trigger chorded action, but stops base")
	{
		WHEN(AKeyIsActuated(Data, ChordKey));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersStarted(Data, ChordedAction));	// Begins tracking trigger...	// TODO: Started -> Permanent Ongoing. The implicit chord action is true, but explict Pressed blocks. Make chord action a 4th type? ImplicitBlocker?
		THEN(!PressingKeyTriggersAction(Data, ChordedAction));	// but does not fire
		ANDALSO(PressingKeyTriggersCompleted(Data, BaseAction));
	}

	// Release main key
	AKeyIsReleased(Data, TestKey);
	InputIsTicked(Data);

	TRIGGER_SUBTEST("Pressing key again triggers chorded action only")
	{
		WHEN(AKeyIsActuated(Data, TestKey));
		AND(InputIsTicked(Data));
		THEN(PressingKeyTriggersAction(Data, ChordedAction));
		ANDALSO(PressingKeyDoesNotTrigger(Data, BaseAction));
	}

	TRIGGER_SUBTEST("Releasing chord key stops chorded action but does not trigger base action")
	{
		WHEN(AKeyIsReleased(Data, ChordKey));
		AND(InputIsTicked(Data));
		THEN(ReleasingKeyTriggersCompleted(Data, ChordedAction));
		ANDALSO(ReleasingKeyDoesNotTrigger(Data, BaseAction));
	}

	return true;
}

// TODO: Action level triggers (simple repeat of device level tests)
// TODO: Variable frame delta tests
// TODO: ActionEventData tests (timing, summed values, etc)