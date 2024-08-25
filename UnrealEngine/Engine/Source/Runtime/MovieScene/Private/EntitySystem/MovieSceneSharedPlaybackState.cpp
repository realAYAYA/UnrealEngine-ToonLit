// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneSharedPlaybackState.h"

#include "Compilation/MovieSceneCompiledDataManager.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "MovieSceneSequenceID.h"

namespace UE::MovieScene
{

FSharedPlaybackState::FSharedPlaybackState()
{
}

FSharedPlaybackState::FSharedPlaybackState(
		UMovieSceneSequence& InRootSequence,
		const FSharedPlaybackStateCreateParams& CreateParams)
	: WeakRootSequence(&InRootSequence)
	, WeakPlaybackContext(CreateParams.PlaybackContext)
	, WeakRunner(CreateParams.Runner)
	, CompiledDataManager(CreateParams.CompiledDataManager)
	, RootInstanceHandle(CreateParams.RootInstanceHandle)
{
	if (CompiledDataManager)
	{
		RootCompiledDataID = CompiledDataManager->GetDataID(&InRootSequence);
	}
}

UMovieSceneEntitySystemLinker* FSharedPlaybackState::GetLinker() const
{
	if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = WeakRunner.Pin())
	{
		return Runner->GetLinker();
	}
	return nullptr;
}

const FMovieSceneSequenceHierarchy* FSharedPlaybackState::GetHierarchy() const
{
	if (CompiledDataManager && RootCompiledDataID.IsValid())
	{
		return CompiledDataManager->FindHierarchy(RootCompiledDataID);
	}
	return nullptr;
}

UMovieSceneSequence* FSharedPlaybackState::GetSequence(FMovieSceneSequenceIDRef InSequenceID) const
{
	if (InSequenceID == MovieSceneSequenceID::Root)
	{
		return WeakRootSequence.Get();
	}
	else
	{
		const FMovieSceneSequenceHierarchy* Hierarchy = GetHierarchy();
		const FMovieSceneSubSequenceData*   SubData   = Hierarchy ? Hierarchy->FindSubData(InSequenceID) : nullptr;
		return SubData ? SubData->GetSequence() : nullptr;
	}
}

void FSharedPlaybackState::InvalidateCachedData()
{
	UMovieSceneEntitySystemLinker* Linker = GetLinker();
	Capabilities.InvalidateCachedData(Linker);
}

} // namespace UE::MovieScene

