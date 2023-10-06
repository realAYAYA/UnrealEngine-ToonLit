// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Logging/LogMacros.h"
#include "Misc/AutomationTest.h"
#include "Binding/States/WidgetStateSettings.h"
#include "Binding/States/WidgetStateBitfield.h"
#include "Binding/States/WidgetStateRegistration.h"
#include "Components/CheckBox.h"

/**
 * Coverage:
 *
 * OperatorBool
 * OperatorBitwiseAnd (Intersection)
 * OperatorBitwiseOr (Union)
 * OperatorBitwiseNot
 * HasFlag
 *
 * Note: Checkbox include is only needed if you want to use 'UWidgetCheckedStateRegistration' pre-defined
 * bitfields. It is possible to just re-create these yourself if you don't want to include checkbox.
 * Or more generally, a specific module that you may / may not know exists.
 */

#if WITH_DEV_AUTOMATION_TESTS

DEFINE_LOG_CATEGORY_STATIC(LogWidgetStateBitfieldTest, Log, All);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOperatorBoolTest, "Slate.WidgetState.OperatorBoolTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOperatorBoolTest::RunTest(const FString& Parameters)
{
	bool GPassing = true;

	{
		FWidgetStateBitfield Test = {};
		GPassing &= TestEqual(TEXT("Unset is false"), Test, false);
	}

	{
		FWidgetStateBitfield Test = {};

		Test.SetBinaryStateSlow(FName("Pressed"), true);
		GPassing &= TestEqual(TEXT("Any binary is true"), Test, true);

		GPassing &= TestEqual(TEXT("Has binary is true"), Test.HasBinaryStates(), true);

		Test.SetBinaryStateSlow(FName("Pressed"), false);
		GPassing &= TestEqual(TEXT("No binary is false"), Test, false);

		GPassing &= TestEqual(TEXT("Has binary is false"), Test.HasBinaryStates(), false);
	}

	{
		FWidgetStateBitfield Test = {};

		Test.SetEnumStateSlow(FName("CheckedState"), 1);
		GPassing &= TestEqual(TEXT("Any enum is true"), Test, true);

		GPassing &= TestEqual(TEXT("Has enum is true"), Test.HasEnumStates(), true);
		GPassing &= TestEqual(TEXT("Has empty unused is false"), Test.HasEmptyUsedEnumStates(), false);

		Test.ClearEnumState(FName("CheckedState"));
		GPassing &= TestEqual(TEXT("No enum is false"), Test, false);

		GPassing &= TestEqual(TEXT("Has enum is false"), Test.HasEnumStates(), false);
		GPassing &= TestEqual(TEXT("Has empty unused is false - none in use"), Test.HasEmptyUsedEnumStates(), false);
	}

	return GPassing;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOperatorBitwiseAndTest, "Slate.WidgetState.OperatorBitwiseAndTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOperatorBitwiseAndTest::RunTest(const FString& Parameters)
{
	bool GPassing = true;

	{
		FWidgetStateBitfield Pressed = {};
		FWidgetStateBitfield Hovered = {};
		FWidgetStateBitfield PressedHovered = {};

		Pressed.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);
		Hovered.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);
		PressedHovered.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);
		PressedHovered.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);

		GPassing &= TestEqual(TEXT("Pressed & Hovered is false"), Pressed.Intersect(Hovered), false);
		GPassing &= TestEqual(TEXT("Hovered & Pressed is false"), Hovered.Intersect(Pressed), false);

		GPassing &= TestEqual(TEXT("PressedHovered & Pressed is true"), PressedHovered.Intersect(Pressed), true);
		GPassing &= TestEqual(TEXT("Pressed & PressedHovered is true"), Pressed.Intersect(PressedHovered), true);

		GPassing &= TestEqual(TEXT("PressedHovered & Hovered is true"), PressedHovered.Intersect(Hovered), true);
		GPassing &= TestEqual(TEXT("Hovered & PressedHovered is true"), Hovered.Intersect(PressedHovered), true);

		GPassing &= TestEqual(TEXT("PressedHovered & PressedHovered is true"), PressedHovered.Intersect(PressedHovered), true);
		GPassing &= TestEqual(TEXT("PressedHovered & PressedHovered is true"), PressedHovered.Intersect(PressedHovered), true);

		GPassing &= TestEqual(TEXT("Bit: Pressed & Hovered is false"), UWidgetPressedStateRegistration::Bit.Intersect(UWidgetHoveredStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Bit: Hovered & Pressed is false"), UWidgetHoveredStateRegistration::Bit.Intersect(UWidgetPressedStateRegistration::Bit), false);

		GPassing &= TestEqual(TEXT("Bit: PressedHovered & Pressed is true"), PressedHovered.Intersect(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Bit: Pressed & PressedHovered is true"), UWidgetPressedStateRegistration::Bit.Intersect(PressedHovered), true);

		GPassing &= TestEqual(TEXT("Bit: PressedHovered & Hovered is true"), PressedHovered.Intersect(UWidgetHoveredStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Bit: Hovered & PressedHovered is true"), UWidgetHoveredStateRegistration::Bit.Intersect(PressedHovered), true);
	}

	{
		FWidgetStateBitfield Checked = {};
		FWidgetStateBitfield Unchecked = {};

		Checked.SetEnumState(UWidgetCheckedStateRegistration::Checked);
		Unchecked.SetEnumState(UWidgetCheckedStateRegistration::Unchecked);

		GPassing &= TestEqual(TEXT("Checked & Unchecked is false"), Checked.Intersect(Unchecked), false);

		GPassing &= TestEqual(TEXT("Bit: Checked & Unchecked is false"), UWidgetCheckedStateRegistration::Checked.Intersect(UWidgetCheckedStateRegistration::Unchecked), false);
		GPassing &= TestEqual(TEXT("Bit: Checked & Unchecked is false"), Checked.Intersect(UWidgetCheckedStateRegistration::Unchecked), false);
		GPassing &= TestEqual(TEXT("Bit: Checked & Unchecked is false"), UWidgetCheckedStateRegistration::Checked.Intersect(Unchecked), false);
	}

	{
		FWidgetStateBitfield Checked = {};
		FWidgetStateBitfield Hovered = {};
		FWidgetStateBitfield CheckedHovered = {};

		Checked.SetEnumState(UWidgetCheckedStateRegistration::Checked);
		Hovered.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);

		CheckedHovered.SetEnumState(UWidgetCheckedStateRegistration::Checked);
		CheckedHovered.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);

		GPassing &= TestEqual(TEXT("Checked & Hovered is false"), Checked.Intersect(Hovered), false);
		GPassing &= TestEqual(TEXT("Checked & CheckedHovered is false"), Checked.Intersect(CheckedHovered), false);
		GPassing &= TestEqual(TEXT("CheckedHovered & Checked is true"), CheckedHovered.Intersect(Checked), true);
		GPassing &= TestEqual(TEXT("Hovered & CheckedHovered is false"), Hovered.Intersect(CheckedHovered), false);
		GPassing &= TestEqual(TEXT("CheckedHovered & Hovered is true"), CheckedHovered.Intersect(Hovered), true);
		GPassing &= TestEqual(TEXT("CheckedHovered & CheckedHovered is true"), CheckedHovered.Intersect(CheckedHovered), true);
	}

	return GPassing;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOperatorBitwiseOrTest, "Slate.WidgetState.OperatorBitwiseOrTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOperatorBitwiseOrTest::RunTest(const FString& Parameters)
{
	bool GPassing = true;

	{
		FWidgetStateBitfield Pressed = {};
		FWidgetStateBitfield Hovered = {};
		FWidgetStateBitfield PressedHovered = {};

		Pressed.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);
		Hovered.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);
		PressedHovered.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);
		PressedHovered.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);

		GPassing &= TestEqual(TEXT("Pressed | Hovered is PressedHovered"), Pressed.Union(Hovered), PressedHovered);
		GPassing &= TestEqual(TEXT("Hovered | Pressed is PressedHovered"), Hovered.Union(Pressed), PressedHovered);

		GPassing &= TestEqual(TEXT("PressedHovered | Pressed is PressedHovered"), PressedHovered.Union(Pressed), PressedHovered);
		GPassing &= TestEqual(TEXT("Pressed | PressedHovered is PressedHovered"), Pressed.Union(PressedHovered), PressedHovered);

		GPassing &= TestEqual(TEXT("PressedHovered | Hovered is PressedHovered"), PressedHovered.Union(Hovered), PressedHovered);
		GPassing &= TestEqual(TEXT("Hovered | PressedHovered is PressedHovered"), Hovered.Union(PressedHovered), PressedHovered);

		GPassing &= TestEqual(TEXT("Bit: Pressed | Hovered is PressedHovered"), UWidgetPressedStateRegistration::Bit.Union(UWidgetHoveredStateRegistration::Bit), PressedHovered);
		GPassing &= TestEqual(TEXT("Bit: Hovered | Pressed is PressedHovered"), UWidgetHoveredStateRegistration::Bit.Union(UWidgetPressedStateRegistration::Bit), PressedHovered);
	}

	{
		FWidgetStateBitfield Checked = {};
		FWidgetStateBitfield Unchecked = {};
		FWidgetStateBitfield CheckedUnchecked = {};

		Checked.SetEnumState(UWidgetCheckedStateRegistration::Checked);
		Unchecked.SetEnumState(UWidgetCheckedStateRegistration::Unchecked);
		CheckedUnchecked.SetEnumState(UWidgetCheckedStateRegistration::Checked.Union(UWidgetCheckedStateRegistration::Unchecked));

		GPassing &= TestEqual(TEXT("Checked | Unchecked is CheckedUnchecked"), Checked.Union(Unchecked), CheckedUnchecked);
		GPassing &= TestEqual(TEXT("Checked | Unchecked is true"), Checked.Union(Unchecked), true);

		GPassing &= TestEqual(TEXT("CheckedUnchecked & Checked is true"), CheckedUnchecked.Intersect(Checked), true);
		GPassing &= TestEqual(TEXT("CheckedUnchecked & Unchecked is true"), CheckedUnchecked.Intersect(Unchecked), true);

		GPassing &= TestEqual(TEXT("Checked & CheckedUnchecked is true"), Checked.Intersect(CheckedUnchecked), true);
		GPassing &= TestEqual(TEXT("Unchecked & CheckedUnchecked is true"), Unchecked.Intersect(CheckedUnchecked), true);
	}

	return GPassing;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOperatorBitwiseNotTest, "Slate.WidgetState.OperatorBitwiseNotTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOperatorBitwiseNotTest::RunTest(const FString& Parameters)
{
	bool GPassing = true;
	{
		FWidgetStateBitfield Test = {};
		GPassing &= TestEqual(TEXT("~{} is true"), ~Test, true);
	}

	{
		FWidgetStateBitfield Pressed = {};

		Pressed.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);

		GPassing &= TestEqual(TEXT("~Pressed is true"), ~Pressed, true);

		GPassing &= TestEqual(TEXT("Pressed & ~Pressed is false"), Pressed.Intersect(~Pressed), false);
		GPassing &= TestEqual(TEXT("Pressed does not have any flags ~Pressed"), Pressed.HasAnyFlags(~Pressed), false);
		GPassing &= TestEqual(TEXT("~Pressed does not have any flags Pressed"), (~Pressed).HasAnyFlags(Pressed), false);
	}

	{
		FWidgetStateBitfield Checked = {};
		FWidgetStateBitfield Unchecked = {};
		FWidgetStateBitfield CheckedUnchecked = {};

		Checked.SetEnumState(UWidgetCheckedStateRegistration::Checked);
		Unchecked.SetEnumState(UWidgetCheckedStateRegistration::Unchecked);
		CheckedUnchecked.SetEnumState(UWidgetCheckedStateRegistration::Checked.Union(UWidgetCheckedStateRegistration::Unchecked));

		GPassing &= TestEqual(TEXT("Checked & ~Checked is false"), Checked.Intersect(~Checked), false);
		GPassing &= TestEqual(TEXT("Unchecked & ~Unchecked is false"), Unchecked.Intersect(~Unchecked), false);
		GPassing &= TestEqual(TEXT("CheckedUnchecked & ~CheckedUnchecked is false"), CheckedUnchecked.Intersect(~CheckedUnchecked), false);
	}

	return GPassing;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHasFlagTest, "Slate.WidgetState.HasFlagTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FHasFlagTest::RunTest(const FString& Parameters)
{
	bool GPassing = true;

	{
		FWidgetStateBitfield Test = {};

		Test.SetBinaryStateSlow(FName("Pressed"), true);
		GPassing &= TestEqual(TEXT("Pressed any flag true"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all flag true"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed any binary flag true"), Test.HasAnyBinaryFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all binary flag true"), Test.HasAllBinaryFlags(UWidgetPressedStateRegistration::Bit), true);

		Test.SetBinaryStateSlow(FName("Pressed"), false);
		GPassing &= TestEqual(TEXT("Pressed any flag false"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed all flag false"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed any binary flag false"), Test.HasAnyBinaryFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed all binary flag false"), Test.HasAllBinaryFlags(UWidgetPressedStateRegistration::Bit), false);

		Test.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);
		GPassing &= TestEqual(TEXT("Pressed any flag true"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all flag true"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed any binary flag true"), Test.HasAnyBinaryFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all binary flag true"), Test.HasAllBinaryFlags(UWidgetPressedStateRegistration::Bit), true);

		Test.SetBinaryState(UWidgetPressedStateRegistration::Bit, false);
		GPassing &= TestEqual(TEXT("Pressed any flag false"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed all flag false"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed any binary flag false"), Test.HasAnyBinaryFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed all binary flag false"), Test.HasAllBinaryFlags(UWidgetPressedStateRegistration::Bit), false);

		// Slow cache results
		uint8 PressedIndex = UWidgetStateSettings::Get()->GetBinaryStateIndex(FName("Pressed"));

		Test.SetBinaryState(PressedIndex, true);
		GPassing &= TestEqual(TEXT("Pressed any flag true"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all flag true"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed any binary flag true"), Test.HasAnyBinaryFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all binary flag true"), Test.HasAllBinaryFlags(UWidgetPressedStateRegistration::Bit), true);

		Test.SetBinaryState(PressedIndex, false);
		GPassing &= TestEqual(TEXT("Pressed any flag false"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed all flag false"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed any binary flag false"), Test.HasAnyBinaryFlags(UWidgetPressedStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Pressed all binary flag false"), Test.HasAllBinaryFlags(UWidgetPressedStateRegistration::Bit), false);
	}

	{
		FWidgetStateBitfield Test = {};
		FWidgetStateBitfield PressedHovered = {};

		PressedHovered.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);
		PressedHovered.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);

		Test.SetBinaryState(UWidgetPressedStateRegistration::Bit, true);
		Test.SetBinaryState(UWidgetHoveredStateRegistration::Bit, true);
		GPassing &= TestEqual(TEXT("Pressed any flag true"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all flag true"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Hovered any flag true"), Test.HasAnyFlags(UWidgetHoveredStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Hovered all flag true"), Test.HasAllFlags(UWidgetHoveredStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("PressedHovered any flag true"), Test.HasAnyFlags(PressedHovered), true);
		GPassing &= TestEqual(TEXT("PressedHovered all flag true"), Test.HasAllFlags(PressedHovered), true);

		Test.SetBinaryState(UWidgetHoveredStateRegistration::Bit, false);
		GPassing &= TestEqual(TEXT("Pressed any flag true"), Test.HasAnyFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Pressed all flag true"), Test.HasAllFlags(UWidgetPressedStateRegistration::Bit), true);
		GPassing &= TestEqual(TEXT("Hovered any flag true"), Test.HasAnyFlags(UWidgetHoveredStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("Hovered all flag true"), Test.HasAllFlags(UWidgetHoveredStateRegistration::Bit), false);
		GPassing &= TestEqual(TEXT("PressedHovered any flag true"), Test.HasAnyFlags(PressedHovered), true);
		GPassing &= TestEqual(TEXT("PressedHovered all flag true"), Test.HasAllFlags(PressedHovered), false);
	}

	{
		FWidgetStateBitfield Test = {};

		Test.SetEnumState(UWidgetCheckedStateRegistration::Checked);
		GPassing &= TestEqual(TEXT("Checked any flag true"), Test.HasAnyFlags(UWidgetCheckedStateRegistration::Checked), true);
		GPassing &= TestEqual(TEXT("Checked all flag true"), Test.HasAllFlags(UWidgetCheckedStateRegistration::Checked), true);
		GPassing &= TestEqual(TEXT("Checked any enum flag true"), Test.HasAnyEnumFlags(UWidgetCheckedStateRegistration::Checked), true);
		GPassing &= TestEqual(TEXT("Checked all enum flag true"), Test.HasAllEnumFlags(UWidgetCheckedStateRegistration::Checked), true);

		Test.SetEnumState(UWidgetCheckedStateRegistration::Unchecked);
		GPassing &= TestEqual(TEXT("Unchecked any flag true"), Test.HasAnyFlags(UWidgetCheckedStateRegistration::Unchecked), true);
		GPassing &= TestEqual(TEXT("Unchecked all flag true"), Test.HasAllFlags(UWidgetCheckedStateRegistration::Unchecked), true);
		GPassing &= TestEqual(TEXT("Unchecked any enum flag true"), Test.HasAnyEnumFlags(UWidgetCheckedStateRegistration::Unchecked), true);
		GPassing &= TestEqual(TEXT("Unchecked all enum flag true"), Test.HasAllEnumFlags(UWidgetCheckedStateRegistration::Unchecked), true);
		GPassing &= TestEqual(TEXT("Checked any flag false"), Test.HasAnyFlags(UWidgetCheckedStateRegistration::Checked), false);
		GPassing &= TestEqual(TEXT("Checked all flag false"), Test.HasAllFlags(UWidgetCheckedStateRegistration::Checked), false);
		GPassing &= TestEqual(TEXT("Checked any enum flag false"), Test.HasAnyEnumFlags(UWidgetCheckedStateRegistration::Checked), false);
		GPassing &= TestEqual(TEXT("Checked all enum flag false"), Test.HasAllEnumFlags(UWidgetCheckedStateRegistration::Checked), false);

		Test.ClearEnumState(UWidgetCheckedStateRegistration::Unchecked);
		GPassing &= TestEqual(TEXT("Unchecked any flag false"), Test.HasAnyFlags(UWidgetCheckedStateRegistration::Unchecked), false);
		GPassing &= TestEqual(TEXT("Unchecked all flag false"), Test.HasAllFlags(UWidgetCheckedStateRegistration::Unchecked), false);
		GPassing &= TestEqual(TEXT("Unchecked any enum flag false"), Test.HasAnyEnumFlags(UWidgetCheckedStateRegistration::Unchecked), false);
		GPassing &= TestEqual(TEXT("Unchecked all enum flag false"), Test.HasAllEnumFlags(UWidgetCheckedStateRegistration::Unchecked), false);
	}

	return GPassing;
}

#endif // WITH_DEV_AUTOMATION_TESTS