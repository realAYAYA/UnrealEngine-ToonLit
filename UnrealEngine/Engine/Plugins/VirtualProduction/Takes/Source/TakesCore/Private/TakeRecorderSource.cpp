// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderSource.h"
#include "Styling/SlateIconFinder.h"
#include "MovieSceneSequenceID.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TakeRecorderSource)

UTakeRecorderSource::UTakeRecorderSource(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
	,bEnabled(true)
	,TakeNumber(0)
{
	TrackTint = FColor(127, 127, 127);
}

const FSlateBrush* UTakeRecorderSource::GetDisplayIconImpl() const
{
	return FSlateIconFinder::FindCustomIconBrushForClass(GetClass(), TEXT("ClassThumbnail"));
}

TArray<UTakeRecorderSource*> UTakeRecorderSource::PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer)
{
	return TArray<UTakeRecorderSource*>();
}
