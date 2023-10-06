// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMenuBuilder;
class SMemoryProfilerWindow;

namespace TraceServices
{
	class IAnalysisSession;
}

namespace Insights
{
	class FMemoryRuleSpec;
	class FQueryTargetWindowSpec;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * A custom widget used to setup and run mem (allocations) queries.
 */
class SMemInvestigationView : public SCompoundWidget
{
public:
	/** Default constructor. */
	SMemInvestigationView();

	/** Virtual destructor. */
	virtual ~SMemInvestigationView();

	SLATE_BEGIN_ARGS(SMemInvestigationView) {}
	SLATE_END_ARGS()

	/**
	 * Converts profiler window weak pointer to a shared pointer and returns it.
	 * Make sure the returned pointer is valid before trying to dereference it.
	 */
	TSharedPtr<SMemoryProfilerWindow> GetProfilerWindow() const
	{
		return ProfilerWindowWeakPtr.Pin();
	}

	/**
	 * Construct this widget
	 * @param InArgs - The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs, TSharedPtr<SMemoryProfilerWindow> InProfilerWindow);

	void Reset();

	void QueryTarget_OnSelectionChanged(TSharedPtr<Insights::FQueryTargetWindowSpec> InRule, ESelectInfo::Type SelectInfo);

private:
	void UpdateSymbolPathsText() const;
	TSharedRef<SWidget> ConstructInvestigationWidgetArea();
	TSharedRef<SWidget> ConstructTimeMarkerWidget(uint32 TimeMarkerIndex);

	/** Called when the analysis session has changed. */
	void InsightsManager_OnSessionChanged();

	const TArray<TSharedPtr<Insights::FQueryTargetWindowSpec>>* GetAvailableQueryTargets();
	TSharedRef<SWidget> QueryTarget_OnGenerateWidget(TSharedPtr<Insights::FQueryTargetWindowSpec> InRule);
	FText QueryTarget_GetSelectedText() const;
	const TArray<TSharedPtr<Insights::FMemoryRuleSpec>>* GetAvailableQueryRules();
	void QueryRule_OnSelectionChanged(TSharedPtr<Insights::FMemoryRuleSpec> InRule, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> QueryRule_OnGenerateWidget(TSharedPtr<Insights::FMemoryRuleSpec> InRule);
	FText QueryRule_GetSelectedText() const;
	FText QueryRule_GetTooltipText() const;
	FReply RunQuery();

	/**
	 * Ticks this widget.  Override in derived classes, but always call the parent implementation.
	 *
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	FReply OnTimeMarkerLabelDoubleClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent, uint32 TimeMarkerIndex);

private:
	/** A weak pointer to the Memory Insights window. */
	TWeakPtr<SMemoryProfilerWindow> ProfilerWindowWeakPtr;

	/** The analysis session used to populate this widget. */
	TSharedPtr<const TraceServices::IAnalysisSession> Session;

	TSharedPtr<SComboBox<TSharedPtr<Insights::FMemoryRuleSpec>>> QueryRuleComboBox;

	bool bIncludeHeapAllocs;

	TSharedPtr<SComboBox<TSharedPtr<Insights::FQueryTargetWindowSpec>>> QueryTargetComboBox;
	
	TSharedPtr<STextBlock> SymbolPathsTextBlock;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
