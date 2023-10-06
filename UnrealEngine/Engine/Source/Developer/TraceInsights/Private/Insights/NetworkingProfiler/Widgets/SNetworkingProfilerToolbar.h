// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FExtender;
class SNetworkingProfilerWindow;

class SNetworkingProfilerToolbar : public SCompoundWidget
{
public:
	/** Default constructor. */
	SNetworkingProfilerToolbar();

	/** Virtual destructor. */
	virtual ~SNetworkingProfilerToolbar();

	SLATE_BEGIN_ARGS(SNetworkingProfilerToolbar) {}
	SLATE_ARGUMENT(TSharedPtr<FExtender>, ToolbarExtender);
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs, TSharedRef<SNetworkingProfilerWindow> InProfilerWindow);
};
