// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FixedFrameRateCustomTimeStep.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"
#include "VPTimecodeCustomTimeStep.generated.h"

class UEngine;
class UTimecodeProvider;

/**
 * Control the engine's time step via the engine's TimecodeProvider.
 * Will sleep and wake up engine when the a new frame is available.
 */
UCLASS(Blueprintable, editinlinenew, meta = (DisplayName = "Timecode Custom Time Step"))
class VPUTILITIES_API UVPTimecodeCustomTimeStep : public UFixedFrameRateCustomTimeStep
{
	GENERATED_BODY()

public:
	//~ UFixedFrameRateCustomTimeStep interface
	virtual bool Initialize(UEngine* InEngine) override;
	virtual void Shutdown(UEngine* InEngine) override;
	virtual bool UpdateTimeStep(UEngine* InEngine) override;
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override { return State; }
	virtual FFrameRate GetFixedFrameRate() const override { return PreviousFrameRate; }

public:
	/** If true, stop the CustomTimeStep if the new timecode value doesn't follow the previous timecode value. */
	UPROPERTY(EditAnywhere, Category = "CustomTimeStep")
	bool bErrorIfFrameAreNotConsecutive = true;

	/** If true, stop the CustomTimeStep if the engine's TimeProvider changed since last frame. */
	UPROPERTY(EditAnywhere, Category = "CustomTimeStep")
	bool bErrorIfTimecodeProviderChanged = true;

	/** If the timecode doesn't change after that amount of time, stop the CustomTimeStep. */
	UPROPERTY(EditAnywhere, Category = "CustomTimeStep")
	float MaxDeltaTime = 0.5f;

private:
	bool InitializeFirstStep(UEngine* InEngine);

	void OnTimecodeProviderChanged();

private:
	/** The current SynchronizationState of the CustomTimeStep. */
	ECustomTimeStepSynchronizationState State = ECustomTimeStepSynchronizationState::Closed;

	/** The last timecode of the TimecodeProvider. */
	FTimecode PreviousTimecode;

	/** The last frame rate of the TimecodeProvider. */
	FFrameRate PreviousFrameRate;

	/** The time at initialization. */
	double InitializedSeconds = 0.0;

	/** Only warn once about the synchronization state. */
	bool bWarnAboutSynchronizationState = false;
};
