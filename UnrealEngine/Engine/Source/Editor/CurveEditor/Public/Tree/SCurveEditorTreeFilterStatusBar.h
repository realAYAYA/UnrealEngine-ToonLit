// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Visibility.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCurveEditor;
class STextBlock;

/** Widget that shows the metrics for the current tree filter in the curve editor (in the form "Showing {0} of {1} items ({2} selected)") */
class CURVEEDITOR_API SCurveEditorTreeFilterStatusBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCurveEditorTreeFilterStatusBar){}
	SLATE_END_ARGS()

	/** Construct the status bar */
	void Construct(const FArguments& InArgs, TSharedPtr<FCurveEditor> InCurveEditor);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void ShowStatusBar();
	void HideStatusBar();
	void FadeOutStatusBar();

private:

	/** Request the visibility of the clear hyperlink widget based on whether there's a filter active or not */
	EVisibility GetVisibilityFromFilter() const;

	/** Update the filter text to represent the current filter states in the tree */
	void UpdateText();

	/** Clear all the filters in the tree - called as a result of the user clicking on the 'clear' hyperlink */
	void ClearFilters();

private:

	TWeakPtr<FCurveEditor> WeakCurveEditor;
	TSharedPtr<STextBlock> TextBlock;
	double OpacityThrobEndTime = 0;
};