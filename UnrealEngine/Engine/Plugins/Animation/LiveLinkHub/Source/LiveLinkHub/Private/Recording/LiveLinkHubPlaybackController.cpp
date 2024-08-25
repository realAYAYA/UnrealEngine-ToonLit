// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubPlaybackController.h"

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "ILiveLinkClient.h"
#include "Implementations/LiveLinkUAssetRecordingPlayer.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkRecording.h"
#include "LiveLinkPreset.h"
#include "LiveLinkTypes.h"
#include "Features/IModularFeatures.h"
#include "HAL/RunnableThread.h"
#include "HAL/Event.h"
#include "LiveLinkClient.h"
#include "UI/Widgets/SLiveLinkHubPlaybackWidget.h"
#include "UObject/Object.h"
#include "UObject/Package.h"

/**
 * Thread safe way to load and store a FQualifiedFrameTime. This is necessary because atomic<FQualifiedFrameTime> isn't
 * necessarily cross platform compatible when performing copy operations.
 */
class FLiveLinkHubAtomicQualifiedFrameTime {
public:
	FLiveLinkHubAtomicQualifiedFrameTime()
		: Value(FQualifiedFrameTime()) {}

	FLiveLinkHubAtomicQualifiedFrameTime(const FLiveLinkHubAtomicQualifiedFrameTime& Other) {
		FScopeLock Lock(&Other.Mutex);
		Value = Other.Value;
	}

	FLiveLinkHubAtomicQualifiedFrameTime& operator=(const FLiveLinkHubAtomicQualifiedFrameTime& Other) {
		if (this != &Other) {
			FScopeLock Lock(&Other.Mutex);
			Value = Other.Value;
		}
		return *this;
	}

	/** Set the underlying value. */
	void SetValue(const FQualifiedFrameTime& NewPlayhead) {
		FScopeLock Lock(&Mutex);
		Value = NewPlayhead;
	}

	/** Retrieve the underlying value. */
	FQualifiedFrameTime GetValue() const {
		FScopeLock Lock(&Mutex);
		return Value;
	}

private:
	/** Mutex for reading/writing underlying value. */
	mutable FCriticalSection Mutex;
	/** The underlying qualified frame time value. */
	FQualifiedFrameTime Value;
};

FLiveLinkHubPlaybackController::FLiveLinkHubPlaybackController()
{
	Client = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	OnSourceRemovedHandle = Client->OnLiveLinkSourceRemoved().AddRaw(this, &FLiveLinkHubPlaybackController::OnSourceRemoved);
	
	RecordingPlayer = MakeUnique<FLiveLinkUAssetRecordingPlayer>();
	Playhead = MakeShared<FLiveLinkHubAtomicQualifiedFrameTime, ESPMode::ThreadSafe>();
}

FLiveLinkHubPlaybackController::~FLiveLinkHubPlaybackController()
{
	StopPlayback();
	Stopping = true;
	PlaybackEvent->Trigger();

	if (Thread.IsValid())
	{
		Thread->WaitForCompletion();
		Thread.Reset();
	}

	if (OnSourceRemovedHandle.IsValid() && Client)
	{
		Client->OnLiveLinkSourceRemoved().Remove(OnSourceRemovedHandle);
	}
}

TSharedRef<SWidget> FLiveLinkHubPlaybackController::MakePlaybackWidget()
{
	return SNew(SLiveLinkHubPlaybackWidget)
		.IsEnabled_Raw(this, &FLiveLinkHubPlaybackController::IsReady)
		.OnPlayForward_Raw(this, &FLiveLinkHubPlaybackController::BeginPlayback, false)
		.OnPlayReverse_Raw(this, &FLiveLinkHubPlaybackController::BeginPlayback, true)
		.OnFirstFrame_Lambda([this]()
		{
			GoToTime(GetSelectionStartTime());
		})
		.OnLastFrame_Lambda([this]()
		{
			GoToTime(GetSelectionEndTime());
		})
		.OnPreviousFrame_Lambda([this]()
		{
			GoToTime(FQualifiedFrameTime(FFrameTime(GetCurrentFrame() - 1), GetFrameRate()));
		})
		.OnNextFrame_Lambda([this]()
		{
			GoToTime(FQualifiedFrameTime(FFrameTime(GetCurrentFrame() + 1), GetFrameRate()));
		})
		.SetCurrentTime_Raw(this, &FLiveLinkHubPlaybackController::GoToTime)
		.GetViewRange_Lambda([this]()
		{
			return SliderViewRange;
		})
		.SetViewRange_Lambda([this](TRange<double> NewRange)
		{
			SliderViewRange = MoveTemp(NewRange);
		})
		.GetTotalLength_Raw(this, &FLiveLinkHubPlaybackController::GetLength)
		.GetCurrentTime_Raw(this, &FLiveLinkHubPlaybackController::GetCurrentTime)
		.GetSelectionStartTime_Raw(this, &FLiveLinkHubPlaybackController::GetSelectionStartTime)
		.SetSelectionStartTime_Raw(this,& FLiveLinkHubPlaybackController::SetSelectionStartTime)
		.GetSelectionEndTime_Raw(this, &FLiveLinkHubPlaybackController::GetSelectionEndTime)
		.SetSelectionEndTime_Raw(this,& FLiveLinkHubPlaybackController::SetSelectionEndTime)
		.IsPaused_Raw(this, &FLiveLinkHubPlaybackController::IsPaused)
		.IsInReverse_Raw(this, &FLiveLinkHubPlaybackController::IsPlayingInReverse)
		.IsLooping_Raw(this, &FLiveLinkHubPlaybackController::IsLooping)
		.OnSetLooping_Raw(this, &FLiveLinkHubPlaybackController::SetLooping)
		.GetFrameRate_Raw(this, &FLiveLinkHubPlaybackController::GetFrameRate);
}

void FLiveLinkHubPlaybackController::StartPlayback()
{
	ResumePlayback();
	PlaybackStartTime = FPlatformTime::Seconds();
	
	FPlatformMisc::MemoryBarrier();
	PlaybackEvent->Trigger();
}

void FLiveLinkHubPlaybackController::ResumePlayback()
{
	bIsPlaying = true;
	bIsPaused = false;
	FQualifiedFrameTime CurrentTime = GetCurrentTime();

	// Clamp to selection start/end.
	if (CurrentTime.AsSeconds() < GetSelectionStartTime().AsSeconds() && !bIsReverse)
	{
		CurrentTime = GetSelectionStartTime();
	}
	else if (CurrentTime.AsSeconds() > GetSelectionEndTime().AsSeconds() && bIsReverse)
	{
		CurrentTime = GetSelectionEndTime();
	}

	Playhead->SetValue(CurrentTime);
	
	StartTimestamp = CurrentTime.AsSeconds();
	// Force sync so interpolation doesn't interfere if the first frame isn't the current frame
	SyncToFrame(CurrentTime.Time.GetFrame());
}

void FLiveLinkHubPlaybackController::PreparePlayback(ULiveLinkRecording* InLiveLinkRecording)
{
	if (InLiveLinkRecording == nullptr)
	{
		UE_LOG(LogLiveLinkHub, Warning, TEXT("Started a recording playback with an invalid recording."));
	}
	else if (InLiveLinkRecording != RecordingToPlay.Get())
	{
		TStrongObjectPtr<ULiveLinkRecording> RecordingStrongPtr(InLiveLinkRecording);

		auto PreparePlaybackCallback = [this, RecordingStrongPtr]()
		{
			if (RecordingStrongPtr.IsValid())
			{
				// Make sure PreparingPlayback is set to false when the scope exits.
				TGuardValue<bool> PreparingPlaybackGuard(bIsPreparingPlayback, true);

				RecordingToPlay.Reset(RecordingStrongPtr.Get());
				RecordingPlayer->PreparePlayback(RecordingToPlay.Get());
		
				CurrentFrameRate = RecordingPlayer->GetInitialFramerate();

				// The start and end of playback.
				SetSelectionStartTime(FQualifiedFrameTime(FFrameTime::FromDecimal(0.f), GetFrameRate()));
				SetSelectionEndTime(GetLength());
		
				// The range the user sees.
				SliderViewRange = TRange<double>(SelectionStartTime.AsSeconds(), SelectionEndTime.AsSeconds());
		
				const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), ULiveLinkPreset::StaticClass(), TEXT("RecordingRollbackPreset"));
				RollbackPreset.Reset(NewObject<ULiveLinkPreset>(GetTransientPackage(), UniqueName));
				// Save the current state of the sources/subjects in a rollback preset.
				RollbackPreset->BuildFromClient();

				// This clears out any live streams which might be occurring. They will be restored when exiting playback later.
				{
					FLiveLinkClient& LiveLinkClient = IModularFeatures::Get().GetModularFeature<FLiveLinkClient>(ILiveLinkClient::ModularFeatureName);

					LiveLinkClient.RemoveAllSources();
					LiveLinkClient.Tick();
				}
		
				RecordingToPlay->RecordingPreset->ApplyToClientLatent([this](bool)
				{
					bIsReady = true;
					SyncToFrame(0); // Needed to establish connection with client
				});
			}
		};

		if (RecordingToPlay.IsValid())
		{
			Eject(PreparePlaybackCallback);
		}
		else
		{
			PreparePlaybackCallback();
		}
	}
}

void FLiveLinkHubPlaybackController::PlayRecording(ULiveLinkRecording* InLiveLinkRecording)
{
	PreparePlayback(InLiveLinkRecording);
}

void FLiveLinkHubPlaybackController::BeginPlayback(bool bInReverse)
{
	const bool bReverseChange = bIsReverse != bInReverse;
	bIsReverse = bInReverse;
	
	// Either we are paused and should unpause, or we are toggling forward/reverse play modes.
	if (bIsPaused || !bIsPlaying || bReverseChange)
	{
		if (ShouldRestart())
		{
			// Check if we're at the end of the recording and restart, ie user pressed play again.
			RestartPlayback();
		}
		else
		{
			if (bIsReverse)
			{
				RecordingPlayer->RestartPlayback(GetCurrentFrame().Value);
			}
			
			// Resume as normal for anywhere else in the recording.
			PlaybackStartTime = FPlatformTime::Seconds();
		}
		
		ResumePlayback();
	}
	else if (bIsPlaying)
	{
		PausePlayback();
	}

	FPlatformMisc::MemoryBarrier();
	PlaybackEvent->Trigger();
}

void FLiveLinkHubPlaybackController::RestartPlayback()
{
	const bool bOldReverse = bIsReverse; // Stop playback resets reverse
	StopPlayback();
	StartTimestamp = GetCurrentTime().AsSeconds();
	RecordingPlayer->RestartPlayback(GetCurrentFrame().Value);
	bIsPlaying = true;
	bIsReverse = bOldReverse;
}

void FLiveLinkHubPlaybackController::PausePlayback()
{
	bIsPaused = true;
}

void FLiveLinkHubPlaybackController::StopPlayback()
{
	bIsPlaying = false;

	// Wait for the playback thread to exit...
	const uint32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
	if (CurrentThreadId != Thread->GetThreadID())
	{
		while(!bIsPlaybackWaiting) { }
	}
	
	const bool bReverse = bIsReverse.load();
	Playhead->SetValue(bReverse ? GetSelectionEndTime() : GetSelectionStartTime());

	PlaybackStartTime = FPlatformTime::Seconds();

	RecordingPlayer->RestartPlayback();
	bIsReverse = false;
}

void FLiveLinkHubPlaybackController::Eject(TFunction<void()> CompletionCallback)
{
	bIsReady = false;
	
	StopPlayback();
	
	bIsPaused = false;
	RecordingPlayer->RestartPlayback(0);

	SetSelectionStartTime(FQualifiedFrameTime(FFrameTime::FromDecimal(0), GetFrameRate()));
	SetSelectionEndTime(FQualifiedFrameTime(FFrameTime::FromDecimal(0), GetFrameRate()));
	Playhead->SetValue(FQualifiedFrameTime(FFrameTime::FromDecimal(0), GetFrameRate()));
	StartTimestamp = 0.f;

	// Recording is done, clear the pointer.
	RecordingToPlay.Reset();
	
	if (RollbackPreset.IsValid())
	{
		RollbackPreset->ApplyToClientLatent([CompletionCallback](bool)
		{
			if (CompletionCallback)
			{
				CompletionCallback();
			}
		});
	}
	else if (CompletionCallback)
	{
		CompletionCallback();
	}
}

void FLiveLinkHubPlaybackController::GoToTime(FQualifiedFrameTime InTime)
{
	// Stop needs to occur to restart playback.
	StopPlayback();

	const double TimeDouble = InTime.AsSeconds();
	
	PlaybackStartTime -= TimeDouble;
	Playhead->SetValue(InTime);

	SyncToFrame(InTime.Time.GetFrame());
}

FQualifiedFrameTime FLiveLinkHubPlaybackController::GetSelectionStartTime() const
{
	return SelectionStartTime;
}

void FLiveLinkHubPlaybackController::SetSelectionStartTime(FQualifiedFrameTime InTime)
{
	SelectionStartTime = InTime;
}

FQualifiedFrameTime FLiveLinkHubPlaybackController::GetSelectionEndTime() const
{
	return SelectionEndTime;
}

void FLiveLinkHubPlaybackController::SetSelectionEndTime(FQualifiedFrameTime InTime)
{
	SelectionEndTime = InTime;
}

FQualifiedFrameTime FLiveLinkHubPlaybackController::GetLength() const
{
	const FFrameRate FrameRate = GetFrameRate();

	const double Length = RecordingToPlay ? RecordingToPlay->LengthInSeconds : 0.f;
	
	const double TotalFramesDouble = Length * FrameRate.Numerator;
	const int32 TotalFrames = FMath::FloorToInt(TotalFramesDouble);
	
	const FFrameNumber LastFrameNumber(TotalFrames - 1);
	
	const FQualifiedFrameTime FrameTime(LastFrameNumber, FrameRate);

	return FrameTime;
}

FQualifiedFrameTime FLiveLinkHubPlaybackController::GetCurrentTime() const
{
	const FQualifiedFrameTime Time = Playhead->GetValue();
	return Time;
}

FFrameNumber FLiveLinkHubPlaybackController::GetCurrentFrame() const
{
	return GetCurrentTime().Time.GetFrame();
}

FFrameRate FLiveLinkHubPlaybackController::GetFrameRate() const
{
	return CurrentFrameRate;
}

void FLiveLinkHubPlaybackController::Start()
{
	FString ThreadName = TEXT("LiveLinkHub Playback Controller ");
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());

	Thread.Reset(FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask()));
}

void FLiveLinkHubPlaybackController::Stop()
{
	Stopping = true;
}

uint32 FLiveLinkHubPlaybackController::Run()
{
	while (!Stopping.load())
	{
		bIsPlaybackWaiting = true;
		PlaybackEvent->Wait();
		bIsPlaybackWaiting = false;
		while (bIsPlaying)
		{
			if (bIsPaused)
			{
				FPlatformProcess::Sleep(0.002);
			}
			else
			{
				const bool bSynced = SyncToPlayhead();

				auto SetPlayhead = [&]()
				{
					const double Delta = FPlatformTime::Seconds() - PlaybackStartTime;
					double Position = bIsReverse ? StartTimestamp - Delta : StartTimestamp + Delta;
					Position = FMath::Clamp(Position, GetSelectionStartTime().AsSeconds(), GetSelectionEndTime().AsSeconds());
					Playhead->SetValue(FQualifiedFrameTime(FFrameTime::FromDecimal(Position * GetFrameRate().Numerator), GetFrameRate()));
				};

				SetPlayhead();
			
				// Don't sleep if we pushed frames since that can take a small amount of time.
				if (!bSynced)
				{
					FPlatformProcess::Sleep(0.002);
					SetPlayhead();
				}
			}
			
			if (ShouldRestart())
			{
				if (bLoopPlayback && RecordingToPlay->LengthInSeconds != 0 && !bIsPaused)
				{
					RestartPlayback();
				}
				else
				{
					// Stop playback
					break;
				}
			}

		}

		// If the loop ended because the recording is over.
		bIsPlaying = false;
		
		// Trigger the playback finished delegate on the game thread.
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FLiveLinkHubPlaybackController::OnPlaybackFinished_Internal), TStatId(), nullptr, ENamedThreads::GameThread);
	}

	return 0;
}

void FLiveLinkHubPlaybackController::OnPlaybackFinished_Internal()
{
	PlaybackFinishedDelegate.Broadcast();
}

void FLiveLinkHubPlaybackController::OnSourceRemoved(FGuid Guid)
{
	if (RecordingToPlay.IsValid() && !bIsPreparingPlayback)
	{
		// Look for a source that is for this recording and eject. This can occur if the user presses the trash icon
		// on the playback source while it is in playback.
		for (const FLiveLinkSourcePreset& Presets : RecordingToPlay->RecordingPreset->GetSourcePresets())
		{
			if (Presets.Guid == Guid)
			{
				Eject();
				break;
			}
		}
	}
}

void FLiveLinkHubPlaybackController::PushSubjectData(const FLiveLinkRecordedFrame& NextFrame, bool bForceSync)
{
	// If we're sending static data
	if (NextFrame.LiveLinkRole)
	{
		FLiveLinkStaticDataStruct StaticDataStruct;
		StaticDataStruct.InitializeWith(NextFrame.Data.GetScriptStruct(), (FLiveLinkBaseStaticData*)NextFrame.Data.GetMemory());
		Client->PushSubjectStaticData_AnyThread(NextFrame.SubjectKey, NextFrame.LiveLinkRole, MoveTemp(StaticDataStruct));
	}
	else
	{
		FLiveLinkFrameDataStruct FrameDataStruct;
		FrameDataStruct.InitializeWith(NextFrame.Data.GetScriptStruct(), (FLiveLinkBaseFrameData*)NextFrame.Data.GetMemory());

		CurrentFrameRate = FrameDataStruct.GetBaseData()->MetaData.SceneTime.Rate;
		
		if (bForceSync)
		{
			FrameDataStruct.GetBaseData()->MetaData.StringMetaData.Add(TEXT("ForceSync"), TEXT("true"));
		}
		Client->PushSubjectFrameData_AnyThread(NextFrame.SubjectKey, MoveTemp(FrameDataStruct));
	}
}

bool FLiveLinkHubPlaybackController::SyncToPlayhead()
{
	const double Timestamp = Playhead->GetValue().AsSeconds();
	TArray<FLiveLinkRecordedFrame> NextFrames = bIsReverse ? RecordingPlayer->FetchPreviousFramesAtTimestamp(Timestamp)
		: RecordingPlayer->FetchNextFramesAtTimestamp(Timestamp);
	
	for (const FLiveLinkRecordedFrame& NextFrame : NextFrames)
	{
		// todo: Have to forcesync -- interpolation fails due to improper frame times
		const bool bForceSync = bIsReverse;
		PushSubjectData(NextFrame, bForceSync);
	}
	
	return NextFrames.Num() > 0;
}

bool FLiveLinkHubPlaybackController::SyncToFrame(const FFrameNumber& InFrameNumber)
{
	TArray<FLiveLinkRecordedFrame> NextFrames = RecordingPlayer->FetchNextFramesAtIndex(InFrameNumber.Value);
	
	for (const FLiveLinkRecordedFrame& NextFrame : NextFrames)
	{
		PushSubjectData(NextFrame, true);
	}

	return NextFrames.Num() > 0;
}

bool FLiveLinkHubPlaybackController::ShouldRestart() const
{
	const FFrameNumber CurrentFrame = GetCurrentFrame();
	return RecordingToPlay.IsValid() && ((bIsReverse && CurrentFrame <= GetSelectionStartTime().Time.GetFrame())
		|| (!bIsReverse && CurrentFrame >= GetSelectionEndTime().Time.GetFrame()));
}
