// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/Views/PropertyAnimatorCoreEditorViewItem.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/SCompoundWidget.h"

class SPropertyAnimatorCoreEditorEditPanel;

class SPropertyAnimatorCoreEditorPropertiesView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPropertyAnimatorCoreEditorPropertiesView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> InEditPanel);

	void Update();

	TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> GetEditPanel() const
	{
		return EditPanelWeak.Pin();
	}

protected:
	TSharedRef<ITableRow> OnGenerateRow(FPropertiesViewItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	void OnGetChildren(FPropertiesViewItemPtr InItem, TArray<FPropertiesViewItemPtr>& OutChildren) const;

	EVisibility GetPropertyTextVisibility() const;

private:
	TSharedPtr<STreeView<FPropertiesViewItemPtr>> PropertiesTree;
	TArray<FPropertiesViewItemPtr> PropertiesTreeSource;
	TWeakPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanelWeak;
};