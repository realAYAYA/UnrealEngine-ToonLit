// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/CinematicShotTrackModel.h"

#include "MovieSceneTrack.h"
#include "Templates/Casts.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"

namespace UE
{
namespace Sequencer
{

TSharedPtr<FTrackModel> FCinematicShotTrackModel::CreateTrackModel(UMovieSceneTrack* Track)
{
	if (UMovieSceneCinematicShotTrack* CinematicShotTrack = Cast<UMovieSceneCinematicShotTrack>(Track))
	{
		return MakeShared<FCinematicShotTrackModel>(CinematicShotTrack);
	}
	return nullptr;
}

FCinematicShotTrackModel::FCinematicShotTrackModel(UMovieSceneCinematicShotTrack* Track)
	: FTrackModel(Track)
{
}

FSortingKey FCinematicShotTrackModel::GetSortingKey() const
{
	FSortingKey SortingKey;
	if (UMovieSceneTrack* Track = GetTrack())
	{
		SortingKey.CustomOrder = Track->GetSortingOrder();
	}
	return SortingKey.PrioritizeBy(5);
}

} // namespace Sequencer
} // namespace UE

