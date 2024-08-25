// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/SequenceDirectorPlaybackCapability.h"

#include "EntitySystem/MovieSceneSharedPlaybackState.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"

namespace UE::MovieScene
{

TPlaybackCapabilityID<FSequenceDirectorPlaybackCapability> FSequenceDirectorPlaybackCapability::ID = TPlaybackCapabilityID<FSequenceDirectorPlaybackCapability>::Register();

FString FSequenceDirectorPlaybackCapability::FDirectorInstanceCache::GetReferencerName() const
{
	return TEXT("FSequenceDirectorPlaybackCapability");
}

void FSequenceDirectorPlaybackCapability::FDirectorInstanceCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : DirectorInstances)
	{
		Collector.AddReferencedObject(Pair.Value);
	}
}

void FSequenceDirectorPlaybackCapability::ResetDirectorInstances()
{
	if (Cache)
	{
		Cache->DirectorInstances.Reset();
	}
}

UObject* FSequenceDirectorPlaybackCapability::GetOrCreateDirectorInstance(TSharedRef<const FSharedPlaybackState> SharedPlaybackState, FMovieSceneSequenceIDRef SequenceID)
{
	if (!Cache)
	{
		Cache = MakeUnique<FDirectorInstanceCache>();
	}

	UObject* ExistingDirectorInstance = Cache->DirectorInstances.FindRef(SequenceID);
#if WITH_EDITOR
	if (ExistingDirectorInstance)
	{
		// Invalidate our cached director instance if it has been recompiled.
		UClass* DirectorClass = ExistingDirectorInstance->GetClass();
		if (!DirectorClass || DirectorClass->HasAnyClassFlags(CLASS_NewerVersionExists))
		{
			ExistingDirectorInstance = nullptr;
		}
	}
#endif
	if (ExistingDirectorInstance)
	{
		return ExistingDirectorInstance;
	}

	UObject* NewDirectorInstance = nullptr;
	if (SequenceID == MovieSceneSequenceID::Root)
	{
		if (UMovieSceneSequence* Sequence = SharedPlaybackState->GetRootSequence())
		{
			NewDirectorInstance = Sequence->CreateDirectorInstance(SharedPlaybackState, SequenceID);
		}
	}
	else if (const FMovieSceneSequenceHierarchy* Hierarchy = SharedPlaybackState->GetHierarchy())
	{
		const FMovieSceneSubSequenceData* SubData = Hierarchy->FindSubData(SequenceID);
		check(SubData);
		NewDirectorInstance = SubData->GetSequence()->CreateDirectorInstance(SharedPlaybackState, SequenceID);
	}

	if (NewDirectorInstance)
	{
		Cache->DirectorInstances.Add(SequenceID, NewDirectorInstance);
	}

	return NewDirectorInstance;
}

}  // namespace UE::MovieScene

