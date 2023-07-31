// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/SequencerTrackAreaViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "Sequencer.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/GenericCommands.h"

namespace UE
{
namespace Sequencer
{

FSequencerTrackAreaViewModel::FSequencerTrackAreaViewModel(TSharedRef<ISequencer> InSequencer)
	: WeakSequencer(InSequencer)
{
}

FFrameRate FSequencerTrackAreaViewModel::GetTickResolution() const
{
	if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin())
	{
		return Sequencer->GetFocusedTickResolution();
	}
	return FFrameRate();
}

TRange<double> FSequencerTrackAreaViewModel::GetViewRange() const
{
	if (TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin())
	{
		return Sequencer->GetViewRange();
	}
	return TRange<double>::Empty();
}

} // namespace Sequencer
} // namespace UE

