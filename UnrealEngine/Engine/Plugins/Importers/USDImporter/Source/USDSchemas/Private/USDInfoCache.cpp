// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDInfoCache.h"

#include "USDGeomMeshConversion.h"
#include "USDSchemasModule.h"
#include "USDSchemaTranslator.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"

#include "Async/ParallelFor.h"
#include "Modules/ModuleManager.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
	#include "pxr/usd/sdf/path.h"
	#include "pxr/usd/usd/prim.h"
	#include "pxr/usd/usdGeom/mesh.h"
	#include "pxr/usd/usdGeom/pointInstancer.h"
	#include "pxr/usd/usdGeom/subset.h"
#include "USDIncludesEnd.h"
#endif // USE_USD_SDK

bool FUsdInfoCache::Serialize( FArchive& Ar )
{
	FWriteScopeLock ScopeLock( Lock );
	Ar << AssetPathsToCollapsedRoot;
	Ar << ComponentPathsToCollapsedRoot;
	Ar << ExpectedVertexCountPerSubtree;
	Ar << ExpectedMaterialSlotCountPerSubtree;

	return true;
}

bool FUsdInfoCache::ContainsInfoAboutPrim( const UE::FSdfPath& Path ) const
{
	return AssetPathsToCollapsedRoot.Contains( Path )
		|| ComponentPathsToCollapsedRoot.Contains( Path )
		|| ExpectedVertexCountPerSubtree.Contains( Path )
		|| ExpectedMaterialSlotCountPerSubtree.Contains( Path );
}

bool FUsdInfoCache::IsPathCollapsed( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const
{
	FReadScopeLock ScopeLock( Lock );
	const TMap< UE::FSdfPath, UE::FSdfPath >* MapToUse =
		CollapsingType == ECollapsingType::Assets
			? &AssetPathsToCollapsedRoot
			: &ComponentPathsToCollapsedRoot;

	if ( const UE::FSdfPath* FoundResult = MapToUse->Find( Path ) )
	{
		// A non-empty path to another prim means this prim is collapsed into that one
		return !FoundResult->IsEmpty() && ( *FoundResult ) != Path;
	}

	// This should never happen: We should have cached the entire tree
	ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	return false;
}

bool FUsdInfoCache::DoesPathCollapseChildren( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const
{
	FReadScopeLock ScopeLock( Lock );
	const TMap< UE::FSdfPath, UE::FSdfPath >* MapToUse =
		CollapsingType == ECollapsingType::Assets
			? &AssetPathsToCollapsedRoot
			: &ComponentPathsToCollapsedRoot;

	if ( const UE::FSdfPath* FoundResult = MapToUse->Find( Path ) )
	{
		// We store our own Path in there when we collapse children. Otherwise we hold the path of our collapse root, or empty (in case nothing is collapsed up to here)
		return (*FoundResult) == Path;
	}

	// This should never happen: We should have cached the entire tree
	ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	return false;
}

UE::FSdfPath FUsdInfoCache::UnwindToNonCollapsedPath( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const
{
	FReadScopeLock ScopeLock( Lock );
	const TMap< UE::FSdfPath, UE::FSdfPath >* MapToUse =
		CollapsingType == ECollapsingType::Assets
			? &AssetPathsToCollapsedRoot
			: &ComponentPathsToCollapsedRoot;

	if ( const UE::FSdfPath* FoundResult = MapToUse->Find( Path ) )
	{
		// An empty path here means that we are not collapsed at all
		if ( FoundResult->IsEmpty() )
		{
			return Path;
		}
		// Otherwise we have our own path in there (in case we collapse children) or the path to the prim that collapsed us
		else
		{
			return *FoundResult;
		}
	}

	// This should never happen: We should have cached the entire tree
	ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	return Path;
}

namespace UE::USDInfoCacheImpl::Private
{
#if USE_USD_SDK
	void GetPrimVertexCountAndSlots(
		const pxr::UsdPrim& UsdPrim,
		const FUsdSchemaTranslationContext& Context,
		const TMap< UE::FSdfPath, uint64 >& InSubtreeToVertexCounts,
		const TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot> >& InSubtreeToMaterialSlots,
		uint64& OutVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutMaterialSlots,
		FRWLock& Lock
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( UE::USDInfoCacheImpl::Private::GetPrimVertexCountAndSlots );

		if ( UsdPrim.IsA<pxr::UsdGeomMesh>() || UsdPrim.IsA<pxr::UsdGeomSubset>() )
		{
			if ( pxr::UsdGeomMesh Mesh{ UsdPrim } )
			{
				if ( pxr::UsdAttribute Points = Mesh.GetPointsAttr() )
				{
					pxr::VtArray< pxr::GfVec3f > PointsArray;
					Points.Get( &PointsArray, pxr::UsdTimeCode( Context.Time ) );
					OutVertexCount = PointsArray.size();
				}
			}

			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if ( !Context.RenderContext.IsNone() )
			{
				RenderContextToken = UnrealToUsd::ConvertToken( *Context.RenderContext.ToString() ).Get();
			}

			pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
			if ( !Context.MaterialPurpose.IsNone() )
			{
				MaterialPurposeToken = UnrealToUsd::ConvertToken( *Context.MaterialPurpose.ToString() ).Get();
			}

			const bool bProvideMaterialIndices = false;
			UsdUtils::FUsdPrimMaterialAssignmentInfo LocalInfo = UsdUtils::GetPrimMaterialAssignments(
				UsdPrim,
				Context.Time,
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			);

			OutMaterialSlots.Append( MoveTemp( LocalInfo.Slots ) );
		}
		else if ( pxr::UsdGeomPointInstancer PointInstancer{ UsdPrim } )
		{
			const pxr::UsdRelationship& Prototypes = PointInstancer.GetPrototypesRel();

			pxr::SdfPathVector PrototypePaths;
			if ( Prototypes.GetTargets( &PrototypePaths ) )
			{
				TArray<uint64> PrototypeVertexCounts;
				PrototypeVertexCounts.SetNumZeroed( PrototypePaths.size() );

				{
					FReadScopeLock ScopeLock( Lock );
					for ( uint32 PrototypeIndex = 0; PrototypeIndex < PrototypePaths.size(); ++PrototypeIndex )
					{
						const pxr::SdfPath& PrototypePath = PrototypePaths[ PrototypeIndex ];

						// If we're calling this for a point instancer we should have parsed the results for our
						// prototype subtrees already
						if ( const uint64* FoundPrototypeVertexCount = InSubtreeToVertexCounts.Find( UE::FSdfPath{ PrototypePath } ) )
						{
							PrototypeVertexCounts[ PrototypeIndex ] = *FoundPrototypeVertexCount;
						}

						if ( const TArray<UsdUtils::FUsdPrimMaterialSlot>* FoundPrototypeSlots = InSubtreeToMaterialSlots.Find( UE::FSdfPath{ PrototypePath } ) )
						{
							OutMaterialSlots.Append( *FoundPrototypeSlots );
						}
					}
				}

				if ( pxr::UsdAttribute ProtoIndicesAttr = PointInstancer.GetProtoIndicesAttr() )
				{
					pxr::VtArray<int> ProtoIndicesArr;
					if ( ProtoIndicesAttr.Get( &ProtoIndicesArr, pxr::UsdTimeCode::EarliestTime() ) )
					{
						for ( int ProtoIndex : ProtoIndicesArr )
						{
							OutVertexCount += PrototypeVertexCounts[ ProtoIndex ];
						}
					}
				}
			}
		}
	}

	void RecursiveRebuildCache(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdSchemaTranslatorRegistry& Registry,
		TMap< UE::FSdfPath, UE::FSdfPath >& AssetPathsToCollapsedRoot,
		TMap< UE::FSdfPath, UE::FSdfPath >& ComponentPathsToCollapsedRoot,
		FRWLock& Lock,
		TMap< UE::FSdfPath, uint64 >& InOutSubtreeToVertexCounts,
		TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& InOutSubtreeToMaterialSlots,
		TArray< FString >& InOutPointInstancerPaths,
		uint64& OutSubtreeVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutSubtreeSlots,
		const pxr::SdfPath& AssetCollapsedRoot = pxr::SdfPath::EmptyPath(),
		const pxr::SdfPath& ComponentCollapsedRoot = pxr::SdfPath::EmptyPath()
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( UE::USDInfoCacheImpl::Private::RecursiveRebuildCache );
		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();

		// Prevents allocation by referencing instead of copying
		const pxr::SdfPath* AssetCollapsedRootOverride = &AssetCollapsedRoot;
		const pxr::SdfPath* ComponentCollapsedRootOverride = &ComponentCollapsedRoot;

		bool bIsAssetCollapsed = !AssetCollapsedRoot.IsEmpty();
		bool bIsComponentCollapsed = !ComponentCollapsedRoot.IsEmpty();

		if ( !bIsAssetCollapsed || !bIsComponentCollapsed )
		{
			if ( TSharedPtr< FUsdSchemaTranslator > SchemaTranslator = Registry.CreateTranslatorForSchema( Context.AsShared(), UE::FUsdTyped( UsdPrim ) ) )
			{
				if ( !bIsAssetCollapsed )
				{
					if ( SchemaTranslator->CollapsesChildren( ECollapsingType::Assets ) )
					{
						AssetCollapsedRootOverride = &UsdPrimPath;
					}
				}

				if ( !bIsComponentCollapsed )
				{
					if ( SchemaTranslator->CollapsesChildren( ECollapsingType::Components ) )
					{
						ComponentCollapsedRootOverride = &UsdPrimPath;
					}
				}
			}
		}

		pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren( pxr::UsdTraverseInstanceProxies( pxr::UsdPrimAllPrimsPredicate ) );

		TArray<pxr::UsdPrim> Prims;
		for ( pxr::UsdPrim Child : PrimChildren )
		{
			Prims.Emplace( Child );
		}

		const uint32 NumChildren = Prims.Num();

		TArray<uint64> ChildSubtreeVertexCounts;
		ChildSubtreeVertexCounts.SetNumUninitialized( NumChildren );

		TArray<TArray<UsdUtils::FUsdPrimMaterialSlot>> ChildSubtreeMaterialSlots;
		ChildSubtreeMaterialSlots.SetNum( NumChildren );

		const int32 MinBatchSize = 1;

		ParallelFor( TEXT( "RecursiveRebuildCache" ), Prims.Num(), MinBatchSize,
			[&]( int32 Index )
			{
				RecursiveRebuildCache(
					Prims[ Index ],
					Context,
					Registry,
					AssetPathsToCollapsedRoot,
					ComponentPathsToCollapsedRoot,
					Lock,
					InOutSubtreeToVertexCounts,
					InOutSubtreeToMaterialSlots,
					InOutPointInstancerPaths,
					ChildSubtreeVertexCounts[ Index ],
					ChildSubtreeMaterialSlots[ Index ],
					*AssetCollapsedRootOverride,
					*ComponentCollapsedRootOverride
				);
			}
		);

		OutSubtreeVertexCount = 0;
		OutSubtreeSlots.Empty();

		bool bIsPointInstancer = false;
		if ( pxr::UsdGeomPointInstancer PointInstancer{ UsdPrim } )
		{
			bIsPointInstancer = true;
		}
		else
		{
			GetPrimVertexCountAndSlots(
				UsdPrim,
				Context,
				InOutSubtreeToVertexCounts,
				InOutSubtreeToMaterialSlots,
				OutSubtreeVertexCount,
				OutSubtreeSlots,
				Lock
			);

			for ( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
			{
				OutSubtreeVertexCount += ChildSubtreeVertexCounts[ ChildIndex ];
				OutSubtreeSlots.Append( ChildSubtreeMaterialSlots[ ChildIndex ] );
			}
		}

		{
			FWriteScopeLock ScopeLock( Lock );

			// For point instancers we can't guarantee we parsed the prototypes yet because they
			// could technically be anywhere, so store them here for a later pass
			if ( bIsPointInstancer )
			{
				InOutPointInstancerPaths.Emplace( UE::FSdfPath{ UsdPrimPath }.GetString() );
			}
			// While we will compute the totals for any and all children normally, don't just append the regular
			// traversal vertex count to the point instancer prim itself just yet, as that doesn't really represent
			// what will happen. We'll later do another pass to handle point instancers where we'll properly instance
			// stuff, and then we'll updadte all ancestors
			else
			{
				InOutSubtreeToVertexCounts.Emplace( UsdPrimPath, OutSubtreeVertexCount );
				InOutSubtreeToMaterialSlots.Emplace( UsdPrimPath, OutSubtreeSlots );
			}

			// These paths will be still empty in case nothing has collapsed yet, hold UsdPrimPath in case UsdPrim
			// collapses that type, or hold the path to the collapsed root passed in via our caller, in case we're
			// collapsed
			AssetPathsToCollapsedRoot.Emplace( UsdPrimPath, *AssetCollapsedRootOverride );
			ComponentPathsToCollapsedRoot.Emplace( UsdPrimPath, *ComponentCollapsedRootOverride );

		}
	}

	/**
	 * Updates the subtree counts with point instancer instancing info.
	 *
	 * This has to be done outside of the main recursion because point instancers may reference any prim in the
	 * stage to be their prototypes (including other point instancers), so we must first parse the entire
	 * stage (forcing point instancer vertex/material slot counts to zero), and only then use the parsed counts
	 * of prim subtrees all over to build the final counts of point instancers that use them as prototypes, and
	 * then update their parents.
	 */
	void UpdateInfoForPointInstancers(
		const pxr::UsdStageRefPtr& Stage,
		TArray< FString >& PointInstancerPaths,
		const FUsdSchemaTranslationContext& Context,
		TMap< UE::FSdfPath, uint64 >& InOutExpectedVertexCountPerSubtree,
		TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& InOutSubtreeMaterialSlots,
		FRWLock& Lock
	)
	{
		// We must sort point instancers in a particular order in case they depend on each other.
		// At least we know that an ordering like this should be possible, because A with B as a prototype and B with A
		// as a prototype leads to an invalid USD stage.
		TFunction<bool(const FString&, const FString&)> SortFunction = [Stage]( const FString& LHS, const FString& RHS )
		{
			FScopedUsdAllocs Allocs;

			pxr::SdfPath LPath = UnrealToUsd::ConvertPath( *LHS ).Get();
			pxr::SdfPath RPath = UnrealToUsd::ConvertPath( *RHS ).Get();

			pxr::UsdGeomPointInstancer LPointInstancer = pxr::UsdGeomPointInstancer{ Stage->GetPrimAtPath( LPath ) };
			pxr::UsdGeomPointInstancer RPointInstancer = pxr::UsdGeomPointInstancer{ Stage->GetPrimAtPath( RPath ) };
			if ( LPointInstancer && RPointInstancer )
			{
				const pxr::UsdRelationship& LPrototypes = LPointInstancer.GetPrototypesRel();
				pxr::SdfPathVector LPrototypePaths;
				if ( LPrototypes.GetTargets( &LPrototypePaths ) )
				{
					for ( const pxr::SdfPath& LPrototypePath : LPrototypePaths )
					{
						// Consider RPointInstancer at RPath "/LPointInstancer/Prototypes/Nest/RPointInstancer", and
						// LPointInstancer has prototype "/LPointInstancer/Prototypes/Nest". If RPath has the LPrototypePath as prefix,
						// we should have R come before L in the sort order.
						// Of course, in this scenario we could get away with just sorting by length, but that wouldn't help if the
						// point instancers were not inside each other (e.g. siblings).
						if ( RPath.HasPrefix( LPrototypePath ) )
						{
							return false;
						}
					}

					// Give it the benefit of the doubt here and say that if R doesn't *need* to come before L, let's ensure L
					// goes before R just in case
					return true;
				}
			}

			return LHS < RHS;
		};
		PointInstancerPaths.Sort(SortFunction);

		for ( const FString& PointInstancerPath : PointInstancerPaths )
		{
			UE::FSdfPath UsdPointInstancerPath{ *PointInstancerPath };

			if ( pxr::UsdPrim PointInstancer = Stage->GetPrimAtPath( UnrealToUsd::ConvertPath( *PointInstancerPath ).Get() ) )
			{
				uint64 PointInstancerVertexCount = 0;
				TArray<UsdUtils::FUsdPrimMaterialSlot> PointInstancerMaterialSlots;

				GetPrimVertexCountAndSlots(
					PointInstancer,
					Context,
					InOutExpectedVertexCountPerSubtree,
					InOutSubtreeMaterialSlots,
					PointInstancerVertexCount,
					PointInstancerMaterialSlots,
					Lock
				);

				InOutExpectedVertexCountPerSubtree.Emplace( UsdPointInstancerPath, PointInstancerVertexCount );
				InOutSubtreeMaterialSlots.Emplace( UsdPointInstancerPath, PointInstancerMaterialSlots );

				// Now that we have info on the point instancer itself, update the counts of all ancestors.
				// Note: The vertex/material slot count for the entire point instancer subtree are just the counts
				// for the point instancer itself, as we stop regular traversal when we hit them
				UE::FSdfPath ParentPath = UsdPointInstancerPath.GetParentPath();
				pxr::UsdPrim Prim = Stage->GetPrimAtPath( ParentPath );
				while ( Prim )
				{
					// If our ancestor is a point instancer itself, just abort as we'll only get the actual counts
					// when we handle that ancestor directly. We don't want to update the ancestor point instancer's
					// ancestors with incorrect values
					if ( Prim.IsA<pxr::UsdGeomPointInstancer>() )
					{
						break;
					}

					InOutExpectedVertexCountPerSubtree[ ParentPath ] += PointInstancerVertexCount;
					InOutSubtreeMaterialSlots[ ParentPath ].Append( PointInstancerMaterialSlots );

					// Break only here so we update the pseudoroot too
					if ( Prim.IsPseudoRoot() )
					{
						break;
					}

					ParentPath = ParentPath.GetParentPath();
					Prim = Stage->GetPrimAtPath( ParentPath );
				}
			}
		}
	}

	/**
	 * Condenses our collected material slots for all subtress (SubtreeMaterialSlots) into just material slot counts,
	 * (OutMaterialSlotCounts), according to bMergeIdenticalSlots.
	 *
	 * We do this after the main pass because then the main material slot collecting code on
	 * the main recursive pass just adds them to arrays, and we're allowed to handle bMergeIdenticalSlots
	 * only here.
	 */
	void CollectMaterialSlotCounts(
		const TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& SubtreeMaterialSlots,
		TMap< UE::FSdfPath, uint64 >& OutMaterialSlotCounts,
		bool bMergeIdenticalSlots
	)
	{
		if ( bMergeIdenticalSlots )
		{
			for ( const TPair< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& Pair : SubtreeMaterialSlots )
			{
				TSet<UsdUtils::FUsdPrimMaterialSlot> SlotsSet{ Pair.Value };
				OutMaterialSlotCounts.FindOrAdd( Pair.Key ) = SlotsSet.Num();
			}
		}
		else
		{
			for ( const TPair< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& Pair : SubtreeMaterialSlots )
			{
				OutMaterialSlotCounts.FindOrAdd( Pair.Key ) = Pair.Value.Num();
			}
		}
	}
#endif // USE_USD_SDK
}

TOptional<uint64> FUsdInfoCache::GetSubtreeVertexCount( const UE::FSdfPath& Path )
{
	if ( uint64* FoundCount = ExpectedVertexCountPerSubtree.Find( Path ) )
	{
		return *FoundCount;
	}

	return {};
}

TOptional<uint64> FUsdInfoCache::GetSubtreeMaterialSlotCount( const UE::FSdfPath& Path )
{
	if ( uint64* FoundCount = ExpectedMaterialSlotCountPerSubtree.Find( Path ) )
	{
		return *FoundCount;
	}

	return {};
}

void FUsdInfoCache::RebuildCacheForSubtree( const UE::FUsdPrim& Prim, FUsdSchemaTranslationContext& Context )
{
#if USE_USD_SDK
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdInfoCache::RebuildCacheForSubtree );

	// We can't deallocate our info cache pointer with the Usd allocator
	FScopedUnrealAllocs UEAllocs;

	// We don't want the translation context to try using its info cache during the rebuild process, as that's the entire point
	TGuardValue< TSharedPtr<FUsdInfoCache> > Guard{ Context.InfoCache, nullptr };
	{
		FScopedUsdAllocs Allocs;

		pxr::UsdPrim UsdPrim{Prim};
		if ( !UsdPrim )
		{
			return;
		}

		Clear();

		IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked< IUsdSchemasModule >( TEXT( "USDSchemas" ) );
		FUsdSchemaTranslatorRegistry& Registry = UsdSchemasModule.GetTranslatorRegistry();

		TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>> TempSubtreeSlots;
		TArray< FString > PointInstancerPaths;

		uint64 SubtreeVertexCount = 0;
		TArray<UsdUtils::FUsdPrimMaterialSlot> SubtreeSlots;
		UE::USDInfoCacheImpl::Private::RecursiveRebuildCache(
			UsdPrim,
			Context,
			Registry,
			AssetPathsToCollapsedRoot,
			ComponentPathsToCollapsedRoot,
			Lock,
			ExpectedVertexCountPerSubtree,
			TempSubtreeSlots,
			PointInstancerPaths,
			SubtreeVertexCount,
			SubtreeSlots
		);

		UE::USDInfoCacheImpl::Private::UpdateInfoForPointInstancers(
			UsdPrim.GetStage(),
			PointInstancerPaths,
			Context,
			ExpectedVertexCountPerSubtree,
			TempSubtreeSlots,
			Lock
		);

		UE::USDInfoCacheImpl::Private::CollectMaterialSlotCounts(
			TempSubtreeSlots,
			ExpectedMaterialSlotCountPerSubtree,
			Context.bMergeIdenticalMaterialSlots
		);
	}
#endif // USE_USD_SDK
}

void FUsdInfoCache::Clear()
{
	FWriteScopeLock ScopeLock( Lock );

	AssetPathsToCollapsedRoot.Empty();
	ComponentPathsToCollapsedRoot.Empty();
	ExpectedVertexCountPerSubtree.Empty();
	ExpectedMaterialSlotCountPerSubtree.Empty();
}

bool FUsdInfoCache::IsEmpty()
{
	FReadScopeLock ScopeLock( Lock );

	return AssetPathsToCollapsedRoot.IsEmpty()
		&& ComponentPathsToCollapsedRoot.IsEmpty()
		&& ExpectedVertexCountPerSubtree.IsEmpty()
		&& ExpectedMaterialSlotCountPerSubtree.IsEmpty();
}