// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "IKRetargetEditorController.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Framework/Commands/UICommandList.h"

class FTextFilterExpressionEvaluator;
class FIKRigEditorController;
class SIKRetargetChainMapList;
class FIKRetargetEditor;
struct FBoneChain;

class FRetargetChainMapElement
{
public:

	TSharedRef<ITableRow> MakeListRowWidget(
		const TSharedRef<STableViewBase>& InOwnerTable,
        TSharedRef<FRetargetChainMapElement> InStackElement,
        TSharedPtr<SIKRetargetChainMapList> InChainList);

	static TSharedRef<FRetargetChainMapElement> Make(TObjectPtr<URetargetChainSettings> InChainMap)
	{
		return MakeShareable(new FRetargetChainMapElement(InChainMap));
	}

	TWeakObjectPtr<URetargetChainSettings> ChainMap;

private:
	
	/** Hidden constructor, always use Make above */
	FRetargetChainMapElement(TObjectPtr<URetargetChainSettings> InChainMap) : ChainMap(InChainMap) {}

	/** Hidden constructor, always use Make above */
	FRetargetChainMapElement() = default;
};

typedef TSharedPtr< FRetargetChainMapElement > FRetargetChainMapElementPtr;
class SIKRetargetChainMapRow : public SMultiColumnTableRow< FRetargetChainMapElementPtr >
{
public:
	
	void Construct(
        const FArguments& InArgs,
        const TSharedRef<STableViewBase>& InOwnerTableView,
        TSharedRef<FRetargetChainMapElement> InChainElement,
        TSharedPtr<SIKRetargetChainMapList> InChainList);

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the table row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	void OnSourceChainComboSelectionChanged(TSharedPtr<FString> InName, ESelectInfo::Type SelectInfo);
	
private:

	FReply OnResetToDefaultClicked();

	EVisibility GetResetToDefaultVisibility() const;
	
	FText GetSourceChainName() const;

	FText GetTargetIKGoalName() const;

	TArray<TSharedPtr<FString>> SourceChainOptions;

	TWeakPtr<FRetargetChainMapElement> ChainMapElement;
	
	TWeakPtr<SIKRetargetChainMapList> ChainMapList;

	friend SIKRetargetChainMapList;
};

struct FChainMapFilterOptions
{
	bool bHideUnmappedChains = false;
	bool bHideMappedChains = false;
	bool bHideChainsWithoutIK = false;
};

typedef SListView< TSharedPtr<FRetargetChainMapElement> > SRetargetChainMapListViewType;

class SIKRetargetChainMapList : public SCompoundWidget, public FEditorUndoClient
{
	
public:
	
	SLATE_BEGIN_ARGS(SIKRetargetChainMapList) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FIKRetargetEditorController> InEditorController);

	void ClearSelection() const;

	void ResetChainSettings(URetargetChainSettings* Settings) const;
	
private:
	
	/** menu for adding new solver commands */
	TSharedPtr<FUICommandList> CommandList;
	
	/** editor controller */
	TWeakPtr<FIKRetargetEditorController> EditorController;

	/** list view */
	TSharedPtr<SRetargetChainMapListViewType> ListView;
	TArray< TSharedPtr<FRetargetChainMapElement> > ListViewItems;
	/** END list view */

	UIKRetargeterController* GetRetargetController() const;

	/** callbacks */
	FText GetSourceRootBone() const;
	FText GetTargetRootBone() const;
	bool IsChainMapEnabled() const;
	/** when a chain is clicked on in the table view */
	void OnItemClicked(TSharedPtr<FRetargetChainMapElement> InItem) const;
	/** when edit global settings button clicked */
	FReply OnGlobalSettingsButtonClicked() const;
	/** when edit root settings button clicked */
	FReply OnRootSettingsButtonClicked() const;

	/** auto-map chain button*/
	EVisibility IsAutoMapButtonVisible() const;
	FReply OnAutoMapButtonClicked() const;
	/** END auto-map chain button*/

	/** filtering the list with search box */
	TSharedRef<SWidget> CreateFilterMenuWidget();
	void OnFilterTextChanged(const FText& SearchText);
	TSharedPtr<FTextFilterExpressionEvaluator> TextFilter;
	FChainMapFilterOptions ChainFilterOptions;

	/** list view generate row callback */
	TSharedRef<ITableRow> MakeListRowWidget(TSharedPtr<FRetargetChainMapElement> InElement, const TSharedRef<STableViewBase>& OwnerTable);

	/** call to refresh the list view */
	void RefreshView();

	friend SIKRetargetChainMapRow;
	friend FIKRetargetEditorController;
};
