// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/Extensions/ISortableExtension.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "Templates/SharedPointer.h"

class UMovieSceneCinematicShotTrack;
class UMovieSceneTrack;
namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }

namespace UE
{
namespace Sequencer
{

class MOVIESCENETOOLS_API FCinematicShotTrackModel
	: public FTrackModel
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FCinematicShotTrackModel, FTrackModel);

	static TSharedPtr<FTrackModel> CreateTrackModel(UMovieSceneTrack* Track);

	explicit FCinematicShotTrackModel(UMovieSceneCinematicShotTrack* Track);

	FSortingKey GetSortingKey() const override;
};

} // namespace Sequencer
} // namespace UE

