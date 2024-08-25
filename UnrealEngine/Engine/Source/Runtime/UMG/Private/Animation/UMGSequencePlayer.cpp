// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/UMGSequencePlayer.h"
#include "MovieScene.h"
#include "UMGPrivate.h"
#include "Animation/WidgetAnimation.h"
#include "MovieSceneTimeHelpers.h"
#include "Evaluation/MovieScenePlayback.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Animation/UMGSequenceTickManager.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UMGSequencePlayer)

extern TAutoConsoleVariable<bool> CVarUserWidgetUseParallelAnimation;

namespace UE::UMG
{

	bool GAsyncAnimationControlFlow = true;
	FAutoConsoleVariableRef CVarAsyncAnimationControlFlow(
		TEXT("UMG.AsyncAnimationControlFlow"),
		GAsyncAnimationControlFlow,
		TEXT("(Default: true) Whether to perform animation control flow functions (Play, Pause, Stop etc) asynchronously.")
	);

	bool GVarAnimationMarkers = false;
	FAutoConsoleVariableRef CVarAnimationMarkers(
		TEXT("UMG.AnimationMarkers"),
		GVarAnimationMarkers,
		TEXT("(Default: false) Whether to emit profiling frame markers for starting and stopping UMG animations.")
	);

} // namespace UE::UMG

UUMGSequencePlayer::UUMGSequencePlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerStatus = EMovieScenePlayerStatus::Stopped;
	TimeCursorPosition = FFrameTime(0);
	PlaybackSpeed = 1;
	bRestoreState = false;
	Animation = nullptr;
	bIsStopping = false;
	bIsBeginningPlay = false;
	bCompleteOnPostEvaluation = false;
	UserTag = NAME_None;
	BlockedDeltaTimeCompensation = 0.f;
}

void UUMGSequencePlayer::InitSequencePlayer(UWidgetAnimation& InAnimation, UUserWidget& InUserWidget)
{
	Animation = &InAnimation;
	UserWidget = &InUserWidget;

	UMovieScene* MovieScene = Animation->GetMovieScene();

	// Cache the time range of the sequence to determine when we stop
	Duration = UE::MovieScene::DiscreteSize(MovieScene->GetPlaybackRange());
	AnimationResolution = MovieScene->GetTickResolution();
	AbsolutePlaybackStart = UE::MovieScene::DiscreteInclusiveLower(MovieScene->GetPlaybackRange());
}

UMovieSceneEntitySystemLinker* UUMGSequencePlayer::ConstructEntitySystemLinker()
{
	UUserWidget* Widget = UserWidget.Get();
	if (ensure(Widget) && !EnumHasAnyFlags(Animation->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation))
	{
		if (!ensure(Widget->AnimationTickManager))
		{
			// @todo: There should be no possible way that the animation tick manager is null here, but there is a very low-rate
			// crash caused by it being null that is very hard to track down, so patching with a band-aid for now.
			Widget->AnimationTickManager = UUMGSequenceTickManager::Get(Widget);
			Widget->AnimationTickManager->AddWidget(Widget);
		}

		return Widget->AnimationTickManager->GetLinker();
	}

	return UMovieSceneEntitySystemLinker::CreateLinker(Widget ? Widget->GetWorld() : nullptr, UE::MovieScene::EEntitySystemLinkerRole::UMG);
}

void UUMGSequencePlayer::Tick(float DeltaTime)
{
	if (IsEvaluating())
	{
		BlockedDeltaTimeCompensation += DeltaTime;
		return;
	}

	if (bIsStopping || bIsBeginningPlay)
	{
		return;
	}

	DeltaTime += BlockedDeltaTimeCompensation;
	BlockedDeltaTimeCompensation = 0.f;

	if ( PlayerStatus == EMovieScenePlayerStatus::Playing )
	{
		FFrameTime DeltaFrameTime = (bIsPlayingForward ? DeltaTime * PlaybackSpeed : -DeltaTime * PlaybackSpeed) * AnimationResolution;

		FFrameTime LastTimePosition = TimeCursorPosition;
		TimeCursorPosition += DeltaFrameTime;

		// Check if we crossed over bounds
		const bool bCrossedLowerBound = TimeCursorPosition < FFrameTime(0);
		const bool bCrossedUpperBound = TimeCursorPosition >= FFrameTime(Duration);
		const bool bCrossedEndTime = bIsPlayingForward
			? LastTimePosition < EndTime && EndTime <= TimeCursorPosition
			: LastTimePosition > EndTime && EndTime >= TimeCursorPosition;

		// Increment the num loops if we crossed any bounds.
		if (bCrossedLowerBound || bCrossedUpperBound || (bCrossedEndTime && NumLoopsCompleted >= NumLoopsToPlay - 1))
		{
			NumLoopsCompleted++;
		}

		// Did the animation complete
		const bool bCompleted = NumLoopsToPlay != 0 && NumLoopsCompleted >= NumLoopsToPlay;

		// Handle times and see if we need to loop or ping-pong. If looping/ping-ponging,
		// we update the sequence for the last little bit before it happens.
		const FFrameTime LastValidFrame(Duration-1, 0.99999994f);
		if (bCrossedLowerBound)
		{
			if (bCompleted)
			{
				TimeCursorPosition = FFrameTime(0);
			}
			else
			{
				UpdateInternal(LastTimePosition, FFrameTime(0), false);

				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = FMath::Abs(TimeCursorPosition);
					LastTimePosition = FFrameTime(0);
				}
				else
				{
					TimeCursorPosition += FFrameTime(Duration);
					LastTimePosition = LastValidFrame;
				}
			}
		}
		else if (bCrossedUpperBound)
		{
			if (bCompleted)
			{
				TimeCursorPosition = LastValidFrame;
			}
			else
			{
				UpdateInternal(LastTimePosition, LastValidFrame, false);

				if (PlayMode == EUMGSequencePlayMode::PingPong)
				{
					bIsPlayingForward = !bIsPlayingForward;
					TimeCursorPosition = LastValidFrame - (TimeCursorPosition - FFrameTime(Duration));
					LastTimePosition = LastValidFrame;
				}
				else
				{
					TimeCursorPosition = TimeCursorPosition - FFrameTime(Duration);
					LastTimePosition = FFrameTime(0);
				}
			}
		}
		else if (bCrossedEndTime)
		{
			if (bCompleted)
			{
				TimeCursorPosition = EndTime;
			}
		}

		bCompleteOnPostEvaluation = bCompleted;

		const bool bHasJumped = (bCrossedLowerBound || bCrossedUpperBound || bCrossedEndTime);
		UpdateInternal(LastTimePosition, TimeCursorPosition, bHasJumped);
	}
}

void UUMGSequencePlayer::UpdateInternal(FFrameTime LastTimePosition, FFrameTime NextTimePosition, bool bHasJumped)
{
	if (RootTemplateInstance.IsValid())
	{
		UMovieScene* MovieScene = Animation->GetMovieScene();

		FMovieSceneContext Context(
				FMovieSceneEvaluationRange(
					AbsolutePlaybackStart + TimeCursorPosition,
					AbsolutePlaybackStart + LastTimePosition,
					AnimationResolution),
				PlayerStatus);
		Context.SetHasJumped(bHasJumped);

		TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner();
		if (Runner)
		{
			UE::MovieScene::ERunnerUpdateFlags UpdateFlags = UE::MovieScene::ERunnerUpdateFlags::None;
			FSimpleDelegate OnComplete;

			if (bCompleteOnPostEvaluation)
			{
				bIsStopping = true;
				bCompleteOnPostEvaluation = false;

				OnComplete = FSimpleDelegate::CreateUObject(this, &UUMGSequencePlayer::HandleLatentStop);
				UpdateFlags = UE::MovieScene::ERunnerUpdateFlags::Flush;
			}

			Runner->QueueUpdate(Context, RootTemplateInstance.GetRootInstanceHandle(), MoveTemp(OnComplete), UpdateFlags);

			if (Runner == SynchronousRunner || !UE::UMG::GAsyncAnimationControlFlow)
			{
				Runner->Flush();
				ApplyLatentActions();
			}
		}
	}
}

void UUMGSequencePlayer::PlayInternal(double StartAtTime, double EndAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed, bool bInRestoreState)
{
	UUserWidget* Widget = UserWidget.Get();
	UUMGSequenceTickManager* TickManager = Widget ? ToRawPtr(Widget->AnimationTickManager) : nullptr;

	if (UE::UMG::GVarAnimationMarkers && Animation && Widget)
	{
		CSV_EVENT_GLOBAL(TEXT("Play Animation [%s::%s]"), *Widget->GetName(), *Animation->GetName());
	}

	TSharedPtr<FMovieSceneEntitySystemRunner> RunnerToUse = TickManager ? TickManager->GetRunner() : nullptr;
	if (EnumHasAnyFlags(Animation->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation))
	{
		SynchronousRunner = MakeShared<FMovieSceneEntitySystemRunner>();
		RunnerToUse = SynchronousRunner;
	}

	RootTemplateInstance.Initialize(*Animation, *this, nullptr, RunnerToUse);

	if (bInRestoreState)
	{
		RootTemplateInstance.EnableGlobalPreAnimatedStateCapture();
	}

	bIsBeginningPlay = true;
	bRestoreState = bInRestoreState;
	PlaybackSpeed = FMath::Abs(InPlaybackSpeed);
	PlayMode = InPlayMode;

	FFrameTime LastValidFrame(Duration-1, 0.99999994f);

	if (PlayMode == EUMGSequencePlayMode::Reverse)
	{
		// When playing in reverse count subtract the start time from the end.
		TimeCursorPosition = LastValidFrame - StartAtTime * AnimationResolution;
	}
	else
	{
		TimeCursorPosition = StartAtTime * AnimationResolution;
	}

	// Clamp the start time and end time to be within the bounds
	TimeCursorPosition = FMath::Clamp(TimeCursorPosition, FFrameTime(0), LastValidFrame);
	EndTime = FMath::Clamp(EndAtTime * AnimationResolution, FFrameTime(0), LastValidFrame);

	if ( PlayMode == EUMGSequencePlayMode::PingPong )
	{
		// When animating in ping-pong mode double the number of loops to play so that a loop is a complete forward/reverse cycle.
		NumLoopsToPlay = 2 * InNumLoopsToPlay;
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}

	NumLoopsCompleted = 0;
	bIsPlayingForward = InPlayMode != EUMGSequencePlayMode::Reverse;

	PlayerStatus = EMovieScenePlayerStatus::Playing;

	// Playback assumes the start frame has already been evaulated, so we also want to evaluate any events on the start frame here.
	if (RunnerToUse)
	{
		const FMovieSceneContext Context(FMovieSceneEvaluationRange(AbsolutePlaybackStart + TimeCursorPosition, AbsolutePlaybackStart + TimeCursorPosition, AnimationResolution), PlayerStatus);

		auto OnBegunPlay = [this]
		{
			this->bIsBeginningPlay = false;
		};

		// We queue an update instead of immediately flushing the entire linker so that we don't incur a cascade of flushes on frames when multiple animations are played
		// In rare cases where the linker must be flushed immediately PreTick, the queue should be manually flushed 
		RunnerToUse->QueueUpdate(Context, RootTemplateInstance.GetRootInstanceHandle(), FSimpleDelegate::CreateWeakLambda(this, OnBegunPlay), UE::MovieScene::ERunnerUpdateFlags::Flush);

		if (RunnerToUse == SynchronousRunner || !UE::UMG::GAsyncAnimationControlFlow)
		{
			RunnerToUse->Flush();
		}
	}
}

void UUMGSequencePlayer::Play(float StartAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed, bool bInRestoreState)
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(
					this, &UUMGSequencePlayer::Play, StartAtTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed, bInRestoreState));
		return;
	}

	PlayInternal(StartAtTime, 0.0, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed, bInRestoreState);
}

void UUMGSequencePlayer::PlayTo(float StartAtTime, float EndAtTime, int32 InNumLoopsToPlay, EUMGSequencePlayMode::Type InPlayMode, float InPlaybackSpeed, bool bInRestoreState)
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(
					this, &UUMGSequencePlayer::PlayTo, StartAtTime, EndAtTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed, bInRestoreState));
		return;
	}

	PlayInternal(StartAtTime, EndAtTime, InNumLoopsToPlay, InPlayMode, InPlaybackSpeed, bInRestoreState);
}

void UUMGSequencePlayer::Pause()
{
	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UUMGSequencePlayer::Pause));
		return;
	}

	// Purposely don't trigger any OnFinished events
	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	// Evaluate the sequence at its current time, with a status of 'stopped' to ensure that animated state pauses correctly. (ie. audio sounds should stop/pause)
	TSharedPtr<FMovieSceneEntitySystemRunner> RunnerToUpdate = RootTemplateInstance.HasEverUpdated() ? RootTemplateInstance.GetRunner() : nullptr;
	if (RunnerToUpdate)
	{
		const FMovieSceneContext Context(FMovieSceneEvaluationRange(AbsolutePlaybackStart + TimeCursorPosition, AbsolutePlaybackStart + TimeCursorPosition, AnimationResolution), PlayerStatus);
		RunnerToUpdate->QueueUpdate(Context, RootTemplateInstance.GetRootInstanceHandle(), UE::MovieScene::ERunnerUpdateFlags::Flush);

		if (RunnerToUpdate == SynchronousRunner || !UE::UMG::GAsyncAnimationControlFlow)
		{
			RunnerToUpdate->Flush();
		}
	}
}

void UUMGSequencePlayer::Reverse()
{
	if (PlayerStatus == EMovieScenePlayerStatus::Playing)
	{
		bIsPlayingForward = !bIsPlayingForward;
	}
}

void UUMGSequencePlayer::Stop()
{
	using namespace UE::MovieScene;

	if (NeedsQueueLatentAction())
	{
		QueueLatentAction(FMovieSceneSequenceLatentActionDelegate::CreateUObject(this, &UUMGSequencePlayer::Stop));
		return;
	}

	if (PlayerStatus == EMovieScenePlayerStatus::Stopped)
	{
		return;
	}

	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	UMovieSceneSequence* MovieSceneSequence = RootTemplateInstance.GetSequence(MovieSceneSequenceID::Root);
	const bool bIsRunningWithTickManager = CVarUserWidgetUseParallelAnimation.GetValueOnGameThread();
	const bool bIsSequenceBlocking = EnumHasAnyFlags(MovieSceneSequence->GetFlags(), EMovieSceneSequenceFlags::BlockingEvaluation);
	const bool bIsAsync = bIsRunningWithTickManager && !bIsSequenceBlocking && UE::UMG::GAsyncAnimationControlFlow == true;

	UUserWidget* Widget = UserWidget.Get();
	UUMGSequenceTickManager* TickManager = Widget ? ToRawPtr(Widget->AnimationTickManager) : nullptr;

	TimeCursorPosition = FFrameTime(0);

	if (!TickManager || !RootTemplateInstance.IsValid())
	{
		HandleLatentStop();
		return;
	}

	if (!RootTemplateInstance.HasEverUpdated())
	{
		TickManager->ClearLatentActions(this);
		LatentActions.Empty();
		HandleLatentStop();
		return;
	}

	const FMovieSceneContext Context(FMovieSceneEvaluationRange(AbsolutePlaybackStart, AnimationResolution), PlayerStatus);

	// Prevent any other updates
	bIsStopping = true;

	if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner())
	{
		// It would be better if we did not have to actively re-evaluate the animation at the start time when Stop is called
		// so we don't have to queue an additional update, but this has to be included for legacy reasons.
		Runner->QueueUpdate(Context, RootTemplateInstance.GetRootInstanceHandle(), FSimpleDelegate::CreateUObject(this, &UUMGSequencePlayer::HandleLatentStop), UE::MovieScene::ERunnerUpdateFlags::Flush);

		if (Runner == SynchronousRunner || !UE::UMG::GAsyncAnimationControlFlow)
		{
			Runner->Flush();
		}
	}
}

void UUMGSequencePlayer::HandleLatentStop()
{
	if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner())
	{
		Runner->QueueFinalUpdate(RootTemplateInstance.GetRootInstanceHandle());

		// Even if our request to Finish the instance was queued, we can wait until the next flush for those effects to be seen
		// This will most likely happen immediately anyway since the runner will keep looping until its queue is empty,
		// And we are already inside an active evaluation
	}

	PlayerStatus = EMovieScenePlayerStatus::Stopped;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RootTemplateInstance.ResetDirectorInstances();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (bRestoreState)
	{
		RestorePreAnimatedState();
	}

	bIsStopping = false;

	UUserWidget* Widget = UserWidget.Get();
	if (Widget)
	{
		if (UE::UMG::GVarAnimationMarkers && Animation)
		{
			CSV_EVENT_GLOBAL(TEXT("Stop Animation [%s::%s]"), *Widget->GetName(), *Animation->GetName());
		}

		Widget->OnAnimationFinishedPlaying(*this);
	}

	OnSequenceFinishedPlayingEvent.Broadcast(*this);
}


void UUMGSequencePlayer::SetNumLoopsToPlay(int32 InNumLoopsToPlay)
{
	if (PlayMode == EUMGSequencePlayMode::PingPong)
	{
		NumLoopsToPlay = (2 * InNumLoopsToPlay);
	}
	else
	{
		NumLoopsToPlay = InNumLoopsToPlay;
	}
}

void UUMGSequencePlayer::SetPlaybackSpeed(float InPlaybackSpeed)
{
	PlaybackSpeed = InPlaybackSpeed;
}

EMovieScenePlayerStatus::Type UUMGSequencePlayer::GetPlaybackStatus() const
{
	return PlayerStatus;
}

UObject* UUMGSequencePlayer::GetPlaybackContext() const
{
	return UserWidget.Get();
}

TArray<UObject*> UUMGSequencePlayer::GetEventContexts() const
{
	TArray<UObject*> EventContexts;
	if (UserWidget.IsValid())
	{
		EventContexts.Add(UserWidget.Get());
	}
	return EventContexts;
}

void UUMGSequencePlayer::SetPlaybackStatus(EMovieScenePlayerStatus::Type InPlaybackStatus)
{
	PlayerStatus = InPlaybackStatus;
}

void UUMGSequencePlayer::PopulateUpdateFlags(UE::MovieScene::ESequenceInstanceUpdateFlags& OutFlags)
{
	// Leave empty so Pre and PostEvaluation are not called
}

bool UUMGSequencePlayer::NeedsQueueLatentAction() const
{
	return IsEvaluating();
}

void UUMGSequencePlayer::QueueLatentAction(FMovieSceneSequenceLatentActionDelegate Delegate)
{
	if (CVarUserWidgetUseParallelAnimation.GetValueOnGameThread())
	{
		UUserWidget* Widget = UserWidget.Get();
		UUMGSequenceTickManager* TickManager = Widget ? ToRawPtr(Widget->AnimationTickManager) : nullptr;

		if (ensure(TickManager))
		{
			TickManager->AddLatentAction(Delegate);
		}
	}
	else
	{
		LatentActions.Add(Delegate);
	}
}

void UUMGSequencePlayer::ApplyLatentActions()
{
	if (CVarUserWidgetUseParallelAnimation.GetValueOnGameThread())
	{
		UUserWidget* Widget = UserWidget.Get();
		UUMGSequenceTickManager* TickManager = Widget ? ToRawPtr(Widget->AnimationTickManager) : nullptr;
		if (TickManager)
		{
			TickManager->RunLatentActions();
		}
	}
	else
	{
		while (LatentActions.Num() > 0)
		{
			const FMovieSceneSequenceLatentActionDelegate& Delegate = LatentActions[0];
			Delegate.ExecuteIfBound();
			LatentActions.RemoveAt(0);
		}
	}
}

void UUMGSequencePlayer::RemoveEvaluationData()
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker           = RootTemplateInstance.GetEntitySystemLinker();
	FSequenceInstance*             SequenceInstance = RootTemplateInstance.FindInstance(MovieSceneSequenceID::Root);

	if (SequenceInstance && Linker)
	{
		TSharedPtr<FMovieSceneEntitySystemRunner> Runner = RootTemplateInstance.GetRunner();
		if (Runner && Runner->IsCurrentlyEvaluating())
		{
			Runner->FlushOutstanding();
		}

		SequenceInstance->Ledger.UnlinkEverything(Linker);
		SequenceInstance->InvalidateCachedData();
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	RootTemplateInstance.ResetDirectorInstances();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void UUMGSequencePlayer::TearDown()
{
	RootTemplateInstance.TearDown();
}

void UUMGSequencePlayer::BeginDestroy()
{
	RootTemplateInstance.TearDown();

	// Remove any latent actions added by this player.
	if (CVarUserWidgetUseParallelAnimation.GetValueOnGameThread())
	{
		UUserWidget* Widget = UserWidget.Get();
		UUMGSequenceTickManager* TickManager = Widget ? ToRawPtr(Widget->AnimationTickManager) : nullptr;
		if (TickManager)
		{
			TickManager->ClearLatentActions(this);
		}
	}
	else
	{
		LatentActions.Empty();
	}

	Super::BeginDestroy();
}


