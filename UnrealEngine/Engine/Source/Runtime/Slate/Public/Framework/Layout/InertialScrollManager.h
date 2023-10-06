// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"

/**
 * A helper class to calculate inertial scrolling.  This class combines a percentage of velocity lost
 * per second coupled with a static amount of velocity lost per second in order to achieve a quick decay
 * when the velocity grows small enough, and the percentage of friction lost prevents large velocities
 * from scrolling forever.
 */
class FInertialScrollManager
{
	/**
	 * This is the percentage of velocity loss per second.
	 */
	static SLATE_API float FrictionCoefficient;

	/**
	 * This is a constant amount of velocity lost per second.
	 */
	static SLATE_API float StaticVelocityDrag;

public:
	/**
	  * Constructor
	  * @param ScrollDecceleration	The acceleration against the velocity causing it to decay.
	  * @param SampleTimeout		Samples older than this amount of time will be discarded.
	  */
	SLATE_API FInertialScrollManager(double SampleTimeout = 0.1f);

	/** Adds a scroll velocity sample to help calculate a smooth velocity */
	SLATE_API void AddScrollSample(float Delta, double CurrentTime);

	/** Updates the current scroll velocity. Call every frame. */
	SLATE_API void UpdateScrollVelocity(const float InDeltaTime);

	/** Instantly end the inertial scroll */
	SLATE_API void ClearScrollVelocity();

	/** Gets the calculated velocity of the scroll. */
	float GetScrollVelocity() const { return ScrollVelocity; }

private:
	struct FScrollSample
	{
		double Time;
		float Delta;

		FScrollSample(float InDelta, double InTime)
			: Time(InTime)
			, Delta(InDelta)
		{}
	};

	/** Used to calculate the appropriate scroll velocity over the last few frames while inertial scrolling */
	TArray<FScrollSample> ScrollSamples;

	/** The current velocity of the scroll */
	float ScrollVelocity;

	/** Samples older than this amount of time will be discarded. */
	double SampleTimeout;
};
