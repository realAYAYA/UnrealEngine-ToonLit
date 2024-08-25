// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/Views/SOutlinerTrackView.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

namespace UE::Sequencer
{

void SOutlinerTrackView::Construct(
		const FArguments& InArgs,
		TWeakViewModelPtr<IOutlinerExtension> InWeakOutlinerExtension,
		TWeakPtr<FSequencerEditorViewModel> InWeakEditor,
		const TSharedRef<ISequencerTreeViewRow>& InTableRow)
{
	SOutlinerItemViewBase::Construct(InArgs, InWeakOutlinerExtension, InWeakEditor, InTableRow);
}

} // namespace UE::Sequencer

