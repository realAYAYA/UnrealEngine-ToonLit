// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshTranslationImpl.h"

#include "USDAssetCache2.h"
#include "USDAssetImportData.h"
#include "USDGeomMeshConversion.h"
#include "USDInfoCache.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDProjectSettings.h"
#include "USDTypesConversion.h"

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
#include "pxr/usd/usd/editContext.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/xformable.h"
#include "pxr/usd/usdShade/material.h"
#include "USDIncludesEnd.h"

TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> MeshTranslationImpl::ResolveMaterialAssignmentInfo(
	const pxr::UsdPrim& UsdPrim,
	const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& AssignmentInfo,
	UUsdAssetCache2& AssetCache,
	FUsdInfoCache& InfoCache,
	EObjectFlags Flags
)
{
	FScopedUnrealAllocs Allocs;

	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials;

	uint32 GlobalResolvedMaterialIndex = 0;
	for (int32 InfoIndex = 0; InfoIndex < AssignmentInfo.Num(); ++InfoIndex)
	{
		const TArray< UsdUtils::FUsdPrimMaterialSlot >& Slots = AssignmentInfo[InfoIndex].Slots;

		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex, ++GlobalResolvedMaterialIndex)
		{
			const UsdUtils::FUsdPrimMaterialSlot& Slot = Slots[SlotIndex];
			UMaterialInterface* Material = nullptr;

			switch (Slot.AssignmentType)
			{
			case UsdUtils::EPrimAssignmentType::DisplayColor:
			{
				// Try reusing an already created DisplayColor material
				if (UMaterialInterface* ExistingMaterial = Cast<UMaterialInterface>(AssetCache.GetCachedAsset(Slot.MaterialSource)))
				{
					Material = ExistingMaterial;
				}

				// Need to create a new DisplayColor material
				if (Material == nullptr)
				{
					if (TOptional< UsdUtils::FDisplayColorMaterial > DisplayColorDesc = UsdUtils::FDisplayColorMaterial::FromString(Slot.MaterialSource))
					{
						UMaterialInstance* MaterialInstance = nullptr;

						if (GIsEditor)  // Editor, PIE => true; Standlone, packaged => false
						{
							MaterialInstance = UsdUtils::CreateDisplayColorMaterialInstanceConstant(DisplayColorDesc.GetValue());
#if WITH_EDITOR
							// Leave PrimPath as empty as it likely will be reused by many prims
							UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >(MaterialInstance, TEXT("USDAssetImportData"));
							MaterialInstance->AssetImportData = ImportData;
#endif // WITH_EDITOR
						}
						else
						{
							MaterialInstance = UsdUtils::CreateDisplayColorMaterialInstanceDynamic(DisplayColorDesc.GetValue());
						}

						// We can only cache transient assets
						MaterialInstance->SetFlags(RF_Transient);

						AssetCache.CacheAsset(Slot.MaterialSource, MaterialInstance);
						Material = MaterialInstance;
					}
				}

				break;
			}
			case UsdUtils::EPrimAssignmentType::MaterialPrim:
			{
				UMaterialInstance* OneSidedMat = nullptr;

				TSet<UMaterialInstance*> ExistingMaterials = InfoCache.GetAssetsForPrim<UMaterialInstance>(UE::FSdfPath{*Slot.MaterialSource});
				for (UMaterialInstance* ExistingMaterial : ExistingMaterials)
				{
					const bool bExistingIsTwoSided = ExistingMaterial->IsTwoSided();

					if (!bExistingIsTwoSided)
					{
						OneSidedMat = ExistingMaterial;
					}

					if (Slot.bMeshIsDoubleSided == bExistingIsTwoSided)
					{
						Material = ExistingMaterial;
					}
				}

				FString MaterialHash = AssetCache.GetHashForAsset(OneSidedMat);

				if (Slot.bMeshIsDoubleSided)
				{
					MaterialHash = MaterialHash + UnrealIdentifiers::TwoSidedMaterialSuffix;

					// Need to create a two-sided material on-demand
					if (!Material)
					{
						// By now we parsed all materials so we must have the single-sided version of this material
						if (!OneSidedMat)
						{
							UE_LOG(LogUsd, Warning, TEXT("Failed to generate a two-sided material from the material prim at path '%s' as no single-sided material was generated for it."), *Slot.MaterialSource);
							continue;
						}

						// Check if for some reason we already have a two-sided material ready due to a complex scenario
						// related to the global cache
						UMaterialInstance* TwoSidedMat = Cast<UMaterialInstance>(AssetCache.GetCachedAsset(MaterialHash));
						if (!TwoSidedMat)
						{
							// Important to not use GetBaseMaterial() here because if our parent is the translucent we'll
							// get the reference UsdPreviewSurface instead, as that is also *its* reference
							UMaterialInterface* ReferenceMaterial = OneSidedMat->Parent.Get();
							UMaterialInterface* ReferenceMaterialTwoSided =
								MeshTranslationImpl::GetTwoSidedVersionOfReferencePreviewSurfaceMaterial(ReferenceMaterial);
							if (!ensure(ReferenceMaterialTwoSided && ReferenceMaterialTwoSided != ReferenceMaterial))
							{
								continue;
							}

							const FName NewInstanceName = MakeUniqueObjectName(
								GetTransientPackage(),
								UMaterialInstance::StaticClass(),
								*(FPaths::GetBaseFilename(Slot.MaterialSource) + UnrealIdentifiers::TwoSidedMaterialSuffix)
							);

#if WITH_EDITOR
							UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(OneSidedMat);
							if (GIsEditor && MIC)
							{
								UMaterialInstanceConstant* TwoSidedMIC = NewObject<UMaterialInstanceConstant>(
									GetTransientPackage(),
									NewInstanceName,
									Flags
								);
								if (TwoSidedMIC)
								{
									UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >(
										TwoSidedMIC,
										TEXT("USDAssetImportData")
									);
									ImportData->PrimPath = Slot.MaterialSource;
									TwoSidedMIC->AssetImportData = ImportData;
								}

								TwoSidedMIC->SetParentEditorOnly(ReferenceMaterialTwoSided);
								TwoSidedMIC->CopyMaterialUniformParametersEditorOnly(OneSidedMat);

								TwoSidedMat = TwoSidedMIC;
							}
							else
#endif // WITH_EDITOR
							if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(OneSidedMat))
							{
								UMaterialInstanceDynamic* TwoSidedMID = UMaterialInstanceDynamic::Create(
									ReferenceMaterialTwoSided,
									GetTransientPackage(),
									NewInstanceName
								);
								if (!ensure(TwoSidedMID))
								{
									continue;
								}

								TwoSidedMID->CopyParameterOverrides(MID);

								TwoSidedMat = TwoSidedMID;
							}

						}

						if (TwoSidedMat)
						{
							TwoSidedMat->SetFlags(RF_Transient);
							Material = TwoSidedMat;
						}
					}
				}

				// Cache the material to "ping it" as active, but also register two sided materials for the
				// first time
				AssetCache.CacheAsset(MaterialHash, Material);
				InfoCache.LinkAssetToPrim(UE::FSdfPath{*Slot.MaterialSource}, Material);

				break;
			}
			case UsdUtils::EPrimAssignmentType::UnrealMaterial:
			{
				Material = Cast< UMaterialInterface >(FSoftObjectPath(Slot.MaterialSource).TryLoad());
				if (!Material)
				{
					UE_LOG(LogUsd, Warning, TEXT("UE material '%s' for prim '%s' could not be loaded or was not found."),
						*Slot.MaterialSource,
						*UsdToUnreal::ConvertPath(UsdPrim.GetPrimPath()));
				}
				else if (!Material->IsTwoSided() && Slot.bMeshIsDoubleSided)
				{
					UE_LOG(LogUsd, Warning, TEXT("Using one-sided UE material '%s' for doubleSided prim '%s'"),
						*Slot.MaterialSource,
						*UsdToUnreal::ConvertPath(UsdPrim.GetPrimPath())
					);
				}

				break;
			}
			case UsdUtils::EPrimAssignmentType::None:
			default:
			{
				ensure(false);
				break;
			}
			}

			ResolvedMaterials.Add(&Slot, Material);
		}
	}

	return ResolvedMaterials;
}

void MeshTranslationImpl::SetMaterialOverrides(
	const pxr::UsdPrim& Prim,
	const TArray<UMaterialInterface*>& ExistingAssignments,
	UMeshComponent& MeshComponent,
	UUsdAssetCache2& AssetCache,
	FUsdInfoCache& InfoCache,
	float Time,
	EObjectFlags Flags,
	bool bInterpretLODs,
	const FName& RenderContext,
	const FName& MaterialPurpose
)
{
	FScopedUsdAllocs Allocs;

	pxr::SdfPath PrimPath = Prim.GetPath();
	pxr::UsdStageRefPtr Stage = Prim.GetStage();

	pxr::TfToken RenderContextToken = pxr::UsdShadeTokens->universalRenderContext;
	if (!RenderContext.IsNone())
	{
		RenderContextToken = UnrealToUsd::ConvertToken(*RenderContext.ToString()).Get();
	}

	pxr::TfToken MaterialPurposeToken = pxr::UsdShadeTokens->allPurpose;
	if (!MaterialPurpose.IsNone())
	{
		MaterialPurposeToken = UnrealToUsd::ConvertToken(*MaterialPurpose.ToString()).Get();
	}

	TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToAssignments;
	const bool bProvideMaterialIndices = false; // We have no use for material indices and it can be slow to retrieve, as it will iterate all faces

	// Extract material assignment info from prim if it is a LOD mesh
	bool bInterpretedLODs = false;
	if (bInterpretLODs && UsdUtils::IsGeomMeshALOD(Prim))
	{
		TMap<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo> LODIndexToAssignmentsMap;
		TFunction<bool(const pxr::UsdGeomMesh&, int32)> IterateLODs = [&](const pxr::UsdGeomMesh& LODMesh, int32 LODIndex)
		{
			// In here we need to parse the assignments again and can't rely on the cache because the info cache
			// only has info about the default variant selection state of the stage: It won't have info about the
			// LOD variant set setups as that involves actively toggling variants.
			// TODO: Make the cache rebuild collect this info. Right now is not a good time for this as that would
			// break the parallel-for setup that that function has
			UsdUtils::FUsdPrimMaterialAssignmentInfo LODInfo = UsdUtils::GetPrimMaterialAssignments(
				LODMesh.GetPrim(),
				pxr::UsdTimeCode(Time),
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			);
			LODIndexToAssignmentsMap.Add(LODIndex, LODInfo);
			return true;
		};

		pxr::UsdPrim ParentPrim = Prim.GetParent();
		bInterpretedLODs = UsdUtils::IterateLODMeshes(ParentPrim, IterateLODs);

		if (bInterpretedLODs)
		{
			LODIndexToAssignmentsMap.KeySort(TLess<int32>());
			for (TPair<int32, UsdUtils::FUsdPrimMaterialAssignmentInfo>& Entry : LODIndexToAssignmentsMap)
			{
				LODIndexToAssignments.Add(MoveTemp(Entry.Value));
			}
		}
	}

	// Refresh reference to Prim because variant switching potentially invalidated it
	pxr::UsdPrim ValidPrim = Stage->GetPrimAtPath(PrimPath);

	// Extract material assignment info from prim if its *not* a LOD mesh, or if we failed to parse LODs
	if (!bInterpretedLODs)
	{
		// Try to pull the material slot info from the info cache first, which is useful if ValidPrim is a collapsed
		// prim subtree: Querying it's material assignments directly is likely not what we want, as ValidPrim is
		// likely just some root Xform prim.
		// Note: This only works because we'll rebuild the cache when our material purpose/render context changes,
		// and because in USD relationships (and so material bindings) can't vary with time
		TOptional<TArray<UsdUtils::FUsdPrimMaterialSlot>> SubtreeSlots = InfoCache.GetSubtreeMaterialSlots(UE::FSdfPath{PrimPath});
		if (SubtreeSlots.IsSet())
		{
			UsdUtils::FUsdPrimMaterialAssignmentInfo& NewInfo = LODIndexToAssignments.Emplace_GetRef();
			NewInfo.Slots = MoveTemp(SubtreeSlots.GetValue());
		}
		else
		{
			LODIndexToAssignments = {
				UsdUtils::GetPrimMaterialAssignments(
					ValidPrim,
					pxr::UsdTimeCode(Time),
					bProvideMaterialIndices,
					RenderContextToken,
					MaterialPurposeToken
				)
			};
		}
	}

	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials = MeshTranslationImpl::ResolveMaterialAssignmentInfo(
		ValidPrim,
		LODIndexToAssignments,
		AssetCache,
		InfoCache,
		Flags
	);

	// Compare resolved materials with existing assignments, and create overrides if we need to
	uint32 StaticMeshSlotIndex = 0;
	for (int32 LODIndex = 0; LODIndex < LODIndexToAssignments.Num(); ++LODIndex)
	{
		const TArray< UsdUtils::FUsdPrimMaterialSlot >& LODSlots = LODIndexToAssignments[LODIndex].Slots;
		for (int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++StaticMeshSlotIndex)
		{
			const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[LODSlotIndex];

			UMaterialInterface* Material = nullptr;
			if (UMaterialInterface** FoundMaterial = ResolvedMaterials.Find(&Slot))
			{
				Material = *FoundMaterial;
			}
			else
			{
				UE_LOG(LogUsd, Error, TEXT("Lost track of resolved material for slot '%d' of LOD '%d' for mesh '%s'"), LODSlotIndex, LODIndex, *UsdToUnreal::ConvertPath(Prim.GetPath()));
				continue;
			}

			// If we don't even have as many existing assignments as we have overrides just stop here.
			// This should happen often now because we'll always at least attempt at setting overrides on every
			// component (but only ever set anything if we really need to).
			// Previously we only attempted setting overrides in case the component didn't "own" the mesh prim,
			// but now it is not feasible to do that given the global asset cache and how assets may have come
			// from an entirely new stage/session.
			if (!ExistingAssignments.IsValidIndex(StaticMeshSlotIndex))
			{
				continue;
			}

			UMaterialInterface* ExistingMaterial = ExistingAssignments[StaticMeshSlotIndex];
			if (ExistingMaterial == Material)
			{
				continue;
			}
			else
			{
				MeshComponent.SetMaterial(StaticMeshSlotIndex, Material);
			}
		}
	}
}

UMaterialInterface* MeshTranslationImpl::GetReferencePreviewSurfaceMaterial(EUsdReferenceMaterialProperties ReferenceMaterialProperties)
{
	const UUsdProjectSettings* Settings = GetDefault<UUsdProjectSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	const bool bIsTranslucent = EnumHasAnyFlags(ReferenceMaterialProperties, EUsdReferenceMaterialProperties::Translucent);
	const bool bIsVT = EnumHasAnyFlags(ReferenceMaterialProperties, EUsdReferenceMaterialProperties::VT);
	const bool bIsTwoSided = EnumHasAnyFlags(ReferenceMaterialProperties, EUsdReferenceMaterialProperties::TwoSided);

	const FSoftObjectPath* TargetMaterialPath = nullptr;
	if (bIsTranslucent)
	{
		if (bIsVT)
		{
			if (bIsTwoSided)
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceTranslucentTwoSidedVTMaterial;
			}
			else
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceTranslucentVTMaterial;
			}
		}
		else
		{
			if (bIsTwoSided)
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial;
			}
			else
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceTranslucentMaterial;
			}
		}
	}
	else
	{
		if (bIsVT)
		{
			if (bIsTwoSided)
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceTwoSidedVTMaterial;
			}
			else
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceVTMaterial;
			}
		}
		else
		{
			if (bIsTwoSided)
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceTwoSidedMaterial;
			}
			else
			{
				TargetMaterialPath = &Settings->ReferencePreviewSurfaceMaterial;
			}
		}
	}

	if (!TargetMaterialPath)
	{
		return nullptr;
	}

	return Cast< UMaterialInterface >(TargetMaterialPath->TryLoad());
}

UMaterialInterface* MeshTranslationImpl::GetVTVersionOfReferencePreviewSurfaceMaterial(UMaterialInterface* ReferenceMaterial)
{
	if (!ReferenceMaterial)
	{
		return nullptr;
	}

	const UUsdProjectSettings* Settings = GetDefault<UUsdProjectSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	const FSoftObjectPath PathName = ReferenceMaterial->GetPathName();
	if (PathName.ToString().Contains(TEXT("VT"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		return ReferenceMaterial;
	}
	else if (PathName == Settings->ReferencePreviewSurfaceMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTwoSidedMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceTwoSidedVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceTranslucentVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceTranslucentTwoSidedVTMaterial.TryLoad());
	}

	// We should only ever call this function with a ReferenceMaterial that matches one of the above paths
	ensure(false);
	return nullptr;
}

UMaterialInterface* MeshTranslationImpl::GetTwoSidedVersionOfReferencePreviewSurfaceMaterial(UMaterialInterface* ReferenceMaterial)
{
	if (!ReferenceMaterial)
	{
		return nullptr;
	}

	const UUsdProjectSettings* Settings = GetDefault<UUsdProjectSettings>();
	if (!Settings)
	{
		return nullptr;
	}

	const FSoftObjectPath PathName = ReferenceMaterial->GetPathName();
	if (PathName.ToString().Contains(TEXT("TwoSided"), ESearchCase::CaseSensitive, ESearchDir::FromEnd))
	{
		return ReferenceMaterial;
	}
	else if (PathName == Settings->ReferencePreviewSurfaceMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceTwoSidedMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceVTMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceTwoSidedVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentVTMaterial)
	{
		return Cast< UMaterialInterface >(Settings->ReferencePreviewSurfaceTranslucentTwoSidedVTMaterial.TryLoad());
	}

	// We should only ever call this function with a ReferenceMaterial that matches one of the above paths
	ensure(false);
	return nullptr;
}

#endif // #if USE_USD_SDK