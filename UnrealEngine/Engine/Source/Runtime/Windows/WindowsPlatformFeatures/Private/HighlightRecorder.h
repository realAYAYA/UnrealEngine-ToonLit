// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WmfPrivate.h"

#include "RHI.h"
#include "RHIResources.h"
#include "Containers/CircularQueue.h"
#include "HAL/Thread.h"
#include "HAL/ThreadSafeBool.h"

#include "WmfRingBuffer.h"

#include "GameplayMediaEncoder.h"

class FWmfMp4Writer;
class FThread;

DECLARE_LOG_CATEGORY_EXTERN(HighlightRecorder, Log, VeryVerbose);

class FHighlightRecorder final : private IGameplayMediaEncoderListener
{
public:
	FHighlightRecorder();
	~FHighlightRecorder();

	enum class EState { Stopped, Recording, Paused };

	EState GetState() const
	{ return State; }

	bool Start(double RingBufferDurationSecs);
	bool Pause(bool bPause);
	void Stop();
	bool IsSaving() const
	{
		return bSaving;
	}

	using FDoneCallback = TFunction<void(bool /* bSuccess */, const FString& /* FullPathToFile */)>;
	bool SaveHighlight(const TCHAR* Filename, FDoneCallback DoneCallback, double MaxDurationSecs = 1.0 * 60 * 60);

private:
	bool SaveHighlightInBackground(const FString& Filename, double MaxDurationSecs);
	bool SaveHighlightInBackgroundImpl(const FString& Filename, double MaxDurationSecs);
	bool InitialiseMp4Writer(const FString& Filename, bool bHasAudio);
	bool GetSavingStart(const TArray<AVEncoder::FMediaPacket>& Samples, FTimespan MaxDuration, int& OutStartIndex, FTimespan& OutStartTime) const;

	// takes into account if we've been paused and shifts current time back to compensate paused state
	// so all timestamps are continuous even over paused pieces
	FTimespan GetRecordingTime() const;

	//
	// IGameplayMediaEncoderListener implementation
	//
	void OnMediaSample(const AVEncoder::FMediaPacket& Sample) override;

private:
	TAtomic<EState> State{ EState::Stopped };

	TUniquePtr<FWmfMp4Writer> Mp4Writer;

	FWmfRingBuffer RingBuffer;
	// we take note of how long we've been paused and then "shift" samples timestamps by paused duration
	// so effectively gluing different pieces of video together

	uint64 NumPushedFrames = 0;

	FTimespan RecordingStartTime = 0;

	// if currently paused, when it happened
	FTimespan PauseTimestamp = 0;
	// for how long recording has been paused since it's started
	FTimespan TotalPausedDuration = 0;

	TUniquePtr<FThread> BackgroundSaving;
	FDoneCallback DoneCallback;
	FThreadSafeBool bSaving = false;

	DWORD AudioStreamIndex = 0;
	DWORD VideoStreamIndex = 0;

#pragma region testing
public:
	static void Start(const TArray<FString>& Args, UWorld*, FOutputDevice& Output)
	{
		// Initialize the Singleton if necessary. This is only useful if using project other than Fortnite. E.g: QAGame.
		// When using QAGame, there is no HighlightFeature, and we just use these direct console commands to test
		if (!Singleton)
		{
			Singleton = new FHighlightRecorder();
		}

		if (Args.Num() > 1)
		{
			Output.Logf(ELogVerbosity::Error, TEXT("zero or one parameter expected: Start [max_duration_secs=30.0]"));
			return;
		}

		double MaxDurationSecs = 30;
		if (Args.Num() == 1)
		{
			MaxDurationSecs = FCString::Atod(*Args[0]);
		}
		Get()->Start(MaxDurationSecs);
	}

	static void StopCmd()
	{
		if (!CheckSingleton())
		{
			return;
		}

		Get()->Stop();
		delete Singleton;
		Singleton = nullptr;
	}

	static void PauseCmd()
	{
		if (!CheckSingleton())
		{
			return;
		}

		Get()->Pause(true);
	}

	static void ResumeCmd()
	{
		if (!CheckSingleton())
		{
			return;
		}

		Get()->Pause(false);
	}

	static void SaveCmd(const TArray<FString>& Args, UWorld*, FOutputDevice& Output)
	{
		if (!CheckSingleton())
		{
			return;
		}

		if (Args.Num() > 2)
		{
			Output.Logf(ELogVerbosity::Error, TEXT("0-2 parameters expected: Save [filename=\"test.mp4\"] [max_duration_secs= ring buffer duration]"));
			return;
		}

		FString Filename = "test.mp4";
		if (Args.Num() >= 1)
		{
			Filename = Args[0];
		}

		double MaxDurationSecs = 1 * 60 * 60;
		if (Args.Num() == 2)
		{
			MaxDurationSecs = FCString::Atod(*Args[1]);
		}

		Get()->SaveHighlight(
			*Filename,
			[](bool bRes, const FString& InFullPathToFile)
			{
				UE_LOG(HighlightRecorder, Log, TEXT("saving done: %d"), bRes);
			},
			MaxDurationSecs
		);
	}

private:

	static bool CheckSingleton()
	{
		if (Singleton)
		{
			return true;
		}
		else
		{
			UE_LOG(HighlightRecorder, Error, TEXT("HighlightRecorder not initialized."));
			return false;
		}
	}

	static FHighlightRecorder* Get()
	{
		check(Singleton);
		return Singleton;
	}
	static FHighlightRecorder* Singleton;
#pragma endregion testing
};


