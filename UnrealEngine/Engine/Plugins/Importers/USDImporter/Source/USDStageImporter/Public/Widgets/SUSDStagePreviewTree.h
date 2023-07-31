// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UsdWrappers/ForwardDeclarations.h"
#include "Widgets/IUSDTreeViewItem.h"
#include "Widgets/SUSDTreeView.h"

#include "Templates/SharedPointer.h"

using FUsdPrimPreviewModelViewRef = TSharedRef< struct FUsdPrimPreviewModelView >;
using FUsdPrimPreviewModelViewPtr = TSharedPtr< struct FUsdPrimPreviewModelView >;
using FUsdPrimPreviewModelViewWeak = TWeakPtr< struct FUsdPrimPreviewModelView >;

struct FUsdPrimPreviewModel
{
	// Prim's name
	FText Name;

	// Prim's type name (like "Mesh" or "Camera")
	FText TypeName;

	// Whether this prim should be imported or not
	bool bShouldImport = true;

	// Only used to make filtering items with SUsdStagePreviewTree::CurrentFilterText a bit faster: This is not
	// actually used to display anything
	bool bPassesFilter = true;

	// Our source of truth for expansion state. Whenever expansion changes on the views we write to this,
	// and whenever we rebuild our views we write this to the tree's sparse infos about its items
	bool bIsExpanded = true;

	int32 ParentIndex = INDEX_NONE;
	TArray<int32> ChildIndices;
};

struct FUsdPrimPreviewModelView : public IUsdTreeViewItem
{
	int32 ModelIndex = INDEX_NONE;

	FUsdPrimPreviewModelViewWeak Parent;
    TArray< FUsdPrimPreviewModelViewRef > Children;
};

class USDSTAGEIMPORTER_API SUsdStagePreviewTree : public SUsdTreeView< FUsdPrimPreviewModelViewRef >
{
public:
	SLATE_BEGIN_ARGS( SUsdStagePreviewTree ) {}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const UE::FUsdStage& InStage );

    TArray<FString> GetSelectedFullPrimPaths() const;

	void SetFilterText( const FText& NewText );
	const FText& GetFilterText() const { return CurrentFilterText; }

	FUsdPrimPreviewModel& GetModel( int32 ModelIndex ) { return ItemModels[ ModelIndex ]; }
	const FUsdPrimPreviewModel& GetModel( int32 ModelIndex ) const { return ItemModels[ ModelIndex ]; }

	void CheckItemsRecursively( const TArray<FUsdPrimPreviewModelViewRef>& Items, bool bCheck );
	void ExpandItemsRecursively( const TArray<FUsdPrimPreviewModelViewRef>& Items, bool bExpand );

private:
    // Begin SUsdTreeView interface
	virtual TSharedRef< ITableRow > OnGenerateRow(
		FUsdPrimPreviewModelViewRef InDisplayNode,
		const TSharedRef< STableViewBase >& OwnerTable
	) override;

	virtual void OnGetChildren(
		FUsdPrimPreviewModelViewRef InParent,
		TArray< FUsdPrimPreviewModelViewRef >& OutChildren
	) const override;

	virtual void SetupColumns() override;
    // End SUsdTreeView interface

	// Returns all selected model views, excluding child views of any other selected view.
	// i.e. if both a child and an ancestor view are selected, the returned array will contain only the ancestor.
	TArray<FUsdPrimPreviewModelViewRef> GetAncestorSelectedViews();

	TSharedPtr< SWidget > ConstructPrimContextMenu();

	// Rebuilds our RootItems array of FUsdPrimPreviewModelViewRefs based on CurrentFilterText and the ItemModels that
	// pass the filter
	void RebuildModelViews();

private:
	FText CurrentFilterText;

	TArray<FUsdPrimPreviewModel> ItemModels;
	TMap<int32, FUsdPrimPreviewModelViewRef> ItemModelsToViews;
};

// Custom row so that we can fetch the owning SUsdStagePreviewTree from the columns
class USDSTAGEIMPORTER_API SUsdStagePreviewTreeRow : public SUsdTreeRow< FUsdPrimPreviewModelViewRef >
{
public:
	SLATE_BEGIN_ARGS( SUsdStagePreviewTreeRow ) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		FUsdPrimPreviewModelViewRef InTreeItem,
		const TSharedRef< STableViewBase >& OwnerTable,
		TSharedPtr< FSharedUsdTreeData > InSharedData
	);

	TSharedPtr< SUsdStagePreviewTree > GetOwnerTree() const;
};