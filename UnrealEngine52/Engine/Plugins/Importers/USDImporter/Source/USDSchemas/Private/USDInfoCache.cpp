// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDInfoCache.h"

#include "USDGeomMeshConversion.h"
#include "USDGeomXformableTranslator.h"
#include "USDLog.h"
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
	#include "pxr/usd/usdShade/materialBindingAPI.h"
	#include "pxr/usd/usdGeom/pointInstancer.h"
	#include "pxr/usd/usdGeom/subset.h"
#include "USDIncludesEnd.h"
#endif // USE_USD_SDK

static int32 GMaxNumVerticesCollapsedMesh = 5000000;
static FAutoConsoleVariableRef CVarMaxNumVerticesCollapsedMesh(
	TEXT("USD.MaxNumVerticesCollapsedMesh"),
	GMaxNumVerticesCollapsedMesh,
	TEXT("Maximum number of vertices that a combined Mesh can have for us to collapse it into a single StaticMesh"));

namespace UE::UsdInfoCache::Private
{
	struct FUsdPrimInfo
	{
		UE::FSdfPath AssetCollapsedRoot;
		UE::FSdfPath ComponentCollapsedRoot;
		uint64 ExpectedVertexCountForSubtree;
		TArray<UsdUtils::FUsdPrimMaterialSlot> SubtreeMaterialSlots;
	};
}

FArchive& operator <<( FArchive& Ar, UE::UsdInfoCache::Private::FUsdPrimInfo& Info )
{
	Ar << Info.AssetCollapsedRoot;
	Ar << Info.ComponentCollapsedRoot;
	Ar << Info.ExpectedVertexCountForSubtree;
	Ar << Info.SubtreeMaterialSlots;

	return Ar;
}

struct FUsdInfoCache::FUsdInfoCacheImpl
{
	// Information we must have about all prims on the stage
	TMap< UE::FSdfPath, UE::UsdInfoCache::Private::FUsdPrimInfo > InfoMap;
	mutable FRWLock InfoMapLock;

	// Information we may have about a subset of prims
	TMap<UE::FSdfPath, TSet<TWeakObjectPtr<UObject>>> PrimPathToAssets;
	mutable FRWLock PrimPathToAssetsLock;

	// Paths to material prims that are actually used by mesh prims in the scene, given the current settings for
	// render context, material purpose, variant selections, etc.
	TSet<UE::FSdfPath> UsedMaterialPaths;
	mutable FRWLock UsedMaterialPathsLock;
};

FUsdInfoCache::FUsdInfoCache()
{
	Impl = MakeUnique< FUsdInfoCache::FUsdInfoCacheImpl>();
}

FUsdInfoCache::~FUsdInfoCache()
{
}

bool FUsdInfoCache::Serialize( FArchive& Ar )
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		{
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			Ar << ImplPtr->InfoMap;
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
			Ar << ImplPtr->PrimPathToAssets;
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->UsedMaterialPathsLock);
			Ar << ImplPtr->UsedMaterialPaths;
		}
	}

	return true;
}

bool FUsdInfoCache::ContainsInfoAboutPrim( const UE::FSdfPath& Path ) const
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);
		return ImplPtr->InfoMap.Contains( Path );
	}

	return false;
}

bool FUsdInfoCache::IsPathCollapsed( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if ( const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find( Path ) )
		{
			const UE::FSdfPath* CollapsedRoot = CollapsingType == ECollapsingType::Assets
				? &FoundInfo->AssetCollapsedRoot
				: &FoundInfo->ComponentCollapsedRoot;

			// A non-empty path to another prim means this prim is collapsed into that one
			return !CollapsedRoot->IsEmpty() && *CollapsedRoot != Path;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	}

	return false;
}

bool FUsdInfoCache::DoesPathCollapseChildren( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if ( const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find( Path ) )
		{
			const UE::FSdfPath* CollapsedRoot = CollapsingType == ECollapsingType::Assets
				? &FoundInfo->AssetCollapsedRoot
				: &FoundInfo->ComponentCollapsedRoot;

			// We store our own Path in there when we collapse children.
			// Otherwise we hold the path of our collapse root, or empty (in case nothing is collapsed up to here)
			return *CollapsedRoot == Path;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	}

	return false;
}

UE::FSdfPath FUsdInfoCache::UnwindToNonCollapsedPath( const UE::FSdfPath& Path, ECollapsingType CollapsingType ) const
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if ( const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find( Path ) )
		{
			const UE::FSdfPath* CollapsedRoot = CollapsingType == ECollapsingType::Assets
				? &FoundInfo->AssetCollapsedRoot
				: &FoundInfo->ComponentCollapsedRoot;

			// An empty path here means that we are not collapsed at all
			if ( CollapsedRoot->IsEmpty() )
			{
				return Path;
			}
			// Otherwise we have our own path in there (in case we collapse children) or the path to the prim that collapsed us
			else
			{
				return *CollapsedRoot;
			}
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	}

	return Path;
}

bool FUsdInfoCache::IsMaterialUsed(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->UsedMaterialPathsLock);

		return ImplPtr->UsedMaterialPaths.Contains(Path);
	}

	return false;
}

namespace UE::USDInfoCacheImpl::Private
{
#if USE_USD_SDK
	void GetPrimVertexCountAndSlots(
		const pxr::UsdPrim& UsdPrim,
		const FUsdSchemaTranslationContext& Context,
		const FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		const TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot> >& InSubtreeToMaterialSlots,
		uint64& OutVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutMaterialSlots
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
					FReadScopeLock ScopeLock(Impl.InfoMapLock);
					for ( uint32 PrototypeIndex = 0; PrototypeIndex < PrototypePaths.size(); ++PrototypeIndex )
					{
						const pxr::SdfPath& PrototypePath = PrototypePaths[ PrototypeIndex ];

						// Skip invisible prototypes here to mirror how they're skipped within
						// USDGeomMeshConversion.cpp, in the RecursivelyCollapseChildMeshes function. Those two
						// traversals have to match at least with respect to the material slots, so that we can use
						// the data collected here to apply material overrides to the meshes generated for the point
						// instancers when they're collapsed
						pxr::UsdPrim PrototypePrim = UsdPrim.GetStage()->GetPrimAtPath(PrototypePath);
						if (pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable(PrototypePrim))
						{
							if (pxr::UsdAttribute VisibilityAttr = UsdGeomImageable.GetVisibilityAttr())
							{
								pxr::TfToken VisibilityToken;
								if (VisibilityAttr.Get(&VisibilityToken) && VisibilityToken == pxr::UsdGeomTokens->invisible)
								{
									continue;
								}
							}
						}

						// If we're calling this for a point instancer we should have parsed the results for our
						// prototype subtrees already
						if ( const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = Impl.InfoMap.Find( UE::FSdfPath{ PrototypePath } ) )
						{
							PrototypeVertexCounts[ PrototypeIndex ] = FoundInfo->ExpectedVertexCountForSubtree;
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

	void RecursivePropagateVertexAndMaterialSlotCounts(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		const pxr::TfToken& MaterialPurposeToken,
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		FUsdSchemaTranslatorRegistry& Registry,
		TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& InOutSubtreeToMaterialSlots,
		TArray< FString >& InOutPointInstancerPaths,
		uint64& OutSubtreeVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutSubtreeSlots
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE( UE::USDInfoCacheImpl::Private::RecursivePropagateVertexAndMaterialSlotCounts );

		if (!UsdPrim)
		{
			return;
		}

		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};

		if (!UsdPrim.IsPseudoRoot())
		{
			pxr::UsdShadeMaterialBindingAPI BindingAPI{UsdPrim};
			if (BindingAPI)
			{
				if (pxr::UsdShadeMaterial ShadeMaterial = BindingAPI.ComputeBoundMaterial(MaterialPurposeToken))
				{
					FWriteScopeLock ScopeLock(Impl.UsedMaterialPathsLock);
					Impl.UsedMaterialPaths.Add(UE::FSdfPath{ShadeMaterial.GetPrim().GetPath()});
				}
			}
			else if (pxr::UsdRelationship Relationship = UsdPrim.GetRelationship(pxr::UsdShadeTokens->materialBinding))
			{
				pxr::SdfPathVector Targets;
				Relationship.GetTargets(&Targets);

				if (Targets.size() > 0)
				{
					const pxr::SdfPath& TargetMaterialPrimPath = Targets[0];
					pxr::UsdPrim MaterialPrim = UsdPrim.GetStage()->GetPrimAtPath(TargetMaterialPrimPath);
					pxr::UsdShadeMaterial UsdShadeMaterial{MaterialPrim};
					if (UsdShadeMaterial)
					{
						FWriteScopeLock ScopeLock(Impl.UsedMaterialPathsLock);
						Impl.UsedMaterialPaths.Add(UE::FSdfPath{TargetMaterialPrimPath});
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

		ParallelFor( TEXT( "RecursivePropagateVertexAndMaterialSlotCounts" ), Prims.Num(), MinBatchSize,
			[&]( int32 Index )
			{
				RecursivePropagateVertexAndMaterialSlotCounts(
					Prims[ Index ],
					Context,
					MaterialPurposeToken,
					Impl,
					Registry,
					InOutSubtreeToMaterialSlots,
					InOutPointInstancerPaths,
					ChildSubtreeVertexCounts[ Index ],
					ChildSubtreeMaterialSlots[ Index ]
				);
			}
		);

		OutSubtreeVertexCount = 0;
		OutSubtreeSlots.Empty();

		// We will still step into invisible prims to collect all info we can, but we won't count their material slots
		// or vertex counts: The main usage of those counts is to handle collapsed meshes, and during collapse we just
		// early out whenever we encounter an invisible prim
		bool bPrimIsInvisible = false;
		if (pxr::UsdGeomImageable UsdGeomImageable = pxr::UsdGeomImageable(UsdPrim))
		{
			if (pxr::UsdAttribute VisibilityAttr = UsdGeomImageable.GetVisibilityAttr())
			{
				pxr::TfToken VisibilityToken;
				if (VisibilityAttr.Get(&VisibilityToken) && VisibilityToken == pxr::UsdGeomTokens->invisible)
				{
					bPrimIsInvisible = true;
				}
			}
		}

		// If the mesh prim has an unselected geometry purpose, it is also essentially invisible
		if (!EnumHasAllFlags(Context.PurposesToLoad, IUsdPrim::GetPurpose(UsdPrim)))
		{
			bPrimIsInvisible = true;
		}

		bool bIsPointInstancer = false;
		if ( pxr::UsdGeomPointInstancer PointInstancer{ UsdPrim } )
		{
			bIsPointInstancer = true;
		}
		else if (!bPrimIsInvisible)
		{
			GetPrimVertexCountAndSlots(
				UsdPrim,
				Context,
				Impl,
				InOutSubtreeToMaterialSlots,
				OutSubtreeVertexCount,
				OutSubtreeSlots
			);

			for ( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
			{
				OutSubtreeVertexCount += ChildSubtreeVertexCounts[ ChildIndex ];
				OutSubtreeSlots.Append( ChildSubtreeMaterialSlots[ ChildIndex ] );
			}
		}

		{
			FWriteScopeLock ScopeLock(Impl.InfoMapLock);

			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd( UE::FSdfPath{ UsdPrimPath } );

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
				Info.ExpectedVertexCountForSubtree = OutSubtreeVertexCount;
				InOutSubtreeToMaterialSlots.Emplace( UsdPrimPath, OutSubtreeSlots );
			}
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
		const FUsdSchemaTranslationContext& Context,
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		TArray< FString >& PointInstancerPaths,
		TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& InOutSubtreeMaterialSlots
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
					Impl,
					InOutSubtreeMaterialSlots,
					PointInstancerVertexCount,
					PointInstancerMaterialSlots
				);

				FWriteScopeLock Lock{Impl.InfoMapLock};
				UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd( UsdPointInstancerPath );

				Info.ExpectedVertexCountForSubtree = PointInstancerVertexCount;
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

					UE::UsdInfoCache::Private::FUsdPrimInfo& ParentInfo = Impl.InfoMap.FindOrAdd( ParentPath );
					ParentInfo.ExpectedVertexCountForSubtree += PointInstancerVertexCount;

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
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		const TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& SubtreeMaterialSlots,
		bool bMergeIdenticalSlots
	)
	{
		FWriteScopeLock Lock{Impl.InfoMapLock};

		if ( bMergeIdenticalSlots )
		{
			for ( const TPair< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& Pair : SubtreeMaterialSlots )
			{
				TSet<UsdUtils::FUsdPrimMaterialSlot> SlotsSet{ Pair.Value };
				UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd( Pair.Key );
				Info.SubtreeMaterialSlots = SlotsSet.Array();
			}
		}
		else
		{
			for ( const TPair< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& Pair : SubtreeMaterialSlots )
			{
				UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd( Pair.Key );
				Info.SubtreeMaterialSlots = Pair.Value;
			}
		}
	}

	bool CanMeshSubtreeBeCollapsed(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		const TSharedPtr< FUsdSchemaTranslator >& Translator
	)
	{
		if (!UsdPrim)
		{
			return false;
		}

		// Only care about collapsing into a StaticMesh: We should always collapse into a SkeletalMesh as we have
		// no real alternative for handling them
		TSharedPtr<FUsdGeomXformableTranslator> XformableTranslator = StaticCastSharedPtr<FUsdGeomXformableTranslator>(Translator);
		if (!XformableTranslator.IsValid())
		{
			return false;
		}

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();

		UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd(UE::FSdfPath{UsdPrimPath});

		if (Info.ExpectedVertexCountForSubtree > GMaxNumVerticesCollapsedMesh)
		{
			return false;
		}

		return true;
	}

	void RecursiveQueryCollapsesChildren(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		FUsdSchemaTranslatorRegistry& Registry,
		const pxr::SdfPath& AssetCollapsedRoot = pxr::SdfPath::EmptyPath(),
		const pxr::SdfPath& ComponentCollapsedRoot = pxr::SdfPath::EmptyPath()
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::USDInfoCacheImpl::Private::RecursiveQueryCollapsesChildren);
		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};

		// Prevents allocation by referencing instead of copying
		const pxr::SdfPath* AssetCollapsedRootOverride = &AssetCollapsedRoot;
		const pxr::SdfPath* ComponentCollapsedRootOverride = &ComponentCollapsedRoot;

		bool bIsAssetCollapsed = !AssetCollapsedRoot.IsEmpty();
		bool bIsComponentCollapsed = !ComponentCollapsedRoot.IsEmpty();

		if (!bIsAssetCollapsed || !bIsComponentCollapsed)
		{
			if (TSharedPtr< FUsdSchemaTranslator > SchemaTranslator = Registry.CreateTranslatorForSchema(Context.AsShared(), UE::FUsdTyped(UsdPrim)))
			{
				const bool bCanMeshSubtreeBeCollapsed = CanMeshSubtreeBeCollapsed(UsdPrim, Context, Impl, SchemaTranslator);

				if (!bIsAssetCollapsed)
				{
					if (SchemaTranslator->CollapsesChildren(ECollapsingType::Assets) && bCanMeshSubtreeBeCollapsed)
					{
						AssetCollapsedRootOverride = &UsdPrimPath;
					}
				}

				if (!bIsComponentCollapsed)
				{
					if (SchemaTranslator->CollapsesChildren(ECollapsingType::Components) && bCanMeshSubtreeBeCollapsed)
					{
						ComponentCollapsedRootOverride = &UsdPrimPath;
					}
				}
			}
		}

		pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

		TArray<pxr::UsdPrim> Prims;
		for (pxr::UsdPrim Child : PrimChildren)
		{
			Prims.Emplace(Child);
		}

		const int32 MinBatchSize = 1;

		ParallelFor(TEXT("RecursiveQueryCollapsesChildren"), Prims.Num(), MinBatchSize,
			[&](int32 Index)
			{
				RecursiveQueryCollapsesChildren(
					Prims[Index],
					Context,
					Impl,
					Registry,
					*AssetCollapsedRootOverride,
					*ComponentCollapsedRootOverride
				);
			}
		);

		{
			FWriteScopeLock ScopeLock(Impl.InfoMapLock);

			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd(UE::FSdfPath{UsdPrimPath});

			// These paths will be still empty in case nothing has collapsed yet, hold UsdPrimPath in case UsdPrim
			// collapses that type, or hold the path to the collapsed root passed in via our caller, in case we're
			// collapsed
			Info.AssetCollapsedRoot = *AssetCollapsedRootOverride;
			Info.ComponentCollapsedRoot = *ComponentCollapsedRootOverride;
		}
	}
#endif // USE_USD_SDK
}

TOptional<uint64> FUsdInfoCache::GetSubtreeVertexCount( const UE::FSdfPath& Path )
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if ( const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find( Path ) )
		{
			return FoundInfo->ExpectedVertexCountForSubtree;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	}

	return {};
}

TOptional<uint64> FUsdInfoCache::GetSubtreeMaterialSlotCount( const UE::FSdfPath& Path )
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if ( const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find( Path ) )
		{
			return FoundInfo->SubtreeMaterialSlots.Num();
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf( false, TEXT( "Prim path '%s' has not been cached!" ), *Path.GetString() );
	}

	return {};
}

TOptional<TArray<UsdUtils::FUsdPrimMaterialSlot>> FUsdInfoCache::GetSubtreeMaterialSlots(const UE::FSdfPath& Path)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			return FoundInfo->SubtreeMaterialSlots;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return {};
}

void FUsdInfoCache::LinkAssetToPrim(const UE::FSdfPath& Path, UObject* Asset)
{
	if (!Asset)
	{
		return;
	}

	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}
	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Linking asset '%s' to prim '%s'"),
		*Asset->GetPathName(),
		*Path.GetString()
	);

	ImplPtr->PrimPathToAssets.FindOrAdd(Path).Add(Asset);
}

TSet<TWeakObjectPtr<UObject>> FUsdInfoCache::RemoveAllAssetPrimLinks(const UE::FSdfPath& Path)
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	TSet<TWeakObjectPtr<UObject>> Values;
	ImplPtr->PrimPathToAssets.RemoveAndCopyValue(Path, Values);
	return Values;
}

TSet<TWeakObjectPtr<UObject>> FUsdInfoCache::GetAllAssetsForPrim(const UE::FSdfPath& Path) const
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	if (const TSet<TWeakObjectPtr<UObject>>* FoundAssets = ImplPtr->PrimPathToAssets.Find(Path))
	{
		return *FoundAssets;
	}

	return {};
}

UE::FSdfPath FUsdInfoCache::GetPrimForAsset(UObject* Asset) const
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	for (const TPair<UE::FSdfPath, TSet<TWeakObjectPtr<UObject>>>& Pair : ImplPtr->PrimPathToAssets)
	{
		if (Pair.Value.Contains(Asset))
		{
			return Pair.Key;
		}
	}

	return {};
}

TMap<UE::FSdfPath, TSet<TWeakObjectPtr<UObject>>> FUsdInfoCache::GetAllAssetPrimLinks() const
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}

	return ImplPtr->PrimPathToAssets;
}

void FUsdInfoCache::RebuildCacheForSubtree( const UE::FUsdPrim& Prim, FUsdSchemaTranslationContext& Context )
{
#if USE_USD_SDK
	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdInfoCache::RebuildCacheForSubtree );

	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if ( !ImplPtr )
	{
		return;
	}

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

		// Don't call Clear() here as we don't want to get rid of PrimPathToAssets because we're rebuilding the cache,
		// as that info is also linked to the asset cache
		{
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			ImplPtr->InfoMap.Empty();
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->UsedMaterialPathsLock);
			ImplPtr->UsedMaterialPaths.Empty();
		}

		IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked< IUsdSchemasModule >( TEXT( "USDSchemas" ) );
		FUsdSchemaTranslatorRegistry& Registry = UsdSchemasModule.GetTranslatorRegistry();

		TMap< UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>> TempSubtreeSlots;
		TArray< FString > PointInstancerPaths;

		pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
		if (!Context.MaterialPurpose.IsNone())
		{
			MaterialPurposeToken = UnrealToUsd::ConvertToken(*Context.MaterialPurpose.ToString()).Get();
		}

		// Propagate vertex and material slot counts before we query CollapsesChildren because the Xformable
		// translator needs to know when it would generate too large a static mesh
		uint64 SubtreeVertexCount = 0;
		TArray<UsdUtils::FUsdPrimMaterialSlot> SubtreeSlots;
		UE::USDInfoCacheImpl::Private::RecursivePropagateVertexAndMaterialSlotCounts(
			UsdPrim,
			Context,
			MaterialPurposeToken,
			*ImplPtr,
			Registry,
			TempSubtreeSlots,
			PointInstancerPaths,
			SubtreeVertexCount,
			SubtreeSlots
		);

		UE::USDInfoCacheImpl::Private::UpdateInfoForPointInstancers(
			UsdPrim.GetStage(),
			Context,
			*ImplPtr,
			PointInstancerPaths,
			TempSubtreeSlots
		);

		UE::USDInfoCacheImpl::Private::CollectMaterialSlotCounts(
			*ImplPtr,
			TempSubtreeSlots,
			Context.bMergeIdenticalMaterialSlots
		);

		UE::USDInfoCacheImpl::Private::RecursiveQueryCollapsesChildren(
			UsdPrim,
			Context,
			*ImplPtr,
			Registry
		);
	}
#endif // USE_USD_SDK
}

void FUsdInfoCache::Clear()
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		{
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			ImplPtr->InfoMap.Empty();
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
			ImplPtr->PrimPathToAssets.Empty();
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->UsedMaterialPathsLock);
			ImplPtr->UsedMaterialPaths.Empty();
		}
	}
}

bool FUsdInfoCache::IsEmpty()
{
	if ( FUsdInfoCacheImpl* ImplPtr = Impl.Get() )
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);
		return ImplPtr->InfoMap.IsEmpty();
	}

	return true;
}