// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneFwd.h"
#include "Misc/Attribute.h"
#include "UObject/ObjectMacros.h"

class IMovieScenePlayer;

struct MOVIESCENE_API FMovieSceneTimeController
{
public:

	virtual ~FMovieSceneTimeController() {}

	/**
	 * Called whenever a sequence starts or resumes playback from a non-playing state
	 */
	void StartPlaying(const FQualifiedFrameTime& InStartTime);

	/**
	 * Called whenever a sequence stops playback
	 */
	void StopPlaying(const FQualifiedFrameTime& InStopTime);

	/**
	 * Ticks this controller
	 *
	 * @param DeltaSeconds     The tick delta in seconds, dilated by the current world settings global dilation
	 * @param InPlayRate       The current play rate of the sequence
	 */
	void Tick(float DeltaSeconds, float InPlayRate);

	/**
	 * Request the current time based on the specified existing time and play rate.
	 * Times should be returned in the same play rate as that specified by InCurrentTime
	 *
	 * @param InCurrentTime    The current time of the sequence
	 * @param InPlayRate       The current play rate of the sequence, multiplied by any world actor settings global dilation
	 */
	FFrameTime RequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate, FFrameRate InDisplayRate);

	/**
	 * Called when the status of the owning IMovieScenePlayer has changed
	 */
	void PlayerStatusChanged(EMovieScenePlayerStatus::Type InStatus, const FQualifiedFrameTime& InCurrentTime);

	/**
	 * Called to stop and resume playback from the specified time
	 */
	void Reset(const FQualifiedFrameTime& InNewStartTime);

protected:

	virtual void OnTick(float DeltaSeconds, float InPlayRate){}
	virtual void OnStartPlaying(const FQualifiedFrameTime& InStartTime){}
	virtual void OnStopPlaying(const FQualifiedFrameTime& InStopTime){}
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) = 0;

protected:

	TOptional<FQualifiedFrameTime> GetPlaybackStartTime() const
	{
		return PlaybackStartTime;
	}

	FFrameRate GetDisplayRate() const
	{
		return DisplayRate;
	}

private:

	TOptional<FQualifiedFrameTime> PlaybackStartTime;

	FFrameRate DisplayRate;
};

/**
 * A timing manager that retrieves its time from an external clock source
 */
struct MOVIESCENE_API FMovieSceneTimeController_ExternalClock : FMovieSceneTimeController
{
protected:

	FMovieSceneTimeController_ExternalClock()
		: AccumulatedDilation(0.0)
	{}

	virtual double GetCurrentTime() const = 0;

protected:

	virtual void OnStartPlaying(const FQualifiedFrameTime& InStartTime) override;
	virtual void OnStopPlaying(const FQualifiedFrameTime& InStopTime) override;
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override;

private:

	double AccumulatedDilation;

	TOptional<double> ClockStartTime;
	TOptional<double> ClockLastUpdateTime;
};

/**
 * A timing manager that retrieves its time from the platform clock
 */
struct MOVIESCENE_API FMovieSceneTimeController_PlatformClock : FMovieSceneTimeController_ExternalClock
{
	virtual double GetCurrentTime() const override;
};

/**
 * A timing manager that retrieves its time from the audio clock
 */
struct MOVIESCENE_API FMovieSceneTimeController_AudioClock : FMovieSceneTimeController_ExternalClock
{
	virtual double GetCurrentTime() const override;
};


/**
* A timing manager that retrieves its time relative to the Timecode clock
*/
struct MOVIESCENE_API FMovieSceneTimeController_RelativeTimecodeClock : FMovieSceneTimeController_ExternalClock
{
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override;
	virtual double GetCurrentTime() const override;
	virtual void OnStopPlaying(const FQualifiedFrameTime& InStopTime) override;

protected:

	TOptional<FFrameTime> LastCurrentFrameTime;
	TOptional<double> TimeSinceCurrentFrameTime;

	double AccumulatedFrameTime;
};


/**
* A timing manager that retrieves its time from the Timecode clock
*/
struct MOVIESCENE_API FMovieSceneTimeController_TimecodeClock : FMovieSceneTimeController
{
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override;
};


/**
 * A timing manager that accumulates delta times from a world
 */
struct MOVIESCENE_API FMovieSceneTimeController_Tick : FMovieSceneTimeController
{
	FMovieSceneTimeController_Tick()
		: CurrentOffsetSeconds(0.0)
	{}

protected:

	virtual void OnTick(float DeltaSeconds, float InPlayRate) override;
	virtual void OnStartPlaying(const FQualifiedFrameTime& InStartTime) override;
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override;

private:
	double CurrentOffsetSeconds;
};

/**
 * A timing manager that plays every display frame for a certain number of seconds
 */
struct MOVIESCENE_API FMovieSceneTimeController_PlayEveryFrame : FMovieSceneTimeController
{
	FMovieSceneTimeController_PlayEveryFrame()
		: PreviousPlatformTime(0.f)
	{}

protected:

	virtual void OnStartPlaying(const FQualifiedFrameTime& InStartTime) override;
	virtual FFrameTime OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate) override;

private:
	double PreviousPlatformTime;
	FFrameTime CurrentTime;
};



