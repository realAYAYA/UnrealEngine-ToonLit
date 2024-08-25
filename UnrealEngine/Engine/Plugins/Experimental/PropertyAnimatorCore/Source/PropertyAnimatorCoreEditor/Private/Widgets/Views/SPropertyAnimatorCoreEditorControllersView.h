// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/Views/PropertyAnimatorCoreEditorViewItem.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/SCompoundWidget.h"

class SPropertyAnimatorCoreEditorEditPanel;

class SPropertyAnimatorCoreEditorControllersView : public SCompoundWidget
{
	friend class SPropertyAnimatorCoreEditorControllersViewTableRow;

public:
	SLATE_BEGIN_ARGS(SPropertyAnimatorCoreEditorControllersView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> InEditPanel);

	void Update();

	TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> GetEditPanel() const
	{
		return EditPanelWeak.Pin();
	}

protected:
	void OnSelectionChanged(FControllersViewItemPtr InItem, ESelectInfo::Type InSelectInfo);

	void OnGetChildren(FControllersViewItemPtr InItem, TArray<FControllersViewItemPtr>& OutChildren);

	TSharedRef<ITableRow> OnGenerateRow(FControllersViewItemPtr ControllersViewItem, const TSharedRef<STableViewBase>& InOwnerTable);

	EVisibility GetControllerTextVisibility() const;

	TSharedPtr<STreeView<FControllersViewItemPtr>> ControllersTree;
	TArray<FControllersViewItemPtr> ControllersTreeSource;
	TWeakPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanelWeak;
};
