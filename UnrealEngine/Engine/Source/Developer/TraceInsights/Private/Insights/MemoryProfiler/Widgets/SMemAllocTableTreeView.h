// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TraceServices/Model/AllocationsProvider.h"

// Insights
#include "Insights/Common/Stopwatch.h"
#include "Insights/MemoryProfiler/ViewModels/MemAllocTable.h"
#include "Insights/Table/Widgets/STableTreeView.h"

namespace Insights
{

class FMemAllocNode;
class FMemoryRuleSpec;

////////////////////////////////////////////////////////////////////////////////////////////////////

class SMemAllocTableTreeView : public STableTreeView
{
public:
	/** Default constructor. */
	SMemAllocTableTreeView();

	/** Virtual destructor. */
	virtual ~SMemAllocTableTreeView();

	SLATE_BEGIN_ARGS(SMemAllocTableTreeView) {}
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 * @param InArgs - The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<FMemAllocTable> InTablePtr);

	virtual TSharedPtr<SWidget> ConstructToolbar() override;
	virtual TSharedPtr<SWidget> ConstructFooter() override;

	TSharedPtr<FMemAllocTable> GetMemAllocTable() { return StaticCastSharedPtr<FMemAllocTable>(GetTable()); }
	const TSharedPtr<FMemAllocTable> GetMemAllocTable() const { return StaticCastSharedPtr<FMemAllocTable>(GetTable()); }

	//void UpdateSourceTable(TSharedPtr<TraceServices::IMemAllocTable> SourceTable);

	virtual void Reset();

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/**
	 * Rebuilds the tree (if necessary).
	 * @param bResync - If true, it forces a resync even if the list did not changed since last sync.
	 */
	virtual void RebuildTree(bool bResync);

	void SetQueryParams(TSharedPtr<FMemoryRuleSpec> InRule, double TimeA = 0.0, double TimeB = 0.0, double TimeC = 0.0, double TimeD = 0.0)
	{
		Rule = InRule;
		TimeMarkers[0] = TimeA;
		TimeMarkers[1] = TimeB;
		TimeMarkers[2] = TimeC;
		TimeMarkers[3] = TimeD;
		OnQueryInvalidated();
	}

	int32 GetTabIndex() const { return TabIndex; }
	void SetTabIndex(int32 InTabIndex) { TabIndex = InTabIndex; }

	////////////////////////////////////////////////////////////////////////////////////////////////////
	// IAsyncOperationStatusProvider implementation

	virtual bool IsRunning() const override;
	virtual double GetAllOperationsDuration() override;
	virtual FText GetCurrentOperationName() const override;

	////////////////////////////////////////////////////////////////////////////////////////////////////

protected:
	virtual void InternalCreateGroupings() override;

	TSharedPtr<FMemAllocNode> GetSingleSelectedMemAllocNode() const;
	uint32 CountSourceFiles(FMemAllocNode& MemAllocNode);

	virtual void ExtendMenu(FMenuBuilder& MenuBuilder) override;
	void BuildOpenSourceSubMenu(FMenuBuilder& MenuBuilder);
	bool CanOpenCallstackFrameSourceFileInIDE() const;
	void OpenCallstackFrameSourceFileInIDE();
	FText GetSelectedCallstackFrameFileName() const;
	void OpenSourceFileInIDE(const TCHAR* File, uint32 Line) const;

private:
	void OnQueryInvalidated();
	void StartQuery();
	void UpdateQuery(TraceServices::IAllocationsProvider::EQueryStatus& OutStatus);
	void CancelQuery();
	void ResetAndStartQuery();

	FText GetSymbolResolutionStatus() const;
	FText GetSymbolResolutionTooltip() const;
	FText GetQueryInfo() const;
	FText GetQueryInfoTooltip() const;

	void UpdateQueryInfo();
	bool virtual ApplyCustomAdvancedFilters(const FTableTreeNodePtr& NodePtr) override;
	virtual void AddCustomAdvancedFilters() override;

	TSharedRef<SWidget> ConstructFunctionToggleButton();
	void CallstackGroupingByFunction_OnCheckStateChanged(ECheckBoxState NewRadioState);
	ECheckBoxState CallstackGroupingByFunction_IsChecked() const;

	void InitAvailableViewPresets();
	void PopulateLLMTagSuggestionList(const FString& Text, TArray<FString>& OutSuggestions);

private:
	const static int FullCallStackIndex;
	const static int LLMFilterIndex;
	int32 TabIndex = -1;
	TSharedPtr<FMemoryRuleSpec> Rule = nullptr;
	double TimeMarkers[4];
	TraceServices::IAllocationsProvider::FQueryHandle Query = 0;
	FText QueryInfo;
	FText QueryInfoTooltip;
	FStopwatch QueryStopwatch;
	bool bHasPendingQueryReset = false;
	bool bIsCallstackGroupingByFunction = true;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
