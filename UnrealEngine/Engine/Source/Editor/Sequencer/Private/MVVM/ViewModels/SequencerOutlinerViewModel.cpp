// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/SequencerOutlinerViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/SequenceModel.h"

#include "Sequencer.h"
#include "SequencerOutlinerItemDragDropOp.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Commands/GenericCommands.h"

namespace UE
{
namespace Sequencer
{

FSequencerOutlinerViewModel::FSequencerOutlinerViewModel()
{
}

TSharedPtr<SWidget> FSequencerOutlinerViewModel::CreateContextMenuWidget()
{
	FSequencerEditorViewModel* EditorViewModel = GetEditor()->CastThisChecked<FSequencerEditorViewModel>();
	TSharedPtr<ISequencer> Sequencer = EditorViewModel->GetSequencer();

	if (Sequencer)
	{
		const bool bShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, Sequencer->GetCommandBindings());

		BuildContextMenu(MenuBuilder);

		MenuBuilder.BeginSection("Edit");
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Paste);
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	return nullptr;
}

void FSequencerOutlinerViewModel::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	FSequencerEditorViewModel* EditorViewModel = GetEditor()->CastThisChecked<FSequencerEditorViewModel>();
	TSharedPtr<FSequencer> Sequencer = EditorViewModel->GetSequencerImpl();

	// let toolkits populate the menu
	MenuBuilder.BeginSection("MainMenu");
	OnGetAddMenuContent.ExecuteIfBound(MenuBuilder, Sequencer.ToSharedRef());
	MenuBuilder.EndSection();

	// let track editors & object bindings populate the menu

	// Always create the section so that we afford extension
	MenuBuilder.BeginSection("ObjectBindings");
	if (Sequencer.IsValid())
	{
		Sequencer->BuildAddObjectBindingsMenu(MenuBuilder);
	}
	MenuBuilder.EndSection();

	// Always create the section so that we afford extension
	MenuBuilder.BeginSection("AddTracks");
	if (Sequencer.IsValid())
	{
		Sequencer->BuildAddTrackMenu(MenuBuilder);
	}
	MenuBuilder.EndSection();
}

void FSequencerOutlinerViewModel::BuildCustomContextMenuForGuid(FMenuBuilder& MenuBuilder, FGuid ObjectBinding)
{
	OnBuildCustomContextMenuForGuid.ExecuteIfBound(MenuBuilder, ObjectBinding);
}

TSharedRef<FDragDropOperation> FSequencerOutlinerViewModel::InitiateDrag(TArray<TWeakViewModelPtr<IOutlinerExtension>>&& InDraggedModels)
{
	FText DefaultText = FText::Format( NSLOCTEXT( "SequencerOutlinerViewModel", "DefaultDragDropFormat", "Move {0} item(s)" ), FText::AsNumber( InDraggedModels.Num() ) );
	return FSequencerOutlinerDragDropOp::New( MoveTemp(InDraggedModels), DefaultText, nullptr );
}

} // namespace Sequencer
} // namespace UE

