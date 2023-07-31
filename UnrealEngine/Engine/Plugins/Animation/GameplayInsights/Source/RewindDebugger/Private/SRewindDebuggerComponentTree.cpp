// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRewindDebuggerComponentTree.h"

#include "ISequencerWidgetsModule.h"
#include "ObjectTrace.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Images/SImage.h"

#include "RewindDebugger.h"

#define LOCTEXT_NAMESPACE "SAnimationInsights"

SRewindDebuggerComponentTree::SRewindDebuggerComponentTree() 
	: SCompoundWidget()
	, DebugComponents(nullptr)
{ 
}

SRewindDebuggerComponentTree::~SRewindDebuggerComponentTree() 
{

}

TSharedRef<ITableRow> SRewindDebuggerComponentTree::ComponentTreeViewGenerateRow(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	FSlateIcon ObjectIcon = InItem->GetIcon();
	
	return 
		SNew(STableRow<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth().Padding(2)
			[
				SNew(SImage).DesiredSizeOverride(FVector2D(16,16))
				.Image(ObjectIcon.GetIcon())
			]
			+SHorizontalBox::Slot()
			[
				SNew(STextBlock)
				.Text(InItem->GetDisplayName())
			]
		];
}

void ComponentTreeViewGetChildren(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, TArray<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>& OutChildren)
{
	InItem->IterateSubTracks([&OutChildren](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> Track)
	{
		if (Track->IsVisible())
		{
			OutChildren.Add(Track);
		}
	});
}

void SRewindDebuggerComponentTree::ComponentTreeViewExpansionChanged(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> InItem, bool bShouldBeExpanded)
{
	InItem->SetIsExpanded(bShouldBeExpanded);
	OnExpansionChanged.ExecuteIfBound();
}

void SRewindDebuggerComponentTree::SetSelection(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SelectedItem)
{
	ComponentTreeView->SetSelection(SelectedItem);
}

void SRewindDebuggerComponentTree::ScrollTo(double ScrollOffset)
{
	ComponentTreeView->SetScrollOffset(ScrollOffset);
}

void SRewindDebuggerComponentTree::Construct(const FArguments& InArgs)
{
	DebugComponents = InArgs._DebugComponents;

	ComponentTreeView = SNew(STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>)
									.ItemHeight(20.0f)
									.TreeItemsSource(DebugComponents)
									.OnGenerateRow(this, &SRewindDebuggerComponentTree::ComponentTreeViewGenerateRow)
									.OnGetChildren_Static(&ComponentTreeViewGetChildren)
									.OnExpansionChanged(this, &SRewindDebuggerComponentTree::ComponentTreeViewExpansionChanged)
									.SelectionMode(ESelectionMode::Single)
									.OnSelectionChanged(InArgs._OnSelectionChanged)
									.OnMouseButtonDoubleClick(InArgs._OnMouseButtonDoubleClick)
									.ExternalScrollbar(InArgs._ExternalScrollBar)
									.AllowOverscroll(EAllowOverscroll::No)
									.OnTreeViewScrolled(InArgs._OnScrolled)
									.ScrollbarDragFocusCause(EFocusCause::SetDirectly)
									.OnContextMenuOpening(InArgs._OnContextMenuOpening);

	ChildSlot
	[
		ComponentTreeView.ToSharedRef()
	];

	OnExpansionChanged = InArgs._OnExpansionChanged;
}

static void RestoreExpansion(TSharedPtr<RewindDebugger::FRewindDebuggerTrack> Track, TSharedPtr<STreeView<TSharedPtr<RewindDebugger::FRewindDebuggerTrack>>>& TreeView)
{
	TreeView->SetItemExpansion(Track, Track->GetIsExpanded());
	Track->IterateSubTracks([&TreeView](TSharedPtr<RewindDebugger::FRewindDebuggerTrack> SubTrack)
	{
		RestoreExpansion(SubTrack, TreeView);
	});
}

void SRewindDebuggerComponentTree::RestoreExpansion()
{
	for (auto& Track : *DebugComponents)
	{
		::RestoreExpansion(Track, ComponentTreeView);
	}
}

void SRewindDebuggerComponentTree::Refresh()
{
	ComponentTreeView->RebuildList();

	if (DebugComponents)
	{
		// make sure any newly added TreeView nodes are created expanded
		RestoreExpansion();
	}
}

#undef LOCTEXT_NAMESPACE
