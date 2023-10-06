// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "EventHandlers/ISequenceDataEventHandler.h"
#include "EventHandlers/MovieSceneDataEventContainer.h"
#include "MVVM/Extensions/DynamicExtensionContainer.h"
#include "MVVM/ViewModelTypeID.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"

class UMovieSceneTrack;
namespace UE::MovieScene { class ISequenceDataEventHandler; }
struct FMovieSceneBinding;

namespace UE
{
namespace Sequencer
{

class FObjectBindingModel;
class FPlaceholderObjectBindingModel;
class FSequenceModel;
class FViewModel;
struct FViewModelChildren;

class SEQUENCER_API FObjectBindingModelStorageExtension
	: public IDynamicExtension
	, private UE::MovieScene::TIntrusiveEventHandler<UE::MovieScene::ISequenceDataEventHandler>
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FObjectBindingModelStorageExtension);

	FObjectBindingModelStorageExtension();

	/**
	 * Implementation function for creating a new model for a binding from its ID. May return a placeholder if the binding does not yet exist.
	 * @param Binding The binding to create a model for
	 */
	TSharedPtr<FViewModel> GetOrCreateModelForBinding(const FGuid& Binding);

	/**
	 * Implementation function for creating a new model for a binding
	 * @param Binding The binding to create a model for
	 */
	TSharedPtr<FViewModel> GetOrCreateModelForBinding(const FMovieSceneBinding& Binding);

	TSharedPtr<FObjectBindingModel> FindModelForObjectBinding(const FGuid& InObjectBindingID) const;

	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;
	virtual void OnReinitialize() override;

private:

	void OnBindingAdded(const FMovieSceneBinding& Binding) override;
	void OnBindingRemoved(const FGuid& ObjectBindingID) override;
	void OnTrackAddedToBinding(UMovieSceneTrack* Track, const FGuid& Binding) override;
	void OnTrackRemovedFromBinding(UMovieSceneTrack* Track, const FGuid& Binding) override;
	void OnBindingParentChanged(const FGuid& Binding, const FGuid& NewParent) override;


	TSharedPtr<FObjectBindingModel> CreateModelForObjectBinding(const FMovieSceneBinding& Binding);

	TSharedPtr<FViewModel> CreatePlaceholderForObjectBinding(const FGuid& ObjectID);
	TSharedPtr<FViewModel> FindPlaceholderForObjectBinding(const FGuid& InObjectBindingID) const;

	void Compact();

private:

	TMap<FGuid, TWeakPtr<FObjectBindingModel>> ObjectBindingToModel;
	TMap<FGuid, TWeakPtr<FPlaceholderObjectBindingModel>> ObjectBindingToPlaceholder;

	FSequenceModel* OwnerModel;
};

} // namespace Sequencer
} // namespace UE

