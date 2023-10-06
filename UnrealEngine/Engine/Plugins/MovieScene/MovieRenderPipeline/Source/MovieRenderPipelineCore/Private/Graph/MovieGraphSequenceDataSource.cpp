// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphSequenceDataSource.h"
#include "Graph/MovieGraphPipeline.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MoviePipelineQueue.h"
#include "CoreGlobals.h"
#include "EngineUtils.h"
#include "MovieRenderPipelineCoreModule.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "Engine/GameViewportClient.h"

UMovieGraphSequenceDataSource::UMovieGraphSequenceDataSource()
{
	CustomSequenceTimeController = MakeShared<UE::MovieGraph::FMovieGraphSequenceTimeController>();
}
 
void UMovieGraphSequenceDataSource::CacheDataPreJob(const FMovieGraphInitConfig& InInitConfig)
{
	// Turn off screen messages as some forms are drawn directly to final render targets
	// which will polute final frames.
	GAreScreenMessagesEnabled = false;

	// Turn off player viewport rendering to avoid overhead of an extra render.
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->bDisableWorldRendering = !InInitConfig.bRenderViewport;
	}

	ULevelSequence* RootSequence = Cast<ULevelSequence>(GetOwningGraph()->GetCurrentJob()->Sequence.TryLoad());
	if (RootSequence)
	{
		CacheLevelSequenceData(RootSequence);
	}
}

void UMovieGraphSequenceDataSource::RestoreCachedDataPostJob()
{
	GAreScreenMessagesEnabled = true;
	if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->bDisableWorldRendering = false;
	}
}

void UMovieGraphSequenceDataSource::UpdateShotList()
{
	ULevelSequence* RootSequence = Cast<ULevelSequence>(GetOwningGraph()->GetCurrentJob()->Sequence.TryLoad());
	if (RootSequence)
	{
		bool bShotsChanged = false;
		UMoviePipelineBlueprintLibrary::UpdateJobShotListFromSequence(RootSequence, GetOwningGraph()->GetCurrentJob(), bShotsChanged);
	}
}

void UMovieGraphSequenceDataSource::CacheLevelSequenceData(ULevelSequence* InSequence)
{
	// There is a reasonable chance that there exists a Level Sequence Actor in the world already set up to play this sequence.
	ALevelSequenceActor* ExistingActor = nullptr;

	for (auto It = TActorIterator<ALevelSequenceActor>(GetWorld()); It; ++It)
	{
		// Iterate through all of them in the event someone has multiple copies in the world on accident.
		if (It->GetSequence() == InSequence)
		{
			// Found it!
			ExistingActor = *It;

			// Stop it from playing if it's already playing.
			if (ExistingActor->GetSequencePlayer())
			{
				ExistingActor->GetSequencePlayer()->Stop();
			}
		}
	}

	LevelSequenceActor = ExistingActor;
	if (!LevelSequenceActor)
	{
		// Spawn a new level sequence
		LevelSequenceActor = GetWorld()->SpawnActor<ALevelSequenceActor>();
		check(LevelSequenceActor);
	}

	// Enforce settings.
	LevelSequenceActor->PlaybackSettings.LoopCount.Value = 0;
	LevelSequenceActor->PlaybackSettings.bAutoPlay = false;
	LevelSequenceActor->PlaybackSettings.bPauseAtEnd = true;
	LevelSequenceActor->PlaybackSettings.bRestoreState = true;

	// Ensure the (possibly new) Level Sequence Actor uses our sequence
	LevelSequenceActor->SetSequence(InSequence);

	LevelSequenceActor->GetSequencePlayer()->SetTimeController(CustomSequenceTimeController);
	LevelSequenceActor->GetSequencePlayer()->Stop();

	LevelSequenceActor->GetSequencePlayer()->OnSequenceUpdated().AddUObject(this, &UMovieGraphSequenceDataSource::OnSequenceEvaluated);

}

void UMovieGraphSequenceDataSource::OnSequenceEvaluated(const UMovieSceneSequencePlayer& Player, FFrameTime CurrentTime, FFrameTime PreviousTime)
{
	// This callback exists for logging purposes. DO NOT HINGE LOGIC ON THIS CALLBACK
	// because this may get called multiple times per frame and may be the result of
	// a seek operation which is reverted before a frame is even rendered.
	UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("Sequence Evaluated. CurrentTime: %s PreviousTime: %s"), *LexToString(CurrentTime), *LexToString(PreviousTime));
}

FFrameRate UMovieGraphSequenceDataSource::GetTickResolution() const
{
	if (ensure(LevelSequenceActor && LevelSequenceActor->GetSequence()))
	{
		return LevelSequenceActor->GetSequence()->GetMovieScene()->GetTickResolution();
	}

	return FFrameRate(24000, 1);
}

FFrameRate UMovieGraphSequenceDataSource::GetDisplayRate() const
{
	if (ensure(LevelSequenceActor && LevelSequenceActor->GetSequence()))
	{
		return LevelSequenceActor->GetSequence()->GetMovieScene()->GetDisplayRate();
	}

	return FFrameRate(24, 1);
}

void UMovieGraphSequenceDataSource::InitializeShot(UMoviePipelineExecutorShot* InShot)
{
	//if (!InShot->ShotInfo.bEmulateFirstFrameMotionBlur)
	{
		// Real warm up frames walk through the Sequence
		LevelSequenceActor->GetSequencePlayer()->Play();
	}
	//else
	//{
	//	// Ensure we don't try to evaluate as we want to sit and wait during warm up and motion blur frames.
	//	LevelSequenceActor->GetSequencePlayer()->Pause();
	//}
}

void UMovieGraphSequenceDataSource::SyncDataSourceTime(const FFrameTime& InTime)
{
	FFrameRate TickResolution = LevelSequenceActor->GetSequence()->GetMovieScene()->GetTickResolution();
	CustomSequenceTimeController->SetCachedFrameTiming(FQualifiedFrameTime(InTime, TickResolution));
}

namespace UE::MovieGraph
{
	FFrameTime FMovieGraphSequenceTimeController::OnRequestCurrentTime(const FQualifiedFrameTime& InCurrentTime, float InPlayRate)
	{
		FFrameTime RequestTime = FFrameRate::TransformTime(TimeCache.Time, TimeCache.Rate, InCurrentTime.Rate);
		UE_LOG(LogMovieRenderPipeline, VeryVerbose, TEXT("[%d] OnRequestCurrentTime: %d %f"), GFrameCounter, RequestTime.FloorToFrame().Value, RequestTime.GetSubFrame());

		return RequestTime;
	}
}