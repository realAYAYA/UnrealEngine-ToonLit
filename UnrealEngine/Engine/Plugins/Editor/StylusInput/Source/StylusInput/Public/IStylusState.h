// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Vector2D.h"

/**
 * The types of stylus inputs that can be potentially supported by a stylus.
 */
enum class STYLUSINPUT_API EStylusInputType
{
	Position,
	Z,
	Pressure,
	Tilt,
	TangentPressure,
	ButtonPressure,
	Twist,
	Size
};

/**
 * The current state of a single stylus, as sent by IStylusMessageHandler. 
 */
class STYLUSINPUT_API FStylusState
{
public:

	FStylusState() : Position(0, 0), Z(0), Tilt(0, 0), Twist(0), Pressure(0), 
		TangentPressure(0), Size(0, 0), IsDown(false), IsInverted(false)
	{
	}

	FStylusState(FVector2D InPosition, float InZ, FVector2D InTilt, float InTwist,
		float InPressure, float InTanPressure, FVector2D InSize, 
		bool InDown, bool InInverted) : 
		Position(InPosition), Z(InZ), Tilt(InTilt), Twist(InTwist),
		Pressure(InPressure), TangentPressure(InTanPressure), Size(InSize),
		IsDown(InDown), IsInverted(InInverted)
	{
	}

	FStylusState(const FStylusState& Other) :
		Position(Other.Position), Z(Other.Z), Tilt(Other.Tilt), Twist(Other.Twist),
		Pressure(Other.Pressure), TangentPressure(Other.TangentPressure), Size(Other.Size),
		IsDown(Other.IsDown), IsInverted(Other.IsInverted)
	{
	}

	/**
	 * The current position of the stylus on (or above) the tablet. Always valid.
	 * This value is in logical coordinates, not pixels.
	 * The mouse position can be used to retrieve the screenspace value.
	 * A value of (0,0) is in the top-left of the tablet.
	 */ 
	FVector2D GetPosition() const { return Position; }

	/**
	 * The current height of the stylus above the tablet.
	 * Defaults to 0 if EStylusInputType::Z is not supported.
	 */
	float GetZ() const { return Z; }

	/** 
	 * The current tilt along the X axis in degrees, normalized to the range of [-90, 90]. 
	 * Defaults to (0,0) if EStylusInputType::Tilt is not supported.
	 * A value of (0,0) means that the stylus is perfectly vertical.
	 * A positive X value means that the stylus is tilted to the right. 
	 * A positive Y value means that the stylus is tilted forwards, away from the user.
	 * A value of -90 or 90 means that the pen is lying on the tablet, though in practice this isn't widely supported.
	 */
	FVector2D GetTilt() const { return Tilt; }

	/**
	 * The current twist amount around the stylus' own axis in degrees, normalized to the range of [0, 360).
	 * Defaults to 0 if EStylusInputType::Twist is not supported.
	 * A value of 1 represents a full rotation clockwise.
	 */
	float GetTwist() const { return Twist; }

	/**
	 * Get the current pressure along the tablet's normal, usually straight down.
	 * Normalized to the range [0, 1]. 
	 * Defaults to 0 if EStylusInputType::Pressure is not supported.
	 */
	float GetPressure() const { return Pressure; }

	/**
	 * Get the current pressure along the tablet's surface.
	 * Normalized to the range [0, 1].
	 * Defaults to 0 if EStylusInputType::TangentPressure is not supported.
	 */
	float GetTangentPressure() const { return TangentPressure; }

	/**
	 * Get the size of the touch in logical coordinates.
	 * Defaults to (0,0) if EStylusInputType::Size is not supported.
	 */
	FVector2D GetSize() const { return Size; }
	
	/**
	 * Is the stylus inverted? Ie. the eraser part is pointing down. 
	 * Defaults to false if EStylusInputType::Tilt is not supported.
	 */
	bool IsStylusInverted() const { return IsInverted; }

	/** Is the stylus currently touching the tablet? */
	bool IsStylusDown() const { return IsDown; }

private:

	FVector2D Position;
	float Z;
	FVector2D Tilt;
	float Twist;
	float Pressure;
	float TangentPressure;
	FVector2D Size;

	bool IsDown : 1;
	bool IsInverted : 1;
};

/** An input device representing a stylus and its current state. */
class STYLUSINPUT_API IStylusInputDevice
{
public:

	virtual ~IStylusInputDevice() {}

	/**
	 * Get the current stylus state.
	 */
	const FStylusState& GetCurrentState() const { return CurrentState; }

	/**
	 * Get the previous stylus state.
	 */
	const FStylusState& GetPreviousState() const { return PreviousState; }

	/**
	 * Get the supported inputs of this tablet.
	 */
	const TArray<EStylusInputType>& GetSupportedInputs() const { return SupportedInputs; }

	/** Update the input device. Not intended to be called externally. */ 
	virtual void Tick() = 0;

	/** Does the input device need to be ticked? */
	bool IsDirty() const { return Dirty; }

protected:
	FStylusState CurrentState;
	FStylusState PreviousState;
	TArray<EStylusInputType> SupportedInputs;
	bool Dirty : 1;
};

/**
 * Interface to implement for classes that want to receive messages when a stylus state change occurs.
 * Will trigger once per frame.
 */
class IStylusMessageHandler
{
public:
	virtual void OnStylusStateChanged(const FStylusState& NewState, int32 StylusIndex) = 0;
};
