// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
template <typename ItemType> class STreeView;
class STableViewBase;
class UMVVMWidgetBlueprintExtension_View;

namespace UE::MVVM
{
class SBindingsPanel;

struct FBindingEntry;

class SBindingsList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBindingsList) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SBindingsPanel> Owner, UMVVMWidgetBlueprintExtension_View* MVVMExtension);
	~SBindingsList();

	void Refresh();

	/** Constructs context menu used for right click and dropdown button */
	TSharedPtr<SWidget> OnSourceConstructContextMenu();

private:
	TSharedRef<ITableRow> GenerateEntryRow(TSharedPtr<FBindingEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	void OnSourceListSelectionChanged(TSharedPtr<FBindingEntry> Entry, ESelectInfo::Type SelectionType) const;
	void GetChildrenOfEntry(TSharedPtr<FBindingEntry> Entry, TArray<TSharedPtr<FBindingEntry>>& OutChildren) const;

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	void OnDeleteSelected();

private:
	TWeakPtr<SBindingsPanel> BindingPanel;
	TSharedPtr<STreeView<TSharedPtr<FBindingEntry>>> TreeView;
	TArray<TSharedPtr<FBindingEntry>> RootWidgets;
	TWeakObjectPtr<UMVVMWidgetBlueprintExtension_View> MVVMExtension;
	mutable bool bSelectionChangedGuard = false;
};

} // namespace UE::MVVM
