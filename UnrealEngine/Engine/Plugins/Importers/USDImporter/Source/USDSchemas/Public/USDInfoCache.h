// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "CoreMinimal.h"

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

/**
 * Caches whether given prim paths represent prims that can have their assets or components collapsed when importing/opening
 * a USD Stage.
 */
struct USDSCHEMAS_API FUsdInfoCache
{
	bool Serialize( FArchive& Ar );

	// Returns whether we contain any info about prim at 'Path' at all
	bool ContainsInfoAboutPrim( const UE::FSdfPath& Path ) const;

	bool IsPathCollapsed( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const;
	bool DoesPathCollapseChildren( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const;

	// Returns Path in case it represents an uncollapsed prim, or returns the path to the prim that collapsed it
	UE::FSdfPath UnwindToNonCollapsedPath( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const;

	// Provides the total vertex or material slots counts for each prim *and* its subtree.
	// This is built inside RebuildCacheForSubtree, so it will factor in the used Context's bMergeIdenticalMaterialSlots.
	// Note that these aren't affected by actual collapsing: A prim that doesn't collapse its children will still
	// provide the total sum of vertex counts of its entire subtree when queried
	TOptional<uint64> GetSubtreeVertexCount( const UE::FSdfPath& Path );
	TOptional<uint64> GetSubtreeMaterialSlotCount( const UE::FSdfPath& Path );

	void RebuildCacheForSubtree( const UE::FUsdPrim& Prim, FUsdSchemaTranslationContext& Context );

	void Clear();
	bool IsEmpty();

private:
	mutable FRWLock Lock;

	// TODO: Maybe use an impl class instead so that we can also clean up some of the parameters of the implementations
	// TODO: Combine these maps into a single map to a struct with all this info
	TMap< UE::FSdfPath, UE::FSdfPath > AssetPathsToCollapsedRoot;
	TMap< UE::FSdfPath, UE::FSdfPath > ComponentPathsToCollapsedRoot;
	TMap< UE::FSdfPath, uint64 > ExpectedVertexCountPerSubtree;
	TMap< UE::FSdfPath, uint64 > ExpectedMaterialSlotCountPerSubtree;
};