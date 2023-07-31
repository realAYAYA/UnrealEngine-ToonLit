// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/Extensions/ISortableExtension.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "Templates/SharedPointer.h"

class UMovieSceneCameraCutTrack;
class UMovieSceneTrack;
namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }

namespace UE
{
namespace Sequencer
{

class MOVIESCENETOOLS_API FCameraCutTrackModel
	: public FTrackModel
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FCameraCutTrackModel, FTrackModel);

	static TSharedPtr<FTrackModel> CreateTrackModel(UMovieSceneTrack* Track);

	explicit FCameraCutTrackModel(UMovieSceneCameraCutTrack* Track);

	FSortingKey GetSortingKey() const override;
};

} // namespace Sequencer
} // namespace UE

