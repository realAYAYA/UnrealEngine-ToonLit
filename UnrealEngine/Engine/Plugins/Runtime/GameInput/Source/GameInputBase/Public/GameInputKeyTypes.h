// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputCoreTypes.h"

/**
 * Key types that are unique to the Game Input plugin
 * These keys are added on module startup of the Game Input module.
 *
 * HEY, LISTEN!
 * If you add or otherwise modify these keys, make sure the change is reflected in FGameInputBaseModule::InitializeGameInputKeys.
 */
struct GAMEINPUTBASE_API FGameInputKeys
{
	//
	// Racing Wheel
	//

	// Analog types
	static const FKey RacingWheel_Brake;
	static const FKey RacingWheel_Clutch;
	static const FKey RacingWheel_Handbrake;
	static const FKey RacingWheel_Throttle;
	static const FKey RacingWheel_Wheel;
	static const FKey RacingWheel_PatternShifterGear;

	// Button types
	static const FKey RacingWheel_None;
	static const FKey RacingWheel_Menu;
	static const FKey RacingWheel_View;
	static const FKey RacingWheel_PreviousGear;
	static const FKey RacingWheel_NextGear;

	//
	// Flight Stick
	//

	// Analog types
	static const FKey FlightStick_Roll;
	static const FKey FlightStick_Pitch;
	static const FKey FlightStick_Yaw;
	static const FKey FlightStick_Throttle;

	// Button types
	static const FKey FlightStick_None;
	static const FKey FlightStick_Menu;
	static const FKey FlightStick_View;
	static const FKey FlightStick_FirePrimary;
	static const FKey FlightStick_FireSecondary;

	//
	// Arcade Stick
	//

	// Button Types
	static const FKey ArcadeStick_None;
	static const FKey ArcadeStick_Menu;
	static const FKey ArcadeStick_View;
	static const FKey ArcadeStick_Up;
	static const FKey ArcadeStick_Down;
	static const FKey ArcadeStick_Left;
	static const FKey ArcadeStick_Right;
	static const FKey ArcadeStick_Action1;
	static const FKey ArcadeStick_Action2;
	static const FKey ArcadeStick_Action3;
	static const FKey ArcadeStick_Action4;
	static const FKey ArcadeStick_Action5;
	static const FKey ArcadeStick_Action6;
	static const FKey ArcadeStick_Special1;
	static const FKey ArcadeStick_Special2;
};