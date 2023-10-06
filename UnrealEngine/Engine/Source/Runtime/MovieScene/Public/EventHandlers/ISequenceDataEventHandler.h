// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/List.h"
#include "CoreTypes.h"
#include "EventHandlers/MovieSceneDataEventContainer.h"

class UMovieScene;
class UMovieSceneFolder;
class UMovieSceneTrack;
struct FGuid;
struct FMovieSceneBinding;

namespace UE
{
namespace MovieScene
{

class ISequenceDataEventHandler
{
public:

	virtual void OnTrackAdded(UMovieSceneTrack*)           {}

	virtual void OnTrackRemoved(UMovieSceneTrack*)         {}

	virtual void OnBindingAdded(const FMovieSceneBinding&) {}

	virtual void OnBindingRemoved(const FGuid&)            {}

	virtual void OnRootFolderAdded(UMovieSceneFolder*)     {}

	virtual void OnRootFolderRemoved(UMovieSceneFolder*)   {}

	virtual void OnTrackAddedToBinding(UMovieSceneTrack* Track, const FGuid& ObjectBindingID) {}

	virtual void OnTrackRemovedFromBinding(UMovieSceneTrack* Track, const FGuid& ObjectBindingID) {}

	virtual void OnBindingParentChanged(const FGuid& ObjectBindingID, const FGuid& NewParent) {}

};

} // namespace MovieScene
} // namespace UE

