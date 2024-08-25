// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequencerAction.h"
#include "AvaSequencer.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"

TSharedPtr<UE::Sequencer::FSequencerSelection> FAvaSequencerAction::GetSequencerSelection() const
{
	using namespace UE::Sequencer;

	if (TSharedPtr<FSequencerEditorViewModel> EditorViewModel = Owner.GetSequencer()->GetViewModel())
	{
		return EditorViewModel->GetSelection();
	}

	return nullptr;
}
