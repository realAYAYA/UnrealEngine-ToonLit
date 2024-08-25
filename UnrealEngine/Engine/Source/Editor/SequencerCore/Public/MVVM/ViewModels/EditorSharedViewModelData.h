// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "MVVM/SharedViewModelData.h"

namespace UE::Sequencer
{

class SEQUENCERCORE_API FEditorSharedViewModelData
	: public FSharedViewModelData
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FEditorSharedViewModelData, FSharedViewModelData);

	FEditorSharedViewModelData(TSharedRef<FEditorViewModel> InEditor)
		: WeakEditor(InEditor)
	{
	}

	TSharedPtr<FEditorViewModel> GetEditor() const
	{
		return WeakEditor.Pin();
	}

private:

	/** The editor view model for this shared data */
	TWeakPtr<FEditorViewModel> WeakEditor;
};

} // namespace UE::Sequencer

