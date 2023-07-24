// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSources.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"
#include "Evaluation/PreAnimatedState/MovieSceneRestoreStateParams.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSources.inl"
#include "EntitySystem/TrackInstance/MovieSceneTrackInstance.h"
#include "Algo/Find.h"

namespace UE
{
namespace MovieScene
{


// Explicit, exported template instantiation for these classes
template struct MOVIESCENE_API TPreAnimatedCaptureSources<FObjectKey>;
template struct MOVIESCENE_API TPreAnimatedCaptureSources<FMovieSceneEvaluationKey>;
template struct MOVIESCENE_API TPreAnimatedCaptureSources<FPreAnimatedEvalHookKeyType>;

FPreAnimatedEvaluationHookCaptureSources::FPreAnimatedEvaluationHookCaptureSources(FPreAnimatedStateExtension* InOwner)
	: TPreAnimatedCaptureSources<FPreAnimatedEvalHookKeyType>(InOwner)
{}

EPreAnimatedCaptureSourceState FPreAnimatedEvaluationHookCaptureSources::BeginTrackingCaptureSource(const UObject* Hook, FMovieSceneSequenceID SequenceID, const FPreAnimatedStateMetaData& MetaData)
{
	return TPreAnimatedCaptureSources<FPreAnimatedEvalHookKeyType>::BeginTrackingCaptureSource(FPreAnimatedEvalHookKeyType{ Hook, SequenceID }, MetaData);
}

void FPreAnimatedEvaluationHookCaptureSources::StopTrackingCaptureSource(const UObject* Hook, FMovieSceneSequenceID SequenceID)
{
	TPreAnimatedCaptureSources<FPreAnimatedEvalHookKeyType>::StopTrackingCaptureSource(FPreAnimatedEvalHookKeyType{ Hook, SequenceID });
}



FPreAnimatedTrackInstanceCaptureSources::FPreAnimatedTrackInstanceCaptureSources(FPreAnimatedStateExtension* InOwner)
	: TPreAnimatedCaptureSources<FObjectKey>(InOwner)
{}

EPreAnimatedCaptureSourceState FPreAnimatedTrackInstanceCaptureSources::BeginTrackingCaptureSource(UMovieSceneTrackInstance* TrackInstance, const FPreAnimatedStateMetaData& MetaData)
{
	FObjectKey Key(TrackInstance);
	return TPreAnimatedCaptureSources<FObjectKey>::BeginTrackingCaptureSource(Key, MetaData);
}

void FPreAnimatedTrackInstanceCaptureSources::StopTrackingCaptureSource(UMovieSceneTrackInstance* TrackInstance)
{
	FObjectKey Key(TrackInstance);
	TPreAnimatedCaptureSources<FObjectKey>::StopTrackingCaptureSource(Key);
}

FPreAnimatedTrackInstanceInputCaptureSources::FPreAnimatedTrackInstanceInputCaptureSources(FPreAnimatedStateExtension* InOwner)
	: TPreAnimatedCaptureSources<FMovieSceneTrackInstanceInput>(InOwner)
{}

FPreAnimatedTemplateCaptureSources::FPreAnimatedTemplateCaptureSources(FPreAnimatedStateExtension* InOwner)
	: TPreAnimatedCaptureSources<FMovieSceneEvaluationKey>(InOwner)
{}


} // namespace MovieScene
} // namespace UE
