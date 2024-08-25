// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Views/SPropertyAnimatorCoreEditorControllersView.h"

#include "Framework/Application/SlateApplication.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"
#include "Widgets/TableRows/SPropertyAnimatorCoreEditorControllersViewTableRow.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorControllersView"

void SPropertyAnimatorCoreEditorControllersView::Construct(const FArguments& InArgs, TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> InEditPanel)
{
	EditPanelWeak = InEditPanel;

	check(InEditPanel.IsValid());

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot(0)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(ControllersTree, STreeView<FControllersViewItemPtr>)
			.TreeItemsSource(&ControllersTreeSource)
			.SelectionMode(ESelectionMode::Multi)
			.ItemHeight(30.f)
			.HeaderRow(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(SPropertyAnimatorCoreEditorEditPanel::HeaderAnimatorColumnName)
				.DefaultLabel(FText::FromName(SPropertyAnimatorCoreEditorEditPanel::HeaderAnimatorColumnName))
				.FillWidth(0.5f)
				+ SHeaderRow::Column(SPropertyAnimatorCoreEditorEditPanel::HeaderPropertyColumnName)
				.DefaultLabel(FText::FromName(SPropertyAnimatorCoreEditorEditPanel::HeaderPropertyColumnName))
				.FillWidth(0.5f)
			)
			.OnSelectionChanged(this, &SPropertyAnimatorCoreEditorControllersView::OnSelectionChanged)
			.OnGetChildren(this, &SPropertyAnimatorCoreEditorControllersView::OnGetChildren)
			.OnGenerateRow(this, &SPropertyAnimatorCoreEditorControllersView::OnGenerateRow)
		]
		+ SOverlay::Slot(1)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
		   SNew(STextBlock)
		   .Text(LOCTEXT("ControllersListEmpty", "No animators found on this actor"))
		   .Visibility(this, &SPropertyAnimatorCoreEditorControllersView::GetControllerTextVisibility)
		]
	];
}

void SPropertyAnimatorCoreEditorControllersView::Update()
{
	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = EditPanelWeak.Pin();

	if (!EditPanel.IsValid())
	{
		return;
	}

	const AActor* ContextActor = EditPanel->GetOptions().GetContextActor();

	if (!ContextActor)
	{
		return;
	}

	// Collect all existing controllers on actor
	const UPropertyAnimatorCoreSubsystem* PropertyControllerSubsystem = UPropertyAnimatorCoreSubsystem::Get();

	if (!PropertyControllerSubsystem)
	{
		return;
	}

	TSet<UPropertyAnimatorCoreBase*> ExistingControllers = PropertyControllerSubsystem->GetExistingAnimators(ContextActor);
	ControllersTreeSource.Empty(ExistingControllers.Num());

	for (UPropertyAnimatorCoreBase* ExistingController : ExistingControllers)
	{
		FControllersViewItemPtr NewItem = MakeShared<FControllersViewItem>();
		NewItem->ControlledProperty.ControllerWeak = ExistingController;
		ControllersTreeSource.Add(NewItem);
	}

	if (ControllersTree.IsValid())
	{
		ControllersTree->RebuildList();

		// Auto expand
		for (const FControllersViewItemPtr& Item : ControllersTreeSource)
		{
			ControllersTree->SetItemExpansion(Item, true);
		}
	}
}

void SPropertyAnimatorCoreEditorControllersView::OnSelectionChanged(FControllersViewItemPtr InItem, ESelectInfo::Type InSelectInfo)
{
	if (InSelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = EditPanelWeak.Pin();

	if (!EditPanel.IsValid())
	{
		return;
	}

	TSet<FPropertiesViewControllerItem>& GlobalSelection = EditPanel->GetGlobalSelection();

	if (!FSlateApplication::Get().GetModifierKeys().IsControlDown())
	{
		GlobalSelection.Reset();
	}

	for (const FControllersViewItemPtr& SelectedItem : ControllersTree->GetSelectedItems())
	{
		GlobalSelection.Add(SelectedItem->ControlledProperty);
	}

	EditPanel->OnGlobalSelectionChangedDelegate.Broadcast();
}

void SPropertyAnimatorCoreEditorControllersView::OnGetChildren(FControllersViewItemPtr InItem, TArray<FControllersViewItemPtr>& OutChildren)
{
	if (!InItem.IsValid())
	{
		 return;
	}

	// Children already loaded
	if (!InItem->Children.IsEmpty())
	{
		OutChildren = InItem->Children;
		return;
	}

	UPropertyAnimatorCoreBase* Controller = InItem->ControlledProperty.ControllerWeak.Get();

	// We reach leaf when we have a property set
	if (!Controller
		|| InItem->ControlledProperty.Property.IsValid())
	{
		return;
	}

	for (const FPropertyAnimatorCoreData& LinkedProperty : Controller->GetLinkedProperties())
	{
		FControllersViewItemPtr NewItem = MakeShared<FControllersViewItem>();
		NewItem->ControlledProperty.ControllerWeak = Controller;
		NewItem->ControlledProperty.Property = MakeShared<FPropertyAnimatorCoreData>(LinkedProperty);
		NewItem->ParentWeak = InItem;
		OutChildren.Add(NewItem);
	}
}

TSharedRef<ITableRow> SPropertyAnimatorCoreEditorControllersView::OnGenerateRow(FControllersViewItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SPropertyAnimatorCoreEditorControllersViewTableRow, InOwnerTable, SharedThis(this), InItem);
}

EVisibility SPropertyAnimatorCoreEditorControllersView::GetControllerTextVisibility() const
{
	return ControllersTreeSource.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE