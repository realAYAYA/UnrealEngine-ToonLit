// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDGeomMeshTranslator.h"

#if USE_USD_SDK

#include "MeshTranslationImpl.h"
#include "UnrealUSDWrapper.h"
#include "USDAssetUserData.h"
#include "USDClassesModule.h"
#include "USDConversionUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDInfoCache.h"
#include "USDIntegrationUtils.h"
#include "USDLog.h"
#include "USDPrimConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

#if WITH_EDITOR
#include "IMeshBuilderModule.h"
#include "MeshBudgetProjectSettings.h"
#endif // WITH_EDITOR

#include "USDIncludesStart.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usd/typed.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/subset.h"
#include "USDIncludesEnd.h"

static float GMeshNormalRepairThreshold = 0.05f;
static FAutoConsoleVariableRef CVarMeshNormalRepairThreshold(
	TEXT("USD.MeshNormalRepairThreshold"),
	GMeshNormalRepairThreshold,
	TEXT("We will try repairing up to this fraction of a Mesh's normals when invalid. If a Mesh has more invalid normals than this, we will recompute all of them. Defaults to 0.05 (5% of all normals)."));

static bool GSkipMeshTangentComputation = false;
static FAutoConsoleVariableRef CVarSkipMeshTangentComputation(
	TEXT("USD.SkipMeshTangentComputation"),
	GSkipMeshTangentComputation,
	TEXT("Skip computing tangents for meshes. With meshes with a huge numer of vertices, it can take a very long time to compute them."));

namespace UsdGeomMeshTranslatorImpl
{
	bool ShouldEnableNanite(
		const TArray<FMeshDescription>& LODIndexToMeshDescription,
		const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& LODIndexToMaterialInfo,
		const FUsdSchemaTranslationContext& Context,
		const UE::FUsdPrim& Prim
	)
	{
		if (LODIndexToMeshDescription.Num() < 1)
		{
			return false;
		}

		FScopedUsdAllocs UsdAllocs;

		UE::FSdfPath PrimPath = Prim.GetPrimPath();

		pxr::UsdPrim UsdPrim{Prim};
		if (!UsdPrim)
		{
			return false;
		}

		bool bHasNaniteOverrideEnabled = false;

		// We want Nanite because of an override on Prim
		if (pxr::UsdAttribute NaniteOverride = UsdPrim.GetAttribute(UnrealIdentifiers::UnrealNaniteOverride))
		{
			pxr::TfToken OverrideValue;
			if (NaniteOverride.Get(&OverrideValue))
			{
				if (OverrideValue == UnrealIdentifiers::UnrealNaniteOverrideEnable)
				{
					bHasNaniteOverrideEnabled = true;
					UE_LOG(LogUsd, Log, TEXT("Trying to enable Nanite for mesh generated for prim '%s' as the '%s' attribute is set to '%s'"),
						*PrimPath.GetString(),
						*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealNaniteOverride),
						*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealNaniteOverrideEnable)
					);

				}
				else if (OverrideValue == UnrealIdentifiers::UnrealNaniteOverrideDisable)
				{
					UE_LOG(LogUsd, Log, TEXT("Not enabling Nanite for mesh generated for prim '%s' as the '%s' attribute is set to '%s'"),
						*PrimPath.GetString(),
						*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealNaniteOverride),
						*UsdToUnreal::ConvertToken(UnrealIdentifiers::UnrealNaniteOverrideDisable)
					);
					return false;
				}
			}
		}

		// We want Nanite because the mesh is large enough for the threshold, which is set to something valid
		if (!bHasNaniteOverrideEnabled && Context.InfoCache.IsValid())
		{
			const int32 NumTriangles = LODIndexToMeshDescription[0].Triangles().Num();
			if (NumTriangles >= Context.NaniteTriangleThreshold)
			{
				UE_LOG(LogUsd, Verbose, TEXT("Trying to enable Nanite for mesh generated for prim '%s' as it has '%d' triangles, and the threshold is '%d'"),
					*PrimPath.GetString(),
					NumTriangles,
					Context.NaniteTriangleThreshold
				);
			}
			else
			{
				UE_LOG(LogUsd, Verbose, TEXT("Not enabling Nanite for mesh generated for prim '%s' as it has '%d' triangles, and the threshold is '%d'"),
					*PrimPath.GetString(),
					NumTriangles,
					Context.NaniteTriangleThreshold
				);
				return false;
			}
		}

		// Don't enable Nanite if we have more than one LOD. This means the Mesh came from the LOD variant set setup, and
		// we're considering the LOD setup "stronger" than the Nanite override: If you have all that LOD variant set situation you
		// likely don't want Nanite for one of the LOD meshes anyway, as that doesn't really make any sense.
		// If the user wants to have Nanite within the variant set all they would otherwise need is to name the variant set something
		// else other than LOD.
		if (LODIndexToMeshDescription.Num() > 1)
		{
			UE_LOG(LogUsd, Warning, TEXT("Not enabling Nanite for mesh generated for prim '%s' as it has more than one generated LOD (and so came from a LOD variant set setup)"),
				*PrimPath.GetString()
			);
			return false;
		}

		if (Context.InfoCache.IsValid())
		{
			TOptional<uint64> SubtreeSectionCount = Context.InfoCache->GetSubtreeMaterialSlotCount(PrimPath);

			const int32 MaxNumSections = 64; // There is no define for this, but it's checked for on NaniteBuilder.cpp, FBuilderModule::Build
			if (!SubtreeSectionCount.IsSet() || SubtreeSectionCount.GetValue() > MaxNumSections)
			{
				UE_LOG(LogUsd, Warning, TEXT("Not enabling Nanite for mesh generated for prim '%s' as LOD0 has '%d' material slots, which is above the Nanite limit of '%d'"),
					*PrimPath.GetString(),
					SubtreeSectionCount.GetValue(),
					MaxNumSections
				);
				return false;
			}
		}

#if !WITH_EDITOR
		UE_LOG(LogUsd, Warning, TEXT("Not enabling Nanite for mesh generated for prim '%s' as we can't setup Nanite during runtime"),
			*PrimPath.GetString()
		);
		return false;
#else
		return true;
#endif
	}

	/** Returns true if material infos have changed on the StaticMesh */
	bool ProcessStaticMeshMaterials(
		const pxr::UsdPrim& UsdPrim,
		const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& LODIndexToMaterialInfo,
		UStaticMesh& StaticMesh,
		UUsdAssetCache2& AssetCache,
		FUsdInfoCache* InfoCache,
		float Time,
		EObjectFlags Flags
	)
	{
		if (!InfoCache)
		{
			return false;
		}

		bool bMaterialAssignementsHaveChanged = false;

		TArray<UMaterialInterface*> ExistingAssignments;
		for (const FStaticMaterial& StaticMaterial : StaticMesh.GetStaticMaterials())
		{
			ExistingAssignments.Add(StaticMaterial.MaterialInterface);
		}

		TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials = MeshTranslationImpl::ResolveMaterialAssignmentInfo(
			UsdPrim,
			LODIndexToMaterialInfo,
			AssetCache,
			*InfoCache,
			Flags
		);

		uint32 StaticMeshSlotIndex = 0;
		for (int32 LODIndex = 0; LODIndex < LODIndexToMaterialInfo.Num(); ++LODIndex)
		{
			const TArray< UsdUtils::FUsdPrimMaterialSlot >& LODSlots = LODIndexToMaterialInfo[LODIndex].Slots;

			for (int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++StaticMeshSlotIndex)
			{
				const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[LODSlotIndex];

				UMaterialInterface* Material = UMaterial::GetDefaultMaterial(MD_Surface);
				if (UMaterialInterface** FoundMaterial = ResolvedMaterials.Find(&Slot))
				{
					Material = *FoundMaterial;
				}
				else
				{
					UE_LOG(LogUsd, Error, TEXT("Failed to resolve material '%s' for slot '%d' of LOD '%d' for mesh '%s'"), *Slot.MaterialSource, LODSlotIndex, LODIndex, *UsdToUnreal::ConvertPath(UsdPrim.GetPath()));
					continue;
				}

				// Create and set the static material
				FStaticMaterial StaticMaterial(Material, *LexToString(StaticMeshSlotIndex));
				if (!StaticMesh.GetStaticMaterials().IsValidIndex(StaticMeshSlotIndex))
				{
					StaticMesh.GetStaticMaterials().Add(MoveTemp(StaticMaterial));
					bMaterialAssignementsHaveChanged = true;
				}
				else if (!(StaticMesh.GetStaticMaterials()[StaticMeshSlotIndex] == StaticMaterial))
				{
					StaticMesh.GetStaticMaterials()[StaticMeshSlotIndex] = MoveTemp(StaticMaterial);
					bMaterialAssignementsHaveChanged = true;
				}

#if WITH_EDITOR
				// Setup the section map so that our LOD material index is properly mapped to the static mesh material index
				// At runtime we don't ever parse these variants as LODs so we don't need this
				if (StaticMesh.GetSectionInfoMap().IsValidSection(LODIndex, LODSlotIndex))
				{
					FMeshSectionInfo MeshSectionInfo = StaticMesh.GetSectionInfoMap().Get(LODIndex, LODSlotIndex);

					if (MeshSectionInfo.MaterialIndex != StaticMeshSlotIndex)
					{
						MeshSectionInfo.MaterialIndex = StaticMeshSlotIndex;
						StaticMesh.GetSectionInfoMap().Set(LODIndex, LODSlotIndex, MeshSectionInfo);

						bMaterialAssignementsHaveChanged = true;
					}
				}
				else
				{
					FMeshSectionInfo MeshSectionInfo;
					MeshSectionInfo.MaterialIndex = StaticMeshSlotIndex;

					StaticMesh.GetSectionInfoMap().Set(LODIndex, LODSlotIndex, MeshSectionInfo);

					bMaterialAssignementsHaveChanged = true;
				}
#endif // WITH_EDITOR
			}
		}

#if WITH_EDITOR
		StaticMesh.GetOriginalSectionInfoMap().CopyFrom(StaticMesh.GetSectionInfoMap());
#endif // WITH_EDITOR

		return bMaterialAssignementsHaveChanged;
	}

	// If UsdMesh is a LOD, will parse it and all of the other LODs, and and place them in OutLODIndexToMeshDescription and OutLODIndexToMaterialInfo.
	// Note that these other LODs will be hidden in other variants, and won't show up on traversal unless we actively switch the variants (which we do here).
	// We use a separate function for this because there is a very specific set of conditions where we successfully can do this, and we
	// want to fall back to just parsing UsdMesh as a simple single-LOD mesh if we fail.
	bool TryLoadingMultipleLODs(
		const UE::FUsdPrim& MeshPrim,
		TArray<FMeshDescription>& OutLODIndexToMeshDescription,
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& OutLODIndexToMaterialInfo,
		const UsdToUnreal::FUsdMeshConversionOptions& Options
	)
	{
		if(!MeshPrim)
		{
			return false;
		}

		UE::FUsdPrim ParentPrim = MeshPrim.GetParent();

		TMap<int32, FMeshDescription> LODIndexToMeshDescriptionMap;
		TMap<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToMaterialInfoMap;

		UsdToUnreal::FUsdMeshConversionOptions OptionsCopy = Options;

		TSet<FString> AllPrimvars;
		TSet<FString> PreferredPrimvars;

		// Here we are choosing to preemptively traverse all LODs to fetch the combined primvars that we'll use for
		// each UV index for the static mesh as a whole
		TFunction<bool(const pxr::UsdGeomMesh&, int32)> CombinePrimvars =
			[&AllPrimvars, &PreferredPrimvars]
			(const pxr::UsdGeomMesh& LODMesh, int32 LODIndex)
			{
				TArray<TUsdStore<pxr::UsdGeomPrimvar>> MeshPrimvars = UsdUtils::GetUVSetPrimvars(
					LODMesh,
					TNumericLimits<int32>::Max()
				);

				for (const TUsdStore<pxr::UsdGeomPrimvar>& MeshPrimvar : MeshPrimvars)
				{
					FString PrimvarName = UsdToUnreal::ConvertToken(MeshPrimvar.Get().GetName());
					PrimvarName.RemoveFromStart(TEXT("primvars:"));

					AllPrimvars.Add(PrimvarName);

					// Keep track of which primvars are texCoord2f as we always want to prefer these over other float2s
					if (MeshPrimvar.Get().GetTypeName().GetRole() == pxr::SdfValueTypeNames->TexCoord2f.GetRole())
					{
						PreferredPrimvars.Add(PrimvarName);
					}
				}

				return true;
			};
		if (!UsdUtils::IterateLODMeshes(ParentPrim, CombinePrimvars))
		{
			return false;
		}
		TMap<FString, int32> PrimvarToUVIndex = UsdUtils::CombinePrimvarsIntoUVSets(
			AllPrimvars,
			PreferredPrimvars
		);

		TFunction<bool(const pxr::UsdGeomMesh&, int32)> ConvertLOD =
			[&OptionsCopy, &Options, &PrimvarToUVIndex, &LODIndexToMeshDescriptionMap, &LODIndexToMaterialInfoMap]
			(const pxr::UsdGeomMesh& LODMesh, int32 LODIndex)
		{
			FMeshDescription TempMeshDescription;

			UsdUtils::FUsdPrimMaterialAssignmentInfo TempMaterialInfo;
				TempMaterialInfo.PrimvarToUVIndex = PrimvarToUVIndex;

			FStaticMeshAttributes StaticMeshAttributes(TempMeshDescription);
			StaticMeshAttributes.Register();

			bool bSuccess = true;

			{
				FScopedUsdAllocs Allocs;
				// The user can't manually hide/unhide a particular LOD in the engine after it is imported, so we should probably bake
				// the particular LOD visibility into the combined mesh. Note how we don't use computed visibility here, as we only really
				// care if this mesh in particular has been marked as invisible
				pxr::TfToken Visibility;
				pxr::UsdAttribute VisibilityAttr = LODMesh.GetVisibilityAttr();
				if (VisibilityAttr && VisibilityAttr.Get(&Visibility, Options.TimeCode) && Visibility == pxr::UsdGeomTokens->inherited)
				{
					// If we're interpreting LODs we must bake the transform from each LOD Mesh into the vertices, because there's no guarantee
					// all LODs have the same transform, so we can't just put the transforms directly on the component. If we are not interpreting
					// LODs we can do that though
					// TODO: Handle resetXformOp here
					bool bResetXformStack = false;
					FTransform MeshTransform = FTransform::Identity;
					bSuccess &= UsdToUnreal::ConvertXformable(
						LODMesh.GetPrim().GetStage(),
						LODMesh,
						MeshTransform,
						Options.TimeCode.GetValue(),
						&bResetXformStack
					);

					if (bSuccess)
					{
						OptionsCopy.AdditionalTransform = MeshTransform * Options.AdditionalTransform;
						OptionsCopy.bMergeIdenticalMaterialSlots = false;	 // We only merge slots when collapsing, and we never collapse LODs

						bSuccess &= UsdToUnreal::ConvertGeomMesh(LODMesh, TempMeshDescription, TempMaterialInfo, OptionsCopy);
					}
				}
			}

			if (bSuccess)
			{
				LODIndexToMeshDescriptionMap.Add(LODIndex, MoveTemp(TempMeshDescription));
				LODIndexToMaterialInfoMap.Add(LODIndex, MoveTemp(TempMaterialInfo));
			}

			return true;
		};
		if (!UsdUtils::IterateLODMeshes(ParentPrim, ConvertLOD))
		{
			return false;
		}

		// Place them in order as we can't have e.g. LOD0 and LOD2 without LOD1, and there's no reason downstream code needs to care about this
		OutLODIndexToMeshDescription.Reset(LODIndexToMeshDescriptionMap.Num());
		OutLODIndexToMaterialInfo.Reset(LODIndexToMaterialInfoMap.Num());
		LODIndexToMeshDescriptionMap.KeySort(TLess<int32>());
		for (TPair<int32, FMeshDescription>& Entry : LODIndexToMeshDescriptionMap)
		{
			const int32 OldLODIndex = Entry.Key;
			OutLODIndexToMeshDescription.Add(MoveTemp(Entry.Value));
			OutLODIndexToMaterialInfo.Add(MoveTemp(LODIndexToMaterialInfoMap[OldLODIndex]));
		}

		return true;
	}

	void LoadMeshDescriptions(
		UE::FUsdPrim MeshPrim,
		TArray<FMeshDescription>& OutLODIndexToMeshDescription,
		TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& OutLODIndexToMaterialInfo,
		const UsdToUnreal::FUsdMeshConversionOptions& Options,
		bool bInterpretLODs = false
	)
	{
		if (!MeshPrim)
		{
			return;
		}

		UE::FUsdStage Stage = MeshPrim.GetStage();
		UE::FSdfPath Path = MeshPrim.GetPrimPath();

		bool bInterpretedLODs = false;
		if (bInterpretLODs)
		{
			bInterpretedLODs = TryLoadingMultipleLODs(MeshPrim, OutLODIndexToMeshDescription, OutLODIndexToMaterialInfo, Options);

			// Have to be very careful here as flipping through LODs invalidates prim references, so we need to
			// re-acquire them
			MeshPrim = Stage.GetPrimAtPath(Path);
		}

		// If we've managed to interpret LODs, we won't place our mesh transform on the static mesh component itself
		// (c.f. FUsdGeomXformableTranslator::UpdateComponents), and will instead expect it to be baked into the mesh.
		// So here we do that
		bool bSuccess = true;
		FTransform MeshTransform = FTransform::Identity;
		if (bInterpretedLODs && OutLODIndexToMeshDescription.Num() > 1)
		{
			// TODO: Handle resetXformOp here
			bool bResetXformStack = false;
			bSuccess &= UsdToUnreal::ConvertXformable(Stage, UE::FUsdTyped{MeshPrim}, MeshTransform, Options.TimeCode.GetValue(), &bResetXformStack);
		}

		if (!bInterpretedLODs)
		{
			FMeshDescription TempMeshDescription;
			UsdUtils::FUsdPrimMaterialAssignmentInfo TempMaterialInfo;

			FStaticMeshAttributes StaticMeshAttributes(TempMeshDescription);
			StaticMeshAttributes.Register();

			if (bSuccess)
			{
				UsdToUnreal::FUsdMeshConversionOptions OptionsCopy = Options;
				OptionsCopy.AdditionalTransform = MeshTransform * Options.AdditionalTransform;
				OptionsCopy.bMergeIdenticalMaterialSlots = false;  // We only merge for collapsed meshes

				FScopedUsdAllocs Allocs;
				pxr::UsdGeomMesh UsdMesh{MeshPrim};

				bSuccess &= UsdToUnreal::ConvertGeomMesh(
					UsdMesh,
					TempMeshDescription,
					TempMaterialInfo,
					OptionsCopy
				);
			}

			if (bSuccess)
			{
				OutLODIndexToMeshDescription = {MoveTemp(TempMeshDescription)};
				OutLODIndexToMaterialInfo = {MoveTemp(TempMaterialInfo)};
			}
		}
	}

	void RepairNormalsAndTangents(const FString& PrimPath, FMeshDescription& MeshDescription)
	{
		FStaticMeshConstAttributes Attributes{MeshDescription};
		TArrayView<const FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals().GetRawArray();

		// Similar to FStaticMeshOperations::AreNormalsAndTangentsValid but we don't care about tangents since we never
		// read those from USD
		uint64 InvalidNormalCount = 0;
		for (const FVertexInstanceID VertexInstanceID : MeshDescription.VertexInstances().GetElementIDs())
		{
			if (VertexInstanceNormals[VertexInstanceID].IsNearlyZero() || VertexInstanceNormals[VertexInstanceID].ContainsNaN())
			{
				++InvalidNormalCount;
			}
		}
		if (InvalidNormalCount == 0)
		{
			return;
		}

		const float InvalidNormalFraction = (float)InvalidNormalCount / (float)VertexInstanceNormals.Num();

		// We always need to do this at this point as ComputeTangentsAndNormals will end up computing tangents anyway
		// and our triangle tangents are always invalid
		FStaticMeshOperations::ComputeTriangleTangentsAndNormals(MeshDescription);

		const static FString MeshNormalRepairThresholdText = TEXT("USD.MeshNormalRepairThreshold");

		// Make sure our normals can be rebuilt from MeshDescription::InitializeAutoGeneratedAttributes in case some tool needs them.
		// Always force-compute tangents here as we never have them anyway. If we don't force them to be recomputed we'll get
		// the worst of both worlds as some of these will be arbitrarily recomputed anyway, and some will be left invalid
		EComputeNTBsFlags Options = GSkipMeshTangentComputation ? EComputeNTBsFlags::None : EComputeNTBsFlags::UseMikkTSpace | EComputeNTBsFlags::Tangents;
		if (InvalidNormalFraction >= GMeshNormalRepairThreshold)
		{
			Options |= EComputeNTBsFlags::Normals;
			UE_LOG(LogUsd, Warning, TEXT("%f%% of the normals from Mesh prim '%s' are invalid. This is at or above the threshold of '%f%%' (configurable via the cvar '%s'), so normals will be discarded and fully recomputed."),
				InvalidNormalFraction * 100.0f,
				*PrimPath,
				GMeshNormalRepairThreshold * 100.0f,
				*MeshNormalRepairThresholdText
			);
		}
		else if (InvalidNormalFraction > 0)
		{
			UE_LOG(LogUsd, Warning, TEXT("%f%% of the normals from Mesh prim '%s' are invalid. This is below the threshold of '%f%%' (configurable via the cvar '%s'), so the invalid normals will be repaired."),
				InvalidNormalFraction * 100.0f,
				*PrimPath,
				GMeshNormalRepairThreshold * 100.0f,
				*MeshNormalRepairThresholdText
			);
		}
		FStaticMeshOperations::ComputeTangentsAndNormals(MeshDescription, Options);
	}

	UStaticMesh* CreateStaticMesh(TArray<FMeshDescription>& LODIndexToMeshDescription, FUsdSchemaTranslationContext& Context, const FString& MeshName, const bool bShouldEnableNanite, bool& bOutIsNew)
	{
		UStaticMesh* StaticMesh = nullptr;

		bool bHasValidMeshDescription = false;

		FSHAHash AllLODHash;
		{
			FSHA1 SHA1;

			for (const FMeshDescription& MeshDescription : LODIndexToMeshDescription)
			{
				const bool bSkipTransientAttributes = true;
				FSHAHash LODHash = FStaticMeshOperations::ComputeSHAHash(MeshDescription, bSkipTransientAttributes);
				SHA1.Update(&LODHash.Hash[0], sizeof(LODHash.Hash));

				if (!MeshDescription.IsEmpty())
				{
					bHasValidMeshDescription = true;
				}
			}

			// Put whether we want Nanite or not within the hash, so that the user could have one instance of the mesh without Nanite and another
			// with Nanite if they want to (using the override parameters). This also nicely handles a couple of edge cases:
			//	- What if we change a mesh from having Nanite disabled to enabled, or vice-versa (e.g. by changing the threshold)? We'd reuse the mesh from the asset cache
			//    in that case, so we'd need to rebuild it;
			//  - What if multiple meshes on the scene hash the same, but only one of them has a Nanite override attribute?
			// If we always enabled Nanite when either the mesh from the asset cache or the new prim wanted it, we wouldn't be able to turn Nanite off from
			// a single mesh that once had it enabled: It would always find the old Nanite-enabled mesh on the cache and leave it enabled.
			// If we always set Nanite to whatever the current prim wants, we could handle a single mesh turning Nanite on/off alright, but then we can't handle
			// the case where multiple meshes on the scene hash the same and only one of them has the override: The last prim would win, and they'd all be randomly either
			// enabled or disabled.
			// Note that we could also fix these problems by trying to check if a mesh is reused due to being in the cache from an old instance of the stage, or due to being used by
			// another prim, but that doesn't seem like a good path to go down
			// Additionally, hashing this bool also prevents us from having to force-rebuild a mesh to switch its Nanite flag, which could be tricky to do since some of
			// these build steps are async/thread-pool based.
			SHA1.Update(reinterpret_cast<const uint8*>(&bShouldEnableNanite), sizeof(bShouldEnableNanite));

			// Hash the threshhold so that if we update it and reload we'll regenerate static meshes
			SHA1.Update(reinterpret_cast<const uint8*>(&GMeshNormalRepairThreshold), sizeof(GMeshNormalRepairThreshold));

			SHA1.Final();
			SHA1.GetHash(&AllLODHash.Hash[0]);
		}

		if (Context.AssetCache)
		{
			StaticMesh = Cast< UStaticMesh >(Context.AssetCache->GetCachedAsset(AllLODHash.ToString()));
		}

		if (!StaticMesh && bHasValidMeshDescription)
		{
			bOutIsNew = true;

			FName AssetName = MakeUniqueObjectName(
				GetTransientPackage(),
				UStaticMesh::StaticClass(),
				*IUsdClassesModule::SanitizeObjectName(FPaths::GetBaseFilename(MeshName))
			);
			StaticMesh = NewObject< UStaticMesh >(GetTransientPackage(), AssetName, Context.ObjectFlags | EObjectFlags::RF_Public | EObjectFlags::RF_Transient);

#if WITH_EDITOR
			for (int32 LODIndex = 0; LODIndex < LODIndexToMeshDescription.Num(); ++LODIndex)
			{
				FMeshDescription& MeshDescription = LODIndexToMeshDescription[LODIndex];

				RepairNormalsAndTangents(MeshName, MeshDescription);

				FStaticMeshSourceModel& SourceModel = StaticMesh->AddSourceModel();
				SourceModel.BuildSettings.bGenerateLightmapUVs = false;
				SourceModel.BuildSettings.bRecomputeNormals = false;
				SourceModel.BuildSettings.bRecomputeTangents = false;
				SourceModel.BuildSettings.bBuildReversedIndexBuffer = false;

				FMeshDescription* StaticMeshDescription = StaticMesh->CreateMeshDescription(LODIndex);
				check(StaticMeshDescription);
				*StaticMeshDescription = MoveTemp(MeshDescription);
			}

			FMeshBudgetProjectSettingsUtils::SetLodGroupForStaticMesh(StaticMesh);

#endif // WITH_EDITOR

			StaticMesh->SetLightingGuid();

			if (Context.AssetCache)
			{
				Context.AssetCache->CacheAsset(AllLODHash.ToString(), StaticMesh);
			}
		}
		else
		{
			//FPlatformMisc::LowLevelOutputDebugStringf( TEXT("Mesh found in cache %s\n"), *StaticMesh->GetName() );
			bOutIsNew = false;
		}

		return StaticMesh;
	}

	void PreBuildStaticMesh(UStaticMesh& StaticMesh)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UsdGeomMeshTranslatorImpl::PreBuildStaticMesh);

		if (StaticMesh.GetRenderData())
		{
			StaticMesh.ReleaseResources();
			StaticMesh.ReleaseResourcesFence.Wait();
		}

		StaticMesh.SetRenderData(MakeUnique< FStaticMeshRenderData >());
		StaticMesh.CreateBodySetup();
		StaticMesh.MarkAsNotHavingNavigationData(); // Needed or else it will warn if we try cooking with body setup
	}

	bool BuildStaticMesh(UStaticMesh& StaticMesh, const FStaticFeatureLevel& FeatureLevel, TArray<FMeshDescription>& LODIndexToMeshDescription)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UsdGeomMeshTranslatorImpl::BuildStaticMesh);

		if (LODIndexToMeshDescription.Num() == 0)
		{
			return false;
		}

#if WITH_EDITOR
		ITargetPlatformManagerModule& TargetPlatformManager = GetTargetPlatformManagerRef();
		ITargetPlatform* RunningPlatform = TargetPlatformManager.GetRunningTargetPlatform();
		check(RunningPlatform);

		const FStaticMeshLODSettings& LODSettings = RunningPlatform->GetStaticMeshLODSettings();
		StaticMesh.GetRenderData()->Cache(RunningPlatform, &StaticMesh, LODSettings);
#else
		StaticMesh.GetRenderData()->AllocateLODResources(LODIndexToMeshDescription.Num());

		// Build render data from each mesh description
		for (int32 LODIndex = 0; LODIndex < LODIndexToMeshDescription.Num(); ++LODIndex)
		{
			FStaticMeshLODResources& LODResources = StaticMesh.GetRenderData()->LODResources[LODIndex];

			FMeshDescription& MeshDescription = LODIndexToMeshDescription[LODIndex];
			TVertexInstanceAttributesConstRef< FVector4f > MeshDescriptionColors = MeshDescription.VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color);

			// Compute normals here if necessary because they're not going to be computed via the regular static mesh build pipeline at runtime
			// (i.e. StaticMeshBuilder is not available at runtime)
			// We need polygon info because ComputeTangentsAndNormals uses it to repair the invalid vertex normals/tangents
			// Can't calculate just the required polygons as ComputeTangentsAndNormals is parallel and we can't guarantee thread-safe access patterns
			FStaticMeshOperations::ComputeTriangleTangentsAndNormals(MeshDescription);
			FStaticMeshOperations::ComputeTangentsAndNormals(MeshDescription, EComputeNTBsFlags::UseMikkTSpace);

			// Manually set this as it seems the UStaticMesh only sets this whenever the mesh is serialized, which we won't do
			LODResources.bHasColorVertexData = MeshDescriptionColors.GetNumElements() > 0;

			StaticMesh.BuildFromMeshDescription(MeshDescription, LODResources);
		}

#endif // WITH_EDITOR

		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("USD.EnableCollision"));
		bool bEnableCollision = CVar && CVar->GetBool();

		if (StaticMesh.GetBodySetup())
		{
			if (bEnableCollision)
			{
				StaticMesh.GetBodySetup()->CreatePhysicsMeshes();
			}
			else
			{
				StaticMesh.GetBodySetup()->DefaultInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
				StaticMesh.GetBodySetup()->DefaultInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			}
		}
		return true;
	}

	void PostBuildStaticMesh(UStaticMesh& StaticMesh, const TArray<FMeshDescription>& LODIndexToMeshDescription)
	{
		// For runtime builds, the analogue for this stuff is already done from within BuildFromMeshDescriptions
		TRACE_CPUPROFILER_EVENT_SCOPE(UsdGeomMeshTranslatorImpl::PostBuildStaticMesh);

		StaticMesh.InitResources();

#if WITH_EDITOR
		// Fetch the MeshDescription from the StaticMesh because we'll have moved it away from LODIndexToMeshDescription CreateStaticMesh
		if (const FMeshDescription* MeshDescription = StaticMesh.GetMeshDescription(0))
		{
			StaticMesh.GetRenderData()->Bounds = MeshDescription->GetBounds();
		}
		StaticMesh.CalculateExtendedBounds();
		StaticMesh.ClearMeshDescriptions(); // Clear mesh descriptions to reduce memory usage, they are kept only in bulk data form
#else
		// Fetch the MeshDescription from the imported LODIndexToMeshDescription as StaticMesh.GetMeshDescription is editor-only
		StaticMesh.GetRenderData()->Bounds = LODIndexToMeshDescription[0].GetBounds();
		StaticMesh.CalculateExtendedBounds();
#endif // WITH_EDITOR
	}
}

FBuildStaticMeshTaskChain::FBuildStaticMeshTaskChain(
	const TSharedRef< FUsdSchemaTranslationContext >& InContext,
	const UE::FSdfPath& InPrimPath,
	const TOptional<UE::FSdfPath>& InAlternativePrimToLinkAssetsTo
)
	: PrimPath(InPrimPath)
	, Context(InContext)
	, AlternativePrimToLinkAssetsTo(InAlternativePrimToLinkAssetsTo)
{
}

void FBuildStaticMeshTaskChain::SetupTasks()
{
	// Ignore meshes from disabled purposes
	if (!EnumHasAllFlags(Context->PurposesToLoad, IUsdPrim::GetPurpose(GetPrim())))
	{
		return;
	}

	// Create static mesh (Main thread)
	Do(ESchemaTranslationLaunchPolicy::Sync,
		[this]()
		{
			// Force load MeshBuilderModule so that it's ready for the async tasks
#if WITH_EDITOR
			FModuleManager::LoadModuleChecked< IMeshBuilderModule >(TEXT("MeshBuilder"));
#endif // WITH_EDITOR

			const FString PrimPathString = PrimPath.GetString();

			// It's useful to have the LOD Mesh prims be named "LOD0", "LOD1", etc. within the LOD variants so that we
			// can easily tell which Mesh is actually meant to be the LOD mesh (in case there are more Meshes in each
			// variant or other Meshes outside of the variant), but it's not ideal to have all the generated assets end
			// up imported as "SM_LOD0_22", "SM_LOD0_23", etc. So here we fetch the parent prim name in case we're a LOD
			FString MeshName;
			if (Context->bAllowInterpretingLODs && UsdUtils::IsGeomMeshALOD(GetPrim()))
			{
				MeshName = PrimPath.GetParentPath().GetString();
			}
			else
			{
				MeshName = PrimPathString;
			}

			bool bIsNew = true;
			const bool bShouldEnableNanite = UsdGeomMeshTranslatorImpl::ShouldEnableNanite(LODIndexToMeshDescription, LODIndexToMaterialInfo, *Context, GetPrim());
			StaticMesh = UsdGeomMeshTranslatorImpl::CreateStaticMesh(LODIndexToMeshDescription, *Context, MeshName, bShouldEnableNanite, bIsNew);

			if (StaticMesh)
			{
				if (Context->InfoCache)
				{
					const UE::FSdfPath& TargetPath = AlternativePrimToLinkAssetsTo.IsSet()
						? AlternativePrimToLinkAssetsTo.GetValue()
						: PrimPath;
					Context->InfoCache->LinkAssetToPrim(TargetPath, StaticMesh);
				}

#if WITH_EDITOR
				StaticMesh->NaniteSettings.bEnabled = bShouldEnableNanite;
#endif // WITH_EDITOR

				UUsdMeshAssetUserData* UserData = StaticMesh->GetAssetUserData<UUsdMeshAssetUserData>();
				if (!UserData)
				{
					UserData = NewObject<UUsdMeshAssetUserData>(StaticMesh, TEXT("UUSDAssetUserData"));
					UserData->PrimvarToUVIndex = LODIndexToMaterialInfo[0].PrimvarToUVIndex;	// We use the same primvar mapping for all LODs
					StaticMesh->AddAssetUserData(UserData);
				}
				UserData->PrimPaths.AddUnique(PrimPath.GetString());

				MeshTranslationImpl::RecordSourcePrimsForMaterialSlots(LODIndexToMaterialInfo, UserData);

				// Only the original creator of the prim at creation time gets to set the material assignments
				// directly on the mesh, all others prims ensure their materials via material overrides on the
				// components
				if (bIsNew)
				{
					UsdGeomMeshTranslatorImpl::ProcessStaticMeshMaterials(
						GetPrim(),
						LODIndexToMaterialInfo,
						*StaticMesh,
						*Context->AssetCache.Get(),
						Context->InfoCache.Get(),
						Context->Time,
						Context->ObjectFlags
					);

#if WITH_EDITOR
					const bool bRebuildAll = true;
					StaticMesh->UpdateUVChannelData(bRebuildAll);
#else
					// UpdateUVChannelData doesn't do anything without the editor
					for (FStaticMaterial& Material : StaticMesh->GetStaticMaterials())
					{
						Material.UVChannelData.bInitialized = true;
					}
#endif // WITH_EDITOR
				}
			}

			// Only need to continue building the mesh if we just created it
			return bIsNew;
		});

#if WITH_EDITOR
	// Commit mesh description (Async)
	Then(ESchemaTranslationLaunchPolicy::Async,
		[this]()
		{
			UStaticMesh::FCommitMeshDescriptionParams Params;
			Params.bMarkPackageDirty = false;
			Params.bUseHashAsGuid = true;

			for (int32 LODIndex = 0; LODIndex < LODIndexToMeshDescription.Num(); ++LODIndex)
			{
				StaticMesh->CommitMeshDescription(LODIndex, Params);
			}

			return true;
		});
#endif // WITH_EDITOR

	// PreBuild static mesh (Main thread)
	Then(ESchemaTranslationLaunchPolicy::Sync,
		[this]()
		{
			RecreateRenderStateContextPtr = MakeShared<FStaticMeshComponentRecreateRenderStateContext>(StaticMesh, true, true);

			UsdGeomMeshTranslatorImpl::PreBuildStaticMesh(*StaticMesh);

			return true;
		});

	// Build static mesh (Async)
	Then(ESchemaTranslationLaunchPolicy::Async,
		[this]() mutable
		{
			FStaticFeatureLevel FeatureLevel = GMaxRHIFeatureLevel;

			UWorld* World = Context->Level ? Context->Level->GetWorld() : nullptr;
			if (!World)
			{
				World = IUsdClassesModule::GetCurrentWorld();
			}
			if (World)
			{
				FeatureLevel = World->GetFeatureLevel();
			}

			if (!UsdGeomMeshTranslatorImpl::BuildStaticMesh(*StaticMesh, FeatureLevel, LODIndexToMeshDescription))
			{
				// Build failed, discard the mesh
				StaticMesh = nullptr;

				return false;
			}

			return true;
		});

	// PostBuild static mesh (Main thread)
	Then(ESchemaTranslationLaunchPolicy::Sync,
		[this]()
		{
			UsdGeomMeshTranslatorImpl::PostBuildStaticMesh(*StaticMesh, LODIndexToMeshDescription);

			RecreateRenderStateContextPtr.Reset();

			return true;
		});
}

FGeomMeshCreateAssetsTaskChain::FGeomMeshCreateAssetsTaskChain(
	const TSharedRef< FUsdSchemaTranslationContext >& InContext,
	const UE::FSdfPath& InPrimPath,
	const TOptional<UE::FSdfPath>& AlternativePrimToLinkAssetsTo,
	const FTransform& InAdditionalTransform
)
	: FBuildStaticMeshTaskChain(InContext, InPrimPath, AlternativePrimToLinkAssetsTo)
	, AdditionalTransform(InAdditionalTransform)
{
	SetupTasks();
}

void FGeomMeshCreateAssetsTaskChain::SetupTasks()
{
	FScopedUnrealAllocs UnrealAllocs;

	// To parse all LODs we need to actively switch variant sets to other variants (triggering prim loading/unloading and notices),
	// which could cause race conditions if other async translation tasks are trying to access those prims
	ESchemaTranslationLaunchPolicy LaunchPolicy = ESchemaTranslationLaunchPolicy::Async;
	if (Context->bAllowInterpretingLODs && UsdUtils::IsGeomMeshALOD(GetPrim()))
	{
		LaunchPolicy = ESchemaTranslationLaunchPolicy::ExclusiveSync;
	}

	// Create mesh descriptions (Async or ExclusiveSync)
	Do(LaunchPolicy,
		[this]() -> bool
		{
			pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
			if (!Context->RenderContext.IsNone())
			{
				RenderContextToken = UnrealToUsd::ConvertToken(*Context->RenderContext.ToString()).Get();
			}

			pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
			if (!Context->MaterialPurpose.IsNone())
			{
				MaterialPurposeToken = UnrealToUsd::ConvertToken(*Context->MaterialPurpose.ToString()).Get();
			}

			UsdToUnreal::FUsdMeshConversionOptions Options;
			Options.TimeCode = Context->Time;
			Options.PurposesToLoad = Context->PurposesToLoad;
			Options.RenderContext = RenderContextToken;
			Options.MaterialPurpose = MaterialPurposeToken;
			Options.bMergeIdenticalMaterialSlots = Context->bMergeIdenticalMaterialSlots;
			Options.AdditionalTransform = AdditionalTransform;

			UsdGeomMeshTranslatorImpl::LoadMeshDescriptions(
				GetPrim(),
				LODIndexToMeshDescription,
				LODIndexToMaterialInfo,
				Options,
				Context->bAllowInterpretingLODs
			);

			// If we have at least one valid LOD, we should keep going
			for (const FMeshDescription& MeshDescription : LODIndexToMeshDescription)
			{
				if (!MeshDescription.IsEmpty())
				{
					return true;
				}
			}
			return false;
		});

	FBuildStaticMeshTaskChain::SetupTasks();
}

void FUsdGeomMeshTranslator::CreateAssets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUsdGeomMeshTranslator::CreateAssets);

	if (!IsMeshPrim())
	{
		return Super::CreateAssets();
	}

	TSharedRef< FGeomMeshCreateAssetsTaskChain > AssetsTaskChain = MakeShared< FGeomMeshCreateAssetsTaskChain >(Context, PrimPath);

	Context->TranslatorTasks.Add(MoveTemp(AssetsTaskChain));
}

USceneComponent* FUsdGeomMeshTranslator::CreateComponents()
{
	if (!IsMeshPrim())
	{
		return Super::CreateComponents();
	}

	TOptional< TSubclassOf< USceneComponent > > ComponentType;

	USceneComponent* SceneComponent = CreateComponentsEx(ComponentType, {});
	UpdateComponents(SceneComponent);

	// Handle material overrides
	// Note: This can be here and not in USDGeomXformableTranslator because there is no way that a collapsed mesh prim could end up with a material override
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(SceneComponent))
	{
		if (Context->InfoCache)
		{
			if (UStaticMesh* StaticMesh = Context->InfoCache->GetSingleAssetForPrim<UStaticMesh>(
				PrimPath
			))
			{
				TArray<UMaterialInterface*> ExistingAssignments;
				for (FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
				{
					ExistingAssignments.Add(StaticMaterial.MaterialInterface);
				}

				MeshTranslationImpl::SetMaterialOverrides(
					GetPrim(),
					ExistingAssignments,
					*StaticMeshComponent,
					*Context->AssetCache.Get(),
					*Context->InfoCache.Get(),
					Context->Time,
					Context->ObjectFlags,
					Context->bAllowInterpretingLODs,
					Context->RenderContext,
					Context->MaterialPurpose
				);
			}
		}
	}

	return SceneComponent;
}

void FUsdGeomMeshTranslator::UpdateComponents(USceneComponent* SceneComponent)
{
	if (!IsMeshPrim())
	{
		return Super::UpdateComponents(SceneComponent);
	}

	if (SceneComponent)
	{
		SceneComponent->Modify();
	}

	if (UsdUtils::IsAnimatedMesh(GetPrim()))
	{
		// The assets might have changed since our attributes are animated
		// Note that we must wait for these to complete as they make take a while and we want to
		// reassign our new static meshes when we get to FUsdGeomXformableTranslator::UpdateComponents
		CreateAssets();
		Context->CompleteTasks();
	}

	Super::UpdateComponents(SceneComponent);
}

bool FUsdGeomMeshTranslator::CollapsesChildren(ECollapsingType CollapsingType) const
{
	if (!IsMeshPrim())
	{
		return Super::CollapsesChildren(CollapsingType);
	}

	// We can't claim we collapse anything here since we'll just parse the mesh for this prim and that's it,
	// otherwise the translation context wouldn't spawn translators for our child prims.
	// Another approach would be to actually recursively collapse our child mesh prims, but that leads to a few
	// issues. For example this translator could end up globbing a child Mesh prim, while the translation context
	// could simultaneously spawn other translators that could also end up accounting for that same mesh.
	// Generally Gprims shouldn't be nested into each other anyway (see https://graphics.pixar.com/usd/release/glossary.html#usdglossary-gprim)
	// so it's likely best to just not collapse anything here.
	return false;
}

bool FUsdGeomMeshTranslator::CanBeCollapsed(ECollapsingType CollapsingType) const
{
	if (!IsMeshPrim())
	{
		return Super::CanBeCollapsed(CollapsingType);
	}

	UE::FUsdPrim Prim = GetPrim();

	// Don't collapse if our final UStaticMesh would have multiple LODs
	if (Context->bAllowInterpretingLODs &&
		CollapsingType == ECollapsingType::Assets &&
		UsdUtils::IsGeomMeshALOD(Prim))
	{
		return false;
	}

	return Super::CanBeCollapsed(CollapsingType);
}

TSet<UE::FSdfPath> FUsdGeomMeshTranslator::CollectAuxiliaryPrims() const
{
	if (!IsMeshPrim())
	{
		return Super::CollectAuxiliaryPrims();
	}

	if (!Context->bIsBuildingInfoCache)
	{
		return Context->InfoCache->GetAuxiliaryPrims(PrimPath);
	}

	TSet<UE::FSdfPath> Result;
	{
		FScopedUsdAllocs UsdAllocs;

		pxr::UsdPrim Prim = GetPrim();

		// The UsdGeomSubset prims are used to express multiple material assignments per mesh. A change in them could
		// mean a change in triangle to material slot mapping
		TArray<TUsdStore<pxr::UsdPrim>> ChildPrims = UsdUtils::GetAllPrimsOfType(
			Prim,
			pxr::TfType::Find<pxr::UsdGeomSubset>()
		);

		Result.Reserve(ChildPrims.Num());
		for (const TUsdStore<pxr::UsdPrim>& ChildPrim : ChildPrims)
		{
			Result.Add(UE::FSdfPath{ChildPrim.Get().GetPrimPath()});
		}
	}
	return Result;
}

bool FUsdGeomMeshTranslator::IsMeshPrim() const
{
	UE::FUsdPrim Prim = GetPrim();
	if (Prim && (Prim.IsA(TEXT("Mesh")) || (Context->bAllowInterpretingLODs && UsdUtils::DoesPrimContainMeshLODs(Prim))))
	{
		return true;
	}

	return false;
}

#endif // #if USE_USD_SDK
