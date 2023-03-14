// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetailColumnSizeData.h"
#include "DetailWidgetRow.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"

class IDetailTreeNode;
class IPropertyRowGenerator;
class IPropertyHandle;
class ITableRow;
class FDetailTreeNode;
class FDetailWidgetRow;
class STableViewBase;

template<typename T>
class STreeView;

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnFilterDetailTreeNode, const TSharedRef<IDetailTreeNode>&);

/** A wrapper used to abstract FDetailTreeNode, which is a private abstract class defined in PropertyEditor/Private */
class FDisplayClusterColorGradingDetailTreeItem : public TSharedFromThis<FDisplayClusterColorGradingDetailTreeItem>
{
public:
	FDisplayClusterColorGradingDetailTreeItem(const TSharedPtr<FDetailTreeNode>& InDetailTreeNode)
		: DetailTreeNode(InDetailTreeNode)
	{ };

	/** Initializes the detail tree item, creating any child tree items needed */
	void Initialize(const FOnFilterDetailTreeNode& NodeFilter);

	/** Gets the parent detail tree item of this item */
	TWeakPtr<FDisplayClusterColorGradingDetailTreeItem> GetParent() const { return Parent; }

	/** Gets whether this tree item has any children */
	bool HasChildren() const { return Children.Num() > 0; }

	/** Gets the list of child tree items of this item */
	void GetChildren(TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>>& OutChildren) const;

	/** Gets the underlying IDetailTreeNode this detail tree item wraps */
	TWeakPtr<IDetailTreeNode> GetDetailTreeNode() const;

	/** Gets the property handle of the property this detail tree item represents */
	TSharedPtr<IPropertyHandle> GetPropertyHandle() const { return PropertyHandle; }

	/** Gets the name of this detail tree item */
	FName GetNodeName() const;

	/** Gets whether this detail tree item should be expanded */
	bool ShouldBeExpanded() const;

	/** Raised when this detail tree item's expansion state has been changed */
	void OnItemExpansionChanged(bool bIsExpanded, bool bShouldSaveState);

	/** Gets whether the "reset to default" button should be visible for this detail tree item */
	bool IsResetToDefaultVisible() const;

	/** Resets the property this detail tree item represents to its default value */
	void ResetToDefault();

	/** Gets an attribute that can be used to determine if property editing is enabled for this detail tree item */
	TAttribute<bool> IsPropertyEditingEnabled() const;

	/** Gets whether this detail tree item is a category */
	bool IsCategory() const;

	/** Gets whether this detail tree item is an item */
	bool IsItem() const;

	/** Gets whether this detail tree item can be reordered through a drag drop action */
	bool IsReorderable() const;

	/** Gets whether this detail tree item can be copied */
	bool IsCopyable() const;

	/** Generates the row widgets for this detail tree item */
	void GenerateDetailWidgetRow(FDetailWidgetRow& OutDetailWidgetRow) const;

private:
	/** A weak pointer to the detail tree node this item wraps */
	TWeakPtr<FDetailTreeNode> DetailTreeNode;

	/** The property handle for the property this item represents */
	TSharedPtr<IPropertyHandle> PropertyHandle;

	/** A weak pointer to this item's parent */
	TWeakPtr<FDisplayClusterColorGradingDetailTreeItem> Parent;

	/** A list of children of this item */
	TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>> Children;
};

/**
 * A custom detail view based on SDetailView that uses a property row generator as a source for the property nodes instead of generating them manually.
 * Using an existing property row generator allows the detail view to display an object's properties much faster than the ordinary SDetailView, which
 * has to regenerate a new property node tree every time the object being displayed is changed
 */
class SDisplayClusterColorGradingDetailView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDisplayClusterColorGradingDetailView) { }
		SLATE_ARGUMENT(TSharedPtr<IPropertyRowGenerator>, PropertyRowGeneratorSource)
		SLATE_EVENT(FOnFilterDetailTreeNode, OnFilterDetailTreeNode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~ SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget interface

	/** Regenerates this widget based on the current state of its property row generator source */
	void Refresh();

	/** Saves the expansion state of all properties being displayed in this detail view to the user's config file */
	void SaveExpandedItems();

	/** Restores the expansion state of all properties being displayed in this detail view from the user's config file */
	void RestoreExpandedItems();

private:
	/** Updates the detail tree using the current state of the property row generator source */
	void UpdateTreeNodes();

	/** Updates the expansion state of the specified tree item using the stored expansion state configuration */
	void UpdateExpansionState(const TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeItem);

	/** Sets the expansion state of the specified tree item, and optionally recursively sets the expansion state of its children */
	void SetNodeExpansionState(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeNode, bool bIsItemExpanded, bool bRecursive);

	/** Generates a table row widget for the specified tree item */
	TSharedRef<ITableRow> GenerateNodeRow(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeNode, const TSharedRef<STableViewBase>& OwnerTable);

	/** Gets a list of child tree items for the specified tree item */
	void GetChildrenForNode(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeNode, TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>>& OutChildren);

	/** Raised when the underlying tree widget is setting the expansion state of the specified tree item recursively */
	void OnSetExpansionRecursive(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeNode, bool bIsItemExpanded);

	/** Raised when the underlying tree widget is setting the expansion state of the specified tree item */
	void OnExpansionChanged(TSharedRef<FDisplayClusterColorGradingDetailTreeItem> InTreeNode, bool bIsItemExpanded);

	/** Raised when the underlying tree widget is releasing the specified table row */
	void OnRowReleased(const TSharedRef<ITableRow>& TableRow);

	/** Gets the visibility of the scrollbar for the detail view */
	EVisibility GetScrollBarVisibility() const;

private:
	typedef STreeView<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>> SDetailTree;

	/** The underlying tree view used to display the property widgets */
	TSharedPtr<SDetailTree> DetailTree;

	/** The source list of the root detail tree nodes being displayed by the tree widget */
	TArray<TSharedRef<FDisplayClusterColorGradingDetailTreeItem>> RootTreeNodes;

	/** The property row generator to generate the property widgets from */
	TSharedPtr<IPropertyRowGenerator> PropertyRowGeneratorSource;

	/** Column sizing data for the properties */
	FDetailColumnSizeData ColumnSizeData;

	/** A list of tree items whose expansion state needs to be set on the next tick */
	TMap<TWeakPtr<FDisplayClusterColorGradingDetailTreeItem>, bool> TreeItemsToSetExpansionState;

	/** A list of currently expanded detail nodes */
	TSet<FString> ExpandedDetailNodes;

	/** Delegate used to filter or process the detail tree nodes that are displayed in the detail view */
	FOnFilterDetailTreeNode OnFilterDetailTreeNode;
};