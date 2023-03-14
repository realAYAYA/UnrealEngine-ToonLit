// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AssetRegistry/AssetData.h"
#include "AssetManagerEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/FilterCollection.h"
#include "EdGraph_ReferenceViewer.generated.h"

class FAssetThumbnailPool;
class UEdGraphNode_Reference;
class SReferenceViewer;
class UReferenceViewerSettings;
enum class EDependencyPinCategory;

/*
*  Holds asset information for building reference graph
*/ 
struct FReferenceNodeInfo
{
	FAssetIdentifier AssetId;

	FAssetData AssetData;

	// immediate children (references or dependencies)
	TArray<TPair<FAssetIdentifier, EDependencyPinCategory>> Children;

	// this node's parent references (how it got included)
	TArray<FAssetIdentifier> Parents;

	// Which direction.  Referencers are left (other assets that depend on me), Dependencies are right (other assets I depend on)
	bool bReferencers;

	int32 OverflowCount;

	// Denote when all children have been manually expanded and the breadth limit should be ignored
	bool bExpandAllChildren;

	FReferenceNodeInfo(const FAssetIdentifier& InAssetId, bool InbReferencers);

	bool IsFirstParent(const FAssetIdentifier& InParentId) const;

	bool IsADuplicate() const;

	// The Provision Size, or vertical spacing required for layout, for a given parent.  
	// At the time of writing, the intent is only the first node manifestation of 
	// an asset will have its children shown
	int32 ProvisionSize(const FAssetIdentifier& InParentId) const;

	// how many nodes worth of children require vertical spacing 
	int32 ChildProvisionSize;

	// Whether or not this nodeinfo passed the current filters 
	bool PassedFilters;

};


UCLASS()
class ASSETMANAGEREDITOR_API UEdGraph_ReferenceViewer : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:
	// UObject implementation
	virtual void BeginDestroy() override;
	// End UObject implementation

	/** Set reference viewer to focus on these assets */
	void SetGraphRoot(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin = FIntPoint(ForceInitToZero));

	/** Returns list of currently focused assets */
	const TArray<FAssetIdentifier>& GetCurrentGraphRootIdentifiers() const;

	/** If you're extending the reference viewer via GetAllGraphEditorContextMenuExtender you can use this to get the list of selected assets to use in your menu extender */
	bool GetSelectedAssetsForMenuExtender(const class UEdGraphNode* Node, TArray<FAssetIdentifier>& SelectedAssets) const;

	/** Accessor for the thumbnail pool in this graph */
	const TSharedPtr<class FAssetThumbnailPool>& GetAssetThumbnailPool() const;

	/** Force the graph to rebuild */
	class UEdGraphNode_Reference* RebuildGraph();

	/** Refilters the nodes, more efficient that a full rebuild.  This function is preferred when the assets, reference types or depth hasn't changed, meaning the NodeInfos didn't change, just 
	 * the presentation or filtering */
	class UEdGraphNode_Reference* RefilterGraph();

	using FIsAssetIdentifierPassingSearchFilterCallback = TFunction<bool(const FAssetIdentifier&)>;
	void SetIsAssetIdentifierPassingSearchFilterCallback(const TOptional<FIsAssetIdentifierPassingSearchFilterCallback>& InIsAssetIdentifierPassingSearchFilterCallback) { IsAssetIdentifierPassingSearchFilterCallback = InIsAssetIdentifierPassingSearchFilterCallback; }

	FName GetCurrentCollectionFilter() const;
	void SetCurrentCollectionFilter(FName NewFilter);

	/* Delegate type to notify when the assets or NodeInfos have changed as opposed to when the filters changed */
	FSimpleMulticastDelegate& OnAssetsChanged() { return OnAssetsChangedDelegate; }

	/* Not to be confused with the above Content Browser Collection name, this is a TFiltercollection, a list of active filters */
	void SetCurrentFilterCollection(TSharedPtr< TFilterCollection<FReferenceNodeInfo&> > NewFilterCollection);

	/* Returns a set of unique asset types as UClass* */
	const TSet<FTopLevelAssetPath>& GetAssetTypes() const { return CurrentClasses; }

	/* Returns true if the current graph has overflow nodes */
	bool BreadthLimitExceeded() const { return bBreadthLimitReached; };

private:
	void SetReferenceViewer(TSharedPtr<SReferenceViewer> InViewer);
	UEdGraphNode_Reference* ConstructNodes(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin);


	bool ExceedsMaxSearchDepth(int32 Depth, int32 MaxDepth) const;
	bool ExceedsMaxSearchBreadth(int32 Breadth) const;
	FAssetManagerDependencyQuery GetReferenceSearchFlags(bool bHardOnly) const;

	UEdGraphNode_Reference* CreateReferenceNode();

	/* Generates a NodeInfo structure then used to generate and layout the graph nodes */
	void RecursivelyPopulateNodeInfos(bool bReferencers, const TArray<FAssetIdentifier>& Identifiers, TMap<FAssetIdentifier, FReferenceNodeInfo>& NodeInfos, int32 CurrentDepth, int32 MaxDepth);

	/* Marks up the NodeInfos with updated filter information and provision sizes */
	void RecursivelyFilterNodeInfos(const FAssetIdentifier& InAssetId, TMap<FAssetIdentifier, FReferenceNodeInfo>& NodeInfos, int32 CurrentDepth, int32 MaxDepth);

	/* Searches for the AssetData for the list of packages derived from the AssetReferences  */
	void GatherAssetData(const TSet<FName>& AllPackageNames, TMap<FName, FAssetData>& OutPackageToAssetDataMap) const;

	/* Uses the NodeInfos map to generate and layout the graph nodes */
	UEdGraphNode_Reference* RecursivelyCreateNodes(
		bool bInReferencers, 
		const FAssetIdentifier& InAssetId, 
		const FIntPoint& InNodeLoc, 
		const FAssetIdentifier& InParentId, 
		UEdGraphNode_Reference* InParentNode, 
		TMap<FAssetIdentifier, FReferenceNodeInfo>& InNodeInfos, 
		int32 InCurrentDepth, 
		int32 InMaxDepth, 
		bool bIsRoot = false
	);

	void ExpandNode(bool bReferencers, const FAssetIdentifier& InAssetIdentifier);

	/** Removes all nodes from the graph */
	void RemoveAllNodes();

	/** Returns true if filtering is enabled and we have a valid collection */
	bool ShouldFilterByCollection() const;

	void GetSortedLinks(const TArray<FAssetIdentifier>& Identifiers, bool bReferencers, const FAssetManagerDependencyQuery& Query, TMap<FAssetIdentifier, EDependencyPinCategory>& OutLinks) const;
	bool IsPackageIdentifierPassingFilter(const FAssetIdentifier& InAssetIdentifier) const;
	bool IsAssetPassingSearchTextFilter(const FAssetIdentifier& InAssetIdentifier) const;

private:
	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool;

	/** Editor for this pool */
	TWeakPtr<SReferenceViewer> ReferenceViewer;

	TArray<FAssetIdentifier> CurrentGraphRootIdentifiers;
	FIntPoint CurrentGraphRootOrigin;

	int32 MaxSearchBreadth;

	/** Stores if the breadth limit was reached on the last refilter*/
	bool bBreadthLimitReached;

	/** Current collection filter. NAME_None for no filter */
	FName CurrentCollectionFilter;

	/** A set of the unique class types referenced */
	TSet<FTopLevelAssetPath> CurrentClasses;

	/* This is a convenience toggle to switch between the old & new methods for computing & displaying the graph */
	bool bUseNodeInfos;

	/* Cached Reference Information used to quickly refilter */
	TMap<FAssetIdentifier, FReferenceNodeInfo> ReferencerNodeInfos;
	TMap<FAssetIdentifier, FReferenceNodeInfo> DependencyNodeInfos;

	TOptional<FIsAssetIdentifierPassingSearchFilterCallback> IsAssetIdentifierPassingSearchFilterCallback;

	/** List of packages the current collection filter allows */
	TSet<FName> CurrentCollectionPackages;

	/** Current filter collection */
	TSharedPtr< TFilterCollection<FReferenceNodeInfo & > > FilterCollection;

	UReferenceViewerSettings* Settings;

	/* A delegate to notify when the underlying assets changed (usually through a root or depth change) */
	FSimpleMulticastDelegate OnAssetsChangedDelegate;

	friend SReferenceViewer;
};
