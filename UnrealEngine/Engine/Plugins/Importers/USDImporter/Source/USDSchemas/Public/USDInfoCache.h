// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

#include "UsdWrappers/SdfPath.h"

struct FUsdSchemaTranslationContext;
namespace UE
{
	class FSdfPath;
	class FUsdPrim;
}

enum class ECollapsingType
{
	Assets,
	Components
};

namespace UsdUtils
{
	struct FUsdPrimMaterialSlot;
}

/**
 * Caches information about a specific USD Stage
 */
class USDSCHEMAS_API FUsdInfoCache
{
public:
	struct FUsdInfoCacheImpl;

	FUsdInfoCache();
	FUsdInfoCache(const FUsdInfoCache& Other);
	virtual ~FUsdInfoCache();

	bool Serialize(FArchive& Ar);

	// Returns whether we contain any info about prim at 'Path' at all
	bool ContainsInfoAboutPrim(const UE::FSdfPath& Path) const;

	// Returns a list of all prims we have generic info about
	TSet<UE::FSdfPath> GetKnownPrims() const;

	void RebuildCacheForSubtree(const UE::FUsdPrim& Prim, FUsdSchemaTranslationContext& Context);

	void Clear();
	bool IsEmpty();

public:
	bool IsPathCollapsed(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const;
	bool DoesPathCollapseChildren(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const;

	// Returns Path in case it represents an uncollapsed prim, or returns the path to the prim that collapsed it
	UE::FSdfPath UnwindToNonCollapsedPath(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const;

public:
	// Returns the paths to prims that, when translated into assets or components, also require reading the prim at
	// 'Path'. e.g. providing the path to a Shader prim will return the paths to all Material prims for which the
	// translation involves reading that particular Shader.
	TSet<UE::FSdfPath> GetMainPrims(const UE::FSdfPath& AuxPrimPath) const;

	// The inverse of the function above: Provide it with the path to a Material prim and it will return the set of
	// paths to all Shader prims that need to be read to translate that Material prim into material assets
	TSet<UE::FSdfPath> GetAuxiliaryPrims(const UE::FSdfPath& MainPrimPath) const;

public:
	// Returns the set of paths to all prims that have a material:binding relationship to the particular material at
	// 'Path', if any.
	// Returns a copy for thread safety.
	TSet<UE::FSdfPath> GetMaterialUsers(const UE::FSdfPath& Path) const;
	bool IsMaterialUsed(const UE::FSdfPath& Path) const;

public:
	// Provides the total vertex or material slots counts for each prim *and* its subtree.
	// This is built inside RebuildCacheForSubtree, so it will factor in the used Context's bMergeIdenticalMaterialSlots.
	// Note that these aren't affected by actual collapsing: A prim that doesn't collapse its children will still
	// provide the total sum of vertex counts of its entire subtree when queried
	TOptional<uint64> GetSubtreeVertexCount(const UE::FSdfPath& Path);
	TOptional<uint64> GetSubtreeMaterialSlotCount(const UE::FSdfPath& Path);
	TOptional<TArray<UsdUtils::FUsdPrimMaterialSlot>> GetSubtreeMaterialSlots(const UE::FSdfPath& Path);

	// Returns true if Path could potentially be collapsed as a Geometry Cache asset
	bool IsPotentialGeometryCacheRoot(const UE::FSdfPath& Path) const;

public:
	void LinkAssetToPrim(const UE::FSdfPath& Path, UObject* Asset);
	void UnlinkAssetFromPrim(const UE::FSdfPath& Path, UObject* Asset);

	TArray<TWeakObjectPtr<UObject>> RemoveAllAssetPrimLinks(const UE::FSdfPath& Path);
	TArray<UE::FSdfPath> RemoveAllAssetPrimLinks(const UObject* Asset);
	void RemoveAllAssetPrimLinks();

	TArray<TWeakObjectPtr<UObject>> GetAllAssetsForPrim(const UE::FSdfPath& Path) const;

	template<typename T = UObject>
	T* GetSingleAssetForPrim(const UE::FSdfPath& Path) const
	{
		TArray<TWeakObjectPtr<UObject>> Assets = GetAllAssetsForPrim(Path);

		// Search back to front so that if we generate a new version of an asset type we prefer
		// returning that
		for (int32 Index = Assets.Num() - 1; Index >= 0; --Index)
		{
			if (T* CastAsset = Cast<T>(Assets[Index].Get()))
			{
				return CastAsset;
			}
		}

		return nullptr;
	}

	template<typename T>
	TArray<T*> GetAssetsForPrim(const UE::FSdfPath& Path) const
	{
		TArray<TWeakObjectPtr<UObject>> Assets = GetAllAssetsForPrim(Path);

		TArray<T*> CastAssets;
		CastAssets.Reserve(Assets.Num());

		for (const TWeakObjectPtr<UObject>& Asset : Assets)
		{
			if (T* CastAsset = Cast<T>(Asset.Get()))
			{
				CastAssets.Add(CastAsset);
			}
		}

		return CastAssets;
	}

	TArray<UE::FSdfPath> GetPrimsForAsset(UObject* Asset) const;
	TMap<UE::FSdfPath, TArray<TWeakObjectPtr<UObject>>> GetAllAssetPrimLinks() const;

private:
	TUniquePtr<FUsdInfoCacheImpl> Impl;
};
