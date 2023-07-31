// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/QuartzQuantizationUtilities.h"
#include "Quartz/QuartzMetronome.h"

namespace Audio
{
	// forwards
	class FMixerDevice;
	class FQuartzClock;
	class FMixerSourceManager;
	class FQuartzClockManager;

	template<class ListenerType>
	class TQuartzShareableCommandQueue;

	using FQuartzClockCommandQueuePtr = TSharedPtr<TQuartzShareableCommandQueue<FQuartzClock>, ESPMode::ThreadSafe>;
	using FQuartzClockCommandQueueWeakPtr = TWeakPtr<TQuartzShareableCommandQueue<FQuartzClock>, ESPMode::ThreadSafe>;

	
	/**
	 *	FQuartzClockProxy:
	 *
	 *		This class is a C++ handle to the underlying clock.
	 *		
	 *		It is mostly a wrapper around a TWeakPtr<FQuartzClock> and
	 *		TSharedPtr<TQuartzShareableCommandQueue<FQuartzClock>.
	 *		
	 *		The getters query the underlying FQuartzClock directly,
	 *		which returns values updated during the last audio-engine tick
	 *
	 *		If you need to add more getters, add copies of the members in question to
	 *		FQuartzClock::FQuartzClockState and update FQuartzClock::UpdateCachedState()
	 *		for thread-safe access (or manually protect access w/ CachedClockStateCritSec)
	 *
	 *		SendCommandToClock() can be used to execute lambdas at the beginning
	 *		of the next clock tick.  These lambdas can call FQuartzClock's public methods safely.
	 *
	 *		Your lambda will take an FQuartzClock* as an argument, which will be passed in by the
	 *		FQuartzClock itself when it pumps the command queue.
	 *
	 */
	class AUDIOMIXER_API FQuartzClockProxy
	{
	public:
		// ctor
		FQuartzClockProxy() {}
		FQuartzClockProxy(const FName& Name) : ClockId(Name){ } // conv ctor from FName
		FQuartzClockProxy(TSharedPtr<FQuartzClock, ESPMode::ThreadSafe> InClock);

		FName GetClockName() const { return ClockId; }

		bool IsValid() const;
		operator bool() const { return IsValid(); }

		bool operator==(const FName& Name) const { return ClockId == Name; }

		bool DoesClockExist() const;

		bool IsClockRunning() const;

		Audio::FQuartzClockTickRate GetTickRate() const;

		float GetEstimatedClockRunTimeSeconds() const;

		FQuartzTransportTimeStamp GetCurrentClockTimestamp() const;

		float GetDurationOfQuantizationTypeInSeconds(const EQuartzCommandQuantization& QuantizationType, float Multiplier) const;

		// returns false if the clock is not valid or has shut down
		bool SendCommandToClock(TFunction<void(FQuartzClock*)> InCommand);

		// implicit cast to underlying ID (FName)
		operator const FName&() const { return ClockId; }

	private:
		FName ClockId;

		FQuartzClockCommandQueueWeakPtr SharedQueue;

	protected:
		TWeakPtr<FQuartzClock, ESPMode::ThreadSafe> ClockWeakPtr;

	}; // class FQuartzClockProxy


	
	/**
	 *	FQuartzClock:
	 *
	 *		This class receives, schedules, and fires quantized commands. 
	 *		The underlying FQuartzMetronome handles all counting / timing logic.
	 *
	 *		This class gets ticked externally (i.e. by some Clock Manager)
	 *		and counts down the time-to-fire the commands in audio frames.
	 *
	 *
	 *		UpdateCachedState() updates a game-thread copy of data accessed via FQuartzClockProxy
	 *		(see FQuartzClockState)
	 */
	class AUDIOMIXER_API FQuartzClock
	{
	public:

		// ctor
		FQuartzClock(const FName& InName, const FQuartzClockSettings& InClockSettings, FQuartzClockManager* InOwningClockManagerPtr = nullptr);

		// dtor
		~FQuartzClock();

		// Transport Control:
		// alter the tick rate (take by-value to make sample-rate adjustments in-place)
		void ChangeTickRate(FQuartzClockTickRate InNewTickRate, int32 NumFramesLeft = 0);

		void ChangeTimeSignature(const FQuartzTimeSignature& InNewTimeSignature);

		void Resume();

		void Pause();

		void Restart(bool bPause = true);

		void Stop(bool CancelPendingEvents); // Pause + Restart

		void SetSampleRate(float InNewSampleRate);

		void ResetTransport();

		// (used for StartOtherClock command to handle the sub-tick as the target clock)
		void AddToTickDelay(int32 NumFramesOfDelayToAdd);

		// (used for StartOtherClock command to handle the sub-tick as the target clock)
		void SetTickDelay(int32 NumFramesOfDelay);

		void Shutdown();

		// Getters:
		FQuartzClockTickRate GetTickRate();

		FName GetName() const;

		bool IgnoresFlush() const;

		bool DoesMatchSettings(const FQuartzClockSettings& InClockSettings) const;

		bool HasPendingEvents() const;

		int32 NumPendingEvents() const;

		bool IsRunning() const;

		float GetDurationOfQuantizationTypeInSeconds(const EQuartzCommandQuantization& QuantizationType, float Multiplier);

		FQuartzTransportTimeStamp GetCurrentTimestamp();

		float GetEstimatedRunTime();

		FMixerDevice* GetMixerDevice();

		FMixerSourceManager* GetSourceManager();

		FQuartzClockManager* GetClockManager();

		FQuartzClockCommandQueueWeakPtr GetCommandQueue() const;

		// Metronome Event Subscription:
		void SubscribeToTimeDivision(FQuartzGameThreadSubscriber InSubscriber, EQuartzCommandQuantization InQuantizationBoundary);

		void SubscribeToAllTimeDivisions(FQuartzGameThreadSubscriber InSubscriber);

		void UnsubscribeFromTimeDivision(FQuartzGameThreadSubscriber InSubscriber, EQuartzCommandQuantization InQuantizationBoundary);

		void UnsubscribeFromAllTimeDivisions(FQuartzGameThreadSubscriber InSubscriber);

		// Quantized Command Management:
		void AddQuantizedCommand(FQuartzQuantizedCommandInitInfo& InQuantizationCommandInitInfo);

		void AddQuantizedCommand(FQuartzQuantizationBoundary InQuantizationBondary, TSharedPtr<IQuartzQuantizedCommand> InNewEvent);

		bool CancelQuantizedCommand(TSharedPtr<IQuartzQuantizedCommand> InCommandPtr);
		
		// low-resolution clock update
		// (not sample-accurate!, useful when running without an Audio Device)
		void LowResolutionTick(float InDeltaTimeSeconds);

		// sample accurate clock update
		void Tick(int32 InNumFramesUntilNextTick);

	private:
		// Contains the pending command and the number of frames it has to wait to fire
		struct PendingCommand
		{
			// ctor
			PendingCommand(TSharedPtr<IQuartzQuantizedCommand> InCommand, int32 InNumFramesUntilExec)
				: Command(InCommand)
				, NumFramesUntilExec(InNumFramesUntilExec)
			{
			}

			// Quantized Command Object
			TSharedPtr<IQuartzQuantizedCommand> Command;

			// Countdown to execution
			int32 NumFramesUntilExec{ 0 };
		}; // struct PendingCommand

		// mutex-protected update at the end of Tick()
		FCriticalSection CachedClockStateCritSec;
		void UpdateCachedState();

		// data is cached when an FQuartzClock is ticked
		struct FQuartzClockState
		{
			FQuartzClockTickRate TickRate;
			FQuartzTransportTimeStamp TimeStamp;
			float RunTimeInSeconds;
		} CachedClockState;

		void TickInternal(int32 InNumFramesUntilNextTick, TArray<PendingCommand>& CommandsToTick, int32 FramesOfLatency = 0, int32 FramesOfDelay = 0);

		bool CancelQuantizedCommandInternal(TSharedPtr<IQuartzQuantizedCommand> InCommandPtr, TArray<PendingCommand>& CommandsToTick);

		// don't allow default ctor, a clock needs to be ready to be used
		// by the clock manager / FMixerDevice once constructed
		FQuartzClock() = delete;

		FQuartzMetronome Metronome;

		FQuartzClockManager* OwningClockManagerPtr{ nullptr };

		FName Name;

		float ThreadLatencyInMilliseconds{ 40.f };

		// Command queue handed out to GameThread objects to queue commands. These get executed at the top of Tick()
		mutable FQuartzClockCommandQueuePtr PreTickCommands; // (mutable for lazy init in GetQuartzSubscriber())

		// Container of external commands to be executed (TUniquePointer<QuantizedAudioCommand>)
		TArray<PendingCommand> ClockAlteringPendingCommands;
		TArray<PendingCommand> PendingCommands;

		FThreadSafeBool bIsRunning{ true };

		bool bIgnoresFlush{ false };

		int32 TickDelayLengthInFrames{ 0 };

	}; // class FQuartzClock

} // namespace Audio
