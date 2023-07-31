// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/SequencerOutlinerViewModel.h"
#include "MVVM/ViewModels/SequencerTrackAreaViewModel.h"
#include "MVVM/CurveEditorExtension.h"
#include "ISequencerModule.h"
#include "Sequencer.h"
#include "MovieSceneSequenceID.h"

namespace UE
{
namespace Sequencer
{

FSequencerEditorViewModel::FSequencerEditorViewModel(TSharedRef<ISequencer> InSequencer, const FSequencerHostCapabilities& InHostCapabilities)
	: WeakSequencer(InSequencer)
	, bSupportsCurveEditor(InHostCapabilities.bSupportsCurveEditor)
{
}

void FSequencerEditorViewModel::PreInitializeEditorImpl()
{
	if (bSupportsCurveEditor)
	{
		AddDynamicExtension(FCurveEditorExtension::ID);
	}
}

TSharedPtr<FViewModel> FSequencerEditorViewModel::CreateRootModelImpl()
{
	TSharedPtr<FSequenceModel> RootSequenceModel = MakeShared<FSequenceModel>(SharedThis(this));
	RootSequenceModel->InitializeExtensions();
	return RootSequenceModel;
}

TSharedPtr<FOutlinerViewModel> FSequencerEditorViewModel::CreateOutlinerImpl()
{
	return MakeShared<FSequencerOutlinerViewModel>();
}

TSharedPtr<FTrackAreaViewModel> FSequencerEditorViewModel::CreateTrackAreaImpl()
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer.IsValid());
	return MakeShared<FSequencerTrackAreaViewModel>(Sequencer.ToSharedRef());
}

TSharedPtr<ISequencer> FSequencerEditorViewModel::GetSequencer() const
{
	return WeakSequencer.Pin();
}

TSharedPtr<FSequencer> FSequencerEditorViewModel::GetSequencerImpl() const
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	return StaticCastSharedPtr<FSequencer>(Sequencer);
}

void FSequencerEditorViewModel::SetSequence(UMovieSceneSequence* InRootSequence)
{
	TSharedPtr<FSequenceModel> SequenceModel = GetRootModel().ImplicitCast();
	SequenceModel->SetSequence(InRootSequence, MovieSceneSequenceID::Root);
}

} // namespace Sequencer
} // namespace UE

