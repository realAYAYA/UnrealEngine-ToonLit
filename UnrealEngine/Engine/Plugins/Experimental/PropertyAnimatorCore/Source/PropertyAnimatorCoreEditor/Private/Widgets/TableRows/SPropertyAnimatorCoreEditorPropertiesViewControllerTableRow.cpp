// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/TableRows/SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow.h"

#include "Styling/SlateIconFinder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"
#include "Widgets/TableRows/SPropertyAnimatorCoreEditorPropertiesViewTableRow.h"

SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow::~SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow()
{
	if (const TSharedPtr<SPropertyAnimatorCoreEditorPropertiesViewTableRow> Row = RowWeak.Pin())
	{
		if (const TSharedPtr<SPropertyAnimatorCoreEditorPropertiesView> View = Row->ViewWeak.Pin())
		{
			if (const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = View->GetEditPanel())
			{
				EditPanel->OnGlobalSelectionChangedDelegate.RemoveAll(this);
			}
		}
	}
}

void SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<SPropertyAnimatorCoreEditorPropertiesViewTableRow> InView, FPropertiesViewControllerItemPtr InItem)
{
	RowWeak = InView;
	TileItemWeak = InItem;

	check(InItem.IsValid())

	const TSharedPtr<SPropertyAnimatorCoreEditorPropertiesView> View = InView->ViewWeak.Pin();

	check(View.IsValid())

	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = View->GetEditPanel();

	check(EditPanel.IsValid())

	EditPanel->OnGlobalSelectionChangedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow::OnGlobalSelectionChanged);

	const UPropertyAnimatorCoreBase* Controller = InItem->ControllerWeak.Get();

	check(Controller);

	const FText ControllerDisplayName = FText::FromString(Controller->GetAnimatorDisplayName());
	const FSlateIcon ControllerIcon = FSlateIconFinder::FindIconForClass(Controller->GetClass());

	ChildSlot
	.Padding(1.f)
	[
		SNew(SScaleBox)
		.Stretch(EStretch::ScaleToFit)
		[
			SNew(SImage)
			.ToolTipText(ControllerDisplayName)
			.Image(ControllerIcon.GetIcon())
		]
	];

	STableRow::ConstructInternal(
		STableRow::FArguments()
		.Padding(5.0f)
		.ShowSelection(true)
		.OnDragDetected(EditPanel.Get(), &SPropertyAnimatorCoreEditorEditPanel::OnDragStart),
		InOwnerTableView
	);

	SetBorderImage(TAttribute<const FSlateBrush*>(this, &SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow::GetBorder));
}

void SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow::OnGlobalSelectionChanged()
{
	const TSharedPtr<SPropertyAnimatorCoreEditorPropertiesViewTableRow> Row = RowWeak.Pin();

	if (!Row.IsValid())
	{
		return;
	}

	const TSharedPtr<SPropertyAnimatorCoreEditorPropertiesView> PropertiesView = Row->ViewWeak.Pin();

	if (!PropertiesView.IsValid())
	{
		return;
	}

	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = PropertiesView->GetEditPanel();

	if (!EditPanel.IsValid())
	{
		return;
	}

	const FPropertiesViewControllerItemPtr RowItem = TileItemWeak.Pin();

	if (!RowItem.IsValid())
	{
		return;
	}

	const TSet<FPropertiesViewControllerItem>& GlobalSelection = EditPanel->GetGlobalSelection();

	if (GlobalSelection.Contains(*RowItem))
	{
		Row->ControllersTile->SetItemSelection(RowItem, true, ESelectInfo::Direct);
	}
	else
	{
		Row->ControllersTile->SetItemSelection(RowItem, false, ESelectInfo::Direct);
	}
}
