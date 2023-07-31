// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStateTreeView.h"

#include "Framework/Commands/Commands.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"

#include "Templates/SharedPointer.h"
#include "Styling/AppStyle.h"
#include "EditorFontGlyphs.h"
#include "StateTreeEditorStyle.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "SPositiveActionButton.h"
#include "SStateTreeViewRow.h"
#include "StateTree.h"

#include "StateTreeViewModel.h"
#include "StateTreeState.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

SStateTreeView::SStateTreeView()
	: RequestedRenameState(nullptr)
	, bItemsDirty(false)
	, bUpdatingSelection(false)
{
}

SStateTreeView::~SStateTreeView()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->GetOnAssetChanged().RemoveAll(this);
		StateTreeViewModel->GetOnStatesRemoved().RemoveAll(this);
		StateTreeViewModel->GetOnStatesMoved().RemoveAll(this);
		StateTreeViewModel->GetOnStateAdded().RemoveAll(this);
		StateTreeViewModel->GetOnStatesChanged().RemoveAll(this);
		StateTreeViewModel->GetOnSelectionChanged().RemoveAll(this);
	}
}

void SStateTreeView::Construct(const FArguments& InArgs, TSharedRef<FStateTreeViewModel> InStateTreeViewModel)
{
	StateTreeViewModel = InStateTreeViewModel;

	StateTreeViewModel->GetOnAssetChanged().AddSP(this, &SStateTreeView::HandleModelAssetChanged);
	StateTreeViewModel->GetOnStatesRemoved().AddSP(this, &SStateTreeView::HandleModelStatesRemoved);
	StateTreeViewModel->GetOnStatesMoved().AddSP(this, &SStateTreeView::HandleModelStatesMoved);
	StateTreeViewModel->GetOnStateAdded().AddSP(this, &SStateTreeView::HandleModelStateAdded);
	StateTreeViewModel->GetOnStatesChanged().AddSP(this, &SStateTreeView::HandleModelStatesChanged);
	StateTreeViewModel->GetOnSelectionChanged().AddSP(this, &SStateTreeView::HandleModelSelectionChanged);

	bUpdatingSelection = false;

	TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	StateTreeViewModel->GetSubTrees(Subtrees);

	TreeView = SNew(STreeView<TWeakObjectPtr<UStateTreeState>>)
		.OnGenerateRow(this, &SStateTreeView::HandleGenerateRow)
		.OnGetChildren(this, &SStateTreeView::HandleGetChildren)
		.TreeItemsSource(&Subtrees)
		.ItemHeight(32)
		.OnSelectionChanged(this, &SStateTreeView::HandleTreeSelectionChanged)
		.OnContextMenuOpening(this, &SStateTreeView::HandleContextMenuOpening)
		.AllowOverscroll(EAllowOverscroll::No)
		.ExternalScrollbar(VerticalScrollBar);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)

				// New State
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SPositiveActionButton)
					.ToolTipText(LOCTEXT("AddStateToolTip", "Add New State"))
					.Icon(FAppStyle::Get().GetBrush("Icons.Plus")) 
					.Text(LOCTEXT("AddState", "Add State"))
					.OnClicked(this, &SStateTreeView::HandleAddStateButton)
				]
			]
		]

		+SVerticalBox::Slot()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(0.0f)
			[
				SAssignNew(ViewBox, SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(HorizontalScrollBar)
				+SScrollBox::Slot()
				[
					TreeView.ToSharedRef()
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				VerticalScrollBar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			HorizontalScrollBar
		]
	];

	UpdateTree(true);
}


void SStateTreeView::SavePersistentExpandedStates()
{
	if (!StateTreeViewModel)
	{
		return;
	}

	TSet<TWeakObjectPtr<UStateTreeState>> ExpandedStates;
	TreeView->GetExpandedItems(ExpandedStates);
	StateTreeViewModel->SetPersistentExpandedStates(ExpandedStates);
}

void SStateTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bItemsDirty)
	{
		UpdateTree(/*bExpandPersistent*/true);
	}

	if (RequestedRenameState && !TreeView->IsPendingRefresh())
	{
		if (TSharedPtr<SStateTreeViewRow> Row = StaticCastSharedPtr<SStateTreeViewRow>(TreeView->WidgetFromItem(RequestedRenameState)))
		{
			Row->RequestRename();
		}
		RequestedRenameState = nullptr;
	}
}

void SStateTreeView::UpdateTree(bool bExpandPersistent)
{
	if (!StateTreeViewModel)
	{
		return;
	}

	TSet<TWeakObjectPtr<UStateTreeState>> ExpandedStates;
	if (bExpandPersistent)
	{
		// Get expanded state from the tree data.
		StateTreeViewModel->GetPersistentExpandedStates(ExpandedStates);
	}
	else
	{
		// Restore current expanded state.
		TreeView->GetExpandedItems(ExpandedStates);
	}

	// Remember selection
	TArray<TWeakObjectPtr<UStateTreeState>> SelectedStates;
	StateTreeViewModel->GetSelectedStates(SelectedStates);

	// Regenerate items
	StateTreeViewModel->GetSubTrees(Subtrees);
	TreeView->SetTreeItemsSource(&Subtrees);

	// Restore expanded state
	for (const TWeakObjectPtr<UStateTreeState>& State : ExpandedStates)
	{
		TreeView->SetItemExpansion(State, true);
	}

	// Restore selected state
	TreeView->ClearSelection();
	TreeView->SetItemSelection(SelectedStates, true);

	bItemsDirty = false;
}



void SStateTreeView::HandleModelAssetChanged()
{
	bItemsDirty = true;
}

void SStateTreeView::HandleModelStatesRemoved(const TSet<UStateTreeState*>& AffectedParents)
{
	bItemsDirty = true;
}

void SStateTreeView::HandleModelStatesMoved(const TSet<UStateTreeState*>& AffectedParents, const TSet<UStateTreeState*>& MovedStates)
{
	bItemsDirty = true;
}

void SStateTreeView::HandleModelStateAdded(UStateTreeState* ParentState, UStateTreeState* NewState)
{
	bItemsDirty = true;

	HandleRenameState(NewState);
}

void SStateTreeView::HandleModelStatesChanged(const TSet<UStateTreeState*>& AffectedStates, const FPropertyChangedEvent& PropertyChangedEvent)
{
	bool bArraysChanged = false;

	// The purpose of the rebuild below is to update the task visualization (number of widgets change).
	// This method is called when anything in a state changes, make sure to only rebuild when needed.
	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UStateTreeState, Tasks))
	{
		bArraysChanged = true;
	}
		
	if (bArraysChanged)
	{
		TreeView->RebuildList();
	}
}

void SStateTreeView::HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UStateTreeState>>& SelectedStates)
{
	if (bUpdatingSelection)
	{
		return;
	}

	TreeView->ClearSelection();

	if (SelectedStates.Num() > 0)
	{
		TreeView->SetItemSelection(SelectedStates, true);
	}
}


TSharedRef<ITableRow> SStateTreeView::HandleGenerateRow(TWeakObjectPtr<UStateTreeState> InState, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	return SNew(SStateTreeViewRow, InOwnerTableView, InState, ViewBox, StateTreeViewModel.ToSharedRef());
}

void SStateTreeView::HandleGetChildren(TWeakObjectPtr<UStateTreeState> InParent, TArray<TWeakObjectPtr<UStateTreeState>>& OutChildren)
{
	if (const UStateTreeState* Parent = InParent.Get())
	{
		OutChildren.Append(Parent->Children);
	}
}

void SStateTreeView::HandleTreeSelectionChanged(TWeakObjectPtr<UStateTreeState> InSelectedItem, ESelectInfo::Type SelectionType)
{
	if (!StateTreeViewModel)
	{
		return;
	}

	// Do not report code based selection changes.
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}

	TArray<TWeakObjectPtr<UStateTreeState>> SelectedItems = TreeView->GetSelectedItems();

	bUpdatingSelection = true;
	StateTreeViewModel->SetSelection(SelectedItems);
	bUpdatingSelection = false;
}

TSharedPtr<SWidget> SStateTreeView::HandleContextMenuOpening()
{
	if (!StateTreeViewModel)
	{
		return nullptr;
	}

	TArray<UStateTreeState*> SelectedStates;
	StateTreeViewModel->GetSelectedStates(SelectedStates);
	UStateTreeState* FirstSelectedState = SelectedStates.Num() > 0 ? SelectedStates[0] : nullptr;

	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddState", "Add State"),
		FText(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SStateTreeView::HandleAddState, FirstSelectedState)));

	if (FirstSelectedState)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddChildState", "Add Child State"),
			FText(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SStateTreeView::HandleAddChildState, FirstSelectedState)));

		MenuBuilder.AddSeparator();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameNodeGroup", "Rename"),
			FText(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SStateTreeView::HandleRenameState, FirstSelectedState)));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteState", "Delete Selected"),
			FText(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SStateTreeView::HandleDeleteItems)));
	}

	return MenuBuilder.MakeWidget();
}


FReply SStateTreeView::HandleAddStateButton()
{
	if (StateTreeViewModel == nullptr)
	{
		return FReply::Handled();
	}
	
	TArray<UStateTreeState*> SelectedStates;
	StateTreeViewModel->GetSelectedStates(SelectedStates);
	UStateTreeState* FirstSelectedState = SelectedStates.Num() > 0 ? SelectedStates[0] : nullptr;

	if (FirstSelectedState != nullptr)
	{
		// If the state is root, add child state, else sibling.
		if (FirstSelectedState->Parent == nullptr)
		{
			StateTreeViewModel->AddChildState(FirstSelectedState);
			TreeView->SetItemExpansion(FirstSelectedState, true);
		}
		else
		{
			StateTreeViewModel->AddState(FirstSelectedState);
		}
	}
	else
	{
		// Add root state at the lowest level.
		StateTreeViewModel->AddState(nullptr);
	}

	return FReply::Handled();
}

void SStateTreeView::HandleAddState(UStateTreeState* AfterState)
{
	if (StateTreeViewModel == nullptr)
	{
		return;
	}

	StateTreeViewModel->AddState(AfterState);
}

void SStateTreeView::HandleRenameState(UStateTreeState* State)
{
	RequestedRenameState = State;
}

void SStateTreeView::HandleAddChildState(UStateTreeState* ParentState)
{
	if (StateTreeViewModel == nullptr)
	{
		return;
	}

	if (ParentState)
	{
		StateTreeViewModel->AddChildState(ParentState);
		TreeView->SetItemExpansion(ParentState, true);
	}
}

void SStateTreeView::HandleDeleteItems()
{
	if (StateTreeViewModel == nullptr)
	{
		return;
	}

	StateTreeViewModel->RemoveSelectedStates();
}



#undef LOCTEXT_NAMESPACE
