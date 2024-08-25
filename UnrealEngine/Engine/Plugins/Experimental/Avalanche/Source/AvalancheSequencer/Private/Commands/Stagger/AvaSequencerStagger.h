// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequencerStaggerSettings.h"
#include "Commands/AvaSequencerAction.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "MVVM/ViewModelPtr.h"
#include "Math/Range.h"
#include "Misc/FrameNumber.h"
#include "SequencerCoreFwd.h"

namespace UE::Sequencer
{
	class FLayerBarModel;
}

class FAvaSequencerStagger : public FAvaSequencerAction
{
	struct FStaggerElement
	{
		explicit FStaggerElement(UE::Sequencer::TViewModelPtr<UE::Sequencer::FLayerBarModel> InLayerBarModel);

		void ComputeRange();

		UE::Sequencer::TViewModelPtr<UE::Sequencer::FLayerBarModel> LayerBarModel;

		TRange<FFrameNumber> ComputedRange;
	};

public:
	explicit FAvaSequencerStagger(FAvaSequencer& InOwner)
		: FAvaSequencerAction(InOwner)
	{
	}

protected:
	//~ Begin FAvaSequencerAction
	virtual void MapAction(const TSharedRef<FUICommandList>& InCommandList) override;
	//~ End FAvaSequencerAction

	bool CanExecute() const;

	void Execute();

private:
	TArray<FStaggerElement> GatherStaggerElements() const;

	bool GetSettings(FAvaSequencerStaggerSettings& OutSettings);

	void Stagger(TArrayView<FStaggerElement> InStaggerElements, const FAvaSequencerStaggerSettings& InSettings);
};
