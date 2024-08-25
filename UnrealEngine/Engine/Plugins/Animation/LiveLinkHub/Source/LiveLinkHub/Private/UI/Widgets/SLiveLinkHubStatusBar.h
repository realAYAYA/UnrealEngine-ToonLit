// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class SWidgetDrawer;

namespace UE::LiveLinkHub::Private
{
	const FName OutputLogId("OutputLog");
}

/**
 * The status bar for Live Link Hub which contains console input and the output log.
 */
class SLiveLinkHubStatusBar : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SLiveLinkHubStatusBar) { }
	SLATE_END_ARGS()

	virtual ~SLiveLinkHubStatusBar() override;

	/**
	 * @param InArgs 
	 * @param StatusBarId The unique status bar id to assign to the widget drawer.
	 */
	void Construct(const FArguments& InArgs, FName StatusBarId);

private:
	/**
	 * Construct the widget drawer.
	 *
	 * @param StatusBarId The unique status bar id to assign.
	 */
	TSharedRef<SWidgetDrawer> MakeWidgetDrawer(FName StatusBarId);
	
private:
	/** The widget drawer for this status bar. */
	TSharedPtr<SWidgetDrawer> WidgetDrawer;
};
