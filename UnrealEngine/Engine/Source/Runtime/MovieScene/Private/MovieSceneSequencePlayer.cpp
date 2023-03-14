// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSequencePlayer.h"
#include "MovieScene.h"
#include "MovieSceneFwd.h"
#include "MovieSceneTimeHelpers.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceTickManager.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/RuntimeErrors.h"
#include "Net/UnrealNetwork.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Algo/BinarySearch.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSequencePlayer)

DEFINE_LOG_CATEGORY_STATIC(LogMovieSceneRepl, Log, All);

DECLARE_STATS_GROUP(TEXT("MovieSceneRepl"), STATGROUP_MovieSceneRepl, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT(TEXT("NumServerSamples"),MovieSceneRepl_NumServerSamples,STATGROUP_MovieSceneRepl);
DECLARE_FLOAT_COUNTER_STAT(TEXT("SmoothedServerTime"),MovieSceneRepl_SmoothedServerTime,STATGROUP_MovieSceneRepl);

float GSequencerNetSyncThresholdMS = 200;
static FAutoConsoleVariableRef CVarSequencerNetSyncThresholdMS(
	TEXT("Sequencer.NetSyncThreshold"),
	GSequencerNetSyncThresholdMS,
	TEXT("(Default: 200ms. Defines the threshold at which clients and servers must be forcibly re-synced during playback.")
	);

int32 GSequencerMaxSmoothedNetSyncSampleAge = 5000;
static FAutoConsoleVariableRef CVarSequencerMaxSmoothedNetSyncSampleAge(
	TEXT("Sequencer.SmoothedMaxNetSyncSampleAge"),
	GSequencerMaxSmoothedNetSyncSampleAge,
	TEXT("(Default: 5000. Defines the range of samples (in milliseconds) required to perform smoothed net sync. Use 0 to disable smoothing.")
	);

int32 GSequencerMaxSmoothedNetSyncSampleCount = 50;
static FAutoConsoleVariableRef CVarSequencerMaxSmoothedNetSyncSampleCount(
	TEXT("Sequencer.SmoothedMaxNetSyncSampleCount"),
	GSequencerMaxSmoothedNetSyncSampleCount,
	TEXT("(Default: 50. The maximum number of samples to keep in memory.")
	);

float GSequencerSmoothedNetSyncDeviationThreshold = 200;
static FAutoConsoleVariableRef CVarSequencerSmoothedNetSyncDeviationThreshold(
	TEXT("Sequencer.SmoothedNetSyncDeviationThreshold"),
	GSequencerSmoothedNetSyncDeviationThreshold,
	TEXT("(Default: 200ms. Defines the acceptable deviation for smoothed net sync samples. Samples outside this deviation will be discarded.")
	);

bool FMovieSceneSequenceLoopCount::SerializeFromMismatchedTag( const FPropertyTag& Tag, FStructuredArchive::FSlot Slot )
{
	if (Tag.Type == NAME_IntProperty)
	{
		Slot << Value;
		return true;
	}

	return false;
}

bool FMovieSceneSequencePlaybackSettings::SerializeFromMismatchedTag( const FPropertyTag& Tag, FStructuredArchive::FSlot Slot )
{
	if (Tag.Type == NAME_StructProperty && Tag.StructName == "LevelSequencePlaybackSettings")
	{
		StaticStruct()->SerializeItem(Slot, this, nullptr);
		return true;
	}

	return false;
}

FFrameTime FMovieSceneSequencePlaybackParams::GetPlaybackPosition(UMovieSceneSequencePlayer* Player) const
{
	FFrameTime PlaybackPosition = Player->GetCurrentTime().Time;

	if (PositionType == EMovieScenePositionType::Frame)
	{
		PlaybackPosition = Frame;
	}
	else if (PositionType == EMovieScenePositionType::Time)
	{
		PlaybackPosition = Time * Player->GetFrameRate();
	}
	else if (PositionType == EMovieScenePositionType::MarkedFrame)
	{
		UMovieScene* MovieScene = Player->GetSequence() ? Player->GetSequence()->GetMovieScene() : nullptr;

		if (MovieScene)
		{
			int32 MarkedIndex = MovieScene->FindMarkedFrameByLabel(MarkedFrame);

			if (MarkedIndex != INDEX_NONE)
			{
				PlaybackPosition = ConvertFrameTime(MovieScene->GetMarkedFrames()[MarkedIndex].FrameNumber, MovieScene->GetTickResolution(), MovieScene->GetDisplayRate());
			}
		}
	}

	return PlaybackPosition;
}

UMovieSceneSequencePlayer::UMovieSceneSequencePlayer(const FObjectInitializer& Init)
	: Super(Init)
	, Status(EMovieScenePlayerStatus::Stopped)
	, bReversePlayback(false)
	, bPendingOnStartedPlaying(false)
	, bIsEvaluating(false)
	, bIsAsyncUpdate(false)
	, bSkipNextUpdate(false)
	, Sequence(nullptr)
	, StartTime(0)
	, DurationFrames(0)
	, DurationSubFrames(0.f)
	, CurrentNumLoops(0)
	, CurrentRunner(nullptr)
{
	PlayPosition.Reset(FFrameTime(0));

	NetSyncProps.LastKnownPosition = FFrameTime(0);
	NetSyncProps.LastKnownStatus   = Status;
}

UMovieSceneSequencePlayer::~UMovieSceneSequencePlayer()
{
	if (GEngine && OldMaxTickRate.IsSet())
	{
		GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
	}
}

void UMovieSceneSequencePlayer::UpdateNetworkSyncProperties()
{
	if (HasAuthority())
	{
		NetSyncProps.LastKnownPosition = PlayPosition.GetCurrentPosition();
		NetSyncProps.LastKnownStatus   = Status;
		NetSyncProps.LastKnownNumLoops = CurrentNumLoops;
	}
}

void UMovieSceneSequencePlayer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMovieSceneSequencePlayer, NetSyncProps);
	DOREPLIFETIME(UMovieSceneSequencePlayer, bReversePlayback);
	DOREPLIFETIME(UMovieSceneSequencePlayer, StartTime);
	DOREPLIFETIME(UMovieSceneSequencePlayer, DurationFrames);
	DOREPLIFETIME(UMovieSceneSequencePlayer, DurationSubFrames);
	DOREPLIFETIME(UMovieSceneSequencePlayer, PlaybackSettings);
	DOREPLIFETIME(UMovieSceneSequencePlayer, Observer);
}

EMovieScenePlayerStatus::Type UMovieSceneSequencePlayer::GetPlaybackStatus() const
{
	return Status;
}

FMovieSceneSpawnRegister& UMovieSceneSequencePlayer::GetSpawnRegister()
{
	return SpawnRegister.IsValid() ? *SpawnRegister : IMovieScenePlayer::GetSpawnRegister();
}

void UMovieSceneSequencePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& InSequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	bool bAllowDefault = PlaybackClient ? PlaybackClient->RetrieveBindingOverrides(InBindingId, SequenceID, OutObjects) : true;

	if (bAllowDefault)
	{
		InSequence.LocateBoundObjects(InBindingId, ResolutionContext, OutObjects);
	}
}

void UMovieSceneSequencePlayer::Play()
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::Play));
		return;
	}

	bReversePlayback = false;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayReverse()
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::PlayReverse));
		return;
	}

	bReversePlayback = true;
	PlayInternal();
}

void UMovieSceneSequencePlayer::ChangePlaybackDirection()
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::ChangePlaybackDirection));
		return;
	}

	bReversePlayback = !bReversePlayback;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayLooping(int32 NumLoops)
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::PlayLooping, NumLoops));
		return;
	}

	PlaybackSettings.LoopCount.Value = NumLoops;
	PlayInternal();
}

void UMovieSceneSequencePlayer::PlayInternal()
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::PlayInternal));
		return;
	}

	if (!IsPlaying() && Sequence && CanPlay())
	{
		// Set playback status to playing before any calls to update the position
		Status = EMovieScenePlayerStatus::Playing;

		float PlayRate = bReversePlayback ? -PlaybackSettings.PlayRate : PlaybackSettings.PlayRate;

		// If at the end and playing forwards, rewind to beginning
		if (GetCurrentTime().Time == GetLastValidTime())
		{
			if (PlayRate > 0.f)
			{
				SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(StartTime), EUpdatePositionMethod::Jump));
			}
		}
		else if (GetCurrentTime().Time == FFrameTime(StartTime))
		{
			if (PlayRate < 0.f)
			{
				SetPlaybackPosition(FMovieSceneSequencePlaybackParams(GetLastValidTime(), EUpdatePositionMethod::Jump));
			}
		}

		// Update now
		if (PlaybackSettings.bRestoreState)
		{
			RootTemplateInstance.EnableGlobalPreAnimatedStateCapture();
		}

		bPendingOnStartedPlaying = true;
		Status = EMovieScenePlayerStatus::Playing;
		TimeController->StartPlaying(GetCurrentTime());
		
		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		UMovieScene*         MovieScene         = MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;

		if (PlayPosition.GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked)
		{
			if (!OldMaxTickRate.IsSet())
			{
				OldMaxTickRate = GEngine->GetMaxFPS();
			}

			GEngine->SetMaxFPS(1.f / PlayPosition.GetInputRate().AsInterval());
		}

		if (!PlayPosition.GetLastPlayEvalPostition().IsSet() || PlayPosition.GetLastPlayEvalPostition() != PlayPosition.GetCurrentPosition())
		{
			UpdateMovieSceneInstance(PlayPosition.PlayTo(PlayPosition.GetCurrentPosition()), EMovieScenePlayerStatus::Playing);
		}

		RunLatentActions();

		UpdateNetworkSyncProperties();

		if (MovieSceneSequence)
		{
			UE_LOG(LogMovieScene, Verbose, TEXT("PlayInternal - MovieSceneSequence: %s"), *MovieSceneSequence->GetName());
		}

		if (bReversePlayback)
		{
			if (OnPlayReverse.IsBound())
			{
				OnPlayReverse.Broadcast();
			}
		}
		else
		{
			if (OnPlay.IsBound())
			{
				OnPlay.Broadcast();
			}
		}
	}
}

void UMovieSceneSequencePlayer::Pause()
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::Pause));
		return;
	}

	if (IsPlaying())
	{
		Status = EMovieScenePlayerStatus::Paused;
		TimeController->StopPlaying(GetCurrentTime());

		PauseOnFrame.Reset();
		LastTickGameTimeSeconds.Reset();

		auto FinishPause = [this]
		{
			this->RunLatentActions();
			this->UpdateNetworkSyncProperties();

			UMovieSceneSequence* MovieSceneSequence = this->RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
			if (MovieSceneSequence)
			{
				UE_LOG(LogMovieScene, Verbose, TEXT("Pause - MovieSceneSequence: %s"), *MovieSceneSequence->GetName());
			}

			if (this->OnPause.IsBound())
			{
				this->OnPause.Broadcast();
			}
		};

		if (Observer && !Observer->CanObserveSequence())
		{
			FinishPause();
		}
		// Evaluate the sequence at its current time, with a status of 'stopped' to ensure that animated state pauses correctly. (ie. audio sounds should stop/pause)
		else if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner())
		{
			FMovieSceneEvaluationRange CurrentTimeRange = PlayPosition.GetCurrentPositionAsRange();
			const FMovieSceneContext Context(CurrentTimeRange, EMovieScenePlayerStatus::Stopped);

			Runner->QueueUpdate(Context, RootTemplateInstance.GetRootInstanceHandle(), FSimpleDelegate::CreateWeakLambda(this, FinishPause));
		}
		else
		{
			FinishPause();
		}
	}
}

void UMovieSceneSequencePlayer::Scrub()
{
	Status = EMovieScenePlayerStatus::Scrubbing;
	TimeController->StopPlaying(GetCurrentTime());

	UpdateNetworkSyncProperties();
}

void UMovieSceneSequencePlayer::Stop()
{
	StopInternal(bReversePlayback ? GetLastValidTime() : FFrameTime(StartTime));
}

void UMovieSceneSequencePlayer::StopAtCurrentTime()
{
	StopInternal(PlayPosition.GetCurrentPosition());
}

void UMovieSceneSequencePlayer::StopInternal(FFrameTime TimeToResetTo)
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::StopInternal, TimeToResetTo));
		return;
	}

	if (IsPlaying() || IsPaused())
	{
		Status = EMovieScenePlayerStatus::Stopped;

		// Put the cursor at the specified position
		PlayPosition.Reset(TimeToResetTo);
		if (TimeController.IsValid())
		{
			TimeController->StopPlaying(GetCurrentTime());
		}

		CurrentNumLoops = 0;
		PauseOnFrame.Reset();
		LastTickGameTimeSeconds.Reset();

		// Reset loop count on stop so that it doesn't persist to the next call to play
		PlaybackSettings.LoopCount.Value = 0;

		if (PlaybackSettings.bRestoreState)
		{
			RestorePreAnimatedState();
		}

		// Lambda that is invoked when the request to finish this sequence has been fulfilled
		auto OnFlushed = [this, TimeToResetTo]
		{
			if (this->OldMaxTickRate.IsSet())
			{
				GEngine->SetMaxFPS(OldMaxTickRate.GetValue());
				this->OldMaxTickRate.Reset();
			}

			if (this->HasAuthority())
			{
				// Explicitly handle Stop() events through an RPC call
				this->RPC_OnStopEvent(TimeToResetTo);
			}
			this->UpdateNetworkSyncProperties();

			this->OnStopped();

			if (this->RootTemplateInstance.IsValid())
			{
				UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
				if (MovieSceneSequence)
				{
					UE_LOG(LogMovieScene, Verbose, TEXT("Stop - MovieSceneSequence: %s"), *MovieSceneSequence->GetName());
				}
			}

			if (this->OnStop.IsBound())
			{
				this->OnStop.Broadcast();
			}

			this->RunLatentActions();
		};

		TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner();

		if (Observer && !Observer->CanObserveSequence())
		{
			OnFlushed();
		}
		else if (Runner)
		{
			// Finish but do not destroy
			if (Runner->QueueFinalUpdate(RootTemplateInstance.GetRootInstanceHandle(), FSimpleDelegate::CreateWeakLambda(this, OnFlushed)))
			{
				Runner->Flush();
			}
		}
	}
	else if (RootTemplateInstance.IsValid() && RootTemplateInstance.HasEverUpdated())
	{
		if (PlaybackSettings.bRestoreState)
		{
			RestorePreAnimatedState();
		}

		TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner();
		if (Runner)
		{
			// Finish but do not destroy
			if (Runner->QueueFinalUpdate(RootTemplateInstance.GetRootInstanceHandle()))
			{
				Runner->Flush();
			}
		}
	}
}

void UMovieSceneSequencePlayer::FinishPlaybackInternal(FFrameTime TimeToFinishAt)
{
	if (PlaybackSettings.bPauseAtEnd)
	{
		Pause();
	}
	else
	{
		StopInternal(TimeToFinishAt);
	}

	TimeController->StopPlaying(GetCurrentTime());

	if (OnFinished.IsBound())
	{
		OnFinished.Broadcast();
	}
}

void UMovieSceneSequencePlayer::GoToEndAndStop()
{
	FFrameTime LastValidTime = GetLastValidTime();

	if (PlayPosition.GetCurrentPosition() == LastValidTime && Status == EMovieScenePlayerStatus::Stopped)
	{
		return;
	}

	Status = EMovieScenePlayerStatus::Playing;
	SetPlaybackPosition(FMovieSceneSequencePlaybackParams(LastValidTime, EUpdatePositionMethod::Jump));
	StopInternal(LastValidTime);
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetCurrentTime() const
{
	FFrameTime Time = PlayPosition.GetCurrentPosition();
	return FQualifiedFrameTime(Time, PlayPosition.GetInputRate());
}

FQualifiedFrameTime UMovieSceneSequencePlayer::GetDuration() const
{
	return FQualifiedFrameTime(FFrameTime(DurationFrames, DurationSubFrames), PlayPosition.GetInputRate());
}

int32 UMovieSceneSequencePlayer::GetFrameDuration() const
{
	return DurationFrames;
}

void UMovieSceneSequencePlayer::SetFrameRate(FFrameRate FrameRate)
{
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (MovieScene)
	{
		if (MovieScene->GetEvaluationType() == EMovieSceneEvaluationType::FrameLocked && !FrameRate.IsMultipleOf(MovieScene->GetTickResolution()))
		{
			UE_LOG(LogMovieScene, Warning, TEXT("Attempting to play back a sequence with tick resolution of %f ticks per second frame locked to %f fps, which is not a multiple of the resolution."), MovieScene->GetTickResolution().AsDecimal(), FrameRate.AsDecimal());
		}
	}

	FFrameRate CurrentInputRate = PlayPosition.GetInputRate();

	StartTime      = ConvertFrameTime(StartTime,                    CurrentInputRate, FrameRate).FloorToFrame();
	DurationFrames = ConvertFrameTime(FFrameNumber(DurationFrames), CurrentInputRate, FrameRate).RoundToFrame().Value;

	PlayPosition.SetTimeBase(FrameRate, PlayPosition.GetOutputRate(), PlayPosition.GetEvaluationType());
}

void UMovieSceneSequencePlayer::SetFrameRange( int32 NewStartTime, int32 Duration, float SubFrames )
{
	Duration = FMath::Max(Duration, 0);

	StartTime      = NewStartTime;
	DurationFrames = Duration;
	DurationSubFrames = SubFrames;

	TOptional<FFrameTime> CurrentTime = PlayPosition.GetCurrentPosition();
	if (CurrentTime.IsSet())
	{
		FFrameTime LastValidTime = GetLastValidTime();

		if (CurrentTime.GetValue() < StartTime)
		{
			PlayPosition.Reset(StartTime);
		}
		else if (CurrentTime.GetValue() > LastValidTime)
		{
			PlayPosition.Reset(LastValidTime);
		}
	}

	if (TimeController.IsValid())
	{
		TimeController->Reset(GetCurrentTime());
	}

	UpdateNetworkSyncProperties();
}

void UMovieSceneSequencePlayer::SetTimeRange( float StartTimeSeconds, float DurationSeconds )
{
	const FFrameRate Rate = PlayPosition.GetInputRate();

	const FFrameNumber StartFrame = ( StartTimeSeconds * Rate ).FloorToFrame();
	const FFrameNumber Duration   = ( DurationSeconds  * Rate ).RoundToFrame();

	SetFrameRange(StartFrame.Value, Duration.Value);
}

void UMovieSceneSequencePlayer::PlayTo(FMovieSceneSequencePlaybackParams InPlaybackParams, FMovieSceneSequencePlayToParams PlayToParams)
{
	FPauseOnArgs Args;
	Args.Time = InPlaybackParams.GetPlaybackPosition(this);
	Args.bExclusive = PlayToParams.bExclusive;
	PauseOnFrame = Args;

	if (GetCurrentTime().Time < Args.Time)
	{
		Play();
	}
	else
	{
		PlayReverse();
	}
}

void UMovieSceneSequencePlayer::SetPlaybackPosition(FMovieSceneSequencePlaybackParams InPlaybackParams)
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::SetPlaybackPosition, InPlaybackParams));
		return;
	}

	FFrameTime NewPosition = InPlaybackParams.GetPlaybackPosition(this);

	UpdateTimeCursorPosition(NewPosition, InPlaybackParams.UpdateMethod, InPlaybackParams.bHasJumped);

	TimeController->Reset(GetCurrentTime());

	if (HasAuthority())
	{
		RPC_ExplicitServerUpdateEvent(InPlaybackParams.UpdateMethod, NewPosition);
	}
}

void UMovieSceneSequencePlayer::RestoreState()
{
	if (!PlaybackSettings.bRestoreState)
	{
		UE_LOG(LogMovieScene, Warning, TEXT("Attempting to restore pre-animated state for a player that was not set to capture pre-animated state. Please enable PlaybackSettings.bRestoreState"));
	}

	RestorePreAnimatedState();
}

bool UMovieSceneSequencePlayer::IsPlaying() const
{
	return Status == EMovieScenePlayerStatus::Playing;
}

bool UMovieSceneSequencePlayer::IsPaused() const
{
	return Status == EMovieScenePlayerStatus::Paused;
}

bool UMovieSceneSequencePlayer::IsReversed() const
{
	return bReversePlayback;
}

float UMovieSceneSequencePlayer::GetPlayRate() const
{
	return PlaybackSettings.PlayRate;
}

void UMovieSceneSequencePlayer::SetPlayRate(float PlayRate)
{
	PlaybackSettings.PlayRate = PlayRate;
}

FFrameTime UMovieSceneSequencePlayer::GetLastValidTime() const
{
	if (DurationFrames > 0)
	{
		if (DurationSubFrames > 0.f)
		{
			return FFrameTime(StartTime + DurationFrames, DurationSubFrames);
		}
		else
		{
			return FFrameTime(StartTime + DurationFrames - 1, 0.99999994f);
		}
	}
	else
	{
		return FFrameTime(StartTime);
	}
}

FFrameRate UMovieSceneSequencePlayer::GetDisplayRate() const
{
	return Sequence && Sequence->GetMovieScene() ? Sequence->GetMovieScene()->GetDisplayRate() : FFrameRate();
}

bool UMovieSceneSequencePlayer::ShouldStopOrLoop(FFrameTime NewPosition) const
{
	bool bShouldStopOrLoop = false;
	if (IsPlaying())
	{
		if (!bReversePlayback)
		{
			bShouldStopOrLoop = NewPosition >= FFrameTime(StartTime + GetFrameDuration(), DurationSubFrames);
		}
		else
		{
			bShouldStopOrLoop = NewPosition.FrameNumber < StartTime;
		}
	}
	return bShouldStopOrLoop;
}

TOptional<TRange<FFrameTime>> UMovieSceneSequencePlayer::GetPauseRange(const FFrameTime& NewPosition) const
{
	if (IsPlaying() && PauseOnFrame.IsSet())
	{
		TRange<FFrameTime> OutRange;

		// NewPosition and PauseOnFrame are stored in Display Rate, but we need to convert to tick resolution for the actual ranges.
		FFrameTime PauseTime = FFrameRate::TransformTime(PauseOnFrame->Time, PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());
		FFrameTime CurrentTime = FFrameRate::TransformTime(PlayPosition.GetCurrentPosition(), PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());
		FFrameTime NewTime = FFrameRate::TransformTime(NewPosition, PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());

		// This logic is a little complicated due to inclusive/exclusive range times for pausing. Inclusive/Exclusive only have any meaning
		// with direction (as we need direction to decide which bound to make inclusive/exclusive).
		if (bReversePlayback)
		{

			// If we're going in reverse then the start frame is the exclusive one
			OutRange = TRange<FFrameTime>(PauseOnFrame->bExclusive
				? TRangeBound<FFrameTime>::Inclusive(PauseTime + FFrameTime(FFrameNumber(1)))
				: TRangeBound<FFrameTime>::Inclusive(PauseTime),
				TRangeBound<FFrameTime>::Inclusive(CurrentTime));
			
		}
		else
		{
			// If we're playing forward then the upper bound is (potentially) exclusive
			OutRange = TRange<FFrameTime>(TRangeBound<FFrameTime>::Inclusive(CurrentTime),
				PauseOnFrame->bExclusive
				? TRangeBound<FFrameTime>::Exclusive(PauseTime)
				: TRangeBound<FFrameTime>::Exclusive(PauseTime + FFrameTime(FFrameNumber(1))));

		}

		// If the new time is outside of bounds, then we should pause.
		if (!OutRange.Contains(NewTime))
		{
			return OutRange;
		}
	}

	return TOptional<TRange<FFrameTime>>();
}

UMovieSceneEntitySystemLinker* UMovieSceneSequencePlayer::ConstructEntitySystemLinker()
{
	if (ensure(TickManager && RegisteredTickInterval.IsSet()) && !EnumHasAnyFlags(Sequence->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation))
	{
		return TickManager->GetLinker(RegisteredTickInterval.GetValue());
	}

	return UMovieSceneEntitySystemLinker::CreateLinker(GetPlaybackContext(), UE::MovieScene::EEntitySystemLinkerRole::Standalone);
}

void UMovieSceneSequencePlayer::InitializeForTick(UObject* Context)
{
	// Store a reference to the global tick manager to keep it alive while there are sequence players active.
	if (ensure(Context))
	{
		TickManager = UMovieSceneSequenceTickManager::Get(Context);
	}
}


void UMovieSceneSequencePlayer::SetPlaybackSettings(const FMovieSceneSequencePlaybackSettings& InSettings)
{
	PlaybackSettings = InSettings;
}

void UMovieSceneSequencePlayer::Initialize(UMovieSceneSequence* InSequence, const FMovieSceneSequencePlaybackSettings& InSettings)
{
	PlaybackSettings = InSettings;
	Initialize(InSequence);
}

void UMovieSceneSequencePlayer::Initialize(UMovieSceneSequence* InSequence)
{
	check(InSequence);
	check(!bIsEvaluating);

	// If we have a valid sequence that may have been played back,
	// Explicitly stop and tear down the template instance before 
	// reinitializing it with the new sequence. Care should be taken
	// here that Stop is not called on the first Initialization as this
	// may be called during PostLoad.
	if (Sequence)
	{
		StopAtCurrentTime();
	}

	Sequence = InSequence;

	FFrameTime StartTimeWithOffset = StartTime;

	EUpdateClockSource ClockToUse = EUpdateClockSource::Tick;

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (MovieScene)
	{
		EMovieSceneEvaluationType EvaluationType    = MovieScene->GetEvaluationType();
		FFrameRate                TickResolution    = MovieScene->GetTickResolution();
		FFrameRate                DisplayRate       = MovieScene->GetDisplayRate();

		UE_LOG(LogMovieScene, Verbose, TEXT("Initialize - MovieSceneSequence: %s, TickResolution: %f, DisplayRate: %d, CurrentTime: %d"), *InSequence->GetName(), TickResolution.Numerator, DisplayRate.Numerator);

		// We set the play position in terms of the display rate,
		// but want evaluation ranges in the moviescene's tick resolution
		PlayPosition.SetTimeBase(DisplayRate, TickResolution, EvaluationType);

		{
			// Set up the default frame range from the sequence's play range
			TRange<FFrameNumber> PlaybackRange   = MovieScene->GetPlaybackRange();

			const FFrameNumber SrcStartFrame = UE::MovieScene::DiscreteInclusiveLower(PlaybackRange);
			const FFrameNumber SrcEndFrame   = UE::MovieScene::DiscreteExclusiveUpper(PlaybackRange);

			const FFrameTime EndingTime = ConvertFrameTime(SrcEndFrame, TickResolution, DisplayRate);

			const FFrameNumber StartingFrame = ConvertFrameTime(SrcStartFrame, TickResolution, DisplayRate).FloorToFrame();
			const FFrameNumber EndingFrame   = EndingTime.FloorToFrame();

			SetFrameRange(StartingFrame.Value, (EndingFrame - StartingFrame).Value, EndingTime.GetSubFrame());
		}

		// Reset the play position based on the user-specified start offset, or a random time
		FFrameTime SpecifiedStartOffset = PlaybackSettings.StartTime * DisplayRate;

		// Setup the starting time
		FFrameTime StartingTimeOffset = PlaybackSettings.bRandomStartTime
			? FFrameTime(FMath::Rand() % GetFrameDuration())
			: FMath::Clamp<FFrameTime>(SpecifiedStartOffset, 0, GetFrameDuration()-1);
			
		StartTimeWithOffset = StartTime + StartingTimeOffset;

		ClockToUse = MovieScene->GetClockSource();

		if (ClockToUse == EUpdateClockSource::Custom)
		{
			TimeController = MovieScene->MakeCustomTimeController(GetPlaybackContext());
		}
	}

	if (!TimeController.IsValid())
	{
		switch (ClockToUse)
		{
		case EUpdateClockSource::Audio:    TimeController = MakeShared<FMovieSceneTimeController_AudioClock>();    break;
		case EUpdateClockSource::Platform: TimeController = MakeShared<FMovieSceneTimeController_PlatformClock>(); break;
		case EUpdateClockSource::RelativeTimecode: TimeController = MakeShared<FMovieSceneTimeController_RelativeTimecodeClock>(); break;
		case EUpdateClockSource::Timecode: TimeController = MakeShared<FMovieSceneTimeController_TimecodeClock>(); break;
		case EUpdateClockSource::PlayEveryFrame: TimeController = MakeShared<FMovieSceneTimeController_PlayEveryFrame>(); break;
		default:                           TimeController = MakeShared<FMovieSceneTimeController_Tick>();          break;
		}

		if (!ensureMsgf(TimeController.IsValid(), TEXT("No time controller specified for sequence playback. Falling back to Engine Tick clock source.")))
		{
			TimeController = MakeShared<FMovieSceneTimeController_Tick>();
		}
	}

	if (!TickManager)
	{
		InitializeForTick(GetPlaybackContext());
	}

	FMovieSceneSequenceTickInterval TickInterval = PlaybackSettings.bInheritTickIntervalFromOwner
		? FMovieSceneSequenceTickInterval::GetInheritedInterval(this)
		: PlaybackSettings.TickInterval;

	if (RegisteredTickInterval.IsSet() && RegisteredTickInterval.GetValue() != TickInterval)
	{
		TickManager->UnregisterTickClient(this);
	}

	RegisteredTickInterval = TickInterval;
	TickManager->RegisterTickClient(RegisteredTickInterval.GetValue(), this);

	TSharedPtr<FMovieSceneEntitySystemRunner> RunnerToUse = TickManager->GetRunner(RegisteredTickInterval.GetValue());
	if (EnumHasAnyFlags(Sequence->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation))
	{
		SynchronousRunner = MakeShared<FMovieSceneEntitySystemRunner>();
		RunnerToUse = SynchronousRunner;
	}

	check(RunnerToUse);
	RootTemplateInstance.Initialize(*Sequence, *this, nullptr, RunnerToUse);

	LatentActionManager.ClearLatentActions();

	// Set up playback position (with offset) after Stop(), which will reset the starting time to StartTime
	PlayPosition.Reset(StartTimeWithOffset);
	TimeController->Reset(GetCurrentTime());

	UpdateNetworkSyncProperties();
}

void UMovieSceneSequencePlayer::Update(const float DeltaSeconds)
{
	UWorld* World = GetPlaybackWorld();
	float CurrentWorldTime = 0.f;
	if (World)
	{
		CurrentWorldTime = World->GetTimeSeconds();
	}

	if (IsPlaying())
	{
		// Delta seconds has already been multiplied by GetEffectiveTimeDilation at this point, so don't pass that through to Tick
		float PlayRate = bReversePlayback ? -PlaybackSettings.PlayRate : PlaybackSettings.PlayRate;

		float DeltaTimeForFunction = DeltaSeconds;

		TimeController->Tick(DeltaTimeForFunction, PlayRate);

		if (World)
		{
			PlayRate *= World->GetWorldSettings()->GetEffectiveTimeDilation();
		}

		if (!bSkipNextUpdate)
		{
			check(!bIsEvaluating);

			FFrameTime NewTime = TimeController->RequestCurrentTime(GetCurrentTime(), PlayRate, GetDisplayRate());
			UpdateTimeCursorPosition(NewTime, EUpdatePositionMethod::Play);
		}

		bSkipNextUpdate = false;

		// CAREFUL with stateful changes after this... in 95% of cases, the sequence evaluation was
		// only queued up, and hasn't run yet!
	}

	if (World)
	{
		LastTickGameTimeSeconds = CurrentWorldTime;
	}
}

void UMovieSceneSequencePlayer::TickFromSequenceTickManager(float DeltaSeconds, FMovieSceneEntitySystemRunner* InRunner)
{
	TGuardValue<FMovieSceneEntitySystemRunner*> RunnerGuard(CurrentRunner, InRunner);
	UpdateAsync(DeltaSeconds);
}

void UMovieSceneSequencePlayer::UpdateAsync(const float DeltaSeconds)
{
	check(!bIsAsyncUpdate);
	bIsAsyncUpdate = true;

	Update(DeltaSeconds);

	bIsAsyncUpdate = false;
}

void UMovieSceneSequencePlayer::UpdateTimeCursorPosition(FFrameTime NewPosition, EUpdatePositionMethod Method, bool bHasJumpedOverride)
{
	if (ensure(!bIsEvaluating))
	{
		UpdateTimeCursorPosition_Internal(NewPosition, Method, bHasJumpedOverride);
	}
}

EMovieScenePlayerStatus::Type UpdateMethodToStatus(EUpdatePositionMethod Method)
{
	switch(Method)
	{
	case EUpdatePositionMethod::Scrub: return EMovieScenePlayerStatus::Scrubbing;
	case EUpdatePositionMethod::Jump:  return EMovieScenePlayerStatus::Stopped;
	case EUpdatePositionMethod::Play:  return EMovieScenePlayerStatus::Playing;
	default:                           return EMovieScenePlayerStatus::Stopped;
	}
}

FMovieSceneEvaluationRange UpdatePlayPosition(FMovieScenePlaybackPosition& InOutPlayPosition, FFrameTime NewTime, EUpdatePositionMethod Method)
{
	if (Method == EUpdatePositionMethod::Play)
	{
		return InOutPlayPosition.PlayTo(NewTime);
	}

	return InOutPlayPosition.JumpTo(NewTime);
}

void UMovieSceneSequencePlayer::UpdateTimeCursorPosition_Internal(FFrameTime NewPosition, EUpdatePositionMethod Method, bool bHasJumpedOverride)
{
	EMovieScenePlayerStatus::Type StatusOverride = UpdateMethodToStatus(Method);

	const int32 Duration = DurationFrames;
	if (Duration == 0)
	{
		UE_LOG(LogMovieScene, Warning, TEXT("Attempting to play back a sequence with zero duration"));
		return;
	}
	
	if (bPendingOnStartedPlaying)
	{
		OnStartedPlaying();
		bPendingOnStartedPlaying = false;
	}

	// If we should pause during this evaluation, we'll handle that below.
	const TOptional<TRange<FFrameTime>> PauseRange = GetPauseRange(NewPosition);
	if (!PauseRange.IsSet() && Method == EUpdatePositionMethod::Play && ShouldStopOrLoop(NewPosition))
	{
		// The actual start time taking into account reverse playback
		FFrameNumber StartTimeWithReversed = bReversePlayback ? GetLastValidTime().FrameNumber : StartTime;

		// The actual end time taking into account reverse playback
		FFrameTime EndTimeWithReversed = bReversePlayback ? StartTime : GetLastValidTime().FrameNumber;

		FFrameTime PositionRelativeToStart = NewPosition.FrameNumber - StartTimeWithReversed;

		const int32 NumTimesLooped    = FMath::Abs(PositionRelativeToStart.FrameNumber.Value / Duration);
		const bool  bLoopIndefinitely = PlaybackSettings.LoopCount.Value < 0;

		// loop playback
		if (bLoopIndefinitely || CurrentNumLoops + NumTimesLooped <= PlaybackSettings.LoopCount.Value)
		{
			CurrentNumLoops += NumTimesLooped;
			if (NumTimesLooped > 0)
			{
				// Reset server time samples when this player has looped. This ensures that
				// smoothed playback (if enabled) does not result in a smoothed frame in the previous
				// loop.
				ServerTimeSamples.Reset();
			}

			// Finish evaluating any frames left in the current loop in case they have events attached
			FFrameTime CurrentPosition = PlayPosition.GetCurrentPosition();
			if ((bReversePlayback && CurrentPosition > EndTimeWithReversed) ||
				(!bReversePlayback && CurrentPosition < EndTimeWithReversed))
			{
				FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(EndTimeWithReversed);
				UpdateMovieSceneInstance(Range, StatusOverride);
			}

			const FFrameTime Overplay = FFrameTime(PositionRelativeToStart.FrameNumber.Value % Duration, PositionRelativeToStart.GetSubFrame());
			FFrameTime NewFrameOffset;
			
			if (bReversePlayback)
			{
				NewFrameOffset = (Overplay > 0) ?  FFrameTime(Duration) + Overplay : Overplay;
			}
			else
			{
				NewFrameOffset = (Overplay < 0) ? FFrameTime(Duration) + Overplay : Overplay;
			}

			if (SpawnRegister.IsValid())
			{
				SpawnRegister->ForgetExternallyOwnedSpawnedObjects(State, *this);
			}

			// Reset the play position, and generate a new range that gets us to the new frame time
			if (bReversePlayback)
			{
				PlayPosition.Reset(Overplay > 0 ? GetLastValidTime() : StartTimeWithReversed);
			}
			else
			{
				PlayPosition.Reset(Overplay < 0 ? GetLastValidTime() : StartTimeWithReversed);
			}

			FMovieSceneEvaluationRange Range = PlayPosition.PlayTo(StartTimeWithReversed + NewFrameOffset);

			const bool bHasJumped = true;
			UpdateMovieSceneInstance(Range, StatusOverride, bHasJumped);

			// Use the exact time here rather than a frame locked time to ensure we don't skip the amount that was overplayed in the time controller
			FQualifiedFrameTime ExactCurrentTime(StartTimeWithReversed + NewFrameOffset, PlayPosition.GetInputRate());
			TimeController->Reset(ExactCurrentTime);

			OnLooped();
		}

		// We reached the end of playback
		else
		{
			// Clamp the position to the duration
			NewPosition = FMath::Clamp(NewPosition, FFrameTime(StartTime), GetLastValidTime());

			FMovieSceneEvaluationRange Range = UpdatePlayPosition(PlayPosition, NewPosition, Method);
			UpdateMovieSceneInstance(Range, StatusOverride);

			// If we are using replicated playback and are not an authoritative sequence player, we need to wait 
			// for the server to tell us to finish.
			const bool bHasAuthorityToFinish = (
					HasAuthority() || 
					PlaybackClient == nullptr ||
					!PlaybackClient->GetIsReplicatedPlayback());
			if (bHasAuthorityToFinish)
			{
				FinishPlaybackInternal(NewPosition);

				// Explicitly tell the clients to finish their playback. They won't have called FinishPlaybackInternal
				// because it's in the line right above, only for sequence players with some authority
				// (client only or server).
				RPC_OnFinishPlaybackEvent(NewPosition);
			}
			else
			{
				const FString SequenceName = GetSequenceName(true);
				UE_LOG(LogMovieScene, Verbose, 
						TEXT("Keeping sequence %s at frame %d, subframe %f while waiting for playback finish from server."),
						*SequenceName, NewPosition.FrameNumber.Value, NewPosition.GetSubFrame());
			}
		}

		UpdateNetworkSyncProperties();
	}
	else
	{
		// If the desired evaluation will take us past where we want to go we need to use a clipped range provided by the PauseRange, otherwise use the normal one.
		FMovieSceneEvaluationRange Range = PauseRange.IsSet() 
			? FMovieSceneEvaluationRange(PauseRange.GetValue(), PlayPosition.GetOutputRate(), bReversePlayback ? EPlayDirection::Backwards : EPlayDirection::Forwards)
			: UpdatePlayPosition(PlayPosition, NewPosition, Method);

		UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
		const bool bIsSequenceBlocking = EnumHasAnyFlags(MovieSceneSequence->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation);
		
		// Just update the time and sequence... if we are in the main level update we want, if possible,
		// to only queue this sequence's update, so everything updates in parallel. If not possible, or if
		// not in the main level update, we run the evaluation synchronously.
		FMovieSceneUpdateArgs Args;
		Args.bIsAsync = (bIsAsyncUpdate && !bIsSequenceBlocking);
		Args.bHasJumped = bHasJumpedOverride;

		PostEvaluationCallbacks.Add(FOnEvaluationCallback::CreateUObject(this, &UMovieSceneSequencePlayer::UpdateNetworkSyncProperties));

		UpdateMovieSceneInstance(Range, StatusOverride, Args);

		// Now that the evaluation has taken place we call Pause (to trigger delegates, etc.), however if we're running the evaluation
		// as async we actually need to queue a latent action to do it so that it happens after the data is actually updated. We can't
		// use the QueueLatentAction inside of Pause() because bIsEvaluating is only set during actual evaluation so it won't be set yet.
		if (PauseRange.IsSet())
		{
			if (Args.bIsAsync)
			{
				QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UMovieSceneSequencePlayer::Pause));
			}
			else
			{
				Pause();
			}
		}
	}

	// WARNING: DO NOT CHANGE PLAYER STATE ANYMORE HERE!
	// The code path above (in the "else" statement) queues an asynchronous evaluation, so any further 
	// state change must be moved in the first first block, with a post-evaluation callback in the second 
	// block... see `UpdateNetworkSyncProperties` as an example.
}

void UMovieSceneSequencePlayer::UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, bool bHasJumped)
{
	FMovieSceneUpdateArgs Args;
	Args.bHasJumped = bHasJumped;
	UpdateMovieSceneInstance(InRange, PlayerStatus, Args);
}

void UMovieSceneSequencePlayer::UpdateMovieSceneInstance(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type PlayerStatus, const FMovieSceneUpdateArgs& Args)
{
	if (Observer && !Observer->CanObserveSequence())
	{
		return;
	}

	UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
	if (!MovieSceneSequence)
	{
		return;
	}

#if !NO_LOGGING
	const FQualifiedFrameTime CurrentTime = GetCurrentTime();
	const FString SequenceName = GetSequenceName(true);
	UE_LOG(LogMovieScene, VeryVerbose, TEXT("Evaluating sequence %s at frame %d, subframe %f (%f fps)."), *SequenceName, CurrentTime.Time.FrameNumber.Value, CurrentTime.Time.GetSubFrame(), CurrentTime.Rate.AsDecimal());
#endif

	// Once we have updated we must no longer skip updates
	bSkipNextUpdate = false;

	// We shouldn't be asked to run an async update if we have a blocking sequence.
	check(!Args.bIsAsync || !EnumHasAnyFlags(MovieSceneSequence->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation));
	// We shouldn't be asked to run an async update if we don't have a tick manager.
	check(!Args.bIsAsync || TickManager != nullptr);

	FMovieSceneContext Context(InRange, PlayerStatus);
	Context.SetHasJumped(Args.bHasJumped);

	TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner();
	if (Runner)
	{
		Runner->QueueUpdate(Context, RootTemplateInstance.GetRootInstanceHandle());
		if (Runner == SynchronousRunner || !Args.bIsAsync)
		{
			Runner->Flush();
		}
	}
}

void UMovieSceneSequencePlayer::TearDown()
{
	if (TickManager)
	{
		TickManager->UnregisterTickClient(this);
		TickManager = nullptr;
	}
}

bool UMovieSceneSequencePlayer::IsDisablingEventTriggers(FFrameTime& DisabledUntilTime) const
{
	if (DisableEventTriggersUntilTime.IsSet())
	{
		DisabledUntilTime = DisableEventTriggersUntilTime.GetValue();
		return true;
	}
	return false;
}

void UMovieSceneSequencePlayer::PreEvaluation(const FMovieSceneContext& Context)
{
	RunPreEvaluationCallbacks();

	bIsEvaluating = true;
}

void UMovieSceneSequencePlayer::PostEvaluation(const FMovieSceneContext& Context)
{
	FFrameTime CurrentTime  = ConvertFrameTime(Context.GetTime(),         Context.GetFrameRate(), PlayPosition.GetInputRate());
	FFrameTime PreviousTime = ConvertFrameTime(Context.GetPreviousTime(), Context.GetFrameRate(), PlayPosition.GetInputRate());
	OnMovieSceneSequencePlayerUpdate.Broadcast(*this, CurrentTime, PreviousTime);

	RunPostEvaluationCallbacks();

	bIsEvaluating = false;
}

void UMovieSceneSequencePlayer::RunPreEvaluationCallbacks()
{
	for (const FOnEvaluationCallback& Callback : PreEvaluationCallbacks)
	{
		Callback.ExecuteIfBound();
	}
	PreEvaluationCallbacks.Reset();
}

void UMovieSceneSequencePlayer::RunPostEvaluationCallbacks()
{
	for (const FOnEvaluationCallback& Callback : PostEvaluationCallbacks)
	{
		Callback.ExecuteIfBound();
	}
	PostEvaluationCallbacks.Reset();
}

FString UMovieSceneSequencePlayer::GetSequenceName(bool bAddClientInfo) const
{
	if (Sequence)
	{
		FString SequenceName = Sequence->GetName();
		if (bAddClientInfo)
		{
			AActor* Actor = GetTypedOuter<AActor>();
			if (Actor && Actor->GetWorld() && Actor->GetWorld()->GetNetMode() == NM_Client)
			{
				SequenceName += FString::Printf(TEXT(" (client %d)"), GPlayInEditorID - 1);
			}
		}
		return SequenceName;
	}
	else
	{
		return LexToString(NAME_None);
	}
}

void UMovieSceneSequencePlayer::SetPlaybackClient(TScriptInterface<IMovieScenePlaybackClient> InPlaybackClient)
{
	PlaybackClient = InPlaybackClient;
}

void UMovieSceneSequencePlayer::SetTimeController(TSharedPtr<FMovieSceneTimeController> InTimeController)
{
	TimeController = InTimeController;
	if (TimeController.IsValid())
	{
		TimeController->Reset(GetCurrentTime());
	}
}

void UMovieSceneSequencePlayer::SetIgnorePlaybackReplication(bool bState)
{
	bIgnorePlaybackReplication = bState;
}

TArray<UObject*> UMovieSceneSequencePlayer::GetBoundObjects(FMovieSceneObjectBindingID ObjectBinding)
{
	TArray<UObject*> Objects;

	for (TWeakObjectPtr<> WeakObject : ObjectBinding.ResolveBoundObjects(MovieSceneSequenceID::Root, *this))
	{
		if (UObject* Object = WeakObject.Get())
		{
			Objects.Add(Object);
		}
	}
	return Objects;
}

TArray<FMovieSceneObjectBindingID> UMovieSceneSequencePlayer::GetObjectBindings(UObject* InObject)
{
	TArray<FMovieSceneObjectBindingID> Bindings;
	State.FilterObjectBindings(InObject, *this, &Bindings);
	return Bindings;
}

UWorld* UMovieSceneSequencePlayer::GetPlaybackWorld() const
{
	UObject* PlaybackContext = GetPlaybackContext();
	return PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
}

bool UMovieSceneSequencePlayer::HasAuthority() const
{
	AActor* Actor = GetTypedOuter<AActor>();
	return Actor && Actor->HasAuthority() && IsValidChecked(this) && !IsUnreachable();
}

FFrameTime UMovieSceneSequencePlayer::UpdateServerTimeSamples()
{
	// Attempt to estimate the server time based on our samples.
	// We need to reproject the samples to the current wall-clock time, based on when they were taken
	const double CurrentWallClock = FPlatformTime::Seconds();
	const double Lifetime         = CurrentWallClock - float(GSequencerMaxSmoothedNetSyncSampleAge) / 1000.f;
	const float PlaybackMultiplier = bReversePlayback ? -1.f : 1.f;

	// Cull any old samples that were taken more than GSequencerMaxSmoothedNetSyncSampleAge ms ago by
	// Finding the index of the first sample younger than this time
	const int32 FirstValidSample = Algo::LowerBoundBy(ServerTimeSamples, Lifetime, &FServerTimeSample::ReceievedTime);
	if (FirstValidSample >= ServerTimeSamples.Num())
	{
		// Never found a sample that is recent enough, all samples are too old
		ServerTimeSamples.Reset();
	}
	else if (FirstValidSample > 0)
	{
		// Remove from the front of the array up until the first valid sample
		ServerTimeSamples.RemoveAt(0, FirstValidSample);
	}

	// If we have too many samples, uniformly cull intermediate samples by compacting them into the MaxNumSamples range
	// Making sure to always keep the most recent sample
	const int32 MaxNumSamples = GSequencerMaxSmoothedNetSyncSampleCount;
	if (ServerTimeSamples.Num() > MaxNumSamples)
	{
		float Step = FMath::Max(ServerTimeSamples.Num() / float(MaxNumSamples), 1.f);

		for (int Index = 1; Index < MaxNumSamples-1; ++Index)
		{
			const int32 RemappedIndex = ServerTimeSamples.Num() - int(Step*Index) - 1;
			ServerTimeSamples[Index] = ServerTimeSamples[RemappedIndex];
		}
		ServerTimeSamples[MaxNumSamples-1] = ServerTimeSamples.Last();
		ServerTimeSamples.RemoveAt(MaxNumSamples, ServerTimeSamples.Num() - MaxNumSamples, true);
	}


	if (ServerTimeSamples.Num() < 10)
	{
		// Fallback to the current time if there are not enough samples
		return PlayPosition.GetCurrentPosition();
	}

	// Compute the Standard Deviation so we can understand the variance in the samples
	double MeanTime = 0;
	for (const FServerTimeSample& Sample : ServerTimeSamples)
	{
		const double ThisSample = Sample.ServerTime + (CurrentWallClock - Sample.ReceievedTime) * PlaybackMultiplier;
		MeanTime += ThisSample;
	}
	MeanTime = MeanTime / ServerTimeSamples.Num();

	
	double StandardDeviation = 0;
	for (const FServerTimeSample& Sample : ServerTimeSamples)
	{
		const double ThisSample = Sample.ServerTime + (CurrentWallClock - Sample.ReceievedTime) * PlaybackMultiplier;
		StandardDeviation += FMath::Square(ThisSample - MeanTime);
	}
	StandardDeviation = StandardDeviation / ServerTimeSamples.Num();
	StandardDeviation = FMath::Sqrt(StandardDeviation);

	const int32 OriginalNum = ServerTimeSamples.Num();

	// Possibly need to recompute the mean if we discard any samples
	double NewMeanTime = MeanTime;

	// If the deviation is greater than our threshold, we start culling samples that lie outside it
	const double DeviationThreshold = ((GSequencerSmoothedNetSyncDeviationThreshold * 0.001f) * PlayPosition.GetInputRate()).AsDecimal();
	if (StandardDeviation > DeviationThreshold)
	{
		// Discard anything outside the standard deviation in the hopes that future samples will converge
		for (int32 SampleIndex = ServerTimeSamples.Num()-1; SampleIndex >= 0; --SampleIndex)
		{
			const double ThisSample = ServerTimeSamples[SampleIndex].ServerTime + (CurrentWallClock - ServerTimeSamples[SampleIndex].ReceievedTime) * PlaybackMultiplier;
			if (FMath::Abs(ThisSample - MeanTime) > StandardDeviation)
			{
				ServerTimeSamples.RemoveAt(SampleIndex, 1, false);
			}
			else
			{
				NewMeanTime += ThisSample;
			}
		}
		NewMeanTime = NewMeanTime / ServerTimeSamples.Num();
	}

	// If we didn't cull too many samples, we have confidence in the data set
	if (ServerTimeSamples.Num() >= OriginalNum/2)
	{
		return NewMeanTime * PlayPosition.GetInputRate();
	}
	else
	{
		// Not enough confidence in the data
		return PlayPosition.GetCurrentPosition();
	}
}

void UMovieSceneSequencePlayer::RPC_ExplicitServerUpdateEvent_Implementation(EUpdatePositionMethod EventMethod, FFrameTime MarkerTime)
{
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Handle an explicit jump/play/scrub command from the server.

	if (HasAuthority() || !Sequence || bIgnorePlaybackReplication)
	{
		// Never run network sync operations on authoritative players
		return;
	}

	// Explicit RPC call - empty our smoothed server samples
	ServerTimeSamples.Reset();

#if !NO_LOGGING
	// Log the sync event if necessary
	if (UE_LOG_ACTIVE(LogMovieScene, Verbose))
	{
		const FFrameTime CurrentTime = PlayPosition.GetCurrentPosition();
		const FString SequenceName = GetSequenceName(true);
		UE_LOG(LogMovieScene, Verbose, TEXT("Explicit update event for sequence %s %s @ frame %d, subframe %f. Server has moved to frame %d, subframe %f with EUpdatePositionMethod::%s."),
			*SequenceName, *UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), Status.GetValue()), CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(),
			NetSyncProps.LastKnownPosition.FrameNumber.Value, NetSyncProps.LastKnownPosition.GetSubFrame(), *UEnum::GetValueAsString(TEXT("MovieScene.EUpdatePositionMethod"), NetSyncProps.LastKnownStatus.GetValue()));
	}
#endif

	// Explicitly repeat the authoritative update event on this client.

	// Note: in the case of PlayToFrame this will not necessarily sweep the exact same range as the server did
	// because this client player is unlikely to be at exactly the same time that the server was at when it performed the operation.
	// This is irrelevant for jumps and scrubs as only the new time is meaningful.
	SetPlaybackPosition(FMovieSceneSequencePlaybackParams(MarkerTime, EventMethod));
}

void UMovieSceneSequencePlayer::RPC_OnStopEvent_Implementation(FFrameTime StoppedTime)
{
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Handle an explicit Stop command from the server.

	if (HasAuthority() || !Sequence || bIgnorePlaybackReplication)
	{
		// Never run network sync operations on authoritative players or players that have not been initialized yet
		return;
	}

	// Explicit RPC call - empty our smoothed server samples
	ServerTimeSamples.Reset();

#if !NO_LOGGING
	if (UE_LOG_ACTIVE(LogMovieSceneRepl, Verbose))
	{
		const FFrameTime CurrentTime = PlayPosition.GetCurrentPosition();
		const FString SequenceName = GetSequenceName(true);
		UE_LOG(LogMovieSceneRepl, Verbose, TEXT("Explicit Stop() event for sequence %s %s @ frame %d, subframe %f. Server has stopped at frame %d, subframe %f."),
			*SequenceName, *UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), Status.GetValue()), CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(),
			NetSyncProps.LastKnownPosition.FrameNumber.Value, NetSyncProps.LastKnownPosition.GetSubFrame());
	}
#endif

	EUpdatePositionMethod UpdatePositionMethod;
	switch (Status.GetValue())
	{
		case EMovieScenePlayerStatus::Playing:
			UpdatePositionMethod = EUpdatePositionMethod::Play;
			break;
		case EMovieScenePlayerStatus::Scrubbing:
			UpdatePositionMethod = EUpdatePositionMethod::Scrub;
			break;
		default:
			UpdatePositionMethod = EUpdatePositionMethod::Jump;
			break;
	}

	// Catch up with any loops we are missing compared to the server. This is generally just 0 or 1 loops.
	// When it's 1 loop, it's generally because we are very close to the end, and the server somehow stopped
	// near the beginning of the next loop, so we have just a little bit of catching up to do.
	const int32 LoopOffset = (NetSyncProps.LastKnownNumLoops - CurrentNumLoops);
	const FFrameTime LoopEndTime = (bReversePlayback ? StartTime : GetLastValidTime());
	for (int32 LoopIndex = 0; LoopIndex < LoopOffset; ++LoopIndex)
	{
		SetPlaybackPosition(FMovieSceneSequencePlaybackParams(LoopEndTime, UpdatePositionMethod));
	}

	// Now do the last bit of catch-up for the current loop.
	if (PlayPosition.GetCurrentPosition() < StoppedTime)
	{
		SetPlaybackPosition(FMovieSceneSequencePlaybackParams(StoppedTime, UpdatePositionMethod));
	}

	StopInternal(StoppedTime);
}

void UMovieSceneSequencePlayer::RPC_OnFinishPlaybackEvent_Implementation(FFrameTime StoppedTime)
{
	if (HasAuthority() || !Sequence || bIgnorePlaybackReplication)
	{
		// Never run network sync operations on authoritative players or players that have not been initialized yet
		return;
	}

	FinishPlaybackInternal(StoppedTime);
}

void UMovieSceneSequencePlayer::PostNetReceive()
{
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Handle a passive update of the replicated status and time properties of the player.

	Super::PostNetReceive();

	if (!ensure(!HasAuthority()) || !Sequence || bIgnorePlaybackReplication)
	{
		// Never run network sync operations on authoritative players or players that have not been initialized yet
		return;
	}

	float PingMs = 0.f;

	UWorld* PlayWorld = GetPlaybackWorld();
	if (PlayWorld)
	{
		UNetDriver* NetDriver = PlayWorld->GetNetDriver();
		if (NetDriver && NetDriver->ServerConnection && NetDriver->ServerConnection->PlayerController && NetDriver->ServerConnection->PlayerController->PlayerState)
		{
			PingMs = NetDriver->ServerConnection->PlayerController->PlayerState->ExactPing * (bReversePlayback ? -1.f : 1.f);
		}
	}

	const bool bHasStartedPlaying = NetSyncProps.LastKnownStatus == EMovieScenePlayerStatus::Playing && Status != EMovieScenePlayerStatus::Playing;
	const bool bHasChangedStatus  = NetSyncProps.LastKnownStatus   != Status;
	const bool bHasChangedTime    = NetSyncProps.LastKnownPosition != PlayPosition.GetCurrentPosition();

	const FFrameTime PingLag      = (PingMs/1000.f) * PlayPosition.GetInputRate();
	//const FFrameTime LagThreshold = 0.2f * PlayPosition.GetInputRate();
	//const FFrameTime LagDisparity = FMath::Abs(PlayPosition.GetCurrentPosition() - NetSyncProps.LastKnownPosition);
	const FFrameTime LagThreshold = (GSequencerNetSyncThresholdMS * 0.001f) * PlayPosition.GetInputRate();

	if (!bHasChangedStatus && !bHasChangedTime)
	{
		// Nothing to do
		return;
	}

#if !NO_LOGGING
	if (UE_LOG_ACTIVE(LogMovieSceneRepl, VeryVerbose))
	{
		const FFrameTime CurrentTime = PlayPosition.GetCurrentPosition();
		const FString SequenceName = GetSequenceName(true);
		UE_LOG(LogMovieSceneRepl, VeryVerbose, TEXT("Network sync for sequence %s %s @ frame %d, subframe %f. Server is %s @ frame %d, subframe %f."),
			*SequenceName, *UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), Status.GetValue()), CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(),
			*UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), NetSyncProps.LastKnownStatus.GetValue()), NetSyncProps.LastKnownPosition.FrameNumber.Value, NetSyncProps.LastKnownPosition.GetSubFrame());
	}
#endif

	// Deal with changes of state from stopped <-> playing separately, as they require slightly different considerations
	if (bHasStartedPlaying)
	{
		// Note: when starting playback, we assume that the client and server were at the same time prior to the server initiating playback
		ServerTimeSamples.Reset();

		// Initiate playback from our current position
		PlayInternal();

		const FFrameTime LagDisparity = FMath::Abs(PlayPosition.GetCurrentPosition() - (NetSyncProps.LastKnownPosition + PingLag));
		if (LagDisparity > LagThreshold)
		{
			// Synchronize to the server time as best we can if there is a large disparity
			SetPlaybackPosition(FMovieSceneSequencePlaybackParams(NetSyncProps.LastKnownPosition + PingLag, EUpdatePositionMethod::Play));
		}
	}
	else
	{
		if (bHasChangedTime)
		{
			// Treat all net updates as the main level update - this ensures they get evaluated as part of the 
			// main tick manager
			bIsAsyncUpdate = true;

			// Make sure the client time matches the server according to the client's current status
			if (Status == EMovieScenePlayerStatus::Playing)
			{
				// When the server has looped back to the start but a client is near the end (and is thus about to loop), we don't want to forcibly synchronize the time unless
				// the *real* difference in time is above the threshold. We compute the real-time difference by adding SequenceDuration*LoopCountDifference to the server position:
				//		start	srv_time																																clt_time		end
				//		0		1		2		3		4		5		6		7		8		9		10		11		12		13		14		15		16		17		18		19		20
				//		|		|																																		|				|
				//
				//		Let NetSyncProps.LastKnownNumLoops = 1, CurrentNumLoops = 0, bReversePlayback = false
				//			=> LoopOffset = 1
				//			   OffsetServerTime = srv_time + FrameDuration*LoopOffset = 1 + 20*1 = 21
				//			   Difference = 21 - 18 = 3 frames
				const int32        LoopOffset       = (NetSyncProps.LastKnownNumLoops - CurrentNumLoops) * (bReversePlayback ? -1 : 1);
				const FFrameTime   OffsetServerTime = (NetSyncProps.LastKnownPosition + PingLag) + GetFrameDuration()*LoopOffset;

				if (LoopOffset != 0)
				{
					// If we crossed a loop boundary, reset the samples
					ServerTimeSamples.Reset();
				}

				const bool bUseSmoothing = GSequencerMaxSmoothedNetSyncSampleAge != 0;
				if (bUseSmoothing)
				{
					ServerTimeSamples.Add(FServerTimeSample{ OffsetServerTime / PlayPosition.GetInputRate(), FPlatformTime::Seconds() });
				}

				const FFrameTime SmoothedServerTime = bUseSmoothing ? UpdateServerTimeSamples() : OffsetServerTime;
				const FFrameTime Difference         = FMath::Abs(PlayPosition.GetCurrentPosition() - SmoothedServerTime);

				SET_DWORD_STAT(MovieSceneRepl_NumServerSamples, ServerTimeSamples.Num());
				SET_FLOAT_STAT(MovieSceneRepl_SmoothedServerTime, SmoothedServerTime.AsDecimal());

				if (bHasChangedStatus)
				{
					// If the status has changed forcibly play to the server position before setting the new status
					SetPlaybackPosition(FMovieSceneSequencePlaybackParams(NetSyncProps.LastKnownPosition + PingLag, EUpdatePositionMethod::Play));
				}
				else if (Difference > LagThreshold + PingLag)
				{
#if !NO_LOGGING
					if (UE_LOG_ACTIVE(LogMovieSceneRepl, Log))
					{
						const FFrameTime CurrentTime = PlayPosition.GetCurrentPosition();
						const FString SequenceName = GetSequenceName(true);
						UE_LOG(LogMovieSceneRepl, Log, TEXT("Correcting de-synced play position for sequence %s %s @ frame %d, subframe %f. Server is %s @ frame %d, subframe %f, (smoothed: frame %d, sf %f). Client ping is %.2fms."),
							*SequenceName, *UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), Status.GetValue()), CurrentTime.FrameNumber.Value, CurrentTime.GetSubFrame(),
							*UEnum::GetValueAsString(TEXT("MovieScene.EMovieScenePlayerStatus"), NetSyncProps.LastKnownStatus.GetValue()),
							NetSyncProps.LastKnownPosition.FrameNumber.Value, NetSyncProps.LastKnownPosition.GetSubFrame(),
							SmoothedServerTime.FrameNumber.Value, SmoothedServerTime.GetSubFrame(),
							PingMs);
					}
#endif
					// We're drastically out of sync with the server so we need to forcibly set the time.
					const FFrameTime LastPosition = FFrameRate::TransformTime(
							PlayPosition.GetCurrentPosition(), PlayPosition.GetInputRate(), PlayPosition.GetOutputRate());

					// Play to the time only if it is further on in the sequence (in our play direction).
					// Otherwise, jump backwards in time (in our play direction).
					const bool bPlayToFrame = bReversePlayback ? SmoothedServerTime < PlayPosition.GetCurrentPosition() : SmoothedServerTime > PlayPosition.GetCurrentPosition();
					if (bPlayToFrame)
					{
						FMovieSceneSequencePlaybackParams Params(SmoothedServerTime, EUpdatePositionMethod::Play);
						// Indicate that the sequence may have jumped a considerable distance.
						// This especially helps the audio track to stay in-sync after a correction
						Params.bHasJumped = true;
						SetPlaybackPosition(Params);
					}
					else
					{
						SetPlaybackPosition(FMovieSceneSequencePlaybackParams(SmoothedServerTime, EUpdatePositionMethod::Jump));
					}

					// When playing back we skip this sequence's ticked update to avoid queuing 2 updates this frame
					bSkipNextUpdate = true;

					// Also skip all events up to the last known position, otherwise if we skipped back in time we
					// will re-trigger events again.
					DisableEventTriggersUntilTime = LastPosition;
				}
			}
			else if (Status == EMovieScenePlayerStatus::Stopped)
			{
				SetPlaybackPosition(FMovieSceneSequencePlaybackParams(NetSyncProps.LastKnownPosition, EUpdatePositionMethod::Jump));
			}
			else if (Status == EMovieScenePlayerStatus::Scrubbing)
			{
				SetPlaybackPosition(FMovieSceneSequencePlaybackParams(NetSyncProps.LastKnownPosition, EUpdatePositionMethod::Scrub));
			}

			bIsAsyncUpdate = false;
		}

		if (bHasChangedStatus)
		{
			ServerTimeSamples.Reset();

			switch (NetSyncProps.LastKnownStatus)
			{
			case EMovieScenePlayerStatus::Paused:    Pause(); break;
			case EMovieScenePlayerStatus::Playing:   Play();  break;
			case EMovieScenePlayerStatus::Scrubbing: Scrub(); break;
			}
		}
	}
}

void UMovieSceneSequencePlayer::BeginDestroy()
{
	RootTemplateInstance.BeginDestroy();

	TearDown();

	Super::BeginDestroy();
}

int32 UMovieSceneSequencePlayer::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Try to use the same logic as function libraries for static functions, will try to use the global context to check authority only/cosmetic
		return GEngine->GetGlobalFunctionCallspace(Function, this, Stack);
	}

	check(GetOuter());
	return GetOuter()->GetFunctionCallspace(Function, Stack);
}

bool UMovieSceneSequencePlayer::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	check(!HasAnyFlags(RF_ClassDefaultObject));

	AActor*     Actor     = GetTypedOuter<AActor>();
	UNetDriver* NetDriver = Actor ? Actor->GetNetDriver() : nullptr;
	if (NetDriver)
	{
		NetDriver->ProcessRemoteFunction(Actor, Function, Parameters, OutParms, Stack, this);
		return true;
	}

	return false;
}

bool UMovieSceneSequencePlayer::NeedsQueueLatentAction() const
{
	return bIsEvaluating;
}

void UMovieSceneSequencePlayer::QueueLatentAction(FMovieSceneSequenceLatentActionDelegate Delegate)
{
	if (ensure(TickManager) && !EnumHasAnyFlags(Sequence->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation))
	{
		// Queue latent actions on the global tick manager.
		TickManager->AddLatentAction(Delegate);
	}
	else
	{
		// Queue latent actions locally.
		LatentActionManager.AddLatentAction(Delegate);
	}
}

void UMovieSceneSequencePlayer::RunLatentActions()
{
	if (!Sequence)
	{
		return;
	}

	if (ensure(TickManager) && !EnumHasAnyFlags(Sequence->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation))
	{
		TickManager->RunLatentActions();
	}
	else
	{
		check(SynchronousRunner);
		LatentActionManager.RunLatentActions([this]
		{
			this->SynchronousRunner->Flush();
		});
	}
}


