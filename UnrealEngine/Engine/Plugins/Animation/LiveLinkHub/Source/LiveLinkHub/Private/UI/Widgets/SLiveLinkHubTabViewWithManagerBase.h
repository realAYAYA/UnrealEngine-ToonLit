// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Docking/TabManager.h"
#include "UI/Widgets/SLiveLinkHubTabViewBase.h"

class FMenuBarBuilder;

/** Base class for tab views that create sub-tabs. */
class SLiveLinkHubTabViewWithManagerBase : public SLiveLinkHubTabViewBase
{
public:

	DECLARE_DELEGATE_TwoParams(FCreateTabs, const TSharedRef<FTabManager>& /*TabManager*/, const TSharedRef<FTabManager::FLayout>& /*Layout*/);
	DECLARE_DELEGATE_OneParam(FCreateWindowMenu, FMenuBarBuilder&);
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOverlayTabs, const TSharedRef<SWidget>& /* Tabs */);
	
	SLATE_BEGIN_ARGS(SLiveLinkHubTabViewWithManagerBase) {}
		/** Which major tab to construct the sub-tabs under. */
		SLATE_ARGUMENT(TSharedPtr<SDockTab>, ConstructUnderMajorTab)
		/** The window in which the sub-tabs will be created. */
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ConstructUnderWindow)
		/** Callback for creating the sub-tabs */
		SLATE_EVENT(FCreateTabs, CreateTabs)
		/** Called to fill the major tab's menu bar */
		SLATE_EVENT(FCreateWindowMenu, CreateMenuBar)
		/** Optional function to overlay all the tabs (for example to overlay a message to execute a console command first) */
		SLATE_EVENT(FOverlayTabs, OverlayTabs)
		/** Name to give the layout. Important for saving config. */
		SLATE_ARGUMENT(FName, LayoutName)
	SLATE_END_ARGS()

	/**
	 * @param InArgs
	 */
	void Construct(const FArguments& InArgs);

protected:
	/** Get the tab manager. */
	TSharedPtr<FTabManager> GetTabManager() const { return TabManager; }
	
private:

	/** Holds the tab manager that manages the front-end's tabs. */
	TSharedPtr<FTabManager> TabManager;
	/** Handles restoring the layout and instantiating tabs. */
	TSharedRef<SWidget> CreateTabs(const FArguments& InArgs);
	/** Creates the top bar menu items. */
	void FillInDefaultMenuItems(FMenuBarBuilder MenuBarBuilder);
	/** Populates the tab spawner's file menu. */
	void FillFileMenu(FMenuBuilder& MenuBuilder);
	/** Populates the tab spawner's window menu. */
	void FillWindowMenu(FMenuBuilder& MenuBuilder);
};
