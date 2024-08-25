// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/SortedMap.h"
#include "Misc/Guid.h"
#include "Delegates/DelegateCombinations.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "MovieSceneSequenceID.h"
#include "EventHandlers/ISignedObjectEventHandler.h"

#include "SequencerCoreFwd.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/ISortableExtension.h"
#include "MVVM/Extensions/ICurveEditorTreeItemExtension.h"

class ISequencer;
class FSequencer;
class FCurveEditor;
class UMovieScene;
class UMovieSceneSequence;

namespace UE
{
namespace Sequencer
{

class FEditorViewModel;
class FSequenceModel;
class FSequencerEditorViewModel;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInitializeSequenceModel, TSharedPtr<FEditorViewModel>, TSharedPtr<FSequenceModel>);

class SEQUENCER_API FSequenceModel
	: public FViewModel
	, public ISortableExtension
	, public IOutlinerDropTargetOutlinerExtension
	, public UE::MovieScene::ISignedObjectEventHandler
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FSequenceModel, FViewModel, ISortableExtension, IOutlinerDropTargetOutlinerExtension);

	static FOnInitializeSequenceModel CreateExtensionsEvent;

	FSequenceModel(TWeakPtr<FSequencerEditorViewModel> InEditorViewModel);

	void InitializeExtensions();

	FMovieSceneSequenceID GetSequenceID() const;
	UMovieSceneSequence* GetSequence() const;
	UMovieScene* GetMovieScene() const;
	void SetSequence(UMovieSceneSequence* InSequence, FMovieSceneSequenceID InSequenceID);

	TSharedPtr<FSequencerEditorViewModel> GetEditor() const;
	TSharedPtr<ISequencer> GetSequencer() const;
	TSharedPtr<FSequencer> GetSequencerImpl() const;

	TSharedPtr<FViewModel> GetBottomSpacer() const { return BottomSpacer; }

	/*~ ISignedObjectEventHandler */
	void OnPostUndo() override;
	void OnModifiedIndirectly(UMovieSceneSignedObject*) override;
	void OnModifiedDirectly(UMovieSceneSignedObject*) override;

	/*~ ISortableExtension */
	void SortChildren() override;
	FSortingKey GetSortingKey() const override;
	void SetCustomOrder(int32 InCustomOrder) override;

	/*~ IOutlinerDropTargetOutlinerExtension */
	TOptional<EItemDropZone> CanAcceptDrop(const FViewModelPtr& TargetModel, const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone) override;
	void PerformDrop(const FViewModelPtr& TargetModel, const FDragDropEvent& DragDropEvent, EItemDropZone InItemDropZone) override;

private:

	FViewModelListHead RootOutlinerItems;

	TWeakObjectPtr<UMovieSceneSequence> WeakSequence;
	TWeakPtr<FSequencerEditorViewModel> WeakEditor;

	TSharedPtr<FViewModel> BottomSpacer;
	FMovieSceneSequenceID SequenceID;

	MovieScene::TNonIntrusiveEventHandler<MovieScene::ISignedObjectEventHandler> SequenceEventHandler;
	MovieScene::TNonIntrusiveEventHandler<MovieScene::ISignedObjectEventHandler> MovieSceneEventHandler;
};

} // namespace Sequencer
} // namespace UE

