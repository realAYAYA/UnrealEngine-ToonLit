// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStateTreeView.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "SPositiveActionButton.h"
#include "SStateTreeViewRow.h"
#include "Debugger/StateTreeDebuggerCommands.h"
#include "StateTreeViewModel.h"
#include "StateTreeState.h"
#include "StateTreeEditorCommands.h"
#include "StateTreeSettings.h"
#include "Framework/Commands/UICommandList.h"

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

void SStateTreeView::Construct(const FArguments& InArgs, TSharedRef<FStateTreeViewModel> InStateTreeViewModel, const TSharedRef<FUICommandList>& InCommandList)
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
		.OnExpansionChanged(this, &SStateTreeView::HandleTreeExpansionChanged)
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
				.FillSize(1.0f)
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

	CommandList = InCommandList;
	BindCommands();
}

void SStateTreeView::BindCommands()
{
	const FStateTreeEditorCommands& Commands = FStateTreeEditorCommands::Get();

	CommandList->MapAction(
		Commands.AddSiblingState,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleAddSiblingState),
		FCanExecuteAction());

	CommandList->MapAction(
		Commands.AddChildState,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleAddChildState),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::HasSelection));

	CommandList->MapAction(
		Commands.CutStates,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleCutSelectedStates),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::HasSelection));

	CommandList->MapAction(
		Commands.CopyStates,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleCopySelectedStates),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::HasSelection));

	CommandList->MapAction(
		Commands.DeleteStates,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleDeleteStates),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::HasSelection));

	CommandList->MapAction(
		Commands.PasteStatesAsSiblings,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandlePasteStatesAsSiblings),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::CanPaste));

	CommandList->MapAction(
		Commands.PasteStatesAsChildren,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandlePasteStatesAsChildren),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::CanPaste));

	CommandList->MapAction(
		Commands.DuplicateStates,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleDuplicateSelectedStates),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::HasSelection));

	CommandList->MapAction(
		Commands.RenameState,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleRenameState),
		FCanExecuteAction::CreateSP(this, &SStateTreeView::HasSelection));

	CommandList->MapAction(
		Commands.EnableStates,
		FExecuteAction::CreateSP(this, &SStateTreeView::HandleEnableSelectedStates),
		FCanExecuteAction(),
		FGetActionCheckState::CreateLambda([this]
			{
				const bool bCanEnable = CanEnableStates();
				const bool bCanDisable = CanDisableStates();
				if (bCanEnable && bCanDisable)
				{
					return ECheckBoxState::Undetermined;
				}
				
				if (bCanDisable)
				{
					return ECheckBoxState::Checked;
				}

				if (bCanEnable)
				{
					return ECheckBoxState::Unchecked;
				}

				// Should not happen since action is not visible in this case
				return ECheckBoxState::Undetermined;
			}),
		FIsActionButtonVisible::CreateLambda([this] { return CanEnableStates() || CanDisableStates(); }));
}

bool SStateTreeView::HasSelection() const
{
	return StateTreeViewModel && StateTreeViewModel->HasSelection();
}

bool SStateTreeView::CanPaste() const
{
	return StateTreeViewModel
			&& StateTreeViewModel->HasSelection()
			&& StateTreeViewModel->CanPasteStatesFromClipboard();
}

bool SStateTreeView::CanEnableStates() const
{
	return StateTreeViewModel
			&& StateTreeViewModel->HasSelection()
			&& StateTreeViewModel->CanEnableStates();
}

bool SStateTreeView::CanDisableStates() const
{
	return StateTreeViewModel
			&& StateTreeViewModel->HasSelection()
			&& StateTreeViewModel->CanDisableStates();
}

FReply SStateTreeView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if(CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	else
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
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

	TreeView->RequestTreeRefresh();

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

	// Request to rename the state immediately.
	RequestedRenameState = NewState;

	if (StateTreeViewModel.IsValid())
	{
		StateTreeViewModel->SetSelection(NewState);
	}
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
		TreeView->SetItemSelection(SelectedStates, /*bSelected*/true);

		if (SelectedStates.Num() == 1)
		{
			TreeView->RequestScrollIntoView(SelectedStates[0]);	
		}
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

void SStateTreeView::HandleTreeExpansionChanged(TWeakObjectPtr<UStateTreeState> InSelectedItem, bool bExpanded)
{
	// Not calling Modify() on the state as we don't want the expansion to dirty the asset.
	// @todo: this is temporary fix for a bug where adding a state will reset the expansion state. 
	if (UStateTreeState* State = InSelectedItem.Get())
	{
		State->bExpanded = bExpanded;
	}
}

TSharedPtr<SWidget> SStateTreeView::HandleContextMenuOpening()
{
	if (!StateTreeViewModel)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.AddSubMenu(
		LOCTEXT("AddState", "Add State"),
		FText(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().AddSiblingState);
			MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().AddChildState);
		}),
		/*bInOpenSubMenuOnClick =*/false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
	);

	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().CutStates);
	MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().CopyStates);

	MenuBuilder.AddSubMenu(
		LOCTEXT("Paste", "Paste"),
		FText(),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().PasteStatesAsSiblings);
			MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().PasteStatesAsChildren);
		}),
		/*bInOpenSubMenuOnClick =*/false,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Paste")
	);
	
	MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().DuplicateStates);
	MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().DeleteStates);
	MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().RenameState);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FStateTreeEditorCommands::Get().EnableStates);

#if WITH_STATETREE_DEBUGGER
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FStateTreeDebuggerCommands::Get().EnableOnEnterStateBreakpoint);
	MenuBuilder.AddMenuEntry(FStateTreeDebuggerCommands::Get().EnableOnExitStateBreakpoint);
#endif // WITH_STATETREE_DEBUGGER
	
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

UStateTreeState* SStateTreeView::GetFirstSelectedState() const
{
	TArray<UStateTreeState*> SelectedStates;
	if (StateTreeViewModel)
	{
		StateTreeViewModel->GetSelectedStates(SelectedStates);
	}
	return SelectedStates.IsEmpty() ? nullptr : SelectedStates[0];
}

void SStateTreeView::HandleAddSiblingState()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->AddState(GetFirstSelectedState());
	}
}

void SStateTreeView::HandleAddChildState()
{
	if (StateTreeViewModel)
	{
		UStateTreeState* ParentState = GetFirstSelectedState();
		if (ParentState)
		{
			StateTreeViewModel->AddChildState(ParentState);
			TreeView->SetItemExpansion(ParentState, true);
		}
	}
}

void SStateTreeView::HandleCutSelectedStates()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->CopySelectedStates();
		StateTreeViewModel->RemoveSelectedStates();
	}
}

void SStateTreeView::HandleCopySelectedStates()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->CopySelectedStates();
	}
}

void SStateTreeView::HandlePasteStatesAsSiblings()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->PasteStatesFromClipboard(GetFirstSelectedState());
	}
}

void SStateTreeView::HandlePasteStatesAsChildren()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->PasteStatesAsChildrenFromClipboard(GetFirstSelectedState());
	}
}

void SStateTreeView::HandleDuplicateSelectedStates()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->DuplicateSelectedStates();
	}
}

void SStateTreeView::HandleDeleteStates()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->RemoveSelectedStates();
	}
}

void SStateTreeView::HandleRenameState()
{
	RequestedRenameState = GetFirstSelectedState();
}

void SStateTreeView::HandleEnableSelectedStates()
{
	if (StateTreeViewModel)
	{
		// Process CanEnable first so in case of undetermined state (mixed selection) we Enable by default. 
		if (CanEnableStates())
		{
			StateTreeViewModel->SetSelectedStatesEnabled(true);	
		}
		else if (CanDisableStates())
		{
			StateTreeViewModel->SetSelectedStatesEnabled(false);
		}
	}
}

void SStateTreeView::HandleDisableSelectedStates()
{
	if (StateTreeViewModel)
	{
		StateTreeViewModel->SetSelectedStatesEnabled(false);
	}
}

#undef LOCTEXT_NAMESPACE
