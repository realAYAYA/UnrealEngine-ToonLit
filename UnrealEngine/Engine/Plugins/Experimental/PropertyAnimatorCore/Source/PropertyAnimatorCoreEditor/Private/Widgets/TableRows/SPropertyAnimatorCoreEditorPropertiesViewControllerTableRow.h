// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/Views/SPropertyAnimatorCoreEditorPropertiesView.h"
#include "Widgets/Views/STableRow.h"

class SPropertyAnimatorCoreEditorPropertiesViewTableRow;

class SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow : public STableRow<FPropertiesViewControllerItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow) {}
	SLATE_END_ARGS()

	virtual ~SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow() override;

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView,
		TSharedPtr<SPropertyAnimatorCoreEditorPropertiesViewTableRow> InView, FPropertiesViewControllerItemPtr InItem);

private:
	void OnGlobalSelectionChanged();

	TWeakPtr<SPropertyAnimatorCoreEditorPropertiesViewTableRow> RowWeak;
	FPropertiesViewControllerItemPtrWeak TileItemWeak;
};