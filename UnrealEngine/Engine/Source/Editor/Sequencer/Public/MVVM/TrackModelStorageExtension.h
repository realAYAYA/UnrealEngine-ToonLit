// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectKey.h"
#include "MVVM/Extensions/DynamicExtensionContainer.h"
#include "EventHandlers/ISequenceDataEventHandler.h"
#include "ISequencerModule.h"

class UMovieSceneTrack;

namespace UE
{
namespace Sequencer
{

class FTrackModel;
class FSequenceModel;
class FViewModel;

class FTrackModelStorageExtension
	: public IDynamicExtension
	, private UE::MovieScene::TIntrusiveEventHandler<UE::MovieScene::ISequenceDataEventHandler>
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FTrackModelStorageExtension)

	FTrackModelStorageExtension(const TArray<FOnCreateTrackModel>& InTrackModelCreators);

	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;
	virtual void OnReinitialize() override;

	TSharedPtr<FTrackModel> CreateModelForTrack(UMovieSceneTrack* InTrack, TSharedPtr<FViewModel> DesiredParent = nullptr);

	TSharedPtr<FTrackModel> FindModelForTrack(UMovieSceneTrack* InTrack) const;

private:

	void OnTrackAdded(UMovieSceneTrack*) override;
	void OnTrackRemoved(UMovieSceneTrack*) override;

private:

	TArray<FOnCreateTrackModel> TrackModelCreators;

	TMap<FObjectKey, TWeakPtr<FTrackModel>> TrackToModel;
	FSequenceModel* OwnerModel;
};

} // namespace Sequencer
} // namespace UE

