// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDInfoCache.h"

#include "USDGeomMeshConversion.h"
#include "USDLog.h"
#include "USDSchemasModule.h"
#include "USDSchemaTranslator.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"

#include "Async/ParallelFor.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#if USE_USD_SDK
#include "USDIncludesStart.h"
#include "pxr/usd/pcp/layerStack.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/collectionAPI.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/primCompositionQuery.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/pointInstancer.h"
#include "pxr/usd/usdGeom/scope.h"
#include "pxr/usd/usdGeom/subset.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/root.h"
#include "USDIncludesEnd.h"
#endif	  // USE_USD_SDK

static int32 GMaxNumVerticesCollapsedMesh = 5000000;
static FAutoConsoleVariableRef CVarMaxNumVerticesCollapsedMesh(
	TEXT("USD.MaxNumVerticesCollapsedMesh"),
	GMaxNumVerticesCollapsedMesh,
	TEXT("Maximum number of vertices that a combined Mesh can have for us to collapse it into a single StaticMesh")
);

// Can toggle on/off to compare performance with StaticMesh instead of GeometryCache
static bool GUseGeometryCacheUSD = true;
static FAutoConsoleVariableRef CVarUsdUseGeometryCache(
	TEXT("USD.GeometryCache.Enable"),
	GUseGeometryCacheUSD,
	TEXT("Use GeometryCache instead of static meshes for loading animated meshes")
);

static int32 GGeometryCacheMaxDepth = 15;
static FAutoConsoleVariableRef CVarGeometryCacheMaxDepth(
	TEXT("USD.GeometryCache.MaxDepth"),
	GGeometryCacheMaxDepth,
	TEXT("Maximum distance between an animated mesh prim to its collapsed geometry cache root")
);

namespace UE::UsdInfoCache::Private
{
	// Flags to hint at the state of a prim for the purpose of geometry cache
	enum class EGeometryCachePrimState : uint8
	{
		None = 0x00,
		Uncollapsible = 0x01,		   // prim cannot be collapsed as part of a geometry cache
		Mesh = 0x02,				   // prim is a mesh, animated or not
		Xform = 0x04,				   // prim is a xform, animated or not
		Collapsible = Mesh | Xform,	   // only meshes and xforms can be collapsed into a geometry cache
		ValidRoot = 0x08			   // prim can collapse itself and its children into a geometry cache
	};
	ENUM_CLASS_FLAGS(EGeometryCachePrimState)

	struct FUsdPrimInfo
	{
		UE::FSdfPath AssetCollapsedRoot;
		UE::FSdfPath ComponentCollapsedRoot;
		uint64 ExpectedVertexCountForSubtree = 0;
		TArray<UsdUtils::FUsdPrimMaterialSlot> SubtreeMaterialSlots;
		int32 GeometryCacheDepth = -1;
		EGeometryCachePrimState GeometryCacheState = EGeometryCachePrimState::None;
	};
}

FArchive& operator<<(FArchive& Ar, UE::UsdInfoCache::Private::FUsdPrimInfo& Info)
{
	Ar << Info.AssetCollapsedRoot;
	Ar << Info.ComponentCollapsedRoot;
	Ar << Info.ExpectedVertexCountForSubtree;
	Ar << Info.SubtreeMaterialSlots;
	Ar << Info.GeometryCacheDepth;
	Ar << Info.GeometryCacheState;

	return Ar;
}

struct FUsdInfoCache::FUsdInfoCacheImpl
{
	FUsdInfoCacheImpl()
		: AllowedExtensionsForGeometryCacheSource(UnrealUSDWrapper::GetNativeFileFormats())
	{
		AllowedExtensionsForGeometryCacheSource.Add(TEXT("abc"));
	}

	FUsdInfoCacheImpl(const FUsdInfoCacheImpl& Other)
		: FUsdInfoCacheImpl()
	{
		// Probably not necessary
		FReadScopeLock ScopedInfoMapLock(Other.InfoMapLock);
		FReadScopeLock ScopedPrimPathToAssetsLock(Other.PrimPathToAssetsLock);
		FReadScopeLock ScopedMaterialUsersLock(Other.MaterialUsersLock);
		FReadScopeLock ScopedAuxPrimsLock(Other.AuxiliaryPrimsLock);

		InfoMap = Other.InfoMap;
		PrimPathToAssets = Other.PrimPathToAssets;
		AssetToPrimPaths = Other.AssetToPrimPaths;
		MaterialUsers = Other.MaterialUsers;
		AuxToMainPrims = Other.AuxToMainPrims;
		MainToAuxPrims = Other.MainToAuxPrims;
		AllowedExtensionsForGeometryCacheSource = Other.AllowedExtensionsForGeometryCacheSource;
	}

	// Information we must have about all prims on the stage
	TMap<UE::FSdfPath, UE::UsdInfoCache::Private::FUsdPrimInfo> InfoMap;
	mutable FRWLock InfoMapLock;

	// Information we may have about a subset of prims
	TMap<UE::FSdfPath, TArray<TWeakObjectPtr<UObject>>> PrimPathToAssets;
	TMap<TWeakObjectPtr<UObject>, TArray<UE::FSdfPath>> AssetToPrimPaths;
	mutable FRWLock PrimPathToAssetsLock;

	// Paths to material prims to the mesh prims they are bound to in the scene, given the current settings for
	// render context, material purpose, variant selections, etc.
	TMap<UE::FSdfPath, TSet<UE::FSdfPath>> MaterialUsers;
	mutable FRWLock MaterialUsersLock;

	// Maps from prims, to all the prims that require also reading this prim to be translated into an asset.
	// Mainly used to update these assets whenever the depencency prim is updated.
	TMap<UE::FSdfPath, TSet<UE::FSdfPath>> AuxToMainPrims;
	TMap<UE::FSdfPath, TSet<UE::FSdfPath>> MainToAuxPrims;
	mutable FRWLock AuxiliaryPrimsLock;

	// Geometry cache can come from a reference or payload of these file types
	TArray<FString> AllowedExtensionsForGeometryCacheSource;

public:
	void RegisterAuxiliaryPrims(const UE::FSdfPath& MainPrimPath, const TSet<UE::FSdfPath>& AuxPrimPaths)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RegisterAuxiliaryPrims);

		if (AuxPrimPaths.Num() == 0)
		{
			return;
		}

		FWriteScopeLock ScopeLock(AuxiliaryPrimsLock);

		MainToAuxPrims.FindOrAdd(MainPrimPath).Append(AuxPrimPaths);

		for (const UE::FSdfPath& AuxPrimPath : AuxPrimPaths)
		{
			UE_LOG(LogUsd, Verbose, TEXT("Registering main prim '%s' and aux prim '%s'"), *MainPrimPath.GetString(), *AuxPrimPath.GetString());

			AuxToMainPrims.FindOrAdd(AuxPrimPath).Add(MainPrimPath);
		}
	}
};

FUsdInfoCache::FUsdInfoCache()
{
	Impl = MakeUnique<FUsdInfoCache::FUsdInfoCacheImpl>();
}

FUsdInfoCache::FUsdInfoCache(const FUsdInfoCache& Other)
{
	Impl = MakeUnique<FUsdInfoCache::FUsdInfoCacheImpl>(*Other.Impl);
}

FUsdInfoCache::~FUsdInfoCache()
{
}

bool FUsdInfoCache::Serialize(FArchive& Ar)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		{
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			Ar << ImplPtr->InfoMap;
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
			Ar << ImplPtr->PrimPathToAssets;
			Ar << ImplPtr->AssetToPrimPaths;
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->MaterialUsersLock);
			Ar << ImplPtr->MaterialUsers;
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->AuxiliaryPrimsLock);
			Ar << ImplPtr->AuxToMainPrims;
			Ar << ImplPtr->MainToAuxPrims;
		}
	}

	return true;
}

bool FUsdInfoCache::ContainsInfoAboutPrim(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);
		return ImplPtr->InfoMap.Contains(Path);
	}

	return false;
}

TSet<UE::FSdfPath> FUsdInfoCache::GetKnownPrims() const
{
	TSet<UE::FSdfPath> Result;

	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		ImplPtr->InfoMap.GetKeys(Result);
		return Result;
	}

	return Result;
}

bool FUsdInfoCache::IsPathCollapsed(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			const UE::FSdfPath* CollapsedRoot = CollapsingType == ECollapsingType::Assets ? &FoundInfo->AssetCollapsedRoot
																						  : &FoundInfo->ComponentCollapsedRoot;

			// A non-empty path to another prim means this prim is collapsed into that one
			return !CollapsedRoot->IsEmpty() && *CollapsedRoot != Path;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return false;
}

bool FUsdInfoCache::DoesPathCollapseChildren(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			const UE::FSdfPath* CollapsedRoot = CollapsingType == ECollapsingType::Assets ? &FoundInfo->AssetCollapsedRoot
																						  : &FoundInfo->ComponentCollapsedRoot;

			// We store our own Path in there when we collapse children.
			// Otherwise we hold the path of our collapse root, or empty (in case nothing is collapsed up to here)
			return *CollapsedRoot == Path;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return false;
}

UE::FSdfPath FUsdInfoCache::UnwindToNonCollapsedPath(const UE::FSdfPath& Path, ECollapsingType CollapsingType) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			const UE::FSdfPath* CollapsedRoot = CollapsingType == ECollapsingType::Assets ? &FoundInfo->AssetCollapsedRoot
																						  : &FoundInfo->ComponentCollapsedRoot;

			// An empty path here means that we are not collapsed at all
			if (CollapsedRoot->IsEmpty())
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
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return Path;
}

TSet<UE::FSdfPath> FUsdInfoCache::GetMainPrims(const UE::FSdfPath& AuxPrimPath) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->AuxiliaryPrimsLock);

		if (const TSet<UE::FSdfPath>* FoundMainPrims = ImplPtr->AuxToMainPrims.Find(AuxPrimPath))
		{
			TSet<UE::FSdfPath> Result = *FoundMainPrims;
			Result.Add(AuxPrimPath);
			return Result;
		}
	}

	return {AuxPrimPath};
}

TSet<UE::FSdfPath> FUsdInfoCache::GetAuxiliaryPrims(const UE::FSdfPath& MainPrimPath) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->AuxiliaryPrimsLock);

		if (const TSet<UE::FSdfPath>* FoundAuxPrims = ImplPtr->MainToAuxPrims.Find(MainPrimPath))
		{
			TSet<UE::FSdfPath> Result = *FoundAuxPrims;
			Result.Add(MainPrimPath);
			return Result;
		}
	}

	return {MainPrimPath};
}

TSet<UE::FSdfPath> FUsdInfoCache::GetMaterialUsers(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->MaterialUsersLock);

		if (const TSet<UE::FSdfPath>* FoundUsers = ImplPtr->MaterialUsers.Find(Path))
		{
			return *FoundUsers;
		}
	}

	return {};
}

bool FUsdInfoCache::IsMaterialUsed(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->MaterialUsersLock);

		return ImplPtr->MaterialUsers.Contains(Path);
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
		const TMap<UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& InSubtreeToMaterialSlots,
		uint64& OutVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutMaterialSlots
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::USDInfoCacheImpl::Private::GetPrimVertexCountAndSlots);

		if (UsdPrim.IsA<pxr::UsdGeomGprim>() || UsdPrim.IsA<pxr::UsdGeomSubset>())
		{
			OutVertexCount = UsdUtils::GetGprimVertexCount(pxr::UsdGeomGprim{UsdPrim}, Context.Time);

			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if (!Context.RenderContext.IsNone())
			{
				RenderContextToken = UnrealToUsd::ConvertToken(*Context.RenderContext.ToString()).Get();
			}

			pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
			if (!Context.MaterialPurpose.IsNone())
			{
				MaterialPurposeToken = UnrealToUsd::ConvertToken(*Context.MaterialPurpose.ToString()).Get();
			}

			const bool bProvideMaterialIndices = false;
			UsdUtils::FUsdPrimMaterialAssignmentInfo LocalInfo = UsdUtils::GetPrimMaterialAssignments(
				UsdPrim,
				Context.Time,
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			);

			OutMaterialSlots.Append(MoveTemp(LocalInfo.Slots));
		}
		else if (pxr::UsdGeomPointInstancer PointInstancer{UsdPrim})
		{
			const pxr::UsdRelationship& Prototypes = PointInstancer.GetPrototypesRel();

			pxr::SdfPathVector PrototypePaths;
			if (Prototypes.GetTargets(&PrototypePaths))
			{
				TArray<uint64> PrototypeVertexCounts;
				PrototypeVertexCounts.SetNumZeroed(PrototypePaths.size());

				{
					FReadScopeLock ScopeLock(Impl.InfoMapLock);
					for (uint32 PrototypeIndex = 0; PrototypeIndex < PrototypePaths.size(); ++PrototypeIndex)
					{
						const pxr::SdfPath& PrototypePath = PrototypePaths[PrototypeIndex];

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
						if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = Impl.InfoMap.Find(UE::FSdfPath{PrototypePath}))
						{
							PrototypeVertexCounts[PrototypeIndex] = FoundInfo->ExpectedVertexCountForSubtree;
						}

						if (const TArray<UsdUtils::FUsdPrimMaterialSlot>* FoundPrototypeSlots = InSubtreeToMaterialSlots.Find(UE::FSdfPath{
								PrototypePath}))
						{
							OutMaterialSlots.Append(*FoundPrototypeSlots);
						}
					}
				}

				if (pxr::UsdAttribute ProtoIndicesAttr = PointInstancer.GetProtoIndicesAttr())
				{
					pxr::VtArray<int> ProtoIndicesArr;
					if (ProtoIndicesAttr.Get(&ProtoIndicesArr, pxr::UsdTimeCode::EarliestTime()))
					{
						for (int ProtoIndex : ProtoIndicesArr)
						{
							OutVertexCount += PrototypeVertexCounts[ProtoIndex];
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
		TMap<UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& InOutSubtreeToMaterialSlots,
		TArray<FString>& InOutPointInstancerPaths,
		uint64& OutSubtreeVertexCount,
		TArray<UsdUtils::FUsdPrimMaterialSlot>& OutSubtreeSlots,
		bool bPossibleInheritedBindings
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::USDInfoCacheImpl::Private::RecursivePropagateVertexAndMaterialSlotCounts);

		if (!UsdPrim)
		{
			return;
		}

		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};
		pxr::UsdStageRefPtr Stage = UsdPrim.GetStage();

		// Material bindings are inherited down to child prims, so if we detect a binding on a parent Xform,
		// we should register the child Mesh prims as users of the material too (regardless of collapsing).
		// Note that we only consider this for direct bindings: Collection-based bindings will already provide the exhaustive
		// list of all the prims that they should apply to when we call ComputeIncludedPaths
		bool bPrimHasInheritableMaterialBindings = false;

		// Register material users
		if (!UsdPrim.IsPseudoRoot())
		{
			pxr::UsdShadeMaterial ShadeMaterial;
			TMap<UE::FSdfPath, TSet<UE::FSdfPath>> NewMaterialUsers;

			pxr::UsdShadeMaterialBindingAPI BindingAPI{UsdPrim};
			if (BindingAPI || bPossibleInheritedBindings)
			{
				// Check for material users via collections-based material bindings
				{
					// When retrieving the relationships directly we'll always need to check the universal render context
					// manually, as it won't automatically "compute the fallback" for us like when we ComputeBoundMaterial()
					std::unordered_set<pxr::TfToken, pxr::TfHash> MaterialPurposeTokens{
						MaterialPurposeToken,
						pxr::UsdShadeTokens->universalRenderContext};
					for (const pxr::TfToken& SomeMaterialPurposeToken : MaterialPurposeTokens)
					{
						// Each one of those relationships must have two targets: A collection, and a material
						for (const pxr::UsdRelationship& Rel : BindingAPI.GetCollectionBindingRels(SomeMaterialPurposeToken))
						{
							const pxr::SdfPath* CollectionPath = nullptr;
							const pxr::SdfPath* MaterialPath = nullptr;

							std::vector<pxr::SdfPath> PathVector;
							if (Rel.GetTargets(&PathVector))
							{
								for (const pxr::SdfPath& Path : PathVector)
								{
									if (Path.IsPrimPath())
									{
										MaterialPath = &Path;
									}
									else if (Path.IsPropertyPath())
									{
										CollectionPath = &Path;
									}
								}
							}

							if (!CollectionPath || !MaterialPath || PathVector.size() != 2)
							{
								// Emit this warning here as USD doesn't seem to and just seems to just ignores this relationship instead
								UE_LOG(
									LogUsd,
									Warning,
									TEXT("Prim '%s' describes a collection-based material binding, but the relationship '%s' is invalid: It should "
										 "contain exactly one Material path and one path to a collection relationship"),
									*PrimPath.GetString(),
									*UsdToUnreal::ConvertToken(Rel.GetName())
								);
								continue;
							}

							if (pxr::UsdCollectionAPI Collection = pxr::UsdCollectionAPI::Get(Stage, *CollectionPath))
							{
								std::set<pxr::SdfPath> IncludedPaths = Collection.ComputeIncludedPaths(Collection.ComputeMembershipQuery(), Stage);

								for (const pxr::SdfPath& IncludedPath : IncludedPaths)
								{
									NewMaterialUsers.FindOrAdd(UE::FSdfPath{*MaterialPath}).Add(UE::FSdfPath{IncludedPath});
								}
							}
							else
							{
								UE_LOG(
									LogUsd,
									Warning,
									TEXT("Failed to find collection at path '%s' when processing collection-based material bindings on prim '%s'"),
									*UsdToUnreal::ConvertPath(CollectionPath->GetPrimPath()),
									*PrimPath.GetString()
								);
							}
						}
					}
				}

				// Check for material bindings directly for this prim
				ShadeMaterial = BindingAPI.ComputeBoundMaterial(MaterialPurposeToken);
				if (ShadeMaterial)
				{
					bPrimHasInheritableMaterialBindings = true;
					NewMaterialUsers.FindOrAdd(UE::FSdfPath{ShadeMaterial.GetPrim().GetPath()}).Add(UE::FSdfPath{UsdPrimPath});
				}
			}
			// Temporary fallback for prims that don't have the MaterialBindingAPI but do have the relationship.
			// USD will emit a warning for these though
			else if (pxr::UsdRelationship Relationship = UsdPrim.GetRelationship(pxr::UsdShadeTokens->materialBinding))
			{
				pxr::SdfPathVector Targets;
				Relationship.GetTargets(&Targets);

				if (Targets.size() > 0)
				{
					const pxr::SdfPath& TargetMaterialPrimPath = Targets[0];
					pxr::UsdPrim MaterialPrim = Stage->GetPrimAtPath(TargetMaterialPrimPath);
					ShadeMaterial = pxr::UsdShadeMaterial{MaterialPrim};
					if (ShadeMaterial)
					{
						bPrimHasInheritableMaterialBindings = true;
						NewMaterialUsers.FindOrAdd(UE::FSdfPath{TargetMaterialPrimPath}).Add(UE::FSdfPath{UsdPrimPath});
					}
				}
			}

			FWriteScopeLock ScopeLock(Impl.MaterialUsersLock);
			for (const TPair<UE::FSdfPath, TSet<UE::FSdfPath>>& NewMaterialToUsers : NewMaterialUsers)
			{
				TSet<UE::FSdfPath>& UserPrimPaths = Impl.MaterialUsers.FindOrAdd(NewMaterialToUsers.Key);
				UserPrimPaths.Reserve(UserPrimPaths.Num() + NewMaterialToUsers.Value.Num());
				for (const UE::FSdfPath& NewUserPath : NewMaterialToUsers.Value)
				{
					pxr::UsdPrim UserPrim = Stage->GetPrimAtPath(NewUserPath);

					// Do this filtering here because Collection.ComputeIncludedPaths() can be very aggressive and return
					// literally *all prims* below an included prim path. That's fine and it really does mean that any Mesh prim
					// in there could use the collection-based material binding, but nevertheless we don't want to register that
					// e.g. Shader prims or SkelAnimation prims are "material users"
					if (UserPrim.IsA<pxr::UsdGeomImageable>())
					{
						UE_LOG(
							LogUsd,
							Verbose,
							TEXT("Registering material user '%s' of material '%s'"),
							*NewUserPath.GetString(),
							*NewMaterialToUsers.Key.GetString()
						);
						UserPrimPaths.Add(NewUserPath);
					}
					// If a UsdGeomSubset is a material user, make its Mesh parent prim into a user too.
					// Our notice handling is somewhat stricter now, and we have no good way of upgrading a simple material info change
					// into a resync change of the StaticMeshComponent when we change a material that is bound directly to a
					// UsdGeomSubset, since the GeomMesh translator doesn't collapse. We'll unwind this path later when fetching material
					// users, so collapsed static meshes are handled OK, skeletal meshes are handled OK, we just need this one exception
					// for handling uncollapsed static meshes, because by default Mesh prims don't "collapse" their child UsdGeomSubsets
					else if (UserPrim.IsA<pxr::UsdGeomSubset>())
					{
						UE_LOG(
							LogUsd,
							Verbose,
							TEXT("Registering parent Mesh prim '%s' of UsdGeomSubset '%s' as a material user of material prim '%s'"),
							*NewUserPath.GetString(),
							*PrimPath.GetString(),
							*NewMaterialToUsers.Key.GetString()
						);
						UserPrimPaths.Add(NewUserPath.GetParentPath());
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

		const uint32 NumChildren = Prims.Num();

		TArray<uint64> ChildSubtreeVertexCounts;
		ChildSubtreeVertexCounts.SetNumUninitialized(NumChildren);

		TArray<TArray<UsdUtils::FUsdPrimMaterialSlot>> ChildSubtreeMaterialSlots;
		ChildSubtreeMaterialSlots.SetNum(NumChildren);

		const int32 MinBatchSize = 1;

		ParallelFor(
			TEXT("RecursivePropagateVertexAndMaterialSlotCounts"),
			Prims.Num(),
			MinBatchSize,
			[&](int32 Index)
			{
				RecursivePropagateVertexAndMaterialSlotCounts(
					Prims[Index],
					Context,
					MaterialPurposeToken,
					Impl,
					Registry,
					InOutSubtreeToMaterialSlots,
					InOutPointInstancerPaths,
					ChildSubtreeVertexCounts[Index],
					ChildSubtreeMaterialSlots[Index],
					bPrimHasInheritableMaterialBindings || bPossibleInheritedBindings
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
		if (pxr::UsdGeomPointInstancer PointInstancer{UsdPrim})
		{
			bIsPointInstancer = true;
		}
		else if (!bPrimIsInvisible)
		{
			GetPrimVertexCountAndSlots(UsdPrim, Context, Impl, InOutSubtreeToMaterialSlots, OutSubtreeVertexCount, OutSubtreeSlots);

			for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				OutSubtreeVertexCount += ChildSubtreeVertexCounts[ChildIndex];
				OutSubtreeSlots.Append(ChildSubtreeMaterialSlots[ChildIndex]);
			}
		}

		{
			FWriteScopeLock ScopeLock(Impl.InfoMapLock);

			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd(UE::FSdfPath{UsdPrimPath});

			// For point instancers we can't guarantee we parsed the prototypes yet because they
			// could technically be anywhere, so store them here for a later pass
			if (bIsPointInstancer)
			{
				InOutPointInstancerPaths.Emplace(UE::FSdfPath{UsdPrimPath}.GetString());
			}
			// While we will compute the totals for any and all children normally, don't just append the regular
			// traversal vertex count to the point instancer prim itself just yet, as that doesn't really represent
			// what will happen. We'll later do another pass to handle point instancers where we'll properly instance
			// stuff, and then we'll updadte all ancestors
			else
			{
				Info.ExpectedVertexCountForSubtree = OutSubtreeVertexCount;
				InOutSubtreeToMaterialSlots.Emplace(UsdPrimPath, OutSubtreeSlots);
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
		TArray<FString>& PointInstancerPaths,
		TMap<UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& InOutSubtreeMaterialSlots
	)
	{
		// We must sort point instancers in a particular order in case they depend on each other.
		// At least we know that an ordering like this should be possible, because A with B as a prototype and B with A
		// as a prototype leads to an invalid USD stage.
		TFunction<bool(const FString&, const FString&)> SortFunction = [Stage](const FString& LHS, const FString& RHS)
		{
			FScopedUsdAllocs Allocs;

			pxr::SdfPath LPath = UnrealToUsd::ConvertPath(*LHS).Get();
			pxr::SdfPath RPath = UnrealToUsd::ConvertPath(*RHS).Get();

			pxr::UsdGeomPointInstancer LPointInstancer = pxr::UsdGeomPointInstancer{Stage->GetPrimAtPath(LPath)};
			pxr::UsdGeomPointInstancer RPointInstancer = pxr::UsdGeomPointInstancer{Stage->GetPrimAtPath(RPath)};
			if (LPointInstancer && RPointInstancer)
			{
				const pxr::UsdRelationship& LPrototypes = LPointInstancer.GetPrototypesRel();
				pxr::SdfPathVector LPrototypePaths;
				if (LPrototypes.GetTargets(&LPrototypePaths))
				{
					for (const pxr::SdfPath& LPrototypePath : LPrototypePaths)
					{
						// Consider RPointInstancer at RPath "/LPointInstancer/Prototypes/Nest/RPointInstancer", and
						// LPointInstancer has prototype "/LPointInstancer/Prototypes/Nest". If RPath has the LPrototypePath as prefix,
						// we should have R come before L in the sort order.
						// Of course, in this scenario we could get away with just sorting by length, but that wouldn't help if the
						// point instancers were not inside each other (e.g. siblings).
						if (RPath.HasPrefix(LPrototypePath))
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

		for (const FString& PointInstancerPath : PointInstancerPaths)
		{
			UE::FSdfPath UsdPointInstancerPath{*PointInstancerPath};

			if (pxr::UsdPrim PointInstancer = Stage->GetPrimAtPath(UnrealToUsd::ConvertPath(*PointInstancerPath).Get()))
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
				UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd(UsdPointInstancerPath);

				Info.ExpectedVertexCountForSubtree = PointInstancerVertexCount;
				InOutSubtreeMaterialSlots.Emplace(UsdPointInstancerPath, PointInstancerMaterialSlots);

				// Now that we have info on the point instancer itself, update the counts of all ancestors.
				// Note: The vertex/material slot count for the entire point instancer subtree are just the counts
				// for the point instancer itself, as we stop regular traversal when we hit them
				UE::FSdfPath ParentPath = UsdPointInstancerPath.GetParentPath();
				pxr::UsdPrim Prim = Stage->GetPrimAtPath(ParentPath);
				while (Prim)
				{
					// If our ancestor is a point instancer itself, just abort as we'll only get the actual counts
					// when we handle that ancestor directly. We don't want to update the ancestor point instancer's
					// ancestors with incorrect values
					if (Prim.IsA<pxr::UsdGeomPointInstancer>())
					{
						break;
					}

					UE::UsdInfoCache::Private::FUsdPrimInfo& ParentInfo = Impl.InfoMap.FindOrAdd(ParentPath);
					ParentInfo.ExpectedVertexCountForSubtree += PointInstancerVertexCount;

					InOutSubtreeMaterialSlots[ParentPath].Append(PointInstancerMaterialSlots);

					// Break only here so we update the pseudoroot too
					if (Prim.IsPseudoRoot())
					{
						break;
					}

					ParentPath = ParentPath.GetParentPath();
					Prim = Stage->GetPrimAtPath(ParentPath);
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
		const TMap<UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& SubtreeMaterialSlots,
		bool bContextMergeIdenticalSlots
	)
	{
		FWriteScopeLock Lock{Impl.InfoMapLock};

		for (const TPair<UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>>& Pair : SubtreeMaterialSlots)
		{
			const UE::FSdfPath& PrimPath = Pair.Key;

			bool bCanMergeSlotsForThisPrim = false;
			if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = Impl.InfoMap.Find(PrimPath))
			{
				const UE::FSdfPath* CollapsedRoot = &FoundInfo->AssetCollapsedRoot;

				// We only merge slots in the context of collapsing
				const bool bPrimIsCollapsedOrCollapseRoot = !CollapsedRoot->IsEmpty() || PrimPath.IsAbsoluteRootPath();

				const bool bPrimIsPotentialGeometryCacheRoot = FoundInfo->GeometryCacheState
															   == UE::UsdInfoCache::Private::EGeometryCachePrimState::ValidRoot;
				bCanMergeSlotsForThisPrim = bPrimIsCollapsedOrCollapseRoot && !bPrimIsPotentialGeometryCacheRoot;
			}

			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindOrAdd(Pair.Key);

			// For now we only ever merge material slots when collapsing
			if (bCanMergeSlotsForThisPrim && bContextMergeIdenticalSlots)
			{
				Info.SubtreeMaterialSlots = TSet<UsdUtils::FUsdPrimMaterialSlot>{Pair.Value}.Array();
			}
			else
			{
				Info.SubtreeMaterialSlots = Pair.Value;
			}
		}
	}

	bool CanMeshSubtreeBeCollapsed(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		const TSharedPtr<FUsdSchemaTranslator>& Translator
	)
	{
		if (!UsdPrim)
		{
			return false;
		}

		// We should never be able to collapse SkelRoots because the UsdSkelSkeletonTranslator doesn't collapse
		if (UsdPrim.IsA<pxr::UsdSkelRoot>())
		{
			return false;
		}

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();

		FWriteScopeLock ScopeLock(Impl.InfoMapLock);
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

		TSharedPtr<FUsdSchemaTranslator> SchemaTranslator = Registry.CreateTranslatorForSchema(Context.AsShared(), UE::FUsdTyped(UsdPrim));
		if (SchemaTranslator)
		{
			if (!bIsAssetCollapsed || !bIsComponentCollapsed)
			{
				const bool bCanMeshSubtreeBeCollapsed = CanMeshSubtreeBeCollapsed(UsdPrim, Context, Impl, SchemaTranslator);

				const UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindChecked(UE::FSdfPath{UsdPrimPath});
				const bool bIsPotentialGeometryCacheRoot = Info.GeometryCacheState == UE::UsdInfoCache::Private::EGeometryCachePrimState::ValidRoot;

				if (!bIsAssetCollapsed)
				{
					// The potential geometry cache root is checked first since the FUsdGeometryCacheTranslator::CollapsesChildren has no logic of its
					// own
					if (bIsPotentialGeometryCacheRoot || (SchemaTranslator->CollapsesChildren(ECollapsingType::Assets) && bCanMeshSubtreeBeCollapsed))
					{
						AssetCollapsedRootOverride = &UsdPrimPath;
					}
				}

				if (!bIsComponentCollapsed)
				{
					if (bIsPotentialGeometryCacheRoot
						|| (SchemaTranslator->CollapsesChildren(ECollapsingType::Components) && bCanMeshSubtreeBeCollapsed))
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

		ParallelFor(
			TEXT("RecursiveQueryCollapsesChildren"),
			Prims.Num(),
			MinBatchSize,
			[&](int32 Index)
			{
				RecursiveQueryCollapsesChildren(Prims[Index], Context, Impl, Registry, *AssetCollapsedRootOverride, *ComponentCollapsedRootOverride);
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

		// This really should be a separate pass, but it does no harm here and we have so many passes already...
		// This needs to happen after we set things into the FUsdPrimInfo for this prim right above this, as it may query
		// whether this prim (or any of its children) collapse
		if (SchemaTranslator)
		{
			const bool bIsCollapsedBySomeParent = AssetCollapsedRootOverride && !AssetCollapsedRootOverride->IsEmpty()
												  && *AssetCollapsedRootOverride != UsdPrimPath;

			// We don't care about prims that were collapsed by another. This because whenever the collapse root
			// registers its auxiliary prims it will already account for all of the collapsed prims that are relevant,
			// according to the translator type. If we registered aux prims for all prims here, we'd get useless aux
			// prim links between e.g. all parent and child prims *within* the collapsed subtree
			if (!bIsCollapsedBySomeParent)
			{
				Impl.RegisterAuxiliaryPrims(PrimPath, SchemaTranslator->CollectAuxiliaryPrims());
			}
		}
	}

	// Returns the paths to all prims on the same local layer stack, that are used as sources for composition
	// arcs that are non-root (i.e. the arcs that are either reference, payload, inherits, etc.).
	// In other words, "instanceable composition arcs from local prims"
	TSet<UE::FSdfPath> GetLocalNonRootCompositionArcSourcePaths(const pxr::UsdPrim& UsdPrim)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetLocalNonRootCompositionArcSourcePaths);

		TSet<UE::FSdfPath> Result;

		if (!UsdPrim)
		{
			return Result;
		}

		pxr::PcpLayerStackRefPtr RootLayerStack;

		pxr::UsdPrimCompositionQuery PrimCompositionQuery = pxr::UsdPrimCompositionQuery(UsdPrim);
		std::vector<pxr::UsdPrimCompositionQueryArc> Arcs = PrimCompositionQuery.GetCompositionArcs();
		Result.Reserve(Arcs.size());
		for (const pxr::UsdPrimCompositionQueryArc& Arc : Arcs)
		{
			pxr::PcpNodeRef TargetNode = Arc.GetTargetNode();

			if (Arc.GetArcType() == pxr::PcpArcTypeRoot)
			{
				RootLayerStack = TargetNode.GetLayerStack();
			}
			// We use this function to collect aux/main prim links for instanceables, and we don't have
			// to track instanceable arcs to outside the local layer stack because those don't generate
			// source prims on the stage that the user could edit anyway!
			else if (TargetNode.GetLayerStack() == RootLayerStack)
			{
				Result.Add(UE::FSdfPath{Arc.GetTargetPrimPath()});
			}
		}

		return Result;
	}

	void RegisterInstanceableAuxPrims(const pxr::UsdPrim& UsdPrim, FUsdSchemaTranslationContext& Context, FUsdInfoCache::FUsdInfoCacheImpl& Impl)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UE::USDInfoCacheImpl::Private::RegisterInstanceableAuxPrims);
		FScopedUsdAllocs Allocs;

		pxr::UsdStageRefPtr Stage = UsdPrim.GetStage();
		for (const pxr::UsdPrim& Prototype : Stage->GetPrototypes())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RegisterInstanceableAuxPrims::Prototype);

			if (!Prototype)
			{
				continue;
			}

			// Step into every instance of this prototype on the stage
			for (const pxr::UsdPrim& Instance : Prototype.GetInstances())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RegisterInstanceableAuxPrims::PrototypeInstance);

				UE::FSdfPath InstancePath{Instance.GetPrimPath()};

				// Adding a dependency on the prototype directly is interesting, even though we currently don't display those.
				// and the prototype paths are mostly transient
				Impl.RegisterAuxiliaryPrims(InstancePath, {UE::FSdfPath{Prototype.GetPrimPath()}});

				// Really what we want is to find the source prim that generated this prototype though. Instances always work
				// through some kind of composition arc, so here we collect all references/payloads/inherits/specializes/etc.
				TSet<UE::FSdfPath> SourcePaths = GetLocalNonRootCompositionArcSourcePaths(Instance);
				Impl.RegisterAuxiliaryPrims(InstancePath, SourcePaths);

				// Here we'll traverse the entire subtree of the instance
				pxr::UsdPrimRange PrimRange(Instance, pxr::UsdTraverseInstanceProxies());
				for (pxr::UsdPrimRange::iterator InstanceChildIt = ++PrimRange.begin(); InstanceChildIt != PrimRange.end(); ++InstanceChildIt)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(RegisterInstanceableAuxPrims::InstanceChild);

					pxr::SdfPath SdfChildPrimPath = InstanceChildIt->GetPrimPath();
					UE::FSdfPath ChildPrimPath{SdfChildPrimPath};

					// Register a dependency from child prim to the analogue prim within the prototype itself
					Impl.RegisterAuxiliaryPrims(ChildPrimPath, {UE::FSdfPath{InstanceChildIt->GetPrimInPrototype().GetPrimPath()}});

					// Register a dependency from child prim to analogue prims on the sources used for the instance.
					// We have to do some path surgery to discover what the analogue paths on the source prims are though
					pxr::SdfPath RelativeChildPath = SdfChildPrimPath.MakeRelativePath(InstancePath);
					for (const UE::FSdfPath& SourcePath : SourcePaths)
					{
						pxr::SdfPath ChildOnSourcePath = pxr::SdfPath{SourcePath}.AppendPath(RelativeChildPath);
						if (pxr::UsdPrim ChildOnSource = Stage->GetPrimAtPath(ChildOnSourcePath))
						{
							Impl.RegisterAuxiliaryPrims(ChildPrimPath, {UE::FSdfPath{ChildOnSourcePath}});
						}
					}
				}
			}
		}
	}

	void FindValidGeometryCacheRoot(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		UE::UsdInfoCache::Private::EGeometryCachePrimState& OutState
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FindValidGeometryCacheRoot);

		using namespace UE::UsdInfoCache::Private;

		FScopedUsdAllocs Allocs;

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};
		{
			FReadScopeLock ScopeLock(Impl.InfoMapLock);

			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindChecked(UE::FSdfPath(UsdPrim.GetPrimPath()));

			// A prim is considered a valid root if its subtree has no uncollapsible branch and a valid depth.
			// A valid depth is positive, meaning it has an animated mesh, and doesn't exceed the limit.
			bool bIsValidDepth = Info.GeometryCacheDepth > -1 && Info.GeometryCacheDepth <= GGeometryCacheMaxDepth;
			if (!EnumHasAnyFlags(Info.GeometryCacheState, EGeometryCachePrimState::Uncollapsible) && bIsValidDepth)
			{
				OutState = EGeometryCachePrimState::ValidRoot;
				Info.GeometryCacheState = EGeometryCachePrimState::ValidRoot;
				return;
			}
			// The prim is not a valid root so it's flagged as uncollapsible since the root will be among its children
			// and the eventual geometry cache cannot be collapsed.
			else
			{
				OutState = EGeometryCachePrimState::Uncollapsible;
				Info.GeometryCacheState = EGeometryCachePrimState::Uncollapsible;
			}
		}

		pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

		// Continue the search for a valid root among the children
		TArray<pxr::UsdPrim> Prims;
		for (pxr::UsdPrim Child : PrimChildren)
		{
			FReadScopeLock ScopeLock(Impl.InfoMapLock);
			const UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindChecked(UE::FSdfPath(Child.GetPrimPath()));

			// A subtree is considered only if it has anything collapsible in the first place
			if (EnumHasAnyFlags(Info.GeometryCacheState, EGeometryCachePrimState::Collapsible))
			{
				FindValidGeometryCacheRoot(Child, Context, Impl, OutState);
			}
		}

		OutState = EGeometryCachePrimState::Uncollapsible;
	}

	void RecursiveCheckForGeometryCache(
		const pxr::UsdPrim& UsdPrim,
		FUsdSchemaTranslationContext& Context,
		FUsdInfoCache::FUsdInfoCacheImpl& Impl,
		bool bIsInsideSkelRoot,
		int32& OutDepth,
		UE::UsdInfoCache::Private::EGeometryCachePrimState& OutState
	)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RecursiveCheckForGeometryCache);

		using namespace UE::UsdInfoCache::Private;

		FScopedUsdAllocs Allocs;

		// With this recursive check for geometry cache, we want to find branches with an animated mesh at the leaf and find the root where they can
		// meet. This root prim will collapses the static and animated meshes under it into a single geometry cache.

		pxr::SdfPath UsdPrimPath = UsdPrim.GetPrimPath();
		UE::FSdfPath PrimPath{UsdPrimPath};

		pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

		TArray<pxr::UsdPrim> Prims;
		for (pxr::UsdPrim Child : PrimChildren)
		{
			Prims.Emplace(Child);
		}

		TArray<int32> Depths;
		Depths.SetNum(Prims.Num());

		TArray<EGeometryCachePrimState> States;
		States.SetNum(Prims.Num());

		const int32 MinBatchSize = 1;
		ParallelFor(
			TEXT("RecursiveCheckForGeometryCache"),
			Prims.Num(),
			MinBatchSize,
			[&Prims, &Context, &Impl, bIsInsideSkelRoot, &Depths, &States](int32 Index)
			{
				RecursiveCheckForGeometryCache(
					Prims[Index],
					Context,
					Impl,
					bIsInsideSkelRoot || Prims[Index].IsA<pxr::UsdSkelRoot>(),
					Depths[Index],
					States[Index]
				);
			}
		);

		bool bIsAnimatedMesh = UsdUtils::IsAnimatedMesh(UsdPrim);
		if (!Context.bIsImporting)
		{
			FWriteScopeLock ScopeLock(Impl.InfoMapLock);

			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindChecked(UE::FSdfPath(UsdPrimPath));

			// When loading on the stage, the GeometryCache root can only be the animated mesh prim itself
			// and there's no collapsing involved since each animated mesh will become a GeometryCache.
			// The depth is irrelevant here.
			Info.GeometryCacheDepth = -1;
			Info.GeometryCacheState = bIsAnimatedMesh ? EGeometryCachePrimState::ValidRoot : EGeometryCachePrimState::Uncollapsible;

			return;
		}

		// A geometry cache "branch" starts from an animated mesh prim for which we assign a depth of 0
		// Other branches, without any animated mesh, we don't care about and will remain at -1
		int32 Depth = -1;
		if (bIsAnimatedMesh)
		{
			Depth = 0;
		}
		else
		{
			// The depth is propagated from children to parent, incremented by 1 at each level,
			// with the parent depth being the deepest of its children depth
			int32 ChildDepth = -1;
			for (int32 Index = 0; Index < Depths.Num(); ++Index)
			{
				if (Depths[Index] > -1)
				{
					ChildDepth = FMath::Max(ChildDepth, Depths[Index] + 1);
				}
			}
			Depth = ChildDepth;
		}

		// Along with the depth, we want some hints on the content of the subtree of the prim as this will tell us
		// if the prim can serve as a root and collapse its children into a GeometryCache. The sole condition for
		// being a valid root is that all the branches of the subtree are collapsible.
		EGeometryCachePrimState ChildrenState = EGeometryCachePrimState::None;
		for (EGeometryCachePrimState ChildState : States)
		{
			ChildrenState |= ChildState;
		}

		EGeometryCachePrimState PrimState = EGeometryCachePrimState::None;
		const bool bIsMesh = !!pxr::UsdGeomMesh(UsdPrim);
		const bool bIsXform = !!pxr::UsdGeomXform(UsdPrim);
		if (bIsMesh)
		{
			// A skinned mesh can never be considered part of a geometry cache.
			// Now that we use the UsdSkelSkeletonTranslator instead of the old UsdSkelRootTranslator we may run into these
			// skinned meshes that were already handled by a SkeletonTranslator elsewhere, and need to manually skip them
			if (GIsEditor && bIsInsideSkelRoot && UsdPrim.HasAPI<pxr::UsdSkelBindingAPI>())
			{
				PrimState = EGeometryCachePrimState::Uncollapsible;
			}
			else
			{
				// Animated or static mesh. Static meshes could potentially be animated by transforms in their hierarchy.
				// A mesh prim should be a leaf, but it can have GeomSubset prims as children, but those don't
				// affect the collapsibility status.
				PrimState = EGeometryCachePrimState::Mesh;
			}
		}
		else if (bIsXform)
		{
			// An xform prim is considered collapsible since it could have a mesh prim under it. It has to bubble up its children state.
			PrimState = ChildrenState != EGeometryCachePrimState::None ? ChildrenState | EGeometryCachePrimState::Xform
																	   : EGeometryCachePrimState::Xform;
		}
		else
		{
			// This prim is not considered collapsible with some exception
			// Like a Scope could have some meshes under it, so it has to bubble up its children state
			const bool bIsException = !!pxr::UsdGeomScope(UsdPrim);
			if (bIsException && EnumHasAnyFlags(ChildrenState, EGeometryCachePrimState::Mesh))
			{
				PrimState = ChildrenState;
			}
			else
			{
				PrimState = EGeometryCachePrimState::Uncollapsible;
			}
		}

		// A prim could be a potential root if it has a reference or payload to an allowed file type for GeometryCache
		bool bIsPotentialRoot = false;
		{
			pxr::UsdPrimCompositionQuery PrimCompositionQuery = pxr::UsdPrimCompositionQuery::GetDirectReferences(UsdPrim);
			for (const pxr::UsdPrimCompositionQueryArc& CompositionArc : PrimCompositionQuery.GetCompositionArcs())
			{
				if (CompositionArc.GetArcType() == pxr::PcpArcTypeReference)
				{
					pxr::SdfReferenceEditorProxy ReferenceEditor;
					pxr::SdfReference UsdReference;

					if (CompositionArc.GetIntroducingListEditor(&ReferenceEditor, &UsdReference))
					{
						FString FilePath = UsdToUnreal::ConvertString(UsdReference.GetAssetPath());
						FString Extension = FPaths::GetExtension(FilePath);

						if (Impl.AllowedExtensionsForGeometryCacheSource.Contains(Extension))
						{
							bIsPotentialRoot = true;
							break;
						}
					}
				}
				else if (CompositionArc.GetArcType() == pxr::PcpArcTypePayload)
				{
					pxr::SdfPayloadEditorProxy PayloadEditor;
					pxr::SdfPayload UsdPayload;

					if (CompositionArc.GetIntroducingListEditor(&PayloadEditor, &UsdPayload))
					{
						FString FilePath = UsdToUnreal::ConvertString(UsdPayload.GetAssetPath());
						FString Extension = FPaths::GetExtension(FilePath);

						if (Impl.AllowedExtensionsForGeometryCacheSource.Contains(Extension))
						{
							bIsPotentialRoot = true;
							break;
						}
					}
				}
			}
		}

		{
			FWriteScopeLock ScopeLock(Impl.InfoMapLock);

			UE::UsdInfoCache::Private::FUsdPrimInfo& Info = Impl.InfoMap.FindChecked(UE::FSdfPath(UsdPrimPath));
			Info.GeometryCacheDepth = Depth;
			Info.GeometryCacheState = PrimState;
		}

		// We've encountered a potential root and the subtree has a geometry cache branch, so find its root
		if (bIsPotentialRoot && Depth > -1)
		{
			if (Depth > GGeometryCacheMaxDepth)
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Prim '%s' is potentially a geometry cache %d levels deep, which exceeds the limit of %d. "
						 "This could affect its imported animation. The limit can be increased with the cvar USD.GeometryCache.MaxDepth if needed."),
					*PrimPath.GetString(),
					Depth,
					GGeometryCacheMaxDepth
				);
			}
			FindValidGeometryCacheRoot(UsdPrim, Context, Impl, PrimState);
			Depth = -1;
		}

		OutDepth = Depth;
		OutState = PrimState;
	}

	void CheckForGeometryCache(const pxr::UsdPrim& UsdPrim, FUsdSchemaTranslationContext& Context, FUsdInfoCache::FUsdInfoCacheImpl& Impl)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CheckForGeometryCache);

		using namespace UE::UsdInfoCache::Private;

		if (!GUseGeometryCacheUSD)
		{
			return;
		}

		// If the stage doesn't contain any animated mesh prims, then don't bother doing a full check
		bool bHasAnimatedMesh = false;
		{
			FScopedUsdAllocs UsdAllocs;
			TArray<TUsdStore<pxr::UsdPrim>> ChildPrims = UsdUtils::GetAllPrimsOfType(UsdPrim, pxr::TfType::Find<pxr::UsdGeomMesh>());
			for (const TUsdStore<pxr::UsdPrim>& ChildPrim : ChildPrims)
			{
				if (UsdUtils::IsAnimatedMesh(ChildPrim.Get()))
				{
					bHasAnimatedMesh = true;
					break;
				}
			}
		}

		if (!bHasAnimatedMesh)
		{
			return;
		}

		const bool bIsInsideSkelRoot = static_cast<bool>(UsdUtils::GetClosestParentSkelRoot(UsdPrim));

		int32 Depth = -1;
		EGeometryCachePrimState State = EGeometryCachePrimState::None;
		RecursiveCheckForGeometryCache(UsdPrim, Context, Impl, bIsInsideSkelRoot, Depth, State);

		// If we end up with a positive depth, it means the check found an animated mesh somewhere
		// but no potential root before reaching the pseudoroot, so find one
		if (Depth > -1)
		{
			if (Depth > GGeometryCacheMaxDepth)
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("The stage has a geometry cache %d levels deep, which exceeds the limit of %d. "
						 "This could affect its imported animation. The limit can be increased with the cvar USD.GeometryCache.MaxDepth if needed."),
					Depth,
					GGeometryCacheMaxDepth
				);
			}

			FScopedUsdAllocs UsdAllocs;

			pxr::UsdPrimSiblingRange PrimChildren = UsdPrim.GetFilteredChildren(pxr::UsdTraverseInstanceProxies(pxr::UsdPrimAllPrimsPredicate));

			// The pseudoroot itself cannot be a root for the geometry cache so start from its children
			TArray<pxr::UsdPrim> Prims;
			for (pxr::UsdPrim Child : PrimChildren)
			{
				FindValidGeometryCacheRoot(Child, Context, Impl, State);
			}
		}
	}
#endif	  // USE_SD_SDK
}	 // namespace UE::USDInfoCacheImpl::Private

bool FUsdInfoCache::IsPotentialGeometryCacheRoot(const UE::FSdfPath& Path) const
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			return FoundInfo->GeometryCacheState == UE::UsdInfoCache::Private::EGeometryCachePrimState::ValidRoot;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return false;
}

TOptional<uint64> FUsdInfoCache::GetSubtreeVertexCount(const UE::FSdfPath& Path)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			return FoundInfo->ExpectedVertexCountForSubtree;
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
	}

	return {};
}

TOptional<uint64> FUsdInfoCache::GetSubtreeMaterialSlotCount(const UE::FSdfPath& Path)
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);

		if (const UE::UsdInfoCache::Private::FUsdPrimInfo* FoundInfo = ImplPtr->InfoMap.Find(Path))
		{
			return FoundInfo->SubtreeMaterialSlots.Num();
		}

		// This should never happen: We should have cached the entire tree
		ensureMsgf(false, TEXT("Prim path '%s' has not been cached!"), *Path.GetString());
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
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}
	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Linking asset '%s' to prim '%s'"), *Asset->GetPathName(), *Path.GetString());

	ImplPtr->PrimPathToAssets.FindOrAdd(Path).AddUnique(Asset);
	ImplPtr->AssetToPrimPaths.FindOrAdd(Asset).AddUnique(Path);
}

void FUsdInfoCache::UnlinkAssetFromPrim(const UE::FSdfPath& Path, UObject* Asset)
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}
	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Unlinking asset '%s' to prim '%s'"),
		*Asset->GetPathName(),
		*Path.GetString()
	);

	if (TArray<TWeakObjectPtr<UObject>>* FoundAssetsForPrim = ImplPtr->PrimPathToAssets.Find(Path))
	{
		FoundAssetsForPrim->Remove(Asset);
	}
	if (TArray<UE::FSdfPath>* FoundPrimPathsForAsset = ImplPtr->AssetToPrimPaths.Find(Asset))
	{
		FoundPrimPathsForAsset->Remove(Path);
	}
}

TArray<TWeakObjectPtr<UObject>> FUsdInfoCache::RemoveAllAssetPrimLinks(const UE::FSdfPath& Path)
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Removing asset prim links for path '%s'"), *Path.GetString());

	TArray<TWeakObjectPtr<UObject>> Assets;
	ImplPtr->PrimPathToAssets.RemoveAndCopyValue(Path, Assets);

	for (const TWeakObjectPtr<UObject>& Asset : Assets)
	{
		if (TArray<UE::FSdfPath>* PrimPaths = ImplPtr->AssetToPrimPaths.Find(Asset))
		{
			PrimPaths->Remove(Path);
		}
	}

	return Assets;
}

TArray<UE::FSdfPath> FUsdInfoCache::RemoveAllAssetPrimLinks(const UObject* Asset)
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Removing asset prim links for asset '%s'"), *Asset->GetPathName());

	TArray<UE::FSdfPath> PrimPaths;
	ImplPtr->AssetToPrimPaths.RemoveAndCopyValue(const_cast<UObject*>(Asset), PrimPaths);

	for (const UE::FSdfPath& Path : PrimPaths)
	{
		if (TArray<TWeakObjectPtr<UObject>>* Assets = ImplPtr->PrimPathToAssets.Find(Path))
		{
			Assets->Remove(const_cast<UObject*>(Asset));
		}
	}

	return PrimPaths;
}

void FUsdInfoCache::RemoveAllAssetPrimLinks()
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}
	FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	UE_LOG(LogUsd, Verbose, TEXT("Removing all asset prim links"));

	ImplPtr->PrimPathToAssets.Empty();
	ImplPtr->AssetToPrimPaths.Empty();
}

TArray<TWeakObjectPtr<UObject>> FUsdInfoCache::GetAllAssetsForPrim(const UE::FSdfPath& Path) const
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	if (const TArray<TWeakObjectPtr<UObject>>* FoundAssets = ImplPtr->PrimPathToAssets.Find(Path))
	{
		return *FoundAssets;
	}

	return {};
}

TArray<UE::FSdfPath> FUsdInfoCache::GetPrimsForAsset(UObject* Asset) const
{
	if (!Asset)
	{
		return {};
	}

	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}
	FReadScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);

	if (const TArray<UE::FSdfPath>* FoundPrims = ImplPtr->AssetToPrimPaths.Find(Asset))
	{
		return *FoundPrims;
	}

	return {};
}

TMap<UE::FSdfPath, TArray<TWeakObjectPtr<UObject>>> FUsdInfoCache::GetAllAssetPrimLinks() const
{
	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return {};
	}

	return ImplPtr->PrimPathToAssets;
}

void FUsdInfoCache::RebuildCacheForSubtree(const UE::FUsdPrim& Prim, FUsdSchemaTranslationContext& Context)
{
#if USE_USD_SDK
	TRACE_CPUPROFILER_EVENT_SCOPE(FUsdInfoCache::RebuildCacheForSubtree);

	FUsdInfoCacheImpl* ImplPtr = Impl.Get();
	if (!ImplPtr)
	{
		return;
	}

	// We can't deallocate our info cache pointer with the Usd allocator
	FScopedUnrealAllocs UEAllocs;

	TGuardValue<bool> Guard{Context.bIsBuildingInfoCache, true};
	{
		FScopedUsdAllocs Allocs;

		pxr::UsdPrim UsdPrim{Prim};
		if (!UsdPrim)
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
			FWriteScopeLock ScopeLock(ImplPtr->MaterialUsersLock);
			ImplPtr->MaterialUsers.Empty();
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->AuxiliaryPrimsLock);
			ImplPtr->AuxToMainPrims.Empty();
			ImplPtr->MainToAuxPrims.Empty();
		}

		IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked<IUsdSchemasModule>(TEXT("USDSchemas"));
		FUsdSchemaTranslatorRegistry& Registry = UsdSchemasModule.GetTranslatorRegistry();

		TMap<UE::FSdfPath, TArray<UsdUtils::FUsdPrimMaterialSlot>> TempSubtreeSlots;
		TArray<FString> PointInstancerPaths;

		pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
		if (!Context.MaterialPurpose.IsNone())
		{
			MaterialPurposeToken = UnrealToUsd::ConvertToken(*Context.MaterialPurpose.ToString()).Get();
		}

		// Propagate vertex and material slot counts before we query CollapsesChildren because the Xformable
		// translator needs to know when it would generate too large a static mesh
		uint64 SubtreeVertexCount = 0;
		TArray<UsdUtils::FUsdPrimMaterialSlot> SubtreeSlots;
		const bool bPossibleInheritedBindings = false;
		UE::USDInfoCacheImpl::Private::RecursivePropagateVertexAndMaterialSlotCounts(
			UsdPrim,
			Context,
			MaterialPurposeToken,
			*ImplPtr,
			Registry,
			TempSubtreeSlots,
			PointInstancerPaths,
			SubtreeVertexCount,
			SubtreeSlots,
			bPossibleInheritedBindings
		);

		UE::USDInfoCacheImpl::Private::UpdateInfoForPointInstancers(UsdPrim.GetStage(), Context, *ImplPtr, PointInstancerPaths, TempSubtreeSlots);

		UE::USDInfoCacheImpl::Private::CheckForGeometryCache(UsdPrim, Context, *ImplPtr);

		UE::USDInfoCacheImpl::Private::RecursiveQueryCollapsesChildren(UsdPrim, Context, *ImplPtr, Registry);

		UE::USDInfoCacheImpl::Private::RegisterInstanceableAuxPrims(UsdPrim, Context, *ImplPtr);

		UE::USDInfoCacheImpl::Private::CollectMaterialSlotCounts(*ImplPtr, TempSubtreeSlots, Context.bMergeIdenticalMaterialSlots);
	}
#endif	  // USE_USD_SDK
}

void FUsdInfoCache::Clear()
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		{
			FWriteScopeLock ScopeLock(ImplPtr->InfoMapLock);
			ImplPtr->InfoMap.Empty();
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->PrimPathToAssetsLock);
			ImplPtr->PrimPathToAssets.Empty();
			ImplPtr->AssetToPrimPaths.Empty();
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->MaterialUsersLock);
			ImplPtr->MaterialUsers.Empty();
		}
		{
			FWriteScopeLock ScopeLock(ImplPtr->AuxiliaryPrimsLock);
			ImplPtr->AuxToMainPrims.Empty();
			ImplPtr->MainToAuxPrims.Empty();
		}
	}
}

bool FUsdInfoCache::IsEmpty()
{
	if (FUsdInfoCacheImpl* ImplPtr = Impl.Get())
	{
		FReadScopeLock ScopeLock(ImplPtr->InfoMapLock);
		return ImplPtr->InfoMap.IsEmpty();
	}

	return true;
}
