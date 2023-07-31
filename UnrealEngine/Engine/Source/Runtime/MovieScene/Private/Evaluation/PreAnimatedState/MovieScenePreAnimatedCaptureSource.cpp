// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSource.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSources.inl"
#include "Evaluation/MovieScenePreAnimatedState.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "MovieSceneSection.h"
#include "MovieSceneSequence.h"

FScopedPreAnimatedCaptureSource::FScopedPreAnimatedCaptureSource(FMovieScenePreAnimatedState* InPreAnimatedState, const FMovieSceneEvaluationKey& InEvalKey, bool bInWantsRestoreState)
	: Variant(TInPlaceType<FMovieSceneEvaluationKey>(), InEvalKey)
	, WeakLinker(InPreAnimatedState ? InPreAnimatedState->GetLinker() : nullptr)
	, OptionalSequencePreAnimatedState(InPreAnimatedState)
	, bWantsRestoreState(bInWantsRestoreState)
{
	FScopedPreAnimatedCaptureSource*& CaptureSourcePtr = GetCaptureSourcePtr();
	PrevCaptureSource = CaptureSourcePtr;
	CaptureSourcePtr = this;
}
FScopedPreAnimatedCaptureSource::FScopedPreAnimatedCaptureSource(FMovieScenePreAnimatedState* InPreAnimatedState, const UObject* InEvalHook, FMovieSceneSequenceID InSequenceID, bool bInWantsRestoreState)
	: Variant(TInPlaceType<FEvalHookType>(), FEvalHookType{InEvalHook, InSequenceID} )
	, WeakLinker(InPreAnimatedState ? InPreAnimatedState->GetLinker() : nullptr)
	, OptionalSequencePreAnimatedState(InPreAnimatedState)
	, bWantsRestoreState(bInWantsRestoreState)
{
	FScopedPreAnimatedCaptureSource*& CaptureSourcePtr = GetCaptureSourcePtr();
	PrevCaptureSource = CaptureSourcePtr;
	CaptureSourcePtr = this;
}
FScopedPreAnimatedCaptureSource::FScopedPreAnimatedCaptureSource(UMovieSceneEntitySystemLinker* InLinker, UMovieSceneTrackInstance* InTrackInstance, bool bInWantsRestoreState)
	: Variant(TInPlaceType<UMovieSceneTrackInstance*>(), InTrackInstance)
	, WeakLinker(InLinker)
	, OptionalSequencePreAnimatedState(nullptr)
	, bWantsRestoreState(bInWantsRestoreState)
{
	FScopedPreAnimatedCaptureSource*& CaptureSourcePtr = GetCaptureSourcePtr();
	PrevCaptureSource = CaptureSourcePtr;
	CaptureSourcePtr = this;
}
FScopedPreAnimatedCaptureSource::FScopedPreAnimatedCaptureSource(UMovieSceneEntitySystemLinker* InLinker, const FMovieSceneTrackInstanceInput& TrackInstanceInput)
	: Variant(TInPlaceType<FMovieSceneTrackInstanceInput>(), TrackInstanceInput)
	, WeakLinker(InLinker)
	, OptionalSequencePreAnimatedState(nullptr)
{
	EMovieSceneCompletionMode CompletionMode = TrackInstanceInput.Section->GetCompletionMode();
	if (CompletionMode == EMovieSceneCompletionMode::ProjectDefault)
	{
		CompletionMode = TrackInstanceInput.Section->GetTypedOuter<UMovieSceneSequence>()->DefaultCompletionMode;
	}
	bWantsRestoreState = (CompletionMode == EMovieSceneCompletionMode::RestoreState);

	FScopedPreAnimatedCaptureSource*& CaptureSourcePtr = GetCaptureSourcePtr();
	PrevCaptureSource = CaptureSourcePtr;
	CaptureSourcePtr = this;
}

FScopedPreAnimatedCaptureSource::~FScopedPreAnimatedCaptureSource()
{
	GetCaptureSourcePtr() = PrevCaptureSource;
}

FScopedPreAnimatedCaptureSource*& FScopedPreAnimatedCaptureSource::GetCaptureSourcePtr()
{
	// Implemented as a static thread-local for now since there are some tests that run without a linker,
	// so we can't put this on UMovieSceneEntitySystemLinker::PreAnimatedState where it should probably belong
	static thread_local FScopedPreAnimatedCaptureSource* GCaptureSource = nullptr;
	return GCaptureSource;
}

UE::MovieScene::FRootInstanceHandle FScopedPreAnimatedCaptureSource::GetRootInstanceHandle(UMovieSceneEntitySystemLinker* Linker) const
{
	if (OptionalSequencePreAnimatedState)
	{
		return OptionalSequencePreAnimatedState->InstanceHandle;
	}

	if (const FMovieSceneTrackInstanceInput* TrackInstanceInput = Variant.TryGet<FMovieSceneTrackInstanceInput>())
	{
		return Linker->GetInstanceRegistry()->GetInstance(TrackInstanceInput->InstanceHandle).GetRootInstanceHandle();
	}
	return UE::MovieScene::FRootInstanceHandle();
}

void FScopedPreAnimatedCaptureSource::BeginTracking(const UE::MovieScene::FPreAnimatedStateMetaData& MetaData, UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	ensureMsgf(!WeakLinker.IsValid() || WeakLinker.Get() == Linker, 
			TEXT("Attempting to track state on a capture source related to a different linker. Are you missing setting a scoped capture source?"));

	if (FMovieSceneEvaluationKey* EvalKey = Variant.TryGet<FMovieSceneEvaluationKey>())
	{
		check(OptionalSequencePreAnimatedState);
		// Make the association to this track template key
		if (!OptionalSequencePreAnimatedState->TemplateMetaData)
		{
			OptionalSequencePreAnimatedState->TemplateMetaData = MakeShared<FPreAnimatedTemplateCaptureSources>(&Linker->PreAnimatedState);
			Linker->PreAnimatedState.AddWeakCaptureSource(OptionalSequencePreAnimatedState->TemplateMetaData);
		}
		OptionalSequencePreAnimatedState->TemplateMetaData->BeginTrackingCaptureSource(*EvalKey, MetaData);
	}
	else if (FScopedPreAnimatedCaptureSource::FEvalHookType* EvalHook = Variant.TryGet<FScopedPreAnimatedCaptureSource::FEvalHookType>())
	{
		check(OptionalSequencePreAnimatedState);
		if (!OptionalSequencePreAnimatedState->EvaluationHookMetaData)
		{
			OptionalSequencePreAnimatedState->EvaluationHookMetaData = MakeShared<FPreAnimatedEvaluationHookCaptureSources>(&Linker->PreAnimatedState);
			Linker->PreAnimatedState.AddWeakCaptureSource(OptionalSequencePreAnimatedState->EvaluationHookMetaData);
		}
		OptionalSequencePreAnimatedState->EvaluationHookMetaData->BeginTrackingCaptureSource(EvalHook->EvalHook, EvalHook->SequenceID, MetaData);
	}
	else if (UMovieSceneTrackInstance* const * TrackInstance = Variant.TryGet<UMovieSceneTrackInstance*>())
	{
		// Track instance meta-data is shared between all players
		FPreAnimatedTrackInstanceCaptureSources* TrackInstanceMetaData = Linker->PreAnimatedState.GetOrCreateTrackInstanceMetaData();
		TrackInstanceMetaData->BeginTrackingCaptureSource(*TrackInstance, MetaData);
	}
	else if (const FMovieSceneTrackInstanceInput* TrackInstanceInput = Variant.TryGet<FMovieSceneTrackInstanceInput>())
	{
		// Track instance meta-data is shared between all players
		FPreAnimatedTrackInstanceInputCaptureSources* TrackInstanceInputMetaData = Linker->PreAnimatedState.GetOrCreateTrackInstanceInputMetaData();
		TrackInstanceInputMetaData->BeginTrackingCaptureSource(*TrackInstanceInput, MetaData);
	}
}
