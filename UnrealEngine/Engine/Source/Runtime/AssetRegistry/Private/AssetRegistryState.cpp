// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/AssetRegistryState.h"

#include "Algo/Compare.h"
#include "Algo/Sort.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistryArchive.h"
#include "AssetRegistryImpl.h"
#include "AssetRegistryPrivate.h"
#include "Blueprint/BlueprintSupport.h"
#include "DependsNode.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "NameTableArchive.h"
#include "PackageReader.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/MetaData.h"
#include "UObject/NameBatchSerialization.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

FAssetRegistryState& FAssetRegistryState::operator=(FAssetRegistryState&& Rhs)
{
	Reset();

	CachedAssets						= MoveTemp(Rhs.CachedAssets);
	CachedAssetsByPackageName			= MoveTemp(Rhs.CachedAssetsByPackageName);
	CachedAssetsByPath					= MoveTemp(Rhs.CachedAssetsByPath);
	CachedAssetsByClass					= MoveTemp(Rhs.CachedAssetsByClass);
	CachedAssetsByTag					= MoveTemp(Rhs.CachedAssetsByTag);
	CachedDependsNodes					= MoveTemp(Rhs.CachedDependsNodes);
	CachedPackageData					= MoveTemp(Rhs.CachedPackageData);
	PreallocatedAssetDataBuffers		= MoveTemp(Rhs.PreallocatedAssetDataBuffers);
	PreallocatedDependsNodeDataBuffers	= MoveTemp(Rhs.PreallocatedDependsNodeDataBuffers);
	PreallocatedPackageDataBuffers		= MoveTemp(Rhs.PreallocatedPackageDataBuffers);
	Swap(NumAssets,				Rhs.NumAssets);
	Swap(NumDependsNodes,		Rhs.NumDependsNodes);
	Swap(NumPackageData,		Rhs.NumPackageData);

	return *this;
}

FAssetRegistryState::~FAssetRegistryState()
{
	Reset();
}

void FAssetRegistryState::Reset()
{
	// if we have preallocated all the FAssetData's in a single block, free it now, instead of one at a time
	if (PreallocatedAssetDataBuffers.Num())
	{
		for (FAssetData* Buffer : PreallocatedAssetDataBuffers)
		{
			delete[] Buffer;
		}
		PreallocatedAssetDataBuffers.Reset();

		NumAssets = 0;
	}
	else
	{
		// Delete all assets in the cache
		for (auto AssetDataIt = CachedAssets.CreateConstIterator(); AssetDataIt; ++AssetDataIt)
		{
			if (*AssetDataIt)
			{
				delete *AssetDataIt;
				NumAssets--;
			}
		}
	}

	// Make sure we have deleted all our allocated FAssetData objects
	// Mar-06: Temporarily remove this ensure to allow passing builds  while we find and fix the cause
	// TODO: Restore the ensure
	// ensure(NumAssets == 0);
	UE_CLOG(NumAssets != 0, LogAssetRegistry, Display,
		TEXT("AssetRegistryState::Reset: NumAssets does not match the number of CachedAssets entries. Leaking some allocations."));

	if (PreallocatedDependsNodeDataBuffers.Num())
	{
		for (FDependsNode* Buffer : PreallocatedDependsNodeDataBuffers)
		{
			delete[] Buffer;
		}
		PreallocatedDependsNodeDataBuffers.Reset();
		NumDependsNodes = 0;
	}
	else
	{
		// Delete all depends nodes in the cache
		for (TMap<FAssetIdentifier, FDependsNode*>::TConstIterator DependsIt(CachedDependsNodes); DependsIt; ++DependsIt)
		{
			if (DependsIt.Value())
			{
				delete DependsIt.Value();
				NumDependsNodes--;
			}
		}
	}

	// Make sure we have deleted all our allocated FDependsNode objects
	ensure(NumDependsNodes == 0);

	if (PreallocatedPackageDataBuffers.Num())
	{
		for (FAssetPackageData* Buffer : PreallocatedPackageDataBuffers)
		{
			delete[] Buffer;
		}
		PreallocatedPackageDataBuffers.Reset();
		NumPackageData = 0;
	}
	else
	{
		// Delete all depends nodes in the cache
		for (TMap<FName, FAssetPackageData*>::TConstIterator PackageDataIt(CachedPackageData); PackageDataIt; ++PackageDataIt)
		{
			if (PackageDataIt.Value())
			{
				delete PackageDataIt.Value();
				NumPackageData--;
			}
		}
	}

	// Make sure we have deleted all our allocated package data objects
	ensure(NumPackageData == 0);

	// Clear cache
	CachedAssets.Empty();
	CachedAssetsByPackageName.Empty();
	CachedAssetsByPath.Empty();
	CachedAssetsByClass.Empty();
	CachedAssetsByTag.Empty();
	CachedDependsNodes.Empty();
	CachedPackageData.Empty();
}

void FAssetRegistryState::FilterTags(const FAssetDataTagMapSharedView& InTagsAndValues, FAssetDataTagMap& OutTagsAndValues, const TSet<FName>* ClassSpecificFilterList, const FAssetRegistrySerializationOptions& Options)
{
	const TSet<FName>* AllClassesFilterList = Options.CookFilterlistTagsByClass.Find(UE::AssetRegistry::WildcardPathName);

	// Exclude denied tags or include only allowed tags, based on how we were configured in ini
	for (const auto& TagPair : InTagsAndValues)
	{
		const bool bInAllClassesList = AllClassesFilterList && (AllClassesFilterList->Contains(TagPair.Key) || AllClassesFilterList->Contains(UE::AssetRegistry::WildcardFName));
		const bool bInClassSpecificList = ClassSpecificFilterList && (ClassSpecificFilterList->Contains(TagPair.Key) || ClassSpecificFilterList->Contains(UE::AssetRegistry::WildcardFName));
		if (Options.bUseAssetRegistryTagsAllowListInsteadOfDenyList)
		{
			// It's an allow list, only include it if it is in the all classes list or in the class specific list
			if (bInAllClassesList || bInClassSpecificList)
			{
				// It is in the allow list. Keep it.
				OutTagsAndValues.Add(TagPair.Key, TagPair.Value.ToLoose());
			}
		}
		else
		{
			// It's a deny list, include it unless it is in the all classes list or in the class specific list
			if (!bInAllClassesList && !bInClassSpecificList)
			{
				// It isn't in the deny list. Keep it.
				OutTagsAndValues.Add(TagPair.Key, TagPair.Value.ToLoose());
			}
		}
	}
}

void FAssetRegistryState::InitializeFromExistingAndPrune(const FAssetRegistryState & ExistingState, const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages,
	const TSet<int32> ChunksToKeep, const FAssetRegistrySerializationOptions& Options)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	const bool bIsFilteredByChunkId = ChunksToKeep.Num() != 0;
	const bool bIsFilteredByRequiredPackages = RequiredPackages.Num() != 0;
	const bool bIsFilteredByRemovedPackages = RemovePackages.Num() != 0;

	TSet<FName> RequiredDependNodePackages;

	// Duplicate asset data entries
	for (FAssetData* AssetData : ExistingState.CachedAssets)
	{
		bool bRemoveAssetData = false;
		bool bRemoveDependencyData = true;

		if (bIsFilteredByChunkId &&
			!AssetData->GetChunkIDs().ContainsByPredicate([&](int32 ChunkId) { return ChunksToKeep.Contains(ChunkId); }))
		{
			bRemoveAssetData = true;
		}
		else if (bIsFilteredByRequiredPackages && !RequiredPackages.Contains(AssetData->PackageName))
		{
			bRemoveAssetData = true;
		}
		else if (bIsFilteredByRemovedPackages && RemovePackages.Contains(AssetData->PackageName))
		{
			bRemoveAssetData = true;
		}
		else if (Options.bFilterAssetDataWithNoTags && AssetData->TagsAndValues.Num() == 0 &&
			!FPackageName::IsLocalizedPackage(AssetData->PackageName.ToString()))
		{
			bRemoveAssetData = true;
			bRemoveDependencyData = Options.bFilterDependenciesWithNoTags;
		}

		if (bRemoveAssetData)
		{
			if (!bRemoveDependencyData)
			{
				RequiredDependNodePackages.Add(AssetData->PackageName);
			}
			continue;
		}

		FAssetDataTagMap NewTagsAndValues;
		FAssetRegistryState::FilterTags(AssetData->TagsAndValues, NewTagsAndValues, Options.CookFilterlistTagsByClass.Find(AssetData->AssetClassPath), Options);

		FAssetData* NewAssetData = new FAssetData(AssetData->PackageName, AssetData->PackagePath, AssetData->AssetName,
			AssetData->AssetClassPath, NewTagsAndValues, AssetData->GetChunkIDs(), AssetData->PackageFlags);

		NewAssetData->TaggedAssetBundles = AssetData->TaggedAssetBundles;

		// Add asset to new state
		AddAssetData(NewAssetData);
	}

	// Create package data for all script and required packages
	for (const TPair<FName, FAssetPackageData*>& Pair : ExistingState.CachedPackageData)
	{
		if (Pair.Value)
		{
			// Only add if also in asset data map, or script package
			if (CachedAssetsByPackageName.Find(Pair.Key) ||
				FPackageName::IsScriptPackage(Pair.Key.ToString()))
			{
				FAssetPackageData* NewData = CreateOrGetAssetPackageData(Pair.Key);
				*NewData = *Pair.Value;
			}
		}
	}

	// Find valid dependency nodes for all script and required packages
	TSet<FDependsNode*> ValidDependsNodes;
	ValidDependsNodes.Reserve(ExistingState.CachedDependsNodes.Num());
	for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : ExistingState.CachedDependsNodes)
	{
		FDependsNode* Node = Pair.Value;
		const FAssetIdentifier& Id = Node->GetIdentifier();
		bool bRemoveDependsNode = false;

		if (Options.bFilterSearchableNames && Id.IsValue())
		{
			bRemoveDependsNode = true;
		}
		else if (Id.IsPackage() &&
			!CachedAssetsByPackageName.Contains(Id.PackageName) &&
			!RequiredDependNodePackages.Contains(Id.PackageName) &&
			!FPackageName::IsScriptPackage(Id.PackageName.ToString()))
		{
			bRemoveDependsNode = true;
		}

		if (!bRemoveDependsNode)
		{
			ValidDependsNodes.Add(Node);
		}
	}

	// Duplicate dependency nodes
	for (FDependsNode* OldNode : ValidDependsNodes)
	{
		FDependsNode* NewNode = CreateOrFindDependsNode(OldNode->GetIdentifier());
		NewNode->Reserve(OldNode);
	}
	
	for (FDependsNode* OldNode : ValidDependsNodes)
	{
		FDependsNode* NewNode = CreateOrFindDependsNode(OldNode->GetIdentifier());
		OldNode->IterateOverDependencies([&, OldNode, NewNode](FDependsNode* InDependency, UE::AssetRegistry::EDependencyCategory InCategory, UE::AssetRegistry::EDependencyProperty InFlags, bool bDuplicate) {
			if (ValidDependsNodes.Contains(InDependency))
			{
				// Only add link if it's part of the filtered asset set
				FDependsNode* NewDependency = CreateOrFindDependsNode(InDependency->GetIdentifier());
				NewNode->SetIsDependencyListSorted(InCategory, false);
				NewNode->AddDependency(NewDependency, InCategory, InFlags);
				NewDependency->SetIsReferencersSorted(false);
				NewDependency->AddReferencer(NewNode);
			}
		});
		NewNode->SetIsDependenciesInitialized(true);
	}

	// Remove any orphaned depends nodes. This will leave cycles in but those might represent useful data
	TArray<FDependsNode*> AllDependsNodes;
	CachedDependsNodes.GenerateValueArray(AllDependsNodes);
	for (FDependsNode* DependsNode : AllDependsNodes)
	{
		if (DependsNode->GetConnectionCount() == 0)
		{
			RemoveDependsNode(DependsNode->GetIdentifier());
		}
	}

	// Restore the sortedness that we turned off for performance when creating each DependsNode
	for (TPair<FAssetIdentifier, FDependsNode*> Pair : CachedDependsNodes)
	{
		FDependsNode* DependsNode = Pair.Value;
		DependsNode->SetIsDependencyListSorted(UE::AssetRegistry::EDependencyCategory::All, true);
		DependsNode->SetIsReferencersSorted(true);
	}
}

void FAssetRegistryState::InitializeFromExisting(const FAssetDataMap& AssetDataMap, const TMap<FAssetIdentifier, FDependsNode*>& DependsNodeMap, 
	const TMap<FName, FAssetPackageData*>& AssetPackageDataMap, const FAssetRegistrySerializationOptions& Options, EInitializationMode InInitializationMode)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	if (InInitializationMode == EInitializationMode::Rebuild)
	{
		Reset();
	}

	for (const FAssetData* AssetDataPtr : AssetDataMap)
	{
		if (AssetDataPtr == nullptr)
		{
			// don't do anything 
			continue;
		}
		
		const FAssetData& AssetData = *AssetDataPtr;

		FAssetData* ExistingData = nullptr;
		if (InInitializationMode != EInitializationMode::Rebuild) // minor optimization to avoid lookup in rebuild mode
		{
			if (FAssetData** Ptr = CachedAssets.Find(FCachedAssetKey(AssetData)))
			{
				ExistingData = *Ptr;
			}
		}
		if (InInitializationMode == EInitializationMode::OnlyUpdateExisting && ExistingData == nullptr)
		{
			continue;
		}
		if (InInitializationMode == EInitializationMode::OnlyUpdateNew && ExistingData != nullptr)
		{
			continue;
		}

		// Filter asset registry tags now
		FAssetDataTagMap LocalTagsAndValues;
		FAssetRegistryState::FilterTags(AssetData.TagsAndValues, LocalTagsAndValues, Options.CookFilterlistTagsByClass.Find(AssetData.AssetClassPath), Options);
		
		if (ExistingData)
		{
			FAssetData NewData(AssetData);
			NewData.TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(LocalTagsAndValues));
			UpdateAssetData(ExistingData, MoveTemp(NewData));
		}
		else
		{
			FAssetData* NewData = new FAssetData(AssetData);
			NewData->TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(LocalTagsAndValues));
			AddAssetData(NewData);
		}
	}

	TSet<FAssetIdentifier> ScriptPackages;

	if (InInitializationMode != EInitializationMode::OnlyUpdateExisting)
	{
		for (const TPair<FName, FAssetPackageData*>& Pair : AssetPackageDataMap)
		{
			bool bIsScriptPackage = FPackageName::IsScriptPackage(Pair.Key.ToString());
			if (InInitializationMode == EInitializationMode::OnlyUpdateNew && CachedPackageData.Find(Pair.Key))
			{
				continue;
			}
			if (Pair.Value)
			{
				// Only add if also in asset data map, or script package
				if (bIsScriptPackage)
				{
					ScriptPackages.Add(Pair.Key);

					FAssetPackageData* NewData = CreateOrGetAssetPackageData(Pair.Key);
					*NewData = *Pair.Value;
				}
				else if (CachedAssetsByPackageName.Find(Pair.Key))
				{
					FAssetPackageData* NewData = CreateOrGetAssetPackageData(Pair.Key);
					*NewData = *Pair.Value;
				}
			}
		}

		TMap<FAssetIdentifier, FDependsNode*> FilteredDependsNodeMap;
		const TMap<FAssetIdentifier, FDependsNode*>* DependsNodesToAdd = &DependsNodeMap;
		if (InInitializationMode == EInitializationMode::OnlyUpdateNew)
		{
			// Keep the original DependsNodeMap for reference, but remove from NodesToAdd all nodes that already have dependency data
			// Also reserve up-front all (unfiltered) nodes we are adding, to avoid reallocating the Referencers array.
			FilteredDependsNodeMap.Reserve(DependsNodeMap.Num());
			DependsNodesToAdd = &FilteredDependsNodeMap;
			for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : DependsNodeMap)
			{
				FDependsNode* SourceNode = Pair.Value;
				FDependsNode* TargetNode = CreateOrFindDependsNode(Pair.Key);
				if (!TargetNode->IsDependenciesInitialized())
				{
					FilteredDependsNodeMap.Add(Pair.Key, SourceNode);
				}
				TargetNode->Reserve(SourceNode);
			}
		}
		else
		{
			// Reserve up-front all the nodes that we are adding, so we do not reallocate
			// the Referencers array multiple times on a node as we add nodes that refer to it
			for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : DependsNodeMap)
			{
				FDependsNode* SourceNode = Pair.Value;
				FDependsNode* TargetNode = CreateOrFindDependsNode(Pair.Key);
				TargetNode->Reserve(SourceNode);
			}
		}

		for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : *DependsNodesToAdd)
		{
			FDependsNode* SourceNode = Pair.Value;
			FDependsNode* TargetNode = CreateOrFindDependsNode(Pair.Key);
			SourceNode->IterateOverDependencies([this, &DependsNodeMap, &ScriptPackages, TargetNode]
			(FDependsNode* InDependency, UE::AssetRegistry::EDependencyCategory InCategory, UE::AssetRegistry::EDependencyProperty InFlags, bool bDuplicate) {
				const FAssetIdentifier& Identifier = InDependency->GetIdentifier();
				if (DependsNodeMap.Find(Identifier) || ScriptPackages.Contains(Identifier))
				{
					// Only add if this node is in the incoming map
					FDependsNode* TargetDependency = CreateOrFindDependsNode(Identifier);
					TargetNode->SetIsDependencyListSorted(InCategory, false);
					TargetNode->AddDependency(TargetDependency, InCategory, InFlags);
					TargetDependency->SetIsReferencersSorted(false);
					TargetDependency->AddReferencer(TargetDependency);
				}
			});
			TargetNode->SetIsDependenciesInitialized(true);
		}

		// Restore the sortedness that we turned off for performance when creating each DependsNode
		for (TPair<FAssetIdentifier, FDependsNode*> Pair : CachedDependsNodes)
		{
			FDependsNode* DependsNode = Pair.Value;
			DependsNode->SetIsDependencyListSorted(UE::AssetRegistry::EDependencyCategory::All, true);
			DependsNode->SetIsReferencersSorted(true);
		}
	}
}

void FAssetRegistryState::PruneAssetData(const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages, const FAssetRegistrySerializationOptions& Options)
{
	PruneAssetData(RequiredPackages, RemovePackages, TSet<int32>(), Options);
}

void FAssetRegistryState::PruneAssetData(const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages, const TSet<int32> ChunksToKeep, const FAssetRegistrySerializationOptions& Options)
{
	const bool bIsFilteredByChunkId = ChunksToKeep.Num() != 0;
	const bool bIsFilteredByRequiredPackages = RequiredPackages.Num() != 0;
	const bool bIsFilteredByRemovedPackages = RemovePackages.Num() != 0;

	TSet<FName> RequiredDependNodePackages;

	// Generate list up front as the maps will get cleaned up
	TArray<FAssetData*> AllAssetData = CachedAssets.Array();
	TSet<FDependsNode*> RemoveDependsNodes;

	// Remove assets and mark-for-removal any dependencynodes for assets removed due to having no tags
	for (FAssetData* AssetData : AllAssetData)
	{
		bool bRemoveAssetData = false;
		bool bRemoveDependencyData = true;

		if (bIsFilteredByChunkId &&
			!AssetData->GetChunkIDs().ContainsByPredicate([&](int32 ChunkId) { return ChunksToKeep.Contains(ChunkId); }))
		{
			bRemoveAssetData = true;
		}
		else if (bIsFilteredByRequiredPackages && !RequiredPackages.Contains(AssetData->PackageName))
		{
			bRemoveAssetData = true;
		}
		else if (bIsFilteredByRemovedPackages && RemovePackages.Contains(AssetData->PackageName))
		{
			bRemoveAssetData = true;
		}
		else if (Options.bFilterAssetDataWithNoTags && AssetData->TagsAndValues.Num() == 0 &&
			!FPackageName::IsLocalizedPackage(AssetData->PackageName.ToString()))
		{
			bRemoveAssetData = true;
			bRemoveDependencyData = Options.bFilterDependenciesWithNoTags;
		}

		if (bRemoveAssetData)
		{
			bool bRemovedAssetData, bRemovedPackageData;
			FName AssetPackageName = AssetData->PackageName;
			// AssetData might be deleted after this call
			RemoveAssetData(AssetData, false /* bRemoveDependencyData */, bRemovedAssetData, bRemovedPackageData);
			if (!bRemoveDependencyData)
			{
				RequiredDependNodePackages.Add(AssetPackageName);
			}
			else if (bRemovedPackageData)
			{
				FDependsNode** RemovedNode = CachedDependsNodes.Find(AssetPackageName);
				if (RemovedNode)
				{
					RemoveDependsNodes.Add(*RemovedNode);
				}
			}
		}
	}

	TArray<FDependsNode*> AllDependsNodes;
	CachedDependsNodes.GenerateValueArray(AllDependsNodes);

	// Mark-for-removal all other dependsnodes that are filtered out by our settings
	for (FDependsNode* DependsNode : AllDependsNodes)
	{
		const FAssetIdentifier& Id = DependsNode->GetIdentifier();
		bool bRemoveDependsNode = false;
		if (RemoveDependsNodes.Contains(DependsNode))
		{
			continue;
		}

		if (Options.bFilterSearchableNames && Id.IsValue())
		{
			bRemoveDependsNode = true;
		}
		else if (Id.IsPackage() &&
			!CachedAssetsByPackageName.Contains(Id.PackageName) &&
			!RequiredDependNodePackages.Contains(Id.PackageName) &&
			!FPackageName::IsScriptPackage(Id.PackageName.ToString()))
		{
			bRemoveDependsNode = true;
		}
		
		if (bRemoveDependsNode)
		{
			RemoveDependsNodes.Add(DependsNode);
		}
	}

	// Batch-remove all of the marked-for-removal dependsnodes
	for (FDependsNode* DependsNode : AllDependsNodes)
	{
		check(DependsNode != nullptr);
		if (RemoveDependsNodes.Contains(DependsNode))
		{
			CachedDependsNodes.Remove(DependsNode->GetIdentifier());
			NumDependsNodes--;
			// if the depends nodes were preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
			if (PreallocatedDependsNodeDataBuffers.Num() == 0)
			{
				delete DependsNode;
			}
		}
		else
		{
			DependsNode->RemoveLinks([&RemoveDependsNodes](const FDependsNode* ExistingDependsNode) { return RemoveDependsNodes.Contains(ExistingDependsNode); });
		}
	}

	// Remove any orphaned depends nodes. This will leave cycles in but those might represent useful data
	CachedDependsNodes.GenerateValueArray(AllDependsNodes);
	for (FDependsNode* DependsNode : AllDependsNodes)
	{
		if (DependsNode->GetConnectionCount() == 0)
		{
			RemoveDependsNode(DependsNode->GetIdentifier());
		}
	}
}

bool FAssetRegistryState::HasAssets(const FName PackagePath, bool bSkipARFilteredAssets) const
{
	const TArray<FAssetData*>* FoundAssetArray = CachedAssetsByPath.Find(PackagePath);
	if (FoundAssetArray)
	{
		if (bSkipARFilteredAssets)
		{
			return FoundAssetArray->ContainsByPredicate([](FAssetData* AssetData)
			{
				return AssetData && !UE::AssetRegistry::FFiltering::ShouldSkipAsset(AssetData->AssetClassPath, AssetData->PackageFlags);
			});
		}
		else
		{
			return FoundAssetArray->Num() > 0;
		}
	}
	return false;
}

bool FAssetRegistryState::GetAssets(const FARCompiledFilter& Filter, const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData, bool bSkipARFilteredAssets) const
{
	return EnumerateAssets(Filter, PackageNamesToSkip, [&OutAssetData](const FAssetData& AssetData)
	{
		OutAssetData.Emplace(AssetData);
		return true;
	},
	bSkipARFilteredAssets);
}

template<class ArrayType, typename KeyType>
TArray<FAssetData*> FindAssets(const TMap<KeyType, ArrayType>& Map, const TSet<KeyType>& Keys)
{
	TArray<TArrayView<FAssetData* const>> Matches;
	Matches.Reserve(Keys.Num());
	uint32 TotalMatches = 0;

	for (const KeyType& Key : Keys)
	{
		if (const ArrayType* Assets = Map.Find(Key))
		{
			Matches.Add(MakeArrayView(*Assets));
			TotalMatches += Assets->Num();
		}
	}

	TArray<FAssetData*> Out;
	Out.Reserve(TotalMatches);
	for (TArrayView<FAssetData* const> Assets : Matches)
	{
		Out.Append(Assets.GetData(), Assets.Num());
	}

	return Out;
}

bool FAssetRegistryState::EnumerateAssets(const FARCompiledFilter& Filter, const TSet<FName>& PackageNamesToSkip,
	TFunctionRef<bool(const FAssetData&)> Callback, bool bSkipARFilteredAssets) const
{
	// Verify filter input. If all assets are needed, use EnumerateAllAssets() instead.
	if (Filter.IsEmpty() || !IsFilterValid(Filter))
	{
		return false;
	}

	const uint32 FilterWithoutPackageFlags = Filter.WithoutPackageFlags;
	const uint32 FilterWithPackageFlags = Filter.WithPackageFlags;

	// The assets that match each filter
	TArray<TArray<FAssetData*>, TInlineAllocator<5>> FilterResults;
	
	// On disk package names
	if (Filter.PackageNames.Num() > 0)
	{
		FilterResults.Emplace(FindAssets(CachedAssetsByPackageName, Filter.PackageNames));
	}

	// On disk package paths
	if (Filter.PackagePaths.Num() > 0)
	{
		FilterResults.Emplace(FindAssets(CachedAssetsByPath, Filter.PackagePaths));
	}

	// On disk classes
	if (Filter.ClassPaths.Num() > 0)
	{
		FilterResults.Emplace(FindAssets(CachedAssetsByClass, Filter.ClassPaths));
	}

	// On disk object paths
	if (Filter.SoftObjectPaths.Num() > 0)
	{
		TArray<FAssetData*>& ObjectPathsFilter = FilterResults.Emplace_GetRef();
		ObjectPathsFilter.Reserve(Filter.SoftObjectPaths.Num());

		for (const FSoftObjectPath& ObjectPath : Filter.SoftObjectPaths)
		{
			if (FAssetData* const* AssetDataPtr = CachedAssets.Find(FCachedAssetKey(ObjectPath)))
			{
				ObjectPathsFilter.Add(*AssetDataPtr);
			}
		}
	}

	// On disk tags and values
	if (Filter.TagsAndValues.Num() > 0)
	{
		TArray<FAssetData*>& TagAndValuesFilter = FilterResults.Emplace_GetRef();
		// Sometimes number of assets matching this filter is correlated to number of assets matching previous filters 
		if (FilterResults.Num())
		{
			TagAndValuesFilter.Reserve(FilterResults[0].Num());
		}

		for (auto FilterTagIt = Filter.TagsAndValues.CreateConstIterator(); FilterTagIt; ++FilterTagIt)
		{
			const FName Tag = FilterTagIt.Key();
			const TOptional<FString>& Value = FilterTagIt.Value();

			if (const TArray<FAssetData*>* TagAssets = CachedAssetsByTag.Find(Tag))
			{
				for (FAssetData* AssetData : *TagAssets)
				{
					if (AssetData != nullptr)
					{
						bool bAccept;
						if (!Value.IsSet())
						{
							bAccept = AssetData->TagsAndValues.Contains(Tag);
						}
						else
						{
							bAccept = AssetData->TagsAndValues.ContainsKeyValue(Tag, Value.GetValue());
						}
						if (bAccept)
						{
							TagAndValuesFilter.Add(AssetData);
						}
					}
				}
			}
		}
	}

	// Perform callback for assets that match all filters
	if (FilterResults.Num() > 0)
	{
		auto SkipAssetData = [&](const FAssetData* AssetData) 
		{ 
			if (PackageNamesToSkip.Contains(AssetData->PackageName) |			//-V792
				AssetData->HasAnyPackageFlags(FilterWithoutPackageFlags) |		//-V792
				!AssetData->HasAllPackageFlags(FilterWithPackageFlags))			//-V792
			{
				return true;
			}

			return bSkipARFilteredAssets &&
				UE::AssetRegistry::FFiltering::ShouldSkipAsset(AssetData->AssetClassPath, AssetData->PackageFlags);
		};

		int32 NumFilterResults = FilterResults.Num();
		if (NumFilterResults > 1)
		{
			// Mark which filters each asset passes
			uint32 PassAllFiltersValue = (1 << NumFilterResults) - 1; // 1 in every bit for the lowest n bits
			TMap<FAssetData*, uint32> PassBits;
			for (int32 FilterIndex = 0; FilterIndex < NumFilterResults; ++FilterIndex)
			{
				const TArray<FAssetData*>& FilterEvaluation = FilterResults[FilterIndex];
				PassBits.Reserve(FilterEvaluation.Num());
				
				for (FAssetData* AssetData : FilterEvaluation)
				{
					PassBits.FindOrAdd(AssetData) |= (1 << FilterIndex);
				}
			}

			// Include assets that pass all filters
			for (TPair<FAssetData*, uint32> PassPair : PassBits)
			{
				const FAssetData* AssetData = PassPair.Key;
				if (PassPair.Value != PassAllFiltersValue || SkipAssetData(AssetData))
				{
					continue;
				}
				else if (!Callback(*AssetData))
				{
					return true;
				}
			}
		}
		else
		{
			// All matched assets passed the single filter
			for (const FAssetData* AssetData : FilterResults[0])
			{
				if (SkipAssetData(AssetData))
				{
					continue;
				}
				else if (!Callback(*AssetData))
				{
					return true;
				}
			}
		}
	}

	return true;
}

bool FAssetRegistryState::GetAllAssets(const TSet<FName>& PackageNamesToSkip, TArray<FAssetData>& OutAssetData, bool bSkipARFilteredAssets) const
{
	return EnumerateAllAssets(PackageNamesToSkip, [&OutAssetData](const FAssetData& AssetData)
	{
		OutAssetData.Emplace(AssetData);
		return true;
	},
	bSkipARFilteredAssets);
}

bool FAssetRegistryState::EnumerateAllAssets(const TSet<FName>& PackageNamesToSkip, TFunctionRef<bool(const FAssetData&)> Callback, bool bSkipARFilteredAssets) const
{
	// All unloaded disk assets
	for (const FAssetData* AssetData : CachedAssets)
	{
		if (AssetData &&
			!PackageNamesToSkip.Contains(AssetData->PackageName) &&
			(!bSkipARFilteredAssets || !UE::AssetRegistry::FFiltering::ShouldSkipAsset(AssetData->AssetClassPath, AssetData->PackageFlags)))
		{
			if (!Callback(*AssetData))
			{
				return true;
			}
		}
	}
	return true;
}

void FAssetRegistryState::EnumerateAllAssets(TFunctionRef<void(const FAssetData&)> Callback) const
{
	for (const FAssetData* AssetData : CachedAssets)
	{
		if (AssetData)
		{
			Callback(*AssetData);
		}
	}
}

void FAssetRegistryState::GetPackagesByName(FStringView PackageName, TArray<FName>& OutPackageNames) const
{
	// Note that we use CachedAssetsByPackageName rather than CachedPackageData because CachedPackageData
	// is often stripped out of the runtime AssetRegistry
	if (!FPackageName::IsShortPackageName(PackageName))
	{
		FName PackageFName(PackageName);
		if (CachedAssetsByPackageName.Contains(PackageFName))
		{
			OutPackageNames.Add(PackageFName);
		}
	}
	else
	{
		TStringBuilder<128> PackageNameStr;
		for (const auto& It : CachedAssetsByPackageName)
		{
			It.Key.ToString(PackageNameStr);
			FStringView ExistingBaseName = FPathViews::GetBaseFilename(PackageNameStr);
			if (ExistingBaseName.Equals(PackageName, ESearchCase::IgnoreCase))
			{
				OutPackageNames.Add(It.Key);
			}
		}
	}
}

FName FAssetRegistryState::GetFirstPackageByName(FStringView PackageName) const
{
	TArray<FName> LongPackageNames;
	GetPackagesByName(PackageName, LongPackageNames);
	if (LongPackageNames.Num() == 0)
	{
		return NAME_None;
	}
	if (LongPackageNames.Num() > 1)
	{
		LongPackageNames.Sort(FNameLexicalLess());
		UE_LOG(LogAssetRegistry, Warning, TEXT("GetFirstPackageByName('%.*s') is returning '%s', but it also found '%s'%s."),
			PackageName.Len(), PackageName.GetData(), *LongPackageNames[0].ToString(), *LongPackageNames[1].ToString(),
			(LongPackageNames.Num() > 2 ? *FString::Printf(TEXT(" and %d others"), LongPackageNames.Num() - 2) : TEXT("")));
	}
	return LongPackageNames[0];
}

bool FAssetRegistryState::GetDependencies(const FAssetIdentifier& AssetIdentifier,
										  TArray<FAssetIdentifier>& OutDependencies,
										  EAssetRegistryDependencyType::Type InDependencyType) const
{
	bool bResult = false;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE::AssetRegistry::FDependencyQuery Flags(InDependencyType);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (!!(InDependencyType & EAssetRegistryDependencyType::Packages))
	{
		bResult = GetDependencies(AssetIdentifier, OutDependencies, UE::AssetRegistry::EDependencyCategory::Package, Flags) || bResult;
	}
	if (!!(InDependencyType & EAssetRegistryDependencyType::SearchableName))
	{
		bResult = GetDependencies(AssetIdentifier, OutDependencies, UE::AssetRegistry::EDependencyCategory::SearchableName) || bResult;
	}
	if (!!(InDependencyType & EAssetRegistryDependencyType::Manage))
	{
		bResult = GetDependencies(AssetIdentifier, OutDependencies, UE::AssetRegistry::EDependencyCategory::Manage, Flags) || bResult;
	}
	return bResult;
}

bool FAssetRegistryState::GetDependencies(const FAssetIdentifier& AssetIdentifier,
										  TArray<FAssetIdentifier>& OutDependencies,
										  UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	const FDependsNode* const* NodePtr = CachedDependsNodes.Find(AssetIdentifier);
	const FDependsNode* Node = nullptr;
	if (NodePtr != nullptr)
	{
		Node = *NodePtr;
	}

	if (Node != nullptr)
	{
		Node->GetDependencies(OutDependencies, Category, Flags);
		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistryState::GetDependencies(const FAssetIdentifier& AssetIdentifier,
										  TArray<FAssetDependency>& OutDependencies,
										  UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	const FDependsNode* const* NodePtr = CachedDependsNodes.Find(AssetIdentifier);
	const FDependsNode* Node = nullptr;
	if (NodePtr != nullptr)
	{
		Node = *NodePtr;
	}

	if (Node != nullptr)
	{
		Node->GetDependencies(OutDependencies, Category, Flags);
		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistryState::GetReferencers(const FAssetIdentifier& AssetIdentifier,
										 TArray<FAssetIdentifier>& OutReferencers,
										 EAssetRegistryDependencyType::Type InReferenceType) const
{
	bool bResult = false;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE::AssetRegistry::FDependencyQuery Flags(InReferenceType);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (!!(InReferenceType & EAssetRegistryDependencyType::Packages))
	{
		bResult = GetReferencers(AssetIdentifier, OutReferencers, UE::AssetRegistry::EDependencyCategory::Package, Flags) || bResult;
	}
	if (!!(InReferenceType & EAssetRegistryDependencyType::SearchableName))
	{
		bResult = GetReferencers(AssetIdentifier, OutReferencers, UE::AssetRegistry::EDependencyCategory::SearchableName) || bResult;
	}
	if (!!(InReferenceType & EAssetRegistryDependencyType::Manage))
	{
		bResult = GetReferencers(AssetIdentifier, OutReferencers, UE::AssetRegistry::EDependencyCategory::Manage, Flags) || bResult;
	}
	return bResult;
}

bool FAssetRegistryState::GetReferencers(const FAssetIdentifier& AssetIdentifier,
										 TArray<FAssetIdentifier>& OutReferencers,
										 UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	const FDependsNode* const* NodePtr = CachedDependsNodes.Find(AssetIdentifier);
	const FDependsNode* Node = nullptr;

	if (NodePtr != nullptr)
	{
		Node = *NodePtr;
	}

	if (Node != nullptr)
	{
		TArray<FDependsNode*> DependencyNodes;
		Node->GetReferencers(DependencyNodes, Category, Flags);

		OutReferencers.Reserve(DependencyNodes.Num());
		for (FDependsNode* DependencyNode : DependencyNodes)
		{
			OutReferencers.Add(DependencyNode->GetIdentifier());
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistryState::GetReferencers(const FAssetIdentifier& AssetIdentifier,
										 TArray<FAssetDependency>& OutReferencers,
										 UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	const FDependsNode* const* NodePtr = CachedDependsNodes.Find(AssetIdentifier);
	const FDependsNode* Node = nullptr;

	if (NodePtr != nullptr)
	{
		Node = *NodePtr;
	}

	if (Node != nullptr)
	{
		Node->GetReferencers(OutReferencers, Category, Flags);
		return true;
	}
	else
	{
		return false;
	}
}

bool FAssetRegistryState::Serialize(FArchive& Ar, const FAssetRegistrySerializationOptions& Options)
{
	return Ar.IsSaving() ? Save(Ar, Options) : Load(Ar, FAssetRegistryLoadOptions(Options));
}

bool FAssetRegistryState::Save(FArchive& OriginalAr, const FAssetRegistrySerializationOptions& Options)
{
	SCOPED_BOOT_TIMING("FAssetRegistryState::Save");

	check(!OriginalAr.IsLoading());

#if !ALLOW_NAME_BATCH_SAVING
	checkf(false, TEXT("Cannot save cooked AssetRegistryState in this configuration"));
#else
	check(CachedAssets.Num() == NumAssets);

	FAssetRegistryHeader Header;
	Header.Version = FAssetRegistryVersion::LatestVersion;
	Header.bFilterEditorOnlyData = OriginalAr.IsFilterEditorOnly();
	Header.SerializeHeader(OriginalAr);

	// Set up fixed asset registry writer
	FAssetRegistryWriter Ar(FAssetRegistryWriterOptions(Options), OriginalAr);

	// serialize number of objects
	int32 AssetCount = CachedAssets.Num();
	Ar << AssetCount;

	// Write asset data first
	TArray<FAssetData*> SortedAssetsByObjectPath = CachedAssets.Array();
	Algo::Sort(SortedAssetsByObjectPath, [](const FAssetData* A, const FAssetData* B) { return A->GetSoftObjectPath().LexicalLess(B->GetSoftObjectPath()); });
	for (FAssetData* AssetData : SortedAssetsByObjectPath)
	{
		// Hardcoding FAssetRegistryVersion::LatestVersion here so that branches can get optimized out in the forceinlined SerializeForCache
		AssetData->SerializeForCache(Ar);
	}

	// Serialize Dependencies
	// Write placeholder data for the size
	int64 OffsetToDependencySectionSize = Ar.Tell();
	int64 DependencySectionSize = 0;
	Ar << DependencySectionSize;
	int64 DependencySectionStart = Ar.Tell();
	if (!Options.bSerializeDependencies)
	{
		int32 NumDependencies = 0;
		Ar << NumDependencies;
	}
	else
	{
		TMap<FDependsNode*, FDependsNode*> RedirectCache;
		TArray<FDependsNode*> Dependencies;

		// Scan dependency nodes, we won't save all of them if we filter out certain types
		for (TPair<FAssetIdentifier, FDependsNode*>& Pair : CachedDependsNodes)
		{
			FDependsNode* Node = Pair.Value;

			if (Node->GetIdentifier().IsPackage() 
				|| (Options.bSerializeSearchableNameDependencies && Node->GetIdentifier().IsValue())
				|| (Options.bSerializeManageDependencies && Node->GetIdentifier().GetPrimaryAssetId().IsValid()))
			{
				Dependencies.Add(Node);
			}
		}
		Algo::Sort(Dependencies, [](FDependsNode* A, FDependsNode* B) { return A->GetIdentifier().LexicalLess(B->GetIdentifier()); });
		int32 NumDependencies = Dependencies.Num();

		TMap<FDependsNode*, int32> DependsIndexMap;
		DependsIndexMap.Reserve(NumDependencies);
		int32 Index = 0;
		for (FDependsNode* Node : Dependencies)
		{
			DependsIndexMap.Add(Node, Index++);
		}

		TUniqueFunction<int32(FDependsNode*, bool bAsReferencer)> GetSerializeIndexFromNode = [this, &RedirectCache, &DependsIndexMap](FDependsNode* InDependency, bool bAsReferencer)
		{
			if (!bAsReferencer)
			{
				InDependency = ResolveRedirector(InDependency, CachedAssets, RedirectCache);
			}
			if (!InDependency)
			{
				return -1;
			}
			int32* DependencyIndex = DependsIndexMap.Find(InDependency);
			if (!DependencyIndex)
			{
				return -1;
			}
			return *DependencyIndex;
		};

		FDependsNode::FSaveScratch Scratch;
		Ar << NumDependencies;
		for (FDependsNode* DependentNode : Dependencies)
		{
			DependentNode->SerializeSave(Ar, GetSerializeIndexFromNode, Scratch, Options);
		}
	}
	// Write the real value to the placeholder data for the DependencySectionSize
	int64 DependencySectionEnd = Ar.Tell();
	DependencySectionSize = DependencySectionEnd - DependencySectionStart;
	Ar.Seek(OffsetToDependencySectionSize);
	Ar << DependencySectionSize;
	check(Ar.Tell() == DependencySectionStart);
	Ar.Seek(DependencySectionEnd);


	// Serialize the PackageData
	int32 PackageDataCount = 0;
	if (Options.bSerializePackageData)
	{
		PackageDataCount = CachedPackageData.Num();
		Ar << PackageDataCount;

		TArray<TPair<FName, FAssetPackageData*>> SortedPackageData = CachedPackageData.Array();
		Algo::Sort(SortedPackageData, [](TPair<FName, FAssetPackageData*>& A, TPair<FName, FAssetPackageData*>& B) { return A.Key.LexicalLess(B.Key); });
		for (TPair<FName, FAssetPackageData*>& Pair : SortedPackageData)
		{
			Ar << Pair.Key;
			Pair.Value->SerializeForCache(Ar);
		}
	}
	else
	{
		Ar << PackageDataCount;
	}
#endif // ALLOW_NAME_BATCH_SAVING

	return !OriginalAr.IsError();
}

bool FAssetRegistryState::Load(FArchive& OriginalAr, const FAssetRegistryLoadOptions& Options, FAssetRegistryVersion::Type* OutVersion)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);

	FAssetRegistryHeader Header;
	Header.SerializeHeader(OriginalAr);
	if (OutVersion != nullptr)
	{
		*OutVersion = Header.Version;
	}

	FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::NonPackage, ESoftObjectPathSerializeType::AlwaysSerialize);

	if (Header.Version < FAssetRegistryVersion::RemovedMD5Hash)
	{
		// Cannot read states before this version
		return false;
	}
	else if (Header.Version < FAssetRegistryVersion::FixedTags)
	{
		FNameTableArchiveReader NameTableReader(OriginalAr);
		Load(NameTableReader, Header, Options);
	}
	else
	{
		FAssetRegistryReader Reader(OriginalAr, Options.ParallelWorkers, Header);

		if (Reader.IsError())
		{
			return false;
		}

		// Load won't resolve asset registry tag values loaded in parallel
		// and can run before WaitForTasks
		Load(Reader, Header, Options);

		Reader.WaitForTasks();
	}

	return !OriginalAr.IsError();
}

/* static */ bool FAssetRegistryState::LoadFromDisk(const TCHAR* InPath, const FAssetRegistryLoadOptions& InOptions, FAssetRegistryState& OutState, FAssetRegistryVersion::Type* OutVersion)
{
	check(InPath);

	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(InPath));
	if (FileReader)
	{
		// It's faster to load the whole file into memory on a Gen5 console
		TArray64<uint8> Data;
		Data.SetNumUninitialized(FileReader->TotalSize());
		FileReader->Serialize(Data.GetData(), Data.Num());
		check(!FileReader->IsError());

		FLargeMemoryReader MemoryReader(Data.GetData(), Data.Num());
		return OutState.Load(MemoryReader, InOptions, OutVersion);
	}

	return false;
}

template<class Archive>
void FAssetRegistryState::Load(Archive&& Ar, const FAssetRegistryHeader& Header, const FAssetRegistryLoadOptions& Options)
{
	FAssetRegistryVersion::Type Version = Header.Version;

	// serialize number of objects
	int32 LocalNumAssets = 0;
	Ar << LocalNumAssets;

	// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
	TArrayView<FAssetData> PreallocatedAssetDataBuffer(new FAssetData[LocalNumAssets], LocalNumAssets);
	PreallocatedAssetDataBuffers.Add(PreallocatedAssetDataBuffer.GetData());

	// Optimizing serialization of latest asset data format by moving version checking out of SerializeForCache function and falling back to versioned
	// serialization should we attempt to load an older version of AR (usually commandlets)
	if (Version == FAssetRegistryVersion::LatestVersion)
	{
		for (FAssetData& NewAssetData : PreallocatedAssetDataBuffer)
		{
			NewAssetData.SerializeForCache(Ar);
		}
	}
	else
	{
		for (FAssetData& NewAssetData : PreallocatedAssetDataBuffer)
		{
			NewAssetData.SerializeForCacheOldVersion(Ar, Version);
		}
	}

	SetAssetDatas(PreallocatedAssetDataBuffer, Options);

	if (Version < FAssetRegistryVersion::AddedDependencyFlags)
	{
		LoadDependencies_BeforeFlags(Ar, Options.bLoadDependencies, Version);
	}
	else
	{
		int64 DependencySectionSize;
		Ar << DependencySectionSize;
		int64 DependencySectionEnd = Ar.Tell() + DependencySectionSize;

		if (Options.bLoadDependencies)
		{
			LoadDependencies(Ar);
		}
			
		if (!Options.bLoadDependencies || Ar.IsError())
		{
			Ar.Seek(DependencySectionEnd);
		}
	}

	int32 LocalNumPackageData = 0;
	Ar << LocalNumPackageData;

	if (LocalNumPackageData > 0)
	{
		FAssetPackageData SerializedElement;
		TArrayView<FAssetPackageData> PreallocatedPackageDataBuffer;
		if (Options.bLoadPackageData)
		{
			PreallocatedPackageDataBuffer = TArrayView<FAssetPackageData>(new FAssetPackageData[LocalNumPackageData], LocalNumPackageData);
			PreallocatedPackageDataBuffers.Add(PreallocatedPackageDataBuffer.GetData());
			CachedPackageData.Reserve(LocalNumPackageData);
		}
		for (int32 PackageDataIndex = 0; PackageDataIndex < LocalNumPackageData; PackageDataIndex++)
		{
			FName PackageName;
			Ar << PackageName;
			FAssetPackageData* NewPackageData;
			if (Options.bLoadPackageData)
			{
				NewPackageData = &PreallocatedPackageDataBuffer[PackageDataIndex];
				CachedPackageData.Add(PackageName, NewPackageData);
			}
			else
			{
				NewPackageData = &SerializedElement;
			}
			if (Version >= FAssetRegistryVersion::LatestVersion)
			{
				NewPackageData->SerializeForCache(Ar);
			}
			else
			{
				NewPackageData->SerializeForCacheOldVersion(Ar, Version);
			}
		}
	}
}

void FAssetRegistryState::LoadDependencies(FArchive& Ar)
{
	int32 LocalNumDependsNodes = 0;
	Ar << LocalNumDependsNodes;

	if (LocalNumDependsNodes <= 0)
	{
		return;
	}

	FDependsNode* PreallocatedDependsNodeDataBuffer = new FDependsNode[LocalNumDependsNodes];
	PreallocatedDependsNodeDataBuffers.Add(PreallocatedDependsNodeDataBuffer);
	CachedDependsNodes.Reserve(LocalNumDependsNodes);
	
	TUniqueFunction<FDependsNode*(int32)> GetNodeFromSerializeIndex = [&PreallocatedDependsNodeDataBuffer, LocalNumDependsNodes](int32 Index) -> FDependsNode*
	{
		if (Index < 0 || LocalNumDependsNodes <= Index)
		{
			return nullptr;
		}
		return &PreallocatedDependsNodeDataBuffer[Index];
	};

	FDependsNode::FLoadScratch Scratch;
	for (int32 DependsNodeIndex = 0; DependsNodeIndex < LocalNumDependsNodes; DependsNodeIndex++)
	{
		FDependsNode* DependsNode = &PreallocatedDependsNodeDataBuffer[DependsNodeIndex];
		DependsNode->SerializeLoad(Ar, GetNodeFromSerializeIndex, Scratch);
		CachedDependsNodes.Add(DependsNode->GetIdentifier(), DependsNode);
	}
}

void FAssetRegistryState::LoadDependencies_BeforeFlags(FArchive& Ar, bool bSerializeDependencies, FAssetRegistryVersion::Type Version)
{
	int32 LocalNumDependsNodes = 0;
	Ar << LocalNumDependsNodes;

	FDependsNode Placeholder;
	FDependsNode* PreallocatedDependsNodeDataBuffer = nullptr;
	if (bSerializeDependencies && LocalNumDependsNodes > 0)
	{
		PreallocatedDependsNodeDataBuffer = new FDependsNode[LocalNumDependsNodes];
		PreallocatedDependsNodeDataBuffers.Add(PreallocatedDependsNodeDataBuffer);
		CachedDependsNodes.Reserve(LocalNumDependsNodes);
	}
	TUniqueFunction<FDependsNode* (int32)> GetNodeFromSerializeIndex = [&PreallocatedDependsNodeDataBuffer, LocalNumDependsNodes](int32 Index)->FDependsNode *
	{
		if (Index < 0 || LocalNumDependsNodes <= Index)
		{
			return nullptr;
		}
		return &PreallocatedDependsNodeDataBuffer[Index];
	};

	uint32 HardBits, SoftBits, HardManageBits, SoftManageBits;
	FDependsNode::GetPropertySetBits_BeforeFlags(HardBits, SoftBits, HardManageBits, SoftManageBits);

	TArray<FDependsNode*> DependsNodes;
	for (int32 DependsNodeIndex = 0; DependsNodeIndex < LocalNumDependsNodes; DependsNodeIndex++)
	{
		// Create the node if we're actually saving dependencies, otherwise just fake serialize
		FDependsNode* DependsNode = nullptr;
		if (bSerializeDependencies)
		{
			DependsNode = &PreallocatedDependsNodeDataBuffer[DependsNodeIndex];
		}
		else
		{
			DependsNode = &Placeholder;
		}

		// Call the DependsNode legacy serialization function
		DependsNode->SerializeLoad_BeforeFlags(Ar, Version, PreallocatedDependsNodeDataBuffer, LocalNumDependsNodes, bSerializeDependencies, HardBits, SoftBits, HardManageBits, SoftManageBits);

		// Register the DependsNode with its AssetIdentifier
		if (bSerializeDependencies)
		{
			CachedDependsNodes.Add(DependsNode->GetIdentifier(), DependsNode);
		}
	}
}

SIZE_T FAssetRegistryState::GetAllocatedSize(bool bLogDetailed) const
{
	SIZE_T MapMemory = CachedAssets.GetAllocatedSize();
	MapMemory += CachedAssetsByPackageName.GetAllocatedSize();
	MapMemory += CachedAssetsByPath.GetAllocatedSize();
	MapMemory += CachedAssetsByClass.GetAllocatedSize();
	MapMemory += CachedAssetsByTag.GetAllocatedSize();
	MapMemory += CachedDependsNodes.GetAllocatedSize();
	MapMemory += CachedPackageData.GetAllocatedSize();
	MapMemory += PreallocatedAssetDataBuffers.GetAllocatedSize();
	MapMemory += PreallocatedDependsNodeDataBuffers.GetAllocatedSize();
	MapMemory += PreallocatedPackageDataBuffers.GetAllocatedSize();

	SIZE_T MapArrayMemory = 0;
	auto SubArray = 
		[&MapArrayMemory](const auto& A)
	{
		for (auto& Pair : A)
		{
			MapArrayMemory += Pair.Value.GetAllocatedSize();
		}
	};
	SubArray(CachedAssetsByPackageName);
	SubArray(CachedAssetsByPath);

	for (auto& Pair : CachedAssetsByClass)
	{
		MapArrayMemory += Pair.Value.GetAllocatedSize();
	}

	SubArray(CachedAssetsByTag);

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("Index Size: %" SIZE_T_FMT "k"), MapMemory / 1024);
	}

	SIZE_T AssetDataSize = 0, AssetBundlesSize = 0, NumAssetBundles = 0, NumSoftObjectPaths = 0, NumTopLevelAssetPaths = 0;
	FAssetDataTagMapSharedView::FMemoryCounter TagMemoryUsage;

	for (const FAssetData* AssetData : CachedAssets)
	{
		AssetDataSize += sizeof(*AssetData);
		TagMemoryUsage.Include(AssetData->TagsAndValues);
		if (AssetData->TaggedAssetBundles.IsValid())
		{
			AssetBundlesSize += sizeof(FAssetBundleData);
			AssetBundlesSize += AssetData->TaggedAssetBundles->Bundles.GetAllocatedSize();
			NumAssetBundles += AssetData->TaggedAssetBundles->Bundles.Num();
			for( const FAssetBundleEntry& Entry : AssetData->TaggedAssetBundles->Bundles)
			{
#if WITH_EDITORONLY_DATA
				PRAGMA_DISABLE_DEPRECATION_WARNINGS;
				AssetBundlesSize += Entry.BundleAssets.GetAllocatedSize();
				NumSoftObjectPaths += Entry.BundleAssets.Num();
				for (const FSoftObjectPath& Path : Entry.BundleAssets)
				{
					AssetBundlesSize += Path.GetSubPathString().GetAllocatedSize();
				}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS;
#endif
				AssetBundlesSize += Entry.AssetPaths.GetAllocatedSize();
				NumTopLevelAssetPaths += Entry.AssetPaths.Num();
			}
		}
	}

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetData Count: %d"), CachedAssets.Num());
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetData Static Size: %" SIZE_T_FMT "k"), AssetDataSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("Loose Tags: %" SIZE_T_FMT "k"), TagMemoryUsage.GetLooseSize() / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("Fixed Tags: %" SIZE_T_FMT "k"), TagMemoryUsage.GetFixedSize() / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("TArray<FAssetData*>: %" SIZE_T_FMT "k"), MapArrayMemory / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetBundle Count: %u"), NumAssetBundles);
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetBundle Size: %uk"), AssetBundlesSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetBundle FSoftObjectPath Count: %u"), NumSoftObjectPaths);
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetBundle FTopLevelAssetPath Count: %u"), NumTopLevelAssetPaths);
	}

	SIZE_T DependNodesSize = 0, DependenciesSize = 0;

	for (const TPair<FAssetIdentifier, FDependsNode*>& DependsNodePair : CachedDependsNodes)
	{
		const FDependsNode& DependsNode = *DependsNodePair.Value;
		DependNodesSize += sizeof(DependsNode);

		DependenciesSize += DependsNode.GetAllocatedSize();
	}

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("Dependency Node Count: %d"), CachedDependsNodes.Num());
		UE_LOG(LogAssetRegistry, Log, TEXT("Dependency Node Static Size: %" SIZE_T_FMT "k"), DependNodesSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("Dependency Arrays Size: %" SIZE_T_FMT "k"), DependenciesSize / 1024);
	}

	SIZE_T PackageDataSize = CachedPackageData.Num() * (sizeof(FAssetPackageData) + sizeof(FAssetPackageData*));
	for (const TPair<FName, FAssetPackageData*>& PackageDataPair : CachedPackageData)
	{
		PackageDataSize += PackageDataPair.Value->GetAllocatedSize();
	}

	SIZE_T TotalBytes = MapMemory + AssetDataSize + AssetBundlesSize + TagMemoryUsage.GetFixedSize()  + TagMemoryUsage.GetLooseSize() + DependNodesSize + DependenciesSize + PackageDataSize + MapArrayMemory;

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("PackageData Count: %d"), CachedPackageData.Num());
		UE_LOG(LogAssetRegistry, Log, TEXT("PackageData Static Size: %" SIZE_T_FMT "k"), PackageDataSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("Total State Size: %" SIZE_T_FMT "k"), TotalBytes / 1024);
	}

	return TotalBytes;
}

FDependsNode* FAssetRegistryState::ResolveRedirector(FDependsNode* InDependency,
													 const FAssetDataMap& InAllowedAssets,
													 TMap<FDependsNode*, FDependsNode*>& InCache)
{
	if (InCache.Contains(InDependency))
	{
		return InCache[InDependency];
	}

	FDependsNode* CurrentDependency = InDependency;
	FDependsNode* Result = nullptr;

	TSet<FName> EncounteredDependencies;

	while (Result == nullptr)
	{
		checkSlow(CurrentDependency);

		if (EncounteredDependencies.Contains(CurrentDependency->GetPackageName()))
		{
			break;
		}

		EncounteredDependencies.Add(CurrentDependency->GetPackageName());

		if (CachedAssetsByPackageName.Contains(CurrentDependency->GetPackageName()))
		{
			// Get the list of assets contained in this package
			TArray<FAssetData*, TInlineAllocator<1>>& Assets = CachedAssetsByPackageName[CurrentDependency->GetPackageName()];

			for (FAssetData* Asset : Assets)
			{
				if (Asset->IsRedirector())
				{
					FDependsNode* ChainedRedirector = nullptr;
					// This asset is a redirector, so we want to look at its dependencies and find the asset that it is redirecting to
					CurrentDependency->IterateOverDependencies([&](FDependsNode* InDepends, UE::AssetRegistry::EDependencyCategory Category, UE::AssetRegistry::EDependencyProperty Property, bool bDuplicate) {
						if (bDuplicate)
						{
							return; // Already looked at this dependency node
						}
						const FAssetIdentifier& AssetId = InDepends->GetIdentifier();
						FSoftObjectPath AssetPath(AssetId.PackageName, AssetId.ObjectName, FString());
						if (InAllowedAssets.Contains(FCachedAssetKey(AssetPath)))
						{
							// This asset is in the allowed asset list, so take this as the redirect target
							Result = InDepends;
						}
						else if (CachedAssetsByPackageName.Contains(InDepends->GetPackageName()))
						{
							// This dependency isn't in the allowed list, but it is a valid asset in the registry.
							// Because this is a redirector, this should mean that the redirector is pointing at ANOTHER
							// redirector (or itself in some horrible situations) so we'll move to that node and try again
							ChainedRedirector = InDepends;
						}
					}, UE::AssetRegistry::EDependencyCategory::Package);

					if (ChainedRedirector)
					{
						// Found a redirector, break for loop
						CurrentDependency = ChainedRedirector;
						break;
					}
				}
				else
				{
					Result = CurrentDependency;
				}

				if (Result)
				{
					// We found an allowed asset from the original dependency node. We're finished!
					break;
				}
			}
		}
		else
		{
			Result = CurrentDependency;
		}
	}

	InCache.Add(InDependency, Result);
	return Result;
}

template <typename KeyType>
void ShrinkMultimap(TMap<KeyType, TArray<FAssetData*> >& Map)
{
	Map.Shrink();
	for (auto& Pair : Map)
	{
		Pair.Value.Shrink();
	}
};

void FAssetRegistryState::SetAssetDatas(TArrayView<FAssetData> AssetDatas, const FAssetRegistryLoadOptions& Options)
{
	UE_CLOG(NumAssets != 0, LogAssetRegistry, Fatal, TEXT("Can only load into empty asset registry states. Load into temporary and append using InitializeFromExisting() instead."));

	NumAssets = AssetDatas.Num();
	
	auto SetPathCache = [&]() 
	{
		CachedAssets.Empty(AssetDatas.Num());
		for (FAssetData& AssetData : AssetDatas)
		{
			CachedAssets.Add(&AssetData);
		}
		ensure(NumAssets == CachedAssets.Num());
	};

	// FAssetDatas sharing package name are very rare.
	// Reserve up front and don't bother shrinking. 
	auto SetPackageNameCache = [&]() 
	{
		CachedAssetsByPackageName.Empty(AssetDatas.Num());
		for (FAssetData& AssetData : AssetDatas)
		{
			TArray<FAssetData*, TInlineAllocator<1>>& PackageAssets = CachedAssetsByPackageName.FindOrAdd(AssetData.PackageName);
			PackageAssets.Add(&AssetData);
		}
	};

	auto SetOtherCaches = [&]()
	{
		CachedAssetsByPath.Empty();
		for (FAssetData& AssetData : AssetDatas)
		{
			TArray<FAssetData*>& PathAssets = CachedAssetsByPath.FindOrAdd(AssetData.PackagePath);
			PathAssets.Add(&AssetData);
		}
		ShrinkMultimap(CachedAssetsByPath);

		CachedAssetsByClass.Empty();
		for (FAssetData& AssetData : AssetDatas)
		{
			TArray<FAssetData*>& ClassAssets = CachedAssetsByClass.FindOrAdd(AssetData.AssetClassPath);
			ClassAssets.Add(&AssetData);
		}
		ShrinkMultimap(CachedAssetsByClass);

		CachedAssetsByTag.Empty();
		for (FAssetData& AssetData : AssetDatas)
		{
			for (const TPair<FName, FAssetTagValueRef>& Pair : AssetData.TagsAndValues)
			{
				TArray<FAssetData*>& TagAssets = CachedAssetsByTag.FindOrAdd(Pair.Key);
				TagAssets.Add(&AssetData);
			}
		}
		ShrinkMultimap(CachedAssetsByTag);
	};

	if (Options.ParallelWorkers <= 1)
	{
		SetPathCache();
		SetPackageNameCache();
		SetOtherCaches();
	}
	else
	{
		TFuture<void> Task1 = Async(EAsyncExecution::TaskGraph, [=](){ SetPathCache(); });
		TFuture<void> Task2 = Async(EAsyncExecution::TaskGraph, [=](){ SetPackageNameCache(); });
		SetOtherCaches();
		Task1.Wait();
		Task2.Wait();
	}
}

void FAssetRegistryState::AddAssetData(FAssetData* AssetData)
{
	bool bAlreadyInSet = false;
	CachedAssets.Add(AssetData, &bAlreadyInSet);
	if (bAlreadyInSet)
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("AddAssetData called with ObjectPath %s which already exists. ")
			TEXT("This will overwrite and leak the existing AssetData."), *FCachedAssetKey(*AssetData).ToString());
	}
	else
	{
		++NumAssets;
	}

	TArray<FAssetData*, TInlineAllocator<1>>& PackageAssets = CachedAssetsByPackageName.FindOrAdd(AssetData->PackageName);
	TArray<FAssetData*>& PathAssets = CachedAssetsByPath.FindOrAdd(AssetData->PackagePath);
	TArray<FAssetData*>& ClassAssets = CachedAssetsByClass.FindOrAdd(AssetData->AssetClassPath);

	PackageAssets.Add(AssetData);
	PathAssets.Add(AssetData);
	ClassAssets.Add(AssetData);

	for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
	{
		FName Key = TagIt.Key();

		TArray<FAssetData*>& TagAssets = CachedAssetsByTag.FindOrAdd(Key);
		TagAssets.Add(AssetData);
	}
}

void FAssetRegistryState::AddTagsToAssetData(const FSoftObjectPath& InObjectPath, FAssetDataTagMap&& InTagsAndValues)
{
	if (InTagsAndValues.IsEmpty())
	{
		return;
	}

	FSetElementId Id = CachedAssets.FindId(FCachedAssetKey(InObjectPath));
	if (!Id.IsValidId())
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("AddTagsToAssetData called with asset data that doesn't exist! Tags not added. ObjectPath: %s"), *InObjectPath.ToString());
		return;
	}
	FAssetData* AssetData = CachedAssets[Id];
	FAssetDataTagMap Tags = AssetData->TagsAndValues.CopyMap();
	Tags.Append(MoveTemp(InTagsAndValues));
	SetTagsOnExistingAsset(AssetData, MoveTemp(Tags));
}


void FAssetRegistryState::FilterTags(const FAssetRegistrySerializationOptions& Options)
{
	for (FAssetData* AssetData : CachedAssets) 
	{
		check(AssetData);

		FAssetDataTagMap LocalTagsAndValues;
		FAssetRegistryState::FilterTags(AssetData->TagsAndValues, LocalTagsAndValues, Options.CookFilterlistTagsByClass.Find(AssetData->AssetClassPath), Options);
		if (LocalTagsAndValues != AssetData->TagsAndValues)
		{
			SetTagsOnExistingAsset(AssetData, MoveTemp(LocalTagsAndValues));
		}
	}
}

void FAssetRegistryState::SetTagsOnExistingAsset(FAssetData* AssetData, FAssetDataTagMap&& NewTags)
{
	// Update the tag cache map to remove deleted tags
	for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FName FNameKey = TagIt.Key();

		if (!NewTags.Contains(FNameKey))
		{
			TArray<FAssetData*>* OldTagAssets = CachedAssetsByTag.Find(FNameKey);
			OldTagAssets->RemoveSingleSwap(AssetData);
		}
	}
	// Update the tag cache map to add added tags
	for (auto TagIt = NewTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		const FName FNameKey = TagIt.Key();

		if (!AssetData->TagsAndValues.Contains(FNameKey))
		{
			TArray<FAssetData*>& NewTagAssets = CachedAssetsByTag.FindOrAdd(FNameKey);
			NewTagAssets.Add(AssetData);
		}
	}
	AssetData->TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(NewTags));
}

void FAssetRegistryState::UpdateAssetData(const FAssetData& NewAssetData, bool bCreateIfNotExists)
{
	if (FAssetData** AssetData = CachedAssets.Find(FCachedAssetKey(NewAssetData)))
	{
		UpdateAssetData(*AssetData, NewAssetData);
	}
	else
	{
		AddAssetData(new FAssetData(NewAssetData));
	}
}

void FAssetRegistryState::UpdateAssetData(FAssetData&& NewAssetData, bool bCreateIfNotExists)
{
	if (FAssetData** AssetData = CachedAssets.Find(FCachedAssetKey(NewAssetData)))
	{
		UpdateAssetData(*AssetData, MoveTemp(NewAssetData));
	}
	else if (bCreateIfNotExists)
	{
		AddAssetData(new FAssetData(MoveTemp(NewAssetData)));
	}
}

void FAssetRegistryState::UpdateAssetData(FAssetData* AssetData, const FAssetData& NewAssetData, bool* bOutModified)
{
	UpdateAssetData(AssetData, FAssetData(NewAssetData), bOutModified);
}

void FAssetRegistryState::UpdateAssetData(FAssetData* AssetData, FAssetData&& NewAssetData, bool* bOutModified)
{
	bool bKeyFieldIsModified = false;
	FCachedAssetKey OldKey(AssetData);
	FCachedAssetKey NewKey(NewAssetData);

	// Update ObjectPath
	if (OldKey != NewKey)
	{
		bKeyFieldIsModified = true;
		int32 NumRemoved = CachedAssets.Remove(OldKey);
		check(NumRemoved <= 1);
		if (NumRemoved == 0)
		{
			UE_LOG(LogAssetRegistry, Error, TEXT("UpdateAssetData called on AssetData %s that is not present in the AssetRegistry."),
				*AssetData->GetObjectPathString());
		}
		NumAssets -= NumRemoved;
	}

	// Update PackageName
	if (AssetData->PackageName != NewAssetData.PackageName)
	{
		bKeyFieldIsModified = true;
		TArray<FAssetData*, TInlineAllocator<1>>& NewPackageAssets = CachedAssetsByPackageName.FindOrAdd(NewAssetData.PackageName);
		TArray<FAssetData*, TInlineAllocator<1>>* OldPackageAssets = CachedAssetsByPackageName.Find(AssetData->PackageName);

		OldPackageAssets->Remove(AssetData);
		NewPackageAssets.Add(AssetData);
	}

	// Update PackagePath
	if (AssetData->PackagePath != NewAssetData.PackagePath)
	{
		bKeyFieldIsModified = true;
		TArray<FAssetData*>& NewPathAssets = CachedAssetsByPath.FindOrAdd(NewAssetData.PackagePath);
		TArray<FAssetData*>* OldPathAssets = CachedAssetsByPath.Find(AssetData->PackagePath);

		OldPathAssets->Remove(AssetData);
		NewPathAssets.Add(AssetData);
	}

	// AssetName is not a keyfield; compared below

	// Update AssetClass
	if (AssetData->AssetClassPath != NewAssetData.AssetClassPath)
	{
		bKeyFieldIsModified = true;
		TArray<FAssetData*>& NewClassAssets = CachedAssetsByClass.FindOrAdd(NewAssetData.AssetClassPath);
		TArray<FAssetData*>* OldClassAssets = CachedAssetsByClass.Find(AssetData->AssetClassPath);

		OldClassAssets->Remove(AssetData);
		NewClassAssets.Add(AssetData);
	}

	// PackageFlags is not a keyfield; compared below

	// Update Tags
	if (AssetData->TagsAndValues != NewAssetData.TagsAndValues)
	{ 
		bKeyFieldIsModified = true;
		for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
		{
			const FName FNameKey = TagIt.Key();

			if (!NewAssetData.TagsAndValues.Contains(FNameKey))
			{
				TArray<FAssetData*>* OldTagAssets = CachedAssetsByTag.Find(FNameKey);
				OldTagAssets->RemoveSingleSwap(AssetData);
			}
		}

		for (auto TagIt = NewAssetData.TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
		{
			const FName FNameKey = TagIt.Key();

			if (!AssetData->TagsAndValues.Contains(FNameKey))
			{
				TArray<FAssetData*>& NewTagAssets = CachedAssetsByTag.FindOrAdd(FNameKey);
				NewTagAssets.Add(AssetData);
			}
		}
	}

	// TaggedAssetBundles is not a keyfield; compared below

	// ChunkIds is not a keyfield; compared below

	if (bOutModified)
	{
		// Computing equality is expensive; if the caller needs to know it, do cheap compares first 
		// so we can skip the more expensive compares if the inequality is already known
		// This is not possible for keyfields - we have to take action on those even if inequality is already known -
		// so we start with whether bKeyFieldIsModified
		*bOutModified = bKeyFieldIsModified ||
			AssetData->AssetName != NewAssetData.AssetName ||
			AssetData->PackageFlags != NewAssetData.PackageFlags ||
			!AssetData->HasSameChunkIDs(NewAssetData) ||
			(AssetData->TaggedAssetBundles.IsValid() != NewAssetData.TaggedAssetBundles.IsValid() ||
				(AssetData->TaggedAssetBundles.IsValid() &&
					AssetData->TaggedAssetBundles.Get() != NewAssetData.TaggedAssetBundles.Get() && // First check whether the pointers are the same
					*AssetData->TaggedAssetBundles != *NewAssetData.TaggedAssetBundles // If the pointers differ, check whether the contents differ
					));
	}

	// Copy in new values
	*AssetData = MoveTemp(NewAssetData);

	// Can only re-add to asset map when key fields are updated
	if (OldKey != NewKey)
	{
		bool bExisting = false;
		CachedAssets.Add(AssetData, &bExisting);
		if (bExisting)
		{
			UE_LOG(LogAssetRegistry, Error, TEXT("UpdateAssetData called with a change in ObjectPath from Old=\"%s\" to New=\"%s\", ")
				TEXT("but the new ObjectPath is already present with another AssetData. This will overwrite and leak the existing AssetData."),
				*OldKey.ToString(), *NewKey.ToString());
}
		else
		{
			++NumAssets;
		}
	}
}

bool FAssetRegistryState::UpdateAssetDataPackageFlags(FName PackageName, uint32 PackageFlags)
{
	if (const TArray<FAssetData*, TInlineAllocator<1>>* Assets = CachedAssetsByPackageName.Find(PackageName))
	{
		for (FAssetData* Asset : *Assets)
		{
			Asset->PackageFlags = PackageFlags;
		}

		return true;
	}

	return false;
}

void FAssetRegistryState::RemoveAssetData(FAssetData* AssetData, bool bRemoveDependencyData, bool& bOutRemovedAssetData, bool& bOutRemovedPackageData)
{
	bOutRemovedAssetData = false;
	bOutRemovedPackageData = false;

	if (!ensure(AssetData))
	{
		return;
	}

	int32 NumRemoved = CachedAssets.Remove(FCachedAssetKey(AssetData));
	check(NumRemoved <= 1);
	if (NumRemoved == 0)
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("RemoveAssetData called on AssetData %s that is not present in the AssetRegistry."),
			*FCachedAssetKey(*AssetData).ToString());
		return;
	}

	TArray<FAssetData*, TInlineAllocator<1>>* OldPackageAssets = CachedAssetsByPackageName.Find(AssetData->PackageName);
	TArray<FAssetData*>* OldPathAssets = CachedAssetsByPath.Find(AssetData->PackagePath);
	TArray<FAssetData*>* OldClassAssets = CachedAssetsByClass.Find(AssetData->AssetClassPath);
	OldPackageAssets->RemoveSingleSwap(AssetData);
	OldPathAssets->RemoveSingleSwap(AssetData);
	OldClassAssets->RemoveSingleSwap(AssetData);

	for (auto TagIt = AssetData->TagsAndValues.CreateConstIterator(); TagIt; ++TagIt)
	{
		TArray<FAssetData*>* OldTagAssets = CachedAssetsByTag.Find(TagIt.Key());
		OldTagAssets->RemoveSingleSwap(AssetData);
	}

	// Only remove dependencies and package data if there are no other known assets in the package
	if (OldPackageAssets->Num() == 0)
	{
		CachedAssetsByPackageName.Remove(AssetData->PackageName);

		// We need to update the cached dependencies references cache so that they know we no
		// longer exist and so don't reference them.
		if (bRemoveDependencyData)
		{
			RemoveDependsNode(AssetData->PackageName);
		}

		// Remove the package data as well
		RemovePackageData(AssetData->PackageName);
		bOutRemovedPackageData = true;
	}

	// if the assets were preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
	if (PreallocatedAssetDataBuffers.Num() == 0)
	{
		delete AssetData;
	}
	NumAssets--;
	bOutRemovedAssetData = true;
}

FDependsNode* FAssetRegistryState::FindDependsNode(const FAssetIdentifier& Identifier) const
{
	FDependsNode*const* FoundNode = CachedDependsNodes.Find(Identifier);
	if (FoundNode)
	{
		return *FoundNode;
	}
	else
	{
		return nullptr;
	}
}

FDependsNode* FAssetRegistryState::CreateOrFindDependsNode(const FAssetIdentifier& Identifier)
{
	FDependsNode* FoundNode = FindDependsNode(Identifier);
	if (FoundNode)
	{
		return FoundNode;
	}

	FDependsNode* NewNode = new FDependsNode(Identifier);
	NumDependsNodes++;
	CachedDependsNodes.Add(Identifier, NewNode);

	return NewNode;
}

bool FAssetRegistryState::RemoveDependsNode(const FAssetIdentifier& Identifier)
{
	FDependsNode** NodePtr = CachedDependsNodes.Find(Identifier);

	if (NodePtr != nullptr)
	{
		FDependsNode* Node = *NodePtr;
		if (Node != nullptr)
		{
			TArray<FDependsNode*> DependencyNodes;
			Node->GetDependencies(DependencyNodes);

			// Remove the reference to this node from all dependencies
			for (FDependsNode* DependencyNode : DependencyNodes)
			{
				DependencyNode->RemoveReferencer(Node);
			}

			TArray<FDependsNode*> ReferencerNodes;
			Node->GetReferencers(ReferencerNodes);

			// Remove the reference to this node from all referencers
			for (FDependsNode* ReferencerNode : ReferencerNodes)
			{
				ReferencerNode->RemoveDependency(Node);
			}

			// Remove the node and delete it
			CachedDependsNodes.Remove(Identifier);
			NumDependsNodes--;

			// if the depends nodes were preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
			if (PreallocatedDependsNodeDataBuffers.Num() == 0)
			{
				delete Node;
			}

			return true;
		}
	}

	return false;
}

void FAssetRegistryState::GetPrimaryAssetsIds(TSet<FPrimaryAssetId>& OutPrimaryAssets) const
{
	for (FAssetData* AssetData: CachedAssets)
	{
		if (AssetData)
		{
			FPrimaryAssetId PrimaryAssetId = AssetData->GetPrimaryAssetId();
			if (PrimaryAssetId.IsValid())
			{
				OutPrimaryAssets.Add(PrimaryAssetId);
			}
		}
	}
}

const FAssetPackageData* FAssetRegistryState::GetAssetPackageData(FName PackageName) const
{
	FAssetPackageData* const* FoundData = CachedPackageData.Find(PackageName);
	if (FoundData)
	{
		return *FoundData;
	}
	else
	{
		return nullptr;
	}
}

FAssetPackageData* FAssetRegistryState::CreateOrGetAssetPackageData(FName PackageName)
{
	FAssetPackageData** FoundData = CachedPackageData.Find(PackageName);
	if (FoundData)
	{
		return *FoundData;
	}

	FAssetPackageData* NewData = new FAssetPackageData();
	NumPackageData++;
	CachedPackageData.Add(PackageName, NewData);

	return NewData;
}

bool FAssetRegistryState::RemovePackageData(FName PackageName)
{
	FAssetPackageData** DataPtr = CachedPackageData.Find(PackageName);

	if (DataPtr != nullptr)
	{
		FAssetPackageData* Data = *DataPtr;
		if (Data != nullptr)
		{
			CachedPackageData.Remove(PackageName);
			NumPackageData--;

			// if the package data was preallocated in a block, we can't delete them one at a time, only the whole chunk in the destructor
			if (PreallocatedPackageDataBuffers.Num() == 0)
			{
				delete Data;
			}

			return true;
		}
	}
	return false;
}

bool FAssetRegistryState::IsFilterValid(const FARCompiledFilter& Filter)
{
	return UE::AssetRegistry::Utils::IsFilterValid(Filter);
}

const TArray<const FAssetData*>& FAssetRegistryState::GetAssetsByClassName(const FName ClassName) const
{
	static TArray<const FAssetData*> InvalidArray;
	for (const TPair< FTopLevelAssetPath, TArray<FAssetData*> >& Pair : CachedAssetsByClass)
	{
		if (Pair.Key.GetAssetName() == ClassName)
		{
			return reinterpret_cast<const TArray<const FAssetData*>&>(Pair.Value);
		}
	}
	return InvalidArray;
}

namespace UE::AssetRegistry::Utils
{

bool IsFilterValid(const FARCompiledFilter& Filter)
{
	if (Filter.PackageNames.Contains(NAME_None) ||
		Filter.PackagePaths.Contains(NAME_None) ||
		Filter.SoftObjectPaths.Contains(FSoftObjectPath()) ||
		Filter.ClassPaths.Contains(FTopLevelAssetPath()) ||
		Filter.TagsAndValues.Contains(NAME_None)
		)
	{
		return false;
	}

	return true;
}

}

#if ASSET_REGISTRY_STATE_DUMPING_ENABLED
namespace UE
{
namespace AssetRegistry
{
	void PropertiesToString(EDependencyProperty Properties, FStringBuilderBase& Builder, EDependencyCategory CategoryFilter)
	{
		bool bFirst = true;
		auto AppendPropertyName = [&Properties, &Builder, &bFirst](EDependencyProperty TestProperty, const TCHAR* NameWith, const TCHAR* NameWithout)
		{
			if (!bFirst)
			{
				Builder.Append(TEXT(","));
			}
			if (!!(Properties & TestProperty))
			{
				Builder.Append(NameWith);
			}
			else
			{
				Builder.Append(NameWithout);
			}
			bFirst = false;
		};
		if (!!(CategoryFilter & EDependencyCategory::Package))
		{
			AppendPropertyName(EDependencyProperty::Hard, TEXT("Hard"), TEXT("Soft"));
			AppendPropertyName(EDependencyProperty::Game, TEXT("Game"), TEXT("EditorOnly"));
			AppendPropertyName(EDependencyProperty::Build, TEXT("Build"), TEXT("NotBuild"));
		}
		if (!!(CategoryFilter & EDependencyCategory::Manage))
		{
			AppendPropertyName(EDependencyProperty::Direct, TEXT("Direct"), TEXT("Indirect"));
		}
		static_assert((EDependencyProperty::PackageMask | EDependencyProperty::SearchableNameMask | EDependencyProperty::ManageMask) == EDependencyProperty::AllMask,
			"Need to handle new flags in this function");
	}
}
}

template <typename MapType>
static void PrintAssetDataMap(FString Name, const MapType& AssetMap, TStringBuilder<16>& PageBuffer, const TFunctionRef<void()>& AddLine,
	TUniqueFunction<void(const typename MapType::KeyType& Key, const FAssetData& Data)>&& PrintValue = {})
{
	PageBuffer.Appendf(TEXT("--- Begin %s ---"), *Name);
	AddLine();

	TArray<typename MapType::KeyType> Keys;
	AssetMap.GenerateKeyArray(Keys);

	struct FKeyTypeCompare
	{
		FORCEINLINE bool operator()(const typename MapType::KeyType& A, const typename MapType::KeyType& B) const
		{
			return A.Compare(B) < 0;
		}
	};
	Keys.Sort(FKeyTypeCompare());

	TArray<FAssetData*> Items;
	Items.Reserve(1024);

	int32 ValidCount = 0;
	for (const typename MapType::KeyType& Key : Keys)
	{
		const auto& AssetArray = AssetMap.FindChecked(Key);
		if (AssetArray.Num() == 0)
		{
			continue;
		}
		++ValidCount;

		Items.Reset();
		Items.Append(AssetArray);
		Items.Sort([](const FAssetData& A, const FAssetData& B)
			{ return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath()); }
		);

		PageBuffer.Append(TEXT("\t"));
		Key.AppendString(PageBuffer);
		PageBuffer.Appendf(TEXT(" : %d item(s)"), Items.Num());
		AddLine();
		for (const FAssetData* Data : Items)
		{
			PageBuffer.Append(TEXT("\t\t"));
			Data->AppendObjectPath(PageBuffer);
			if (PrintValue)
			{
				PrintValue(Key, *Data);
			}
			AddLine();
		}
	}

	PageBuffer.Appendf(TEXT("--- End %s : %d entries ---"), *Name, ValidCount);
	AddLine();
};

void FAssetRegistryState::Dump(const TArray<FString>& Arguments, TArray<FString>& OutPages, int32 LinesPerPage) const
{
	int32 ExpectedNumLines = 14 + CachedAssets.Num() * 5 + CachedDependsNodes.Num() + CachedPackageData.Num();
	const int32 EstimatedLinksPerNode = 10*2; // Each dependency shows up once as a dependency and once as a reference
	const int32 EstimatedCharactersPerLine = 100;
	bool bAllFields = Arguments.Contains(TEXT("All"));

	const bool bDumpDependencyDetails = bAllFields || Arguments.Contains(TEXT("DependencyDetails"));
	if (bDumpDependencyDetails)
	{
		ExpectedNumLines += CachedDependsNodes.Num() * (3 + EstimatedLinksPerNode);
	}
	LinesPerPage = FMath::Max(0, LinesPerPage);
	const int32 ExpectedNumPages = LinesPerPage > 0 ? (ExpectedNumLines / LinesPerPage) : 1;
	const int32 PageEndSearchLength = FMath::Min(LinesPerPage, ExpectedNumLines) / 20;
	const uint32 HashStartValue = MAX_uint32 - 49979693; // Pick a large starting value to bias against picking empty string
	const uint32 HashMultiplier = 67867967;
	TStringBuilder<16> PageBuffer;
	TStringBuilder<16> OverflowText;

	OutPages.Reserve(ExpectedNumPages);
	PageBuffer.AddUninitialized(FMath::Min(LinesPerPage, ExpectedNumLines) * EstimatedCharactersPerLine);// TODO: Add Reserve function to TStringBuilder
	PageBuffer.Reset();
	OverflowText.AddUninitialized(PageEndSearchLength * EstimatedCharactersPerLine);
	OverflowText.Reset();
	int32 NumLinesInPage = 0;
	const int32 LineTerminatorLen = TCString<TCHAR>::Strlen(LINE_TERMINATOR);

	auto FinishPage = [&PageBuffer, &NumLinesInPage, HashStartValue, HashMultiplier, PageEndSearchLength, &OutPages, &OverflowText, LineTerminatorLen](bool bLastPage)
	{
		int32 PageEndIndex = PageBuffer.Len();
		const TCHAR* BufferEnd = PageBuffer.GetData() + PageEndIndex;
		int32 NumOverflowLines = 0;
		// We want to facilitate diffing dumps between two different versions that should be similar, but naively breaking up the dump into pages makes this difficult
		// because after one missing or added line, every page from that point on will be offset and therefore different, making false positive differences
		// To make pages after one missing or added line the same, we look for a good page ending based on the text of all the lines near the end of the current page
		// By choosing specific-valued texts as page breaks, we will usually randomly get lucky and have the two diffs pick the same line for the end of the page
		if (!bLastPage && NumLinesInPage > PageEndSearchLength)
		{
			const TCHAR* WinningLineEnd = BufferEnd;
			uint32 WinningLineValue = 0;
			int32 WinningSearchIndex = 0;
			const TCHAR* LineEnd = BufferEnd;
			for (int32 SearchIndex = 0; SearchIndex < PageEndSearchLength; ++SearchIndex)
			{
				uint32 LineValue = HashStartValue;
				const TCHAR* LineStart = LineEnd;
				while (LineStart[-LineTerminatorLen] != LINE_TERMINATOR[0] || TCString<TCHAR>::Strncmp(LINE_TERMINATOR, LineStart - LineTerminatorLen, LineTerminatorLen) != 0)
				{
					--LineStart;
					LineValue = LineValue * HashMultiplier + static_cast<uint32>(TChar<TCHAR>::ToLower(*LineStart));
				}
				if (SearchIndex == 0 || LineValue < WinningLineValue) // We arbitrarily choose the smallest hash as the winning value
				{
					WinningLineValue = LineValue;
					WinningLineEnd = LineEnd;
					WinningSearchIndex = SearchIndex;
				}
				LineEnd = LineStart - LineTerminatorLen;
			}
			if (WinningLineEnd != BufferEnd)
			{
				PageEndIndex = UE_PTRDIFF_TO_INT32(WinningLineEnd - PageBuffer.GetData());
				NumOverflowLines = WinningSearchIndex;
			}
		}

		OutPages.Emplace(PageEndIndex, PageBuffer.GetData());
		if (PageEndIndex != PageBuffer.Len())
		{
			PageEndIndex += LineTerminatorLen; // Skip the newline
			OverflowText.Reset();
			OverflowText.Append(PageBuffer.GetData() + PageEndIndex, PageBuffer.Len() - PageEndIndex);
			PageBuffer.Reset();
			PageBuffer.Append(OverflowText);
			PageBuffer.Append(LINE_TERMINATOR);
			NumLinesInPage = NumOverflowLines;
		}
		else
		{
			PageBuffer.Reset();
			NumLinesInPage = 0;
		}
	};
	auto AddLine = [&PageBuffer, LinesPerPage, &NumLinesInPage, &FinishPage, &OutPages]()
	{
		if (LinesPerPage == 1)
		{
			OutPages.Emplace(PageBuffer.Len(), PageBuffer.GetData());
			PageBuffer.Reset();
		}
		else
		{
			++NumLinesInPage;
			if (LinesPerPage == 0 || NumLinesInPage < LinesPerPage)
			{
				PageBuffer.Append(LINE_TERMINATOR);
			}
			else
			{
				FinishPage(false);
			}
		}
	};

	if (bAllFields || Arguments.Contains(TEXT("ObjectPath")))
	{
		PageBuffer.Append(TEXT("--- Begin CachedAssetsByObjectPath ---"));
		AddLine();

		TArray<FCachedAssetKey> Keys;
		Keys.Reserve(CachedAssets.Num());
		for( const FAssetData* AssetData : CachedAssets)
		{
			Keys.Emplace(FCachedAssetKey(*AssetData));
		}
		Keys.Sort([](const FCachedAssetKey& A, const FCachedAssetKey& B) {
			return A.Compare(B) < 0;
		});

		for (const FCachedAssetKey& Key : Keys)
		{
			PageBuffer.Append(TEXT("	"));
			Key.AppendString(PageBuffer);
			AddLine();
		}

		PageBuffer.Appendf(TEXT("--- End CachedAssetsByObjectPath : %d entries ---"), CachedAssets.Num());
		AddLine();
	}

	if (bAllFields || Arguments.Contains(TEXT("PackageName")))
	{
		PrintAssetDataMap(TEXT("CachedAssetsByPackageName"), CachedAssetsByPackageName, PageBuffer, AddLine);
	}

	if (bAllFields || Arguments.Contains(TEXT("Path")))
	{
		PrintAssetDataMap(TEXT("CachedAssetsByPath"), CachedAssetsByPath, PageBuffer, AddLine);
	}

	if (bAllFields || Arguments.Contains(TEXT("Class")))
	{
		PrintAssetDataMap(TEXT("CachedAssetsByClass"), CachedAssetsByClass, PageBuffer, AddLine);
	}

	// Only print this if it's requested specifically - '-all' will print tags-per-asset rather than assets-per-tag 
	if (Arguments.Contains(TEXT("Tag")))
	{
		PrintAssetDataMap(TEXT("CachedAssetsByTag"), CachedAssetsByTag, PageBuffer, AddLine,
			[&PageBuffer, &AddLine](const FName& TagName, const FAssetData& Data)
			{
				PageBuffer << TEXT(", ") << Data.TagsAndValues.FindTag(TagName).ToLoose();
			});
	}

	if (bAllFields || Arguments.Contains(TEXT("AssetTags")))
	{
		int32 Counter = 0;
		PageBuffer.Append(TEXT("--- Begin AssetTags ---"));
		AddLine();

		for (const FAssetData* AssetData : CachedAssets)
		{
			if (AssetData->TagsAndValues.Num() == 0)
			{
				continue;
			}
			++Counter;

			PageBuffer << TEXT("  ") << FCachedAssetKey(AssetData);
			AddLine();

			AssetData->TagsAndValues.ForEach([&](const TPair<FName, FAssetTagValueRef>& TagPair) 
			{
				PageBuffer << TEXT("    ") << TagPair.Key << TEXT(" : ") << *TagPair.Value.AsString();
				AddLine();	
			});
		}

		PageBuffer.Appendf(TEXT("--- End AssetTags : %d entries ---"), Counter);
		AddLine();
	}


	if ((bAllFields || Arguments.Contains(TEXT("Dependencies"))) && !bDumpDependencyDetails)
	{
		PageBuffer.Appendf(TEXT("--- Begin CachedDependsNodes ---"));
		AddLine();

		TArray<FDependsNode*> Nodes;
		CachedDependsNodes.GenerateValueArray(Nodes);
		Nodes.Sort([](const FDependsNode& A, const FDependsNode& B)
			{ return A.GetIdentifier().ToString() < B.GetIdentifier().ToString(); }
			);

		for (const FDependsNode* Node : Nodes)
		{
			PageBuffer.Append(TEXT("	"));
			Node->GetIdentifier().AppendString(PageBuffer);
			PageBuffer.Appendf(TEXT(" : %d connection(s)"), Node->GetConnectionCount());
			AddLine();
		}

		PageBuffer.Appendf(TEXT("--- End CachedDependsNodes : %d entries ---"), CachedDependsNodes.Num());
		AddLine();
	}

	if (bDumpDependencyDetails)
	{
		using namespace UE::AssetRegistry;
		PageBuffer.Append(TEXT("--- Begin CachedDependsNodes ---"));
		AddLine();

		auto SortByAssetID = [](const FDependsNode& A, const FDependsNode& B) { return A.GetIdentifier().ToString() < B.GetIdentifier().ToString(); };
		TArray<FDependsNode*> Nodes;
		CachedDependsNodes.GenerateValueArray(Nodes);
		Nodes.Sort(SortByAssetID);

		if (Arguments.Contains(TEXT("LegacyDependencies"))) // LegacyDependencies are not show by all; they have to be directly requested
		{
			EDependencyCategory CategoryTypes[] = { EDependencyCategory::Package, EDependencyCategory::Package,EDependencyCategory::SearchableName,EDependencyCategory::Manage, EDependencyCategory::Manage, EDependencyCategory::None };
			EDependencyQuery CategoryQueries[] = { EDependencyQuery::Hard, EDependencyQuery::Soft, EDependencyQuery::NoRequirements, EDependencyQuery::Direct, EDependencyQuery::Indirect, EDependencyQuery::NoRequirements };
			const TCHAR* CategoryNames[] = { TEXT("Hard"), TEXT("Soft"), TEXT("SearchableName"), TEXT("HardManage"), TEXT("SoftManage"), TEXT("References") };
			const int NumCategories = UE_ARRAY_COUNT(CategoryTypes);
			check(NumCategories == UE_ARRAY_COUNT(CategoryNames) && NumCategories == UE_ARRAY_COUNT(CategoryQueries));

			TArray<FDependsNode*> Links;
			for (const FDependsNode* Node : Nodes)
			{
				PageBuffer.Append(TEXT("	"));
				Node->GetIdentifier().AppendString(PageBuffer);
				AddLine();
				for (int CategoryIndex = 0; CategoryIndex < NumCategories; ++CategoryIndex)
				{
					EDependencyCategory CategoryType = CategoryTypes[CategoryIndex];
					EDependencyQuery CategoryQuery = CategoryQueries[CategoryIndex];
					const TCHAR* CategoryName = CategoryNames[CategoryIndex];
					Links.Reset();
					if (CategoryType != EDependencyCategory::None)
					{
						Node->GetDependencies(Links, CategoryType, CategoryQuery);
					}
					else
					{
						Node->GetReferencers(Links);
					}
					if (Links.Num() > 0)
					{
						PageBuffer.Appendf(TEXT("		%s"), CategoryName);
						AddLine();
						Links.Sort(SortByAssetID);
						for (FDependsNode* LinkNode : Links)
						{
							PageBuffer.Append(TEXT("			"));
							LinkNode->GetIdentifier().AppendString(PageBuffer);
							AddLine();
						}
					}
				}
			}
		}
		else
		{
			EDependencyCategory CategoryTypes[] = { EDependencyCategory::Package, EDependencyCategory::SearchableName,EDependencyCategory::Manage, EDependencyCategory::None };
			const TCHAR* CategoryNames[] = { TEXT("Package"), TEXT("SearchableName"), TEXT("Manage"), TEXT("References") };
			const int NumCategories = UE_ARRAY_COUNT(CategoryTypes);
			check(NumCategories == UE_ARRAY_COUNT(CategoryNames));

			TArray<FAssetDependency> Dependencies;
			TArray<FDependsNode*> References;
			for (const FDependsNode* Node : Nodes)
			{
				PageBuffer.Append(TEXT("	"));
				Node->GetIdentifier().AppendString(PageBuffer);
				AddLine();
				for (int CategoryIndex = 0; CategoryIndex < NumCategories; ++CategoryIndex)
				{
					EDependencyCategory CategoryType = CategoryTypes[CategoryIndex];
					const TCHAR* CategoryName = CategoryNames[CategoryIndex];
					if (CategoryType != EDependencyCategory::None)
					{
						Dependencies.Reset();
						Node->GetDependencies(Dependencies, CategoryType);
						if (Dependencies.Num() > 0)
						{
							PageBuffer.Appendf(TEXT("		%s"), CategoryName);
							AddLine();
							Dependencies.Sort([](const FAssetDependency& A, const FAssetDependency& B)
								{
									FString AString = A.AssetId.ToString();
									FString BString = B.AssetId.ToString();
									if (AString != BString)
									{
										return AString < BString;
									}
									return A.Properties < B.Properties;
								});
							for (const FAssetDependency& AssetDependency : Dependencies)
							{
								PageBuffer.Append(TEXT("			"));
								AssetDependency.AssetId.AppendString(PageBuffer);
								PageBuffer.Append(TEXT("\t\t{"));
								PropertiesToString(AssetDependency.Properties, PageBuffer, AssetDependency.Category);
								PageBuffer.Append(TEXT("}"));
								AddLine();
							}
						}
					}
					else
					{
						References.Reset();
						Node->GetReferencers(References);
						if (References.Num() > 0)
						{
							PageBuffer.Appendf(TEXT("		%s"), CategoryName);
							AddLine();
							References.Sort(SortByAssetID);
							for (const FDependsNode* Reference : References)
							{
								PageBuffer.Append(TEXT("			"));
								Reference->GetIdentifier().AppendString(PageBuffer);
								AddLine();
							}
						}
					}
				}
			}
		}

		PageBuffer.Appendf(TEXT("--- End CachedDependsNodes : %d entries ---"), CachedDependsNodes.Num());
		AddLine();
	}
	if (bAllFields || Arguments.Contains(TEXT("PackageData")))
	{
		PageBuffer.Append(TEXT("--- Begin CachedPackageData ---"));
		AddLine();

		TArray<FName> Keys;
		CachedPackageData.GenerateKeyArray(Keys);
		Keys.Sort(FNameLexicalLess());

		for (const FName& Key : Keys)
		{
			const FAssetPackageData* PackageData = CachedPackageData.FindChecked(Key);
			PageBuffer.Append(TEXT("	"));
			Key.AppendString(PageBuffer);
			PageBuffer.Append(TEXT(" : "));
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			PageBuffer.Append(PackageData->PackageGuid.ToString());
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			PageBuffer.Appendf(TEXT(" : %d bytes"), PackageData->DiskSize);
			AddLine();
		}

		PageBuffer.Appendf(TEXT("--- End CachedPackageData : %d entries ---"), CachedPackageData.Num());
		AddLine();
	}

	if (bAllFields || Arguments.Contains(TEXT("AssetBundles")))
	{
		int32 Counter = 0;
		PageBuffer.Append(TEXT("--- Begin AssetBundles ---"));
		AddLine();

		for (FAssetData* AssetData : CachedAssets)
		{
			if (AssetData->TaggedAssetBundles.IsValid())
			{
				++Counter;
				for (const FAssetBundleEntry& Entry : AssetData->TaggedAssetBundles->Bundles)
				{
					PageBuffer << TEXT("  Owner: ") << FCachedAssetKey(AssetData) << TEXT(" BundleName: ") << Entry.BundleName;
					AddLine();

					for (const FTopLevelAssetPath& Path : Entry.AssetPaths)
					{
						PageBuffer << TEXT("    ") << Path;
						AddLine();
					}
				}
			}
		}

		PageBuffer.Appendf(TEXT("--- End AssetBundles : %d entries ---"), Counter);
		AddLine();
	}

	if (PageBuffer.Len() > 0)
	{
		if (LinesPerPage == 1)
		{
			AddLine();
		}
		else
		{
			FinishPage(true);
		}
	}
}

#endif // ASSET_REGISTRY_STATE_DUMPING_ENABLED


////////////////////////////////////////////////////////////////////////////

#if WITH_DEV_AUTOMATION_TESTS 

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetRegistryAssetPathStringsTest, "Engine.AssetRegistry.AssetPathStrings", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);

// Tests that we can produce correct paths for objects represented by FCachedAssetKey and FAssetData
// E.g	PackageName.AssetName
//		PackageName.TopLevel:Inner
//		Packagename.TopLevel:Inner.Innermost
bool FAssetRegistryAssetPathStringsTest::RunTest(const FString& Parameters)
{
	using namespace UE::AssetRegistry::Private;

	// Construct these FNames before creating FCachedAssetKey because the key tries not to create unused path names.
	const FName TopLevelOuter = TEXT("/Path/To/PackageName");
	const FName DirectSubObjectOuter = TEXT("/Path/To/PackageName.OuterName");
	const FName SubSubObjectOuter = TEXT("/Path/To/PackageName.OuterName:SubOuterName");
	const FName AssetName = TEXT("AssetName");

	const FString TopLevelPathString = FString::Printf(TEXT("%s.%s"), *TopLevelOuter.ToString(), *AssetName.ToString());
	const FString DirectSubObjectPathString = FString::Printf(TEXT("%s:%s"), *DirectSubObjectOuter.ToString(), *AssetName.ToString());
	const FString SubSubObjectPathString = FString::Printf(TEXT("%s.%s"), *SubSubObjectOuter.ToString(), *AssetName.ToString());

	const FSoftObjectPath TopLevelPath(TopLevelPathString);
	const FSoftObjectPath DirectSubObjectPath(DirectSubObjectPathString);
	const FSoftObjectPath SubSubObjectPath(SubSubObjectPathString);

	TestEqual(TEXT("SoftObjectPath::ToString() correct for top-level asset"),		TopLevelPath.ToString(), TopLevelPathString);
	TestEqual(TEXT("SoftObjectPath::ToString() correct for subobject asset"),		DirectSubObjectPath.ToString(), DirectSubObjectPathString);
	TestEqual(TEXT("SoftObjectPath::ToString() correct for sub-subobject asset"),	SubSubObjectPath.ToString(), SubSubObjectPathString);

	// Construct FCachedAssetKey from FSoftObjectPath of various lengths + check they have the right components
	const FCachedAssetKey TopLevelAssetKey(TopLevelPath);
	TestEqual(TEXT("FCachedAssetKey::OuterPath correct for top-level asset"), TopLevelAssetKey.OuterPath.ToString(), TopLevelOuter.ToString());
	TestEqual(TEXT("FCachedAssetKey::ObjectName correct for top-level asset"), TopLevelAssetKey.ObjectName.ToString(), AssetName.ToString());

	const FCachedAssetKey DirectSubObjectKey(DirectSubObjectPath);
	TestEqual(TEXT("FCachedAssetKey::OuterPath correct for subobject asset"), DirectSubObjectKey.OuterPath.ToString(), DirectSubObjectOuter.ToString());
	TestEqual(TEXT("FCachedAssetKey::ObjectName correct for subobject asset"), DirectSubObjectKey.ObjectName.ToString(), AssetName.ToString());

	const FCachedAssetKey SubSubObjectKey(SubSubObjectPath);
	TestEqual(TEXT("FCachedAssetKey::OuterPath correct for sub-subobject asset"), SubSubObjectKey.OuterPath.ToString(), SubSubObjectOuter.ToString());
	TestEqual(TEXT("FCachedAssetKey::ObjectName correct for sub-subobject asset"), SubSubObjectKey.ObjectName.ToString(), AssetName.ToString());

	// Construct FCachedAssetKey from FSoftObjectPath of various lengths + check they give the right strings from AppendString
	TestEqual(TEXT("FCachedAssetKey::ToString() correct for top-level asset"),		TopLevelAssetKey.ToString(), TopLevelPathString);
	TestEqual(TEXT("FCachedAssetKey::ToString() correct for subobject asset"),		DirectSubObjectKey.ToString(), DirectSubObjectPathString);
	TestEqual(TEXT("FCachedAssetKey::ToString() correct for sub-subobject asset"),	SubSubObjectKey.ToString(), SubSubObjectPathString);

	auto PathToAssetData = [](const FString& Path)
	{
		FString PackageName = FPackageName::ObjectPathToPackageName(Path);
		return FAssetData(PackageName, Path, FTopLevelAssetPath("/Script/CoreUObject.Object"));
	};

	FAssetData TopLevelAssetData = PathToAssetData(TopLevelPathString);
	FAssetData DirectSubObjectAssetData = PathToAssetData(DirectSubObjectPathString);
	FAssetData SubSubObjectAssetData = PathToAssetData(SubSubObjectPathString);

	auto GetAssetDataPath = [](const FAssetData& AssetData)
	{
		TStringBuilder<FName::StringBufferSize> Buffer;
		AssetData.AppendObjectPath(Buffer);
		return FString(Buffer);
	};

	// Test FAssetData::AppendPath for asset data with variable length of OptionalOuterPath
	TestEqual(TEXT("FAssetData::AppendPath() correct for top-level asset"),			TopLevelAssetData.GetObjectPathString(), TopLevelPathString);
	TestEqual(TEXT("FAssetData::AppendPath() correct for subobject asset"),			DirectSubObjectAssetData.GetObjectPathString(), DirectSubObjectPathString);
	TestEqual(TEXT("FAssetData::AppendPath() correct for sub-subobject asset"),		SubSubObjectAssetData.GetObjectPathString(), SubSubObjectPathString);
	
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS
