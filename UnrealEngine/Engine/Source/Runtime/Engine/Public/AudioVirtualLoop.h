// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ActiveSound.h"


/**
 * Class that tracks virtualized looping active sounds that are eligible to revive re-trigger
 * as long as no stop request is received from the game thread.
 */
struct ENGINE_API FAudioVirtualLoop
{
private:
	float TimeSinceLastUpdate;
	float TimeVirtualized;
	float UpdateInterval;

	FActiveSound* ActiveSound;

	/**
	  * Check if provided active sound is in audible range.
	  */
	static bool IsInAudibleRange(const FActiveSound& InActiveSound, const FAudioDevice* InAudioDevice = nullptr);

public:
	FAudioVirtualLoop();

	/**
	 * Check to see if listener move is far enough such that a check for virtual loop realization is necessary
	 */
	static bool ShouldListenerMoveForceUpdate(const FTransform& LastTransform, const FTransform& CurrentTransform);

	/**
	 * Checks if provided active sound is available to be virtualized.  If so, returns new active sound ready to be
	 * added to virtual loop management by parent audio device.
	 */
	static bool Virtualize(const FActiveSound& InActiveSound, bool bDoRangeCheck, FAudioVirtualLoop& OutVirtualLoop);
	static bool Virtualize(const FActiveSound& InActiveSound, FAudioDevice& AudioDevice, bool bDoRangeCheck, FAudioVirtualLoop& OutVirtualLoop);

	/**
	 * Whether the virtual loop system is enabled or not
	 */
	static bool IsEnabled();

	/**
	 * Returns the internally-managed active sound
	 */
	FActiveSound& GetActiveSound();

	/**
	 * Returns the time the sound has been virtualized
	 */
	float GetTimeVirtualized() const;

	/**
	 * Returns the wait interval being observed before next update
	 */
	float GetUpdateInterval() const;

	/**
	 */
	const FActiveSound& GetActiveSound() const;

	/**
	 * Overrides the update interval to the provided length
	 */
	void CalculateUpdateInterval();

	/**
	 * Takes aggregate update delta and updates focus so that realization
	 * check can test if ready to play.
	 */
	void UpdateFocusData(float DeltaTime);

	/**
	  * Updates the loop and checks if ready to play (or 'realize').
	  * Returns whether or not the sound is ready to be realized.
	  */
	bool Update(float DeltaTime, bool bForceUpdate);
};
