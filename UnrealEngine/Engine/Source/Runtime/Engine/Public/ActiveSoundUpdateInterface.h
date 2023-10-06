// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "ActiveSoundUpdateInterface.generated.h"

// Forward Declarations 
struct FActiveSound;
struct FSoundParseParameters;

/** Interface for modifying active sounds during their update */
UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class UActiveSoundUpdateInterface : public UInterface
{
	GENERATED_BODY()
};

class IActiveSoundUpdateInterface
{
	GENERATED_BODY()

public:

	/**
	 * Gathers interior data that can affect the active sound.  Non-const as this step can be used to track state about the sound on the implementing object
	 * NOTE! Called on the AudioThread
	 *
	 * @param ActiveSound	The active sound affected
	 * @param ParseParams	The parameters to apply to the wave instances
	 */
	virtual void GatherInteriorData(const FActiveSound& ActiveSound, FSoundParseParameters& ParseParams) {}

	/**
	 * Applies interior data previously collected to the active sound and parse parameters.
	 * NOTE! Called on the AudioThread
	 *
	 * @param ActiveSound	The active sound affected
	 * @param ParseParams	The parameters to apply to the wave instances
	 */
	virtual void ApplyInteriorSettings(const FActiveSound& ActiveSound, FSoundParseParameters& ParseParams) {}

	/**
	 * Called when an active sound is being added to the audio engine
	 * NOTE! Called on the AudioThread
	 *
	 * @param ActiveSound	The active sound being added
	 */
	virtual void OnNotifyAddActiveSound(FActiveSound& ActiveSound) {}

	/**
	 * Called when the active sound is being removed from the audio engine
	 * NOTE! Called on the AudioThread
	 *
	 * @param ActiveSound	The active sound
	 */
	virtual void OnNotifyPendingDelete(const FActiveSound& ActiveSound) {}
};
