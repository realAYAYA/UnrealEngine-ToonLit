// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FDisplayClusterColorGradingDataModel;
class SDetailsSectionView;
class SSplitter;
struct FDisplayClusterColorGradingDrawerState;

/** A panel that displays several property details views based on the color grading data model */
class SDisplayClusterColorGradingDetailsPanel : public SCompoundWidget
{
public:
	/** The maximum number of details sections that are allowed to be displayed at the same time */
	static const int32 MaxNumDetailsSections = 3;

public:
	SLATE_BEGIN_ARGS(SDisplayClusterColorGradingDetailsPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FDisplayClusterColorGradingDataModel>, ColorGradingDataModelSource)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Refreshes the details panel to reflect the current state of the color grading data model */
	void Refresh();

	/** Adds the state of the details panel to the specified drawer state */
	void GetDrawerState(FDisplayClusterColorGradingDrawerState& OutDrawerState);

	/** Sets the state of the details panel from the specified drawer state */
	void SetDrawerState(const FDisplayClusterColorGradingDrawerState& InDrawerState);

private:
	/** Fills the details sections based on the current state of the color grading data model */
	void FillDetailsSections();

	/** Gets the visibility state of the specified details section */
	EVisibility GetDetailsSectionVisibility(int32 SectionIndex) const;

private:
	/** The color grading data model that the panel is displaying */
	TSharedPtr<FDisplayClusterColorGradingDataModel> ColorGradingDataModel;

	TArray<TSharedPtr<SDetailsSectionView>> DetailsSectionViews;
	TSharedPtr<SSplitter> Splitter;
};