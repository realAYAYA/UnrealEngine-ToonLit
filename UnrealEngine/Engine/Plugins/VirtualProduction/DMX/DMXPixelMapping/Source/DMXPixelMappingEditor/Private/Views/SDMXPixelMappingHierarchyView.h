// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXPixelMappingEditorCommon.h"

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Misc/TextFilter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FDMXPixelMappingToolkit;
class UDMXPixelMapping;

class FUICommandList;
class ITableRow;
class SBorder;
class SSearchBox;
class STableViewBase;
template<typename ItemType> class STreeView;
template<typename ItemType> class TreeFilterHandler;


class SDMXPixelMappingHierarchyView
	: public SCompoundWidget, public FEditorUndoClient
{
public:
	using WidgetTextFilter = TTextFilter<FDMXPixelMappingHierarchyItemWidgetModelPtr>;
	using HierarchTreeView = STreeView<FDMXPixelMappingHierarchyItemWidgetModelPtr>;
	using TreeViewPtr = TSharedPtr<HierarchTreeView>;

	enum class EExpandBehavior : uint8
	{
		NeverExpand,
		AlwaysExpand,
		RestoreFromPrevious,
		FromModel
	};

public:

	SLATE_BEGIN_ARGS(SDMXPixelMappingHierarchyView) { }
	SLATE_END_ARGS()

public:
	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit);

	// Begin SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	// End SWidget

	 //~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

	/** force a rebuild of the hierarchy **/
	void RequestRebuildTree();

	/** force a redraw of a component **/
	void RequestComponentRedraw(UDMXPixelMappingBaseComponent* Component);

private:
	TSharedPtr<SWidget> WidgetHierarchy_OnContextMenuOpening();
	void WidgetHierarchy_OnGetChildren(FDMXPixelMappingHierarchyItemWidgetModelPtr InParent, FDMXPixelMappingHierarchyItemWidgetModelArr& OutChildren);
	TSharedRef< ITableRow > WidgetHierarchy_OnGenerateRow(FDMXPixelMappingHierarchyItemWidgetModelPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void WidgetHierarchy_OnSelectionChanged(FDMXPixelMappingHierarchyItemWidgetModelPtr SelectedItem, ESelectInfo::Type SelectInfo);

	/**  Gets an array of strings used for filtering/searching the specified widget. */
	void GetWidgetFilterStrings(FDMXPixelMappingHierarchyItemWidgetModelPtr InModelPtr, TArray<FString>& OutStrings);

	void ConditionallyUpdateTree();
	/** Completely regenerates the treeview */
	void RebuildTreeView();

	void RestoreSelectedItems();

	/** Restores selection for a the item and its children. Returns true if the Model or a child was selected. */
	bool RestoreSelectionForItemAndChildren(FDMXPixelMappingHierarchyItemWidgetModelPtr& Model);

	/** Rebuilds the tree structure based on the current filter options */
	void RefreshTree();

	/** Called when a component was added */
	void OnComponentAdded(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called when a component was removed */
	void OnComponentRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called when the selected widget has changed.  The treeview then needs to match the new selection. */
	void OnEditorSelectionChanged();

	void BeginRename();

	void BeginCut();

	bool CanBeginCut() const;

	void BeginCopy();

	bool CanBeginCopy() const;

	void BeginPaste();

	bool CanBeginPaste() const;

	void BeginDuplicate();

	bool CanBeginDuplicate() const;

	void BeginDelete();

	FText GetSearchText() const { return FText(); }

	void RecursivePaste(UDMXPixelMappingBaseComponent* InComponent);

	bool MoveComponentToComponent(UDMXPixelMappingBaseComponent* Source, UDMXPixelMappingBaseComponent* Destination, const bool bRename);

	void SelectFirstAvailableRenderer();

private:
	TWeakPtr<FDMXPixelMappingToolkit> Toolkit;

	/** Commands specific to the hierarchy. */
	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<SBorder> TreeViewArea;

	TSharedPtr<SSearchBox> SearchBoxPtr;

	/** Handles filtering the hierarchy based on an IFilter. */
	TSharedPtr<TreeFilterHandler<FDMXPixelMappingHierarchyItemWidgetModelPtr>> FilterHandler;

	TSharedPtr<WidgetTextFilter> SearchBoxWidgetFilter;

	/** The source root widgets for the tree. */
	FDMXPixelMappingHierarchyItemWidgetModelArr RootWidgets;

	/** The root widgets which are actually displayed by the TreeView which will be managed by the TreeFilterHandler. */
	FDMXPixelMappingHierarchyItemWidgetModelArr TreeRootWidgets;

	/** Has a full refresh of the tree been requested?  This happens when the user is filtering the tree */
	bool bRefreshRequested;

	/** Is the tree in such a changed state that the whole widget needs rebuilding? */
	bool bRebuildTreeRequested;

	/** Flag to ignore selections while the hierarchy view is updating the selection. */
	bool bIsUpdatingSelection;

	TreeViewPtr WidgetTreeView;

	bool bSelectFirstRenderer;
};
