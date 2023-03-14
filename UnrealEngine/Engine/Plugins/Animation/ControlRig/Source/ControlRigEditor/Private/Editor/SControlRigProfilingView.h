// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/StatsHierarchical.h"
#include "Widgets/Views/STreeView.h"

class SControlRigProfilingView;

class SControlRigProfilingItem
	: public SMultiColumnTableRow<TSharedPtr<FStatsTreeElement>>
{
	SLATE_BEGIN_ARGS(SControlRigProfilingItem) {}
	SLATE_END_ARGS()

	static FName NAME_MarkerName;
	static FName NAME_TotalTimeInclusive;
	static FName NAME_TotalTimeExclusive;
	static FName NAME_AverageTimeInclusive;
	static FName NAME_AverageTimeExclusive;
	static FName NAME_Invocations;

public:

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, SControlRigProfilingView* InProfilingView, const TSharedPtr<FStatsTreeElement> InTreeElement);

	// SMultiColumnTableRow overrides
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	SControlRigProfilingView* ProfilingView;
	TWeakPtr<FStatsTreeElement> WeakTreeElement;

	FText GetLabelText() const;
	FText GetToolTipText() const;
	FSlateColor GetTextColor() const;
	FText GetTotalTimeText(bool bInclusive) const;
	FText GetAverageTimeText(bool bInclusive) const;
	FText GetInvocationsText() const;
	FReply OnDoubleClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent);

	void UpdateVisibilityFromSearch(const FString& InSearchText);

	friend class SControlRigProfilingView;
};

class SControlRigProfilingView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SControlRigProfilingView) {}
	SLATE_END_ARGS()

	~SControlRigProfilingView();

	void Construct(const FArguments& InArgs);

protected:

	/** Rebuild the tree view */
	void RefreshTreeView();

	void SetItemExpansion(const TSharedPtr<FStatsTreeElement>& InItem, bool bExpand = true, bool bRecursive = false);
	void ToggleItemExpansion(const TSharedPtr<FStatsTreeElement>& InItem, bool bRecursive = false);

private:

	/** Make a row widget for the table */
	TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FStatsTreeElement> InTreeElement, const TSharedRef<STableViewBase>& OwnerTable);

	void OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode);

	/** Get children for the tree */
	void HandleGetChildrenForTree(TSharedPtr<FStatsTreeElement> InTreeElement, TArray<TSharedPtr<FStatsTreeElement>>& OutChildren);
	void SortChildren(TArray<TSharedPtr<FStatsTreeElement>>& InOutChildren) const;

	EVisibility GetTreeVisibility() const;
	FSlateColor GetTreeBackgroundColor() const;
	FText GetProfilingButtonText() const;
	FReply HandleProfilingButton();
	FText GetSearchText() const;

	TSharedRef<SWidget> OnGetComboBoxWidget(TSharedPtr<FName> InItem);
	void OnComboBoxChanged(TSharedPtr<FName> InItem, ESelectInfo::Type InSeletionInfo, FName* OutValue);
	FText GetComboBoxValueAsText(FName* InValue) const;
	EActiveTimerReturnType OnRecordingTimerFinished(double InCurrentTime, float InDeltaTime);
	void OnSearchTextCommitted(const FText& InNewText, ETextCommit::Type InCommitType);

	void UntrackedCheckBoxCheckStateChanged(ECheckBoxState CheckState);
	ECheckBoxState GetUntrackedCheckBoxCheckState() const;
	bool IsLoadingEnabled() const;
	bool IsSavingEnabled() const;
	FReply HandleLoadFromFile();
	FReply HandleSaveToFile();

	TSharedPtr<STreeView<TSharedPtr<FStatsTreeElement>>> TreeView;

	FName SortColumn;
	bool bSortAscending;
	bool bShowUntracked;
	FString SearchText;
	FStatsTreeElement RootElement;
	TArray<TSharedPtr<FStatsTreeElement>> RootChildren;
	FName RecordTime;
	TArray<TSharedPtr<FName>> RecordTimeOptions;
	FName DisplayMode;
	TArray<TSharedPtr<FName>> DisplayModeOptions;
	int32 ParallelAnimEvaluationVarPrevValue;

	friend class SControlRigProfilingItem;
};
