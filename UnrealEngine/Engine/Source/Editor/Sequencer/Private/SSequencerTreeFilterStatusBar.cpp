// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSequencerTreeFilterStatusBar.h"
#include "Sequencer.h"

#include "MovieScene.h"
#include "MovieSceneSequence.h"

#include "Algo/Count.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SSequencerTreeFilterStatusBar"

void SSequencerTreeFilterStatusBar::Construct(const FArguments& InArgs, TSharedPtr<FSequencer> InSequencer)
{
	WeakSequencer = InSequencer;

	ChildSlot
	.Padding(FMargin(5.f, 0.f))
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(3.0f, 3.0f))
			.Visibility(EVisibility::HitTestInvisible)
			[
				SAssignNew(TextBlock, STextBlock)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(3.f, 0.f, 0.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(3.0f, 3.0f))
			[
				SNew(SHyperlink)
				.Visibility(this, &SSequencerTreeFilterStatusBar::GetVisibilityFromFilter)
				.Text(LOCTEXT("ClearFilters", "clear"))
				.OnNavigate(this, &SSequencerTreeFilterStatusBar::ClearFilters)
			]
		]
	];
}

void SSequencerTreeFilterStatusBar::ClearFilters()
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer)
	{
		Sequencer->ClearFilters();
	}
}

EVisibility SSequencerTreeFilterStatusBar::GetVisibilityFromFilter() const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	return Sequencer && Sequencer->GetNodeTree()->HasActiveFilter() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SSequencerTreeFilterStatusBar::UpdateText()
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!ensureAlways(Sequencer))
	{
		return;
	}

	FText NewText;
	FLinearColor NewColor = FLinearColor::White;

	const TSharedRef<FSequencerNodeTree> NodeTree = Sequencer->GetNodeTree();
	const TSet<TWeakPtr<FViewModel>>& SelectedOutlinerItems = Sequencer->GetSelection().GetSelectedOutlinerItems();

	FFormatNamedArguments NamedArgs;
	NamedArgs.Add("Total", NodeTree->GetTotalDisplayNodeCount());

	const bool bHasSelection = SelectedOutlinerItems.Num() != 0;
	const bool bHasFilter = NodeTree->HasActiveFilter();
	const int32 NumFiltered = NodeTree->GetFilteredDisplayNodeCount();

	if (bHasSelection)
	{
		NamedArgs.Add("NumSelected", SelectedOutlinerItems.Num());
	}

	if (bHasFilter)
	{
		NamedArgs.Add("NumMatched", NumFiltered);
	}

	if (bHasFilter)
	{
		if (NumFiltered == 0)
		{
			// Red = no matched
			NewColor = FLinearColor( 1.0f, 0.4f, 0.4f );
		}
		else
		{
			// Green = matched filter
			NewColor = FLinearColor( 0.4f, 1.0f, 0.4f );
		}

		if (bHasSelection)
		{
			NewText = FText::Format(LOCTEXT("FilteredStatus_WithSelection", "Showing {NumMatched} of {Total} items ({NumSelected} selected)"), NamedArgs);
		}
		else 
		{
			NewText = FText::Format(LOCTEXT("FilteredStatus_NoSelection", "Showing {NumMatched} of {Total} items"), NamedArgs);
		}
	}
	else if (bHasSelection)
	{
		NewText = FText::Format(LOCTEXT("UnfilteredStatus_WithSelection", "{Total} items ({NumSelected} selected)"), NamedArgs);
	}
	else
	{
		NewText = FText::Format(LOCTEXT("UnfilteredStatus_NoSelection", "{Total} items"), NamedArgs);
	}

	TextBlock->SetColorAndOpacity(NewColor);
	TextBlock->SetText(NewText);
}

#undef LOCTEXT_NAMESPACE
