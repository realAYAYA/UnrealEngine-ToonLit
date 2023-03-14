// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "FixedFrameRateCustomTimeStep.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameRate.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "GenlockedCustomTimeStep.generated.h"

class UObject;

/**
 * Class to control the Engine Timestep from a Genlock signal.
 */
UCLASS(Abstract)
class TIMEMANAGEMENT_API UGenlockedCustomTimeStep : public UFixedFrameRateCustomTimeStep
{
	GENERATED_BODY()

protected:

	/** Blocks until it gets a sync signal. Returns false if unsuccessful */
	virtual bool WaitForSync() PURE_VIRTUAL(UGenlockedCustomTimeStep::WaitForSync, return false;);

	/** Update FApp CurrentTime, IdleTime and DeltaTime */
	void UpdateAppTimes(const double& TimeBeforeSync, const double& TimeAfterSync) const;

public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual FFrameRate GetFixedFrameRate() const PURE_VIRTUAL(UGenlockedCustomTimeStep::GetFixedFrameRate, return FFrameRate(24,1););

	/** Get the sync rate (not always the same as the fixed frame rate) */
	virtual FFrameRate GetSyncRate() const { return GetFixedFrameRate(); };

	/** Returns how many syncs occurred since the last tick */
	virtual uint32 GetLastSyncCountDelta() const PURE_VIRTUAL(UGenlockedCustomTimeStep::GetLastSyncCountDelta, return 1;);

	/** Returns true if the Sync related functions will return valid data */
	virtual bool IsLastSyncDataValid() const PURE_VIRTUAL(UGenlockedCustomTimeStep::IsLastSyncDataValid, return false;);

	/** Returns how many sync counts are expected between ticks*/
	virtual uint32 GetExpectedSyncCountDelta() const;

	/** Whether automatic format detection is supported. */ 
	virtual bool SupportsFormatAutoDetection() const { return false; };

	/** Whether this custom time step should autodetect the video format if supported. */
	UPROPERTY()
	bool bAutoDetectFormat = false;
};
