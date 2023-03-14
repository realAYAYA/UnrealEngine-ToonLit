// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewer/EdGraph_ReferenceViewer.h"
#include "ReferenceViewer/EdGraphNode_Reference.h"
#include "ReferenceViewer/ReferenceViewerSettings.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetThumbnail.h"
#include "SReferenceViewer.h"
#include "SReferenceNode.h"
#include "GraphEditor.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"
#include "AssetManagerEditorModule.h"
#include "Engine/AssetManager.h"
#include "Settings/EditorProjectSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EdGraph_ReferenceViewer)

FReferenceNodeInfo::FReferenceNodeInfo(const FAssetIdentifier& InAssetId, bool InbReferencers)
	: AssetId(InAssetId)
	, bReferencers(InbReferencers)
	, OverflowCount(0)
	, bExpandAllChildren(false)
	, ChildProvisionSize(0)
	, PassedFilters(true)
{}

bool FReferenceNodeInfo::IsFirstParent(const FAssetIdentifier& InParentId) const
{
	return Parents.IsEmpty() || Parents[0] == InParentId;
}

bool FReferenceNodeInfo::IsADuplicate() const
{
	return Parents.Num() > 1;
}

int32 FReferenceNodeInfo::ProvisionSize(const FAssetIdentifier& InParentId) const
{
	return IsFirstParent(InParentId) ? ChildProvisionSize : 1;
}

UEdGraph_ReferenceViewer::UEdGraph_ReferenceViewer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssetThumbnailPool = MakeShareable( new FAssetThumbnailPool(1024) );

	Settings = GetMutableDefault<UReferenceViewerSettings>();
}

void UEdGraph_ReferenceViewer::BeginDestroy()
{
	AssetThumbnailPool.Reset();

	Super::BeginDestroy();
}

void UEdGraph_ReferenceViewer::SetGraphRoot(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin)
{
	CurrentGraphRootIdentifiers = GraphRootIdentifiers;
	CurrentGraphRootOrigin = GraphRootOrigin;

	// If we're focused on a searchable name, enable that flag
	for (const FAssetIdentifier& AssetId : GraphRootIdentifiers)
	{
		if (AssetId.IsValue())
		{
			Settings->SetShowSearchableNames(true);
		}
		else if (AssetId.GetPrimaryAssetId().IsValid())
		{
			if (UAssetManager::IsValid())
			{
				UAssetManager::Get().UpdateManagementDatabase();
			}
			
			Settings->SetShowManagementReferencesEnabled(true);
		}
	}
}

const TArray<FAssetIdentifier>& UEdGraph_ReferenceViewer::GetCurrentGraphRootIdentifiers() const
{
	return CurrentGraphRootIdentifiers;
}

void UEdGraph_ReferenceViewer::SetReferenceViewer(TSharedPtr<SReferenceViewer> InViewer)
{
	ReferenceViewer = InViewer;
}

bool UEdGraph_ReferenceViewer::GetSelectedAssetsForMenuExtender(const class UEdGraphNode* Node, TArray<FAssetIdentifier>& SelectedAssets) const
{
	if (!ReferenceViewer.IsValid())
	{
		return false;
	}
	TSharedPtr<SGraphEditor> GraphEditor = ReferenceViewer.Pin()->GetGraphEditor();

	if (!GraphEditor.IsValid())
	{
		return false;
	}

	TSet<UObject*> SelectedNodes = GraphEditor->GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
	{
		if (UEdGraphNode_Reference* ReferenceNode = Cast<UEdGraphNode_Reference>(*It))
		{
			if (!ReferenceNode->IsCollapsed())
			{
				SelectedAssets.Add(ReferenceNode->GetIdentifier());
			}
		}
	}
	return true;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::RebuildGraph()
{
	RemoveAllNodes();
	UEdGraphNode_Reference* NewRootNode = ConstructNodes(CurrentGraphRootIdentifiers, CurrentGraphRootOrigin);
	NotifyGraphChanged();

	return NewRootNode;
}

FName UEdGraph_ReferenceViewer::GetCurrentCollectionFilter() const
{
	return CurrentCollectionFilter;
}

void UEdGraph_ReferenceViewer::SetCurrentCollectionFilter(FName NewFilter)
{
	CurrentCollectionFilter = NewFilter;
}

void UEdGraph_ReferenceViewer::SetCurrentFilterCollection(TSharedPtr< TFilterCollection< FReferenceNodeInfo& > > InFilterCollection )
{
	FilterCollection = InFilterCollection;
}

FAssetManagerDependencyQuery UEdGraph_ReferenceViewer::GetReferenceSearchFlags(bool bHardOnly) const
{
	using namespace UE::AssetRegistry;
	FAssetManagerDependencyQuery Query;
	Query.Categories = EDependencyCategory::None;
	Query.Flags = EDependencyQuery::NoRequirements;

	bool bLocalIsShowSoftReferences = Settings->IsShowSoftReferences() && !bHardOnly;
	if (bLocalIsShowSoftReferences || Settings->IsShowHardReferences())
	{
		Query.Categories |= EDependencyCategory::Package;
		Query.Flags |= bLocalIsShowSoftReferences ? EDependencyQuery::NoRequirements : EDependencyQuery::Hard;
		Query.Flags |= Settings->IsShowHardReferences() ? EDependencyQuery::NoRequirements : EDependencyQuery::Soft;
		Query.Flags |= Settings->IsShowEditorOnlyReferences() ? EDependencyQuery::NoRequirements : EDependencyQuery::Game;
	}
	if (Settings->IsShowSearchableNames() && !bHardOnly)
	{
		Query.Categories |= EDependencyCategory::SearchableName;
	}
	if (Settings->IsShowManagementReferences())
	{
		Query.Categories |= EDependencyCategory::Manage;
		Query.Flags |= bHardOnly ? EDependencyQuery::Direct : EDependencyQuery::NoRequirements;
	}

	return Query;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::ConstructNodes(const TArray<FAssetIdentifier>& GraphRootIdentifiers, const FIntPoint& GraphRootOrigin )
{
	UEdGraphNode_Reference* RootNode = NULL;
	if (GraphRootIdentifiers.Num() > 0)
	{
		// It both were false, nothing (other than the GraphRootIdentifiers) would be displayed
		check(Settings->IsShowReferencers() || Settings->IsShowDependencies());

		// Refresh the current collection filter
		CurrentCollectionPackages.Empty();
		if (ShouldFilterByCollection())
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			TArray<FSoftObjectPath> AssetPaths;
			CollectionManagerModule.Get().GetAssetsInCollection(CurrentCollectionFilter, ECollectionShareType::CST_All, AssetPaths);
			CurrentCollectionPackages.Reserve(AssetPaths.Num());
			for (const FSoftObjectPath& AssetPath  : AssetPaths)
			{
				CurrentCollectionPackages.Add(AssetPath.GetLongPackageFName());
			}
		}

		// Create & Populate the NodeInfo Maps 
		// Note to add an empty parent to the root so that if the root node again gets found again as a duplicate, that next parent won't be 
		// identified as the primary root and also it will appear as having multiple parents.
		TMap<FAssetIdentifier, FReferenceNodeInfo> NewReferenceNodeInfos;
		FReferenceNodeInfo& RootNodeInfo = NewReferenceNodeInfos.FindOrAdd( GraphRootIdentifiers[0], FReferenceNodeInfo(GraphRootIdentifiers[0], true));
		RootNodeInfo.Parents.Emplace(FAssetIdentifier(NAME_None));
		RecursivelyPopulateNodeInfos(true, GraphRootIdentifiers, NewReferenceNodeInfos, 0, Settings->GetSearchReferencerDepthLimit());

		TMap<FAssetIdentifier, FReferenceNodeInfo> NewDependencyNodeInfos;
		FReferenceNodeInfo& DRootNodeInfo = NewDependencyNodeInfos.FindOrAdd( GraphRootIdentifiers[0], FReferenceNodeInfo(GraphRootIdentifiers[0], false));
		DRootNodeInfo.Parents.Emplace(FAssetIdentifier(NAME_None));
		RecursivelyPopulateNodeInfos(false, GraphRootIdentifiers, NewDependencyNodeInfos, 0, Settings->GetSearchDependencyDepthLimit());

		TSet<FName> AllPackageNames;
		auto AddPackage = [](const FAssetIdentifier& AssetId, TSet<FName>& PackageNames)
		{ 
			// Only look for asset data if this is a package
			if (!AssetId.IsValue() && !AssetId.PackageName.IsNone())
			{
				PackageNames.Add(AssetId.PackageName);
			}
		};

		if (Settings->IsShowReferencers())
		{
			for (TPair<FAssetIdentifier, FReferenceNodeInfo>&  InfoPair : NewReferenceNodeInfos)
			{
				AddPackage(InfoPair.Key, AllPackageNames);
			}
		}

		if (Settings->IsShowDependencies())
		{
			for (TPair<FAssetIdentifier, FReferenceNodeInfo>&  InfoPair : NewDependencyNodeInfos)
			{
				AddPackage(InfoPair.Key, AllPackageNames);
			}
		}

		// Store the AssetData in the NodeInfos
		TMap<FName, FAssetData> PackagesToAssetDataMap;
		GatherAssetData(AllPackageNames, PackagesToAssetDataMap);

		// Store the AssetData in the NodeInfos and collect Asset Type UClasses to populate the filters
		TSet<FTopLevelAssetPath> AllClasses;
		for (TPair<FAssetIdentifier, FReferenceNodeInfo>&  InfoPair : NewReferenceNodeInfos)
		{
			InfoPair.Value.AssetData = PackagesToAssetDataMap.FindRef(InfoPair.Key.PackageName);
			if (InfoPair.Value.AssetData.IsValid())
			{
				AllClasses.Add(InfoPair.Value.AssetData.AssetClassPath);
			}
		}

		for (TPair<FAssetIdentifier, FReferenceNodeInfo>&  InfoPair : NewDependencyNodeInfos)
		{
			InfoPair.Value.AssetData = PackagesToAssetDataMap.FindRef(InfoPair.Key.PackageName);
			if (InfoPair.Value.AssetData.IsValid())
			{
				AllClasses.Add(InfoPair.Value.AssetData.AssetClassPath);
			}
		}

		// Update the cached class types list
		CurrentClasses = AllClasses;
		OnAssetsChangedDelegate.Broadcast();

		ReferencerNodeInfos = NewReferenceNodeInfos;
		DependencyNodeInfos = NewDependencyNodeInfos;
	}
	else
	{
		ReferencerNodeInfos.Empty();
		DependencyNodeInfos.Empty();
	}

	return RefilterGraph();
}


UEdGraphNode_Reference* UEdGraph_ReferenceViewer::RefilterGraph()
{
	RemoveAllNodes();
	UEdGraphNode_Reference* RootNode = NULL;

	bBreadthLimitReached = false;
	if (CurrentGraphRootIdentifiers.Num() > 0 && (!ReferencerNodeInfos.IsEmpty() || !DependencyNodeInfos.IsEmpty()))
	{
		FAssetIdentifier FirstGraphRootIdentifier = CurrentGraphRootIdentifiers[0];

		// Create the root node
		bool bRootIsDuplicated = (Settings->IsShowDependencies() && DependencyNodeInfos.Contains(FirstGraphRootIdentifier) && DependencyNodeInfos[FirstGraphRootIdentifier].IsADuplicate()) || 
								  (Settings->IsShowReferencers() && ReferencerNodeInfos.Contains(FirstGraphRootIdentifier) && ReferencerNodeInfos[FirstGraphRootIdentifier].IsADuplicate());
		const FReferenceNodeInfo& NodeInfo = Settings->IsShowReferencers() ? ReferencerNodeInfos[FirstGraphRootIdentifier] : DependencyNodeInfos[FirstGraphRootIdentifier];
		RootNode = CreateReferenceNode();
		RootNode->SetupReferenceNode(CurrentGraphRootOrigin, CurrentGraphRootIdentifiers, NodeInfo.AssetData, /*bInAllowThumbnail = */ !Settings->IsCompactMode(), /*bIsDuplicate*/ bRootIsDuplicated);
		RootNode->SetMakeCommentBubbleVisible(Settings->IsShowPath());

		if (Settings->IsShowReferencers())
		{
			RecursivelyFilterNodeInfos(FirstGraphRootIdentifier, ReferencerNodeInfos, 0, Settings->GetSearchReferencerDepthLimit());
			RecursivelyCreateNodes(true, FirstGraphRootIdentifier, CurrentGraphRootOrigin, FirstGraphRootIdentifier, RootNode, ReferencerNodeInfos, 0, Settings->GetSearchReferencerDepthLimit(), /*bIsRoot*/ true);
		}

		if (Settings->IsShowDependencies())
		{
			RecursivelyFilterNodeInfos(FirstGraphRootIdentifier, DependencyNodeInfos, 0, Settings->GetSearchDependencyDepthLimit());
			RecursivelyCreateNodes(false, FirstGraphRootIdentifier, CurrentGraphRootOrigin, FirstGraphRootIdentifier, RootNode, DependencyNodeInfos, 0, Settings->GetSearchDependencyDepthLimit(), /*bIsRoot*/ true);
		}

	}

	NotifyGraphChanged();
	return RootNode;
}

void UEdGraph_ReferenceViewer::RecursivelyFilterNodeInfos(const FAssetIdentifier& InAssetId, TMap<FAssetIdentifier, FReferenceNodeInfo>& InNodeInfos, int32 InCurrentDepth, int32 InMaxDepth)
{
	// Filters and Reprovisions the NodeInfo counts 
	int32 NewProvisionSize = 0;

	int32 Breadth = 0;

	InNodeInfos[InAssetId].OverflowCount = 0;
	if (InMaxDepth > 0 && InCurrentDepth < InMaxDepth)
	{
		for (const TPair<FAssetIdentifier, EDependencyPinCategory>& Pair : InNodeInfos[InAssetId].Children)
		{
			FAssetIdentifier ChildId = Pair.Key;

			int32 ChildProvSize = 0;
			if (InNodeInfos[ChildId].IsFirstParent(InAssetId))
			{
				RecursivelyFilterNodeInfos(ChildId, InNodeInfos, InCurrentDepth + 1, InMaxDepth);
				ChildProvSize = InNodeInfos[ChildId].ProvisionSize(InAssetId);
			}
			else if (InNodeInfos[ChildId].PassedFilters && Settings->IsShowDuplicates())
			{
				ChildProvSize = 1;
			}

			if (ChildProvSize > 0)
			{
				if (!ExceedsMaxSearchBreadth(Breadth) || InNodeInfos[InAssetId].bExpandAllChildren)
				{
					NewProvisionSize += ChildProvSize;
					Breadth++;
				}

				else
				{
					InNodeInfos[InAssetId].OverflowCount++;
					Breadth++;
				}
			}
		}
	}

	// Account for an overflow node if necessary
	if (InNodeInfos[InAssetId].OverflowCount > 0)
	{
		NewProvisionSize++;
		bBreadthLimitReached = true;
	}

	bool PassedAssetTypeFilter = FilterCollection && Settings->GetFiltersEnabled() ? FilterCollection->PassesAllFilters(InNodeInfos[InAssetId]) : true;
	bool PassedSearchTextFilter = IsAssetPassingSearchTextFilter(InAssetId);

	bool PassedAllFilters = PassedAssetTypeFilter && PassedSearchTextFilter;	

	InNodeInfos[InAssetId].ChildProvisionSize = NewProvisionSize > 0 ? NewProvisionSize : (PassedAllFilters ? 1 : 0);
	InNodeInfos[InAssetId].PassedFilters = PassedAllFilters;
}

void UEdGraph_ReferenceViewer::GetSortedLinks(const TArray<FAssetIdentifier>& Identifiers, bool bReferencers, const FAssetManagerDependencyQuery& Query, TMap<FAssetIdentifier, EDependencyPinCategory>& OutLinks) const
{
	using namespace UE::AssetRegistry;
	auto CategoryOrder = [](EDependencyCategory InCategory)
	{
		switch (InCategory)
		{
			case EDependencyCategory::Package: 
			{
				return 0;
			}
			case EDependencyCategory::Manage:
			{
				return 1;
			}
			case EDependencyCategory::SearchableName: 
			{
				return 2;
			}
			default: 
			{
				check(false);
				return 3;
			}
		}
	};
	auto IsHard = [](EDependencyProperty Properties)
	{
		return static_cast<bool>(((Properties & EDependencyProperty::Hard) != EDependencyProperty::None) | ((Properties & EDependencyProperty::Direct) != EDependencyProperty::None));
	};

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetDependency> LinksToAsset;
	for (const FAssetIdentifier& AssetId : Identifiers)
	{
		LinksToAsset.Reset();
		if (bReferencers)
		{
			AssetRegistry.GetReferencers(AssetId, LinksToAsset, Query.Categories, Query.Flags);
		}
		else
		{
			AssetRegistry.GetDependencies(AssetId, LinksToAsset, Query.Categories, Query.Flags);
		}

		// Sort the links from most important kind of link to least important kind of link, so that if we can't display them all in an ExceedsMaxSearchBreadth test, we
		// show the most important links.
		Algo::Sort(LinksToAsset, [&CategoryOrder, &IsHard](const FAssetDependency& A, const FAssetDependency& B)
			{
				if (A.Category != B.Category)
				{
					return CategoryOrder(A.Category) < CategoryOrder(B.Category);
				}
				if (A.Properties != B.Properties)
				{
					bool bAIsHard = IsHard(A.Properties);
					bool bBIsHard = IsHard(B.Properties);
					if (bAIsHard != bBIsHard)
					{
						return bAIsHard;
					}
				}
				return A.AssetId.PackageName.LexicalLess(B.AssetId.PackageName);
			});
		for (FAssetDependency LinkToAsset : LinksToAsset)
		{
			EDependencyPinCategory& Category = OutLinks.FindOrAdd(LinkToAsset.AssetId, EDependencyPinCategory::LinkEndActive);
			bool bIsHard = IsHard(LinkToAsset.Properties);
			bool bIsUsedInGame = (LinkToAsset.Category != EDependencyCategory::Package) | ((LinkToAsset.Properties & EDependencyProperty::Game) != EDependencyProperty::None);
			Category |= EDependencyPinCategory::LinkEndActive;
			Category |= bIsHard ? EDependencyPinCategory::LinkTypeHard : EDependencyPinCategory::LinkTypeNone;
			Category |= bIsUsedInGame ? EDependencyPinCategory::LinkTypeUsedInGame : EDependencyPinCategory::LinkTypeNone;
		}
	}

	// Check filters and Filter for our registry source
	TArray<FAssetIdentifier> ReferenceIds;
	OutLinks.GenerateKeyArray(ReferenceIds);
	IAssetManagerEditorModule::Get().FilterAssetIdentifiersForCurrentRegistrySource(ReferenceIds, GetReferenceSearchFlags(false), !bReferencers);

	for (TMap<FAssetIdentifier, EDependencyPinCategory>::TIterator It(OutLinks); It; ++It)
	{
		if (!IsPackageIdentifierPassingFilter(It.Key()))
		{
			It.RemoveCurrent();
		}

		else if (!ReferenceIds.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}

		// Collection Filter
		else if (ShouldFilterByCollection() && It.Key().IsPackage() && !CurrentCollectionPackages.Contains(It.Key().PackageName))
		{
			It.RemoveCurrent();
		}
	}
}

bool UEdGraph_ReferenceViewer::IsPackageIdentifierPassingFilter(const FAssetIdentifier& InAssetIdentifier) const
{
	if (!InAssetIdentifier.IsValue())
	{
		if (!Settings->IsShowCodePackages() && InAssetIdentifier.PackageName.ToString().StartsWith(TEXT("/Script")))
		{
			return false;
		}

	}

	return true;
}

bool UEdGraph_ReferenceViewer::IsAssetPassingSearchTextFilter(const FAssetIdentifier& InAssetIdentifier) const
{
	if (Settings->IsShowFilteredPackagesOnly() && IsAssetIdentifierPassingSearchFilterCallback.IsSet() && !(*IsAssetIdentifierPassingSearchFilterCallback)(InAssetIdentifier))
	{
		return false;
	}

	return true;
}

void
UEdGraph_ReferenceViewer::RecursivelyPopulateNodeInfos(bool bInReferencers, const TArray<FAssetIdentifier>& Identifiers, TMap<FAssetIdentifier, FReferenceNodeInfo>& InNodeInfos, int32 InCurrentDepth, int32 InMaxDepth)
{
	check(Identifiers.Num() > 0);
	int32 ProvisionSize = 0;
	const FAssetIdentifier& InAssetId = Identifiers[0];
	if (InMaxDepth > 0 && InCurrentDepth < InMaxDepth)
	{
		TMap<FAssetIdentifier, EDependencyPinCategory> ReferenceLinks;
		GetSortedLinks(Identifiers, bInReferencers, GetReferenceSearchFlags(false), ReferenceLinks);

		InNodeInfos[InAssetId].Children.Reserve(ReferenceLinks.Num());
		for (const TPair<FAssetIdentifier, EDependencyPinCategory>& Pair : ReferenceLinks)
		{
			FAssetIdentifier ChildId = Pair.Key;
			if (!InNodeInfos.Contains(ChildId))
			{
				FReferenceNodeInfo& NewNodeInfo = InNodeInfos.FindOrAdd(ChildId, FReferenceNodeInfo(ChildId, bInReferencers));
				InNodeInfos[ChildId].Parents.Emplace(InAssetId);
				InNodeInfos[InAssetId].Children.Emplace(Pair);

				RecursivelyPopulateNodeInfos(bInReferencers, { ChildId }, InNodeInfos, InCurrentDepth + 1, InMaxDepth);
				ProvisionSize += InNodeInfos[ChildId].ProvisionSize(InAssetId);
			}

			else if (!InNodeInfos[ChildId].Parents.Contains(InAssetId))
			{
				InNodeInfos[ChildId].Parents.Emplace(InAssetId);
				InNodeInfos[InAssetId].Children.Emplace(Pair);
				ProvisionSize += 1;
			}
		}
	}

	// Account for an overflow node if necessary
	if (InNodeInfos[InAssetId].OverflowCount > 0)
	{
		ProvisionSize++;
	}

	InNodeInfos[InAssetId].ChildProvisionSize = ProvisionSize > 0 ? ProvisionSize : 1;
}

void UEdGraph_ReferenceViewer::GatherAssetData(const TSet<FName>& AllPackageNames, TMap<FName, FAssetData>& OutPackageToAssetDataMap) const
{
	UE::AssetRegistry::GetAssetForPackages(AllPackageNames.Array(), OutPackageToAssetDataMap);
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::RecursivelyCreateNodes(bool bInReferencers, const FAssetIdentifier& InAssetId, const FIntPoint& InNodeLoc, const FAssetIdentifier& InParentId, UEdGraphNode_Reference* InParentNode, TMap<FAssetIdentifier, FReferenceNodeInfo>& InNodeInfos, int32 InCurrentDepth, int32 InMaxDepth, bool bIsRoot)
{
	check(InNodeInfos.Contains(InAssetId));

	const FReferenceNodeInfo& NodeInfo = InNodeInfos[InAssetId];
	int32 NodeProvSize = 1;

	UEdGraphNode_Reference* NewNode = NULL;
	if (bIsRoot)
	{
		NewNode = InParentNode;
		NodeProvSize = NodeInfo.ProvisionSize(FAssetIdentifier(NAME_None));
	}
	else
	{
		NewNode = CreateReferenceNode();
		NewNode->SetupReferenceNode(InNodeLoc, {InAssetId}, NodeInfo.AssetData, /*bInAllowThumbnail*/ !Settings->IsCompactMode() && NodeInfo.PassedFilters, /*bIsADuplicate*/ NodeInfo.Parents.Num() > 1);
		NewNode->SetMakeCommentBubbleVisible(Settings->IsShowPath());
		NewNode->SetIsFiltered(!NodeInfo.PassedFilters);
		NodeProvSize = NodeInfo.ProvisionSize(InParentId);
	}

	bool bIsFirstOccurance = bIsRoot || NodeInfo.IsFirstParent(InParentId);
	FIntPoint ChildLoc = InNodeLoc;
	if (InMaxDepth > 0 && (InCurrentDepth < InMaxDepth) && bIsFirstOccurance) // Only expand the first parent
	{

		// position the children nodes
		const int32 ColumnWidth = Settings->IsCompactMode() ? 500 : 800;
		ChildLoc.X += bInReferencers ? -ColumnWidth : ColumnWidth;

		int32 NodeSizeY = Settings->IsCompactMode() ? 100 : 200;
		NodeSizeY += Settings->IsShowPath() ? 40 : 0;

		ChildLoc.Y -= (NodeProvSize - 1) * NodeSizeY * 0.5 ;

		int32 Breadth = 0;

		for (const TPair<FAssetIdentifier, EDependencyPinCategory>& Pair : InNodeInfos[InAssetId].Children)
		{
			if (ExceedsMaxSearchBreadth(Breadth) && !InNodeInfos[InAssetId].bExpandAllChildren)
			{
				break;
			}

		    FAssetIdentifier ChildId = Pair.Key;
		    int32 ChildProvSize = 0;
		    if (InNodeInfos[ChildId].IsFirstParent(InAssetId))
		   	{
		   		ChildProvSize = InNodeInfos[ChildId].ProvisionSize(InAssetId);
		   	}
		   	else if (InNodeInfos[ChildId].PassedFilters && Settings->IsShowDuplicates())
		   	{
		   		ChildProvSize = 1;
		   	}

		    // The provision size will always be at least 1 if it should be shown, factoring in filters, breadth, duplicates, etc.
		   	if (ChildProvSize > 0)
		    {
				ChildLoc.Y += (ChildProvSize - 1) * NodeSizeY * 0.5;

				UEdGraphNode_Reference* ChildNode = RecursivelyCreateNodes(bInReferencers, ChildId, ChildLoc, InAssetId, NewNode, InNodeInfos, InCurrentDepth + 1, InMaxDepth);	

				if (bInReferencers)
				{
					ChildNode->GetDependencyPin()->PinType.PinCategory = ::GetName(Pair.Value);
					NewNode->AddReferencer( ChildNode );
				}
				else
				{
					ChildNode->GetReferencerPin()->PinType.PinCategory = ::GetName(Pair.Value);
					ChildNode->AddReferencer( NewNode );
				}

				ChildLoc.Y += NodeSizeY * (ChildProvSize + 1) * 0.5;
				Breadth ++;
		    }
		}

		// There were more references than allowed to be displayed. Make a collapsed node.
		if (NodeInfo.OverflowCount > 0)
		{
			UEdGraphNode_Reference* OverflowNode = CreateReferenceNode();
			FIntPoint RefNodeLoc;
			RefNodeLoc.X = ChildLoc.X;
			RefNodeLoc.Y = ChildLoc.Y;

			if ( ensure(OverflowNode) )
			{
				OverflowNode->SetAllowThumbnail(!Settings->IsCompactMode());
				OverflowNode->SetReferenceNodeCollapsed(RefNodeLoc, NodeInfo.OverflowCount);

				if ( bInReferencers )
				{
					NewNode->AddReferencer( OverflowNode );
				}
				else
				{
					OverflowNode->AddReferencer( NewNode );
				}
			}
		}
	}

	return NewNode;
}

void UEdGraph_ReferenceViewer::ExpandNode(bool bReferencers, const FAssetIdentifier& InAssetIdentifier)
{
	if (!bReferencers && DependencyNodeInfos.Contains(InAssetIdentifier))
	{
		DependencyNodeInfos[InAssetIdentifier].bExpandAllChildren = true;
		RefilterGraph();
	}

	else if (bReferencers && ReferencerNodeInfos.Contains(InAssetIdentifier))
	{
		ReferencerNodeInfos[InAssetIdentifier].bExpandAllChildren = true;
		RefilterGraph();
	}
}

const TSharedPtr<FAssetThumbnailPool>& UEdGraph_ReferenceViewer::GetAssetThumbnailPool() const
{
	return AssetThumbnailPool;
}

bool UEdGraph_ReferenceViewer::ExceedsMaxSearchDepth(int32 Depth, int32 MaxDepth) const
{
	// ExceedsMaxSearchDepth requires only greater (not equal than) because, even though the Depth is 1-based indexed (similarly to Breadth), the first index (index 0) corresponds to the root object 
	return Settings->IsSearchDepthLimited() && Depth > MaxDepth;
}

bool UEdGraph_ReferenceViewer::ExceedsMaxSearchBreadth(int32 Breadth) const
{
	// ExceedsMaxSearchBreadth requires greater or equal than because the Breadth is 1-based indexed
	return Settings->IsSearchBreadthLimited() && (Breadth >=  Settings->GetSearchBreadthLimit());
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::CreateReferenceNode()
{
	const bool bSelectNewNode = false;
	return Cast<UEdGraphNode_Reference>(CreateNode(UEdGraphNode_Reference::StaticClass(), bSelectNewNode));
}

void UEdGraph_ReferenceViewer::RemoveAllNodes()
{
	TArray<UEdGraphNode*> NodesToRemove = Nodes;
	for (int32 NodeIndex = 0; NodeIndex < NodesToRemove.Num(); ++NodeIndex)
	{
		RemoveNode(NodesToRemove[NodeIndex]);
	}
}

bool UEdGraph_ReferenceViewer::ShouldFilterByCollection() const
{
	return Settings->GetEnableCollectionFilter() && CurrentCollectionFilter != NAME_None;
}

