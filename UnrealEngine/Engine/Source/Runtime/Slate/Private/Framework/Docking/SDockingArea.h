// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/SDockingNode.h"
#include "Framework/Docking/SDockingSplitter.h"

class STabSidebar;

/** List of tabs that should be in each sidebar */
struct FSidebarTabLists
{
	TArray<TSharedRef<SDockTab>> LeftSidebarTabs;
	TArray<TSharedRef<SDockTab>> RightSidebarTabs;
};

/**
 * Represents the root node in a hierarchy of DockNodes.
 */
class SLATE_API SDockingArea : public SDockingSplitter
{
public:

	SLATE_BEGIN_ARGS(SDockingArea)
		: _ParentWindow()
		, _ShouldManageParentWindow(true)
		, _InitialContent()
		{
			// Visible by default, but don't absorb clicks
			_Visibility = EVisibility::SelfHitTestInvisible;
		}
		
		/* The window whose content area this dock area is directly embedded within.  By default, ShouldManageParentWindow is
		   set to true, which means the dock area will also destroy the window when the last tab goes away.  Assigning a
		   parent window also allows the docking area to embed title area widgets (minimize, maximize, etc) into its content area */
		SLATE_ARGUMENT( TSharedPtr<SWindow>, ParentWindow )

		/** True if this docking area should close the parent window when the last tab in this docking area goes away */
		SLATE_ARGUMENT( bool, ShouldManageParentWindow )

		 /**
		 * What to put into the DockArea initially. Usually a TabStack, so that some tabs can be added to it.
		 */
		SLATE_ARGUMENT( TSharedPtr<SDockingNode>, InitialContent )

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<FTabManager>& InTabManager, const TSharedRef<FTabManager::FArea>& PersistentNode );

	virtual Type GetNodeType() const override
	{
		return SDockingNode::DockArea;
	}
	
	/** Returns this dock area */
	virtual TSharedPtr<SDockingArea> GetDockArea() override;
	virtual TSharedPtr<const SDockingArea> GetDockArea() const override;

	/** Returns the window that this dock area resides in directly and also manages */
	TSharedPtr<SWindow> GetParentWindow() const;

	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override;
	
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override;

	virtual FReply OnUserAttemptingDock( SDockingNode::RelativeDirection Direction, const FDragDropEvent& DragDropEvent ) override;

	void OnTabFoundNewHome( const TSharedRef<SDockTab>& RelocatedTab, const TSharedRef<SWindow>& NewOwnerWindow );

	// Show the dock-from-outside dock targets
	void ShowCross();

	// Hide the dock-from-outside dock targets
	void HideCross();

	/**
	 * Removes redundant stack and splitters. Collapses any widgets that any no longer showing live content.
	 */
	void CleanUp( ELayoutModification RemovalMethod );

	void SetParentWindow( const TSharedRef<SWindow>& NewParentWindow );

	virtual TSharedPtr<FTabManager::FLayoutNode> GatherPersistentLayout() const override;

	TSharedRef<FTabManager> GetTabManager() const;

	/**
	 * Adds a tab to a drawer in the sidebar. 
	 *
	 * @param TabToAdd	The tab to add to the sidebar
	 * @return The location of the sidebar that the tab was added to
	 */
	ESidebarLocation AddTabToSidebar(TSharedRef<SDockTab> TabToAdd);

	/**
	 * Restores a tab from the sidebar to its parent tab stack and removes the tab from the sidebar.  
	 *
	 * @return true if the tab was found in this area and restore
	 */
	bool RestoreTabFromSidebar(TSharedRef<SDockTab> TabToRemove);

	/**
	 * @return true if the specified tab is in the sidebar
	 */
	bool IsTabInSidebar(TSharedRef<SDockTab> Tab) const;

	/**
 	 * Removes a tab from a sidebar
	 * @return true if the specified tab was found and removed
	 */
	bool RemoveTabFromSidebar(TSharedRef<SDockTab> Tab);

	/**
	 * Attempts to open a sidebar drawer that may the tab to open
	 *
	 * @true if the drawer was opened, false if the tab is not in a drawer
	 */
	bool TryOpenSidebarDrawer(TSharedRef<SDockTab> TabToOpen) const;

	/**
	 * Adds all tabs back to a sidebar that were saved in a sidebar from a previous session
	 */
	void AddSidebarTabsFromRestoredLayout(const FSidebarTabLists& SidebarTabs);

	/**
	 * Gets all tabs in all sidebars in this dock area
	 */
	TArray<TSharedRef<SDockTab>> GetAllSidebarTabs() const;

	bool CanHaveSidebar() const { return bCanHaveSidebar; }
protected:
	virtual SDockingNode::ECleanupRetVal CleanUpNodes() override;

private:
	EVisibility TargetCrossVisibility() const;
	EVisibility TargetCrossCenterVisibility() const;
	/** Dock a tab along the outer edge of this DockArea */
	void DockFromOutside(SDockingNode::RelativeDirection Direction, const FDragDropEvent& DragDropEvent);

	/** We were placed in a window, and it is being destroyed */
	void OnOwningWindowBeingDestroyed(const TSharedRef<SWindow>& WindowBeingDestroyed);

	/** We were placed in a window and it is being activated */
	void OnOwningWindowActivated();
	
	virtual void OnLiveTabAdded() override;

	/**
	 * If this dock area controls a window, then we need
	 * to reserve some room in the upper left and upper right tab wells
	 * so that there is no overlap with the window chrome.
	 *
	 * We also update the sidebar to account for major tabs.  Docking areas for major tabs do not have a sidebar.
	 */
	void UpdateWindowChromeAndSidebar();
private:	
	/** Left and right sidebar widgets */
	TSharedPtr<STabSidebar> LeftSidebar;
	TSharedPtr<STabSidebar> RightSidebar;

	/** The window this dock area is embedded within.  If bIsManagingParentWindow is true, the dock area will also
	    destroy the window when the last tab goes away. */
	TWeakPtr<SWindow> ParentWindowPtr;

	/**
	 * We don't want to waste a lot of space for the minimize, restore, close buttons and other windows controls.
	 * DockAreas that manage a parent window will use this slot to house those controls.
	 */
	SOverlay::FOverlaySlot* WindowControlsArea;

	/** True if this docking area should close the parent window when the last tab in this docking area goes away */
	bool bManageParentWindow;

	/** The tab manager that controls this DockArea */
	TWeakPtr<FTabManager> MyTabManager;

	/** The overlay is visible when the user is dragging a tab over the dock area */
	bool bIsOverlayVisible;

	/** The center target is visible when the overlay is visible and there are no live tabs */
	bool bIsCenterTargetVisible;

	/** True when the last tab has been pulled from this area, meaning that this DockArea will not be necessary once that tab finds a new home. */
	bool bCleanUpUponTabRelocation;

	/** True if this area can ever show sidebars (minor tab areas only) */
	bool bCanHaveSidebar;
};
