// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Widgets/DeclarativeSyntaxSupport.h"

class ITableRow;
class FRemoveLensDataListItem;
class STableViewBase;
class SWindow;
class ULensFile;
template<typename ItemType>
class STreeView;
enum class ELensDataCategory : uint8;

/**
 * Remove Dialog widget and window
 */
class SCameraCalibrationRemovePointDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCameraCalibrationRemovePointDialog) {}
		SLATE_EVENT(FSimpleDelegate, OnDataRemoved)
	SLATE_END_ARGS()

	SCameraCalibrationRemovePointDialog();

	void Construct(const FArguments& InArgs, const TSharedRef<SWindow>& InWindow, ULensFile* InLensFile, ELensDataCategory InCategory, float InFocus, TOptional<float> InZoom);

	/**
	 * Open remove points window
	 * @param InLensFile Lens file pointer
	 * @param InCategory Data table category to remove
	 * @param OnDataRemoved On removed callback
	 * @param InFocus Looking focus point
	 * @param InZoom Looking zoom point, could be optional it it need to remove Focus point and all children
	 */
	static void OpenWindow(ULensFile* InLensFile, ELensDataCategory InCategory, FSimpleDelegate OnDataRemoved, float InFocus, TOptional<float> InZoom = TOptional<float>());

private:
	/** Remove selected points */
	void RemoveSelected();

	/** Refresh all widgets */
	void Refresh();

	/** Refresh Tree for remove */
	void RefreshRemoveItemsTree();

	/** Generated Rows handler */
	TSharedRef<ITableRow> OnGenerateDataEntryRow(TSharedPtr<FRemoveLensDataListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Generated children handler */
	void OnGetDataEntryChildren(TSharedPtr<FRemoveLensDataListItem> InItem, TArray<TSharedPtr<FRemoveLensDataListItem>>& OutNodes);

	/** Remove button handler */
	FReply OnRemoveButtonClicked();

	/** Cancel button handler */
	FReply OnCancelButtonClicked() const;

private:
	/** Modal window pointer */
	TWeakPtr<SWindow> WindowWeakPtr;

	/** Items to remove */
	TArray<TSharedPtr<FRemoveLensDataListItem>> RemoveItems;

	/** Remove items tree widget */
	TSharedPtr<STreeView<TSharedPtr<FRemoveLensDataListItem>>> RemoveItemsTree;

	/** Lens data category of that data table */
	ELensDataCategory Category;

	/** Focus value of this item */
	float Focus = 0.f;

	/** Optional zoom value, in case if we want to remove only zoom points */
	TOptional<float> Zoom;

	/** LensFile we're editing */
	TWeakObjectPtr<ULensFile> WeakLensFile;

	/** On remove delegate */
	FSimpleDelegate OnDataRemoved;
};
