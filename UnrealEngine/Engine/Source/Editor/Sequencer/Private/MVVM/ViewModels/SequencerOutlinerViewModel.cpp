// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/SequencerOutlinerViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/SequenceModel.h"

#include "Sequencer.h"
#include "SequencerKeyCollection.h"
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

void FSequencerOutlinerViewModel::RequestUpdate()
{
	FSequencerEditorViewModel* EditorViewModel = GetEditor()->CastThisChecked<FSequencerEditorViewModel>();
	TSharedPtr<ISequencer> Sequencer = EditorViewModel->GetSequencer();

	if (Sequencer)
	{
		Sequencer->RefreshTree();
	}
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

FFrameNumber FSequencerOutlinerViewModel::GetNextKey(const TArray<TSharedRef<UE::Sequencer::FViewModel>>& InNodes, FFrameNumber FrameNumber, EMovieSceneTimeUnit TimeUnit, const TRange<FFrameNumber>& Range)
{
	return GetNextKeyInternal(InNodes, FrameNumber, TimeUnit, Range, EFindKeyDirection::Forwards);
}

FFrameNumber FSequencerOutlinerViewModel::GetPreviousKey(const TArray<TSharedRef<UE::Sequencer::FViewModel>>& InNodes, FFrameNumber FrameNumber, EMovieSceneTimeUnit TimeUnit, const TRange<FFrameNumber>& Range)
{
	return GetNextKeyInternal(InNodes, FrameNumber, TimeUnit, Range, EFindKeyDirection::Backwards);
}

FFrameNumber FSequencerOutlinerViewModel::GetNextKeyInternal(const TArray<TSharedRef<UE::Sequencer::FViewModel>>& InNodes, FFrameNumber FrameNumber, EMovieSceneTimeUnit TimeUnit, const TRange<FFrameNumber>& Range, EFindKeyDirection Direction)
{
	FSequencerEditorViewModel* EditorViewModel = GetEditor()->CastThisChecked<FSequencerEditorViewModel>();
	TSharedPtr<ISequencer> Sequencer = EditorViewModel->GetSequencer();
	if (Sequencer.IsValid())
	{
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();

		FSequencerKeyCollection* KeyCollection = Sequencer->GetKeyCollection();

		const float DuplicateThresholdSeconds = SMALL_NUMBER;
		const int64 TotalMaxSeconds = static_cast<int64>(TNumericLimits<int32>::Max() / TickResolution.AsDecimal());

		FFrameNumber ThresholdFrames = (DuplicateThresholdSeconds * TickResolution).FloorToFrame();
		if (ThresholdFrames.Value < -TotalMaxSeconds)
		{
			ThresholdFrames.Value = TotalMaxSeconds;
		}
		else if (ThresholdFrames.Value > TotalMaxSeconds)
		{
			ThresholdFrames.Value = TotalMaxSeconds;
		}

		if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
		{
			FrameNumber = ConvertFrameTime(FrameNumber, DisplayRate, TickResolution).FloorToFrame();
		}

		KeyCollection->Update(FSequencerKeyCollectionSignature::FromNodesRecursive(InNodes, ThresholdFrames));

		TOptional<FFrameNumber> NextKey = KeyCollection->GetNextKey(FrameNumber, Direction, Range, EFindKeyType::FKT_All);
		if (NextKey.IsSet())
		{
			if (TimeUnit == EMovieSceneTimeUnit::DisplayRate)
			{
				return ConvertFrameTime(NextKey.GetValue(), TickResolution, DisplayRate).FloorToFrame();
			}

			return NextKey.GetValue();
		}
	}

	return FrameNumber;
}

} // namespace Sequencer
} // namespace UE

