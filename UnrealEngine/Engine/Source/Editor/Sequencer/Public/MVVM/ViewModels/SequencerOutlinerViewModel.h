// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DragAndDrop.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "ISequencerModule.h"

class FMenuBuilder;

namespace UE
{
namespace Sequencer
{

class FSequenceModel;

class FSequencerOutlinerViewModel : public FOutlinerViewModel
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FSequencerOutlinerViewModel, FOutlinerViewModel);

	FSequencerOutlinerViewModel();

	void BuildContextMenu(FMenuBuilder& MenuBuilder);
	void BuildCustomContextMenuForGuid(FMenuBuilder& MenuBuilder, FGuid ObjectBinding);

	/*~ FOutlinerViewModel */
	TSharedPtr<SWidget> CreateContextMenuWidget() override;
	TSharedRef<FDragDropOperation> InitiateDrag(TArray<TWeakViewModelPtr<IOutlinerExtension>>&& InDraggedModels) override;

public:

	/** Called when the add menu is created */
	FOnGetAddMenuContent OnGetAddMenuContent;

	/** Called when object is clicked in track list */
	FOnBuildCustomContextMenuForGuid OnBuildCustomContextMenuForGuid;
};

} // namespace Sequencer
} // namespace UE

