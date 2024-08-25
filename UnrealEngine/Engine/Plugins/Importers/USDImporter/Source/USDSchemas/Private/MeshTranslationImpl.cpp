// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshTranslationImpl.h"

#include "USDAssetCache2.h"
#include "USDAssetUserData.h"
#include "USDClassesModule.h"
#include "USDConversionUtils.h"
#include "USDGeomMeshConversion.h"
#include "USDInfoCache.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDProjectSettings.h"
#include "USDShadeConversion.h"
#include "USDTypesConversion.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GeometryCache.h"
#include "GeometryCacheComponent.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MaterialShared.h"
#include "MeshDescription.h"
#include "UObject/Package.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "USDIncludesEnd.h"

#define UNUSED_UV_INDEX USD_PREVIEW_SURFACE_MAX_UV_SETS

static_assert(
	USD_PREVIEW_SURFACE_MAX_UV_SETS <= MAX_MESH_TEXTURE_COORDS_MD,
	"UsdPreviewSurface materials can only have up to as many UV sets as MeshDescription supports!"
);

namespace UE::MeshTranslationImplInternal::Private
{
	UMaterialInterface* CreateTwoSidedVersionOfMaterial(UMaterialInterface* OneSidedMat)
	{
		if (!OneSidedMat)
		{
			return nullptr;
		}

		UMaterialInterface* TwoSidedMat = nullptr;

		UMaterialInstance* OneSidedMaterialInstance = Cast<UMaterialInstance>(OneSidedMat);

		// Important to use Parent.Get() and not GetBaseMaterial() here because if our parent is the translucent we'll
		// get the reference UsdPreviewSurface instead, as that is also *its* reference
		UMaterialInterface* ReferenceMaterial = OneSidedMaterialInstance ? OneSidedMaterialInstance->Parent.Get() : nullptr;
		UMaterialInterface* ReferenceMaterialTwoSided = nullptr;
		if (ReferenceMaterial && MeshTranslationImpl::IsReferencePreviewSurfaceMaterial(ReferenceMaterial))
		{
			ReferenceMaterialTwoSided = MeshTranslationImpl::GetTwoSidedVersionOfReferencePreviewSurfaceMaterial(ReferenceMaterial);
		}

		const FName NewInstanceName = MakeUniqueObjectName(
			GetTransientPackage(),
			UMaterialInstance::StaticClass(),
			*(OneSidedMat->GetName() + UnrealIdentifiers::TwoSidedMaterialSuffix)
		);

		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(OneSidedMat);
#if WITH_EDITOR
		UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(OneSidedMat);

		// One-sided material is an instance of one of our USD reference materials.
		// Just create an instance of the TwoSided version of the same reference material and copy parameter values.
		if (GIsEditor && MIC && ReferenceMaterialTwoSided)
		{
			UMaterialInstanceConstant* TwoSidedMIC = NewObject<UMaterialInstanceConstant>(
				GetTransientPackage(),
				NewInstanceName,
				OneSidedMat->GetFlags()
			);

			TwoSidedMIC->SetParentEditorOnly(ReferenceMaterialTwoSided);
			TwoSidedMIC->CopyMaterialUniformParametersEditorOnly(OneSidedMat);

			TwoSidedMat = TwoSidedMIC;
		}

		// One-sided material is some other material (e.g. MaterialX/MDL-generated material).
		// Create a new material instance of it and set the override to two-sided.
		else if (GIsEditor)
		{
			UMaterialInstanceConstant* TwoSidedMIC = NewObject<UMaterialInstanceConstant>(
				GetTransientPackage(),
				NewInstanceName,
				OneSidedMat->GetFlags()
			);

			TwoSidedMIC->SetParentEditorOnly(OneSidedMat);
			TwoSidedMIC->BasePropertyOverrides.bOverride_TwoSided = true;
			TwoSidedMIC->BasePropertyOverrides.TwoSided = true;

			FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::Default, GMaxRHIShaderPlatform);
			UpdateContext.AddMaterialInstance(TwoSidedMIC);

			TwoSidedMat = TwoSidedMIC;
		}

		else
#endif	  // WITH_EDITOR

			// At runtime all we can do is create another instance of our two-sided reference materials, we cannot set
			// another override
			if (MID && ReferenceMaterialTwoSided)
			{
				UMaterialInstanceDynamic* TwoSidedMID = UMaterialInstanceDynamic::Create(
					ReferenceMaterialTwoSided,
					GetTransientPackage(),
					NewInstanceName
				);
				if (!ensure(TwoSidedMID))
				{
					return nullptr;
				}

				TwoSidedMID->CopyParameterOverrides(MID);

				TwoSidedMat = TwoSidedMID;
			}

		return TwoSidedMat;
	}

	/**
	 * Returns Material in case it is already compatible with the provided MeshPrimvarToUVIndex, otherwise creates a
	 * new material instance of it, and sets the UVIndex material parameters to match a UVIndex setup that is compatible
	 * with the mesh.
	 * This function will also already cache and link the generated material.
	 */
	UMaterialInterface* CreatePrimvarCompatibleVersionOfMaterial(
		UMaterialInterface& Material,
		const TMap<FString, int32>& MeshPrimvarToUVIndex,
		UUsdAssetCache2* AssetCache,
		FUsdInfoCache* InfoCache,
		const FString& MaterialHashPrefix,
		bool bReuseIdenticalAssets
	)
	{
		UUsdMaterialAssetUserData* MaterialAssetUserData = Material.GetAssetUserData<UUsdMaterialAssetUserData>();
		if (!ensureMsgf(
				MaterialAssetUserData,
				TEXT("Expected material '%s' to have an UUsdMaterialAssetUserData at this point!"),
				*Material.GetPathName()
			))
		{
			return nullptr;
		}

		UE_LOG(
			LogUsd,
			Verbose,
			TEXT("Getting compatible material based on Material '%s' (Parameter to primvar: %s, primvar to UV index: %s) matching mesh primvar to UV "
				 "index mapping '%s'"),
			*Material.GetPathName(),
			*UsdUtils::StringifyMap(MaterialAssetUserData->ParameterToPrimvar),
			*UsdUtils::StringifyMap(MaterialAssetUserData->PrimvarToUVIndex),
			*UsdUtils::StringifyMap(MeshPrimvarToUVIndex)
		);

		TArray<TSet<FString>> UVIndexToMeshPrimvars;
		UVIndexToMeshPrimvars.SetNum(USD_PREVIEW_SURFACE_MAX_UV_SETS);
		for (const TPair<FString, int32>& MeshPair : MeshPrimvarToUVIndex)
		{
			if (UVIndexToMeshPrimvars.IsValidIndex(MeshPair.Value))
			{
				UVIndexToMeshPrimvars[MeshPair.Value].Add(MeshPair.Key);
			}
		}

		// Check if mesh and material are compatible. Note that it's perfectly valid for the material to try reading
		// an UVIndex the mesh doesn't provide at all, or trying to read a primvar that doesn't exist on the mesh.
		bool bCompatible = true;
		for (const TPair<FString, int32>& Pair : MaterialAssetUserData->PrimvarToUVIndex)
		{
			const FString& MaterialPrimvar = Pair.Key;
			int32 MaterialUVIndex = Pair.Value;

			// If the mesh has the same primvar the material wants, it should be at the same UVIndex the material
			// will read from
			if (const int32* MeshUVIndex = MeshPrimvarToUVIndex.Find(MaterialPrimvar))
			{
				if (*MeshUVIndex != MaterialUVIndex)
				{
					bCompatible = false;
					// Don't break here so we can show all warnings
				}
			}
			else
			{
				UE_LOG(
					LogUsd,
					Log,
					TEXT("Failed to find primvar '%s' needed by material '%s' on its assigned mesh. Available primvars and UV indices: %s"),
					*MaterialPrimvar,
					*Material.GetPathName(),
					*UsdUtils::StringifyMap(MeshPrimvarToUVIndex)
				);
			}

			// If the material is going to read from a given UVIndex that exists on the mesh, that UV set should
			// contain the primvar data that the material expects to read
			if (UVIndexToMeshPrimvars.IsValidIndex(MaterialUVIndex))
			{
				const TSet<FString>& CompatiblePrimvars = UVIndexToMeshPrimvars[MaterialUVIndex];
				if (!CompatiblePrimvars.Contains(MaterialPrimvar))
				{
					bCompatible = false;
					// Don't break here so we can show all warnings
				}
			}
		}
		if (bCompatible)
		{
			UE_LOG(LogUsd, Verbose, TEXT("Material '%s' is compatible with provided primvar to UV index mapping"), *Material.GetPathName());
			return &Material;
		}

		// We need to find or create another compatible material instead
		UMaterialInterface* CompatibleMaterial = nullptr;

		// First, let's create the target primvar UVIndex assignment that is compatible with this mesh.
		// We use an array of TPairs here so that we can sort these into a deterministic order for hashing later.
		TArray<TPair<FString, int32>> CompatiblePrimvarAndUVIndexPairs;
		CompatiblePrimvarAndUVIndexPairs.Reserve(MaterialAssetUserData->PrimvarToUVIndex.Num());
		for (const TPair<FString, int32>& Pair : MaterialAssetUserData->PrimvarToUVIndex)
		{
			const FString& MaterialPrimvar = Pair.Key;

			bool bFoundUVIndex = false;

			// Mesh has this primvar available at some UV index, point to it
			if (const int32* FoundMeshUVIndex = MeshPrimvarToUVIndex.Find(MaterialPrimvar))
			{
				int32 MeshUVIndex = *FoundMeshUVIndex;
				if (MeshUVIndex >= 0 && MeshUVIndex < USD_PREVIEW_SURFACE_MAX_UV_SETS)
				{
					CompatiblePrimvarAndUVIndexPairs.Add(TPair<FString, int32>{MaterialPrimvar, MeshUVIndex});
					bFoundUVIndex = true;
				}
			}

			if (!bFoundUVIndex)
			{
				// Point this primvar to read an unused UV index instead, since our mesh doesn't have this primvar
				CompatiblePrimvarAndUVIndexPairs.Add(TPair<FString, int32>{MaterialPrimvar, UNUSED_UV_INDEX});
			}
		}

		FString ExistingHash = AssetCache->GetHashForAsset(&Material);
		const bool bMaterialBelongsToAssetCache = !ExistingHash.IsEmpty();
		if (!bMaterialBelongsToAssetCache)
		{
			return nullptr;
		}

		// Generate a deterministic hash based on the original material hash and this primvar UVIndex assignment
		CompatiblePrimvarAndUVIndexPairs.Sort(
			[](const TPair<FString, int32>& LHS, const TPair<FString, int32>& RHS)
			{
				if (LHS.Key == RHS.Key)
				{
					return LHS.Value < RHS.Value;
				}
				else
				{
					return LHS.Key < RHS.Key;
				}
			}
		);
		FSHAHash Hash;
		FSHA1 SHA1;
		SHA1.UpdateWithString(*ExistingHash, ExistingHash.Len());
		for (const TPair<FString, int32>& Pair : CompatiblePrimvarAndUVIndexPairs)
		{
			SHA1.UpdateWithString(*Pair.Key, Pair.Key.Len());
			SHA1.Update((const uint8*)&Pair.Value, sizeof(Pair.Value));
		}
		SHA1.Final();
		SHA1.GetHash(&Hash.Hash[0]);

		// In theory we don't even need to add the prefix here because our ExistingHash will already have the same prefix...
		// However for consistency it's probably for the best to have both assets have the same prefix, so you can tell
		// from the hash that they originated from the same prim
		FString PrefixedMaterialHash = MaterialHashPrefix + Hash.ToString();

		if (UMaterialInterface* ExistingCompatibleMaterial = Cast<UMaterialInterface>(AssetCache->GetCachedAsset(PrefixedMaterialHash)))
		{
			UE_LOG(
				LogUsd,
				Verbose,
				TEXT("Found existing compatible Material '%s' on the asset cache with hash '%s'"),
				*ExistingCompatibleMaterial->GetPathName(),
				*PrefixedMaterialHash
			);
			CompatibleMaterial = ExistingCompatibleMaterial;
		}

		TMap<FString, int32> CompatiblePrimvarToUVIndex;
		CompatiblePrimvarToUVIndex.Reserve(CompatiblePrimvarAndUVIndexPairs.Num());
		for (const TPair<FString, int32>& Pair : CompatiblePrimvarAndUVIndexPairs)
		{
			CompatiblePrimvarToUVIndex.Add(Pair);
		}

		// We have to create a brand new compatible material instance
		bool bCreatedNew = false;
		if (!CompatibleMaterial)
		{
			const FName NewInstanceName = MakeUniqueObjectName(GetTransientPackage(), UMaterialInstance::StaticClass(), Material.GetFName());

			UE_LOG(
				LogUsd,
				Verbose,
				TEXT("Generating compatible version of Material '%s' (Parameter to primvar: %s, primvar to UV index: %s) with hash '%s'"),
				*Material.GetPathName(),
				*UsdUtils::StringifyMap(MaterialAssetUserData->ParameterToPrimvar),
				*UsdUtils::StringifyMap(CompatiblePrimvarToUVIndex),
				*PrefixedMaterialHash
			);

#if WITH_EDITOR
			if (GIsEditor)
			{
				UMaterialInstanceConstant* CompatibleMIC = NewObject<UMaterialInstanceConstant>(
					GetTransientPackage(),
					NewInstanceName,
					Material.GetFlags()
				);

				CompatibleMIC->SetParentEditorOnly(&Material);

				CompatibleMaterial = CompatibleMIC;
			}
			else
#endif	  // WITH_EDITOR
			{
				UMaterialInstanceDynamic* CompatibleMID = UMaterialInstanceDynamic::Create(&Material, GetTransientPackage(), NewInstanceName);

				CompatibleMaterial = CompatibleMID;
			}

			bCreatedNew = true;
		}

		// Update the AssetUserData whether we created a new material instance or reused one from the asset cache.
		// The compatible AssetUserData should always match the original except for the different PrimvarToUVIndex
		UUsdMaterialAssetUserData* CompatibleUserData = nullptr;
		if (CompatibleMaterial)
		{
			CompatibleUserData = DuplicateObject(MaterialAssetUserData, CompatibleMaterial, TEXT("USDAssetUserData"));
			CompatibleUserData->PrimvarToUVIndex = CompatiblePrimvarToUVIndex;

			UsdUtils::SetAssetUserData(CompatibleMaterial, CompatibleUserData);
		}

		// Now that the AssetUserData is done, actually set the UV index material parameters with the target indices
		UMaterialInstance* CompatibleInstance = Cast<UMaterialInstance>(CompatibleMaterial);
		if (bCreatedNew && CompatibleInstance && CompatibleUserData)
		{
			for (const TPair<FString, FString>& ParameterPair : CompatibleUserData->ParameterToPrimvar)
			{
				const FString& Parameter = ParameterPair.Key;
				const FString& Primvar = ParameterPair.Value;

				if (int32* UVIndex = CompatibleUserData->PrimvarToUVIndex.Find(Primvar))
				{
					// Force-disable using the texture at all if the mesh doesn't provide the primvar that should be
					// used to sample it with
					if (*UVIndex == UNUSED_UV_INDEX)
					{
						UsdUtils::SetScalarParameterValue(*CompatibleInstance, *FString::Printf(TEXT("Use%sTexture"), *Parameter), 0.0f);
					}
					else
					{
						UsdUtils::SetScalarParameterValue(
							*CompatibleInstance,
							*FString::Printf(TEXT("%sUVIndex"), *Parameter),
							static_cast<float>(*UVIndex)
						);
					}
				}
			}

#if WITH_EDITOR
			CompatibleInstance->PostEditChange();
#endif	  // WITH_EDITOR
		}

		if (CompatibleMaterial && CompatibleMaterial != &Material)
		{
			if (bMaterialBelongsToAssetCache)
			{
				AssetCache->CacheAsset(PrefixedMaterialHash, CompatibleMaterial);
			}

			if (InfoCache)
			{
				for (const UE::FSdfPath& Prim : InfoCache->GetPrimsForAsset(&Material))
				{
					InfoCache->LinkAssetToPrim(Prim, CompatibleMaterial);
				}
			}
		}

		return CompatibleMaterial;
	}
};	  // namespace UE::MeshTranslationImplInternal::Private

TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> MeshTranslationImpl::ResolveMaterialAssignmentInfo(
	const pxr::UsdPrim& UsdPrim,
	const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& AssignmentInfo,
	UUsdAssetCache2& AssetCache,
	FUsdInfoCache& InfoCache,
	EObjectFlags Flags,
	bool bReuseIdenticalAssets
)
{
	FScopedUnrealAllocs Allocs;

	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials;
	if (AssignmentInfo.Num() == 0)
	{
		return ResolvedMaterials;
	}

	// Generating compatible materials is somewhat elaborate, so we'll cache the generated ones in this call in case we
	// have multiple material slots using the same material. The MeshPrimvarToUVIndex would always be the same for those
	// anyway, so we know we can reuse these compatible materials
	TMap<UMaterialInterface*, UMaterialInterface*> MaterialToCompatibleMaterial;
	const TMap<FString, int32>& MeshPrimvarToUVIndex = AssignmentInfo[0].PrimvarToUVIndex;

	uint32 GlobalResolvedMaterialIndex = 0;
	for (int32 InfoIndex = 0; InfoIndex < AssignmentInfo.Num(); ++InfoIndex)
	{
		const TArray<UsdUtils::FUsdPrimMaterialSlot>& Slots = AssignmentInfo[InfoIndex].Slots;

		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex, ++GlobalResolvedMaterialIndex)
		{
			const UsdUtils::FUsdPrimMaterialSlot& Slot = Slots[SlotIndex];
			UMaterialInterface* Material = nullptr;

			switch (Slot.AssignmentType)
			{
				case UsdUtils::EPrimAssignmentType::DisplayColor:
				{
					TOptional<IUsdClassesModule::FDisplayColorMaterial> DisplayColorDesc = IUsdClassesModule::FDisplayColorMaterial::FromString(
						Slot.MaterialSource
					);

					if (DisplayColorDesc.IsSet())
					{
						FString DisplayColorHash;
						{
							FSHAHash Hash;
							FSHA1 SHA1;
							SHA1.UpdateWithString(*Slot.MaterialSource, Slot.MaterialSource.Len());

							const FSoftObjectPath* ReferencePath = IUsdClassesModule::GetReferenceMaterialPath(DisplayColorDesc.GetValue());
							if (ReferencePath)
							{
								FString ReferencePathString = ReferencePath->ToString();
								SHA1.UpdateWithString(*ReferencePathString, ReferencePathString.Len());
							}

							SHA1.Final();
							SHA1.GetHash(&Hash.Hash[0]);
							DisplayColorHash = Hash.ToString();
						}
						const FString PrefixedHash = UsdUtils::GetAssetHashPrefix(UsdPrim, bReuseIdenticalAssets) + DisplayColorHash;

						// Try reusing an already created DisplayColor material
						if (UMaterialInterface* ExistingMaterial = Cast<UMaterialInterface>(AssetCache.GetCachedAsset(PrefixedHash)))
						{
							Material = ExistingMaterial;
						}

						// Need to create a new DisplayColor material
						if (Material == nullptr)
						{
							UMaterialInstance* MaterialInstance = nullptr;

							if (GIsEditor)	  // Editor, PIE => true; Standlone, packaged => false
							{
								MaterialInstance = IUsdClassesModule::CreateDisplayColorMaterialInstanceConstant(DisplayColorDesc.GetValue());
							}
							else
							{
								MaterialInstance = IUsdClassesModule::CreateDisplayColorMaterialInstanceDynamic(DisplayColorDesc.GetValue());
							}

							if (MaterialInstance)
							{
								// Leave PrimPath as empty as it likely will be reused by many prims
								UUsdAssetUserData* UserData = NewObject<UUsdAssetUserData>(MaterialInstance, TEXT("USDAssetUserData"));
								MaterialInstance->AddAssetUserData(UserData);
							}

							// We can only cache transient assets
							MaterialInstance->SetFlags(RF_Transient);

							AssetCache.CacheAsset(PrefixedHash, MaterialInstance);
							Material = MaterialInstance;
						}
					}

					break;
				}
				case UsdUtils::EPrimAssignmentType::MaterialPrim:
				{
					UMaterialInterface* OneSidedMat = nullptr;
					bool bOneSidedMatIsInstanceOfReferencePreviewSurface = false;

					UE::FSdfPath MaterialPrimPath{*Slot.MaterialSource};

					TArray<UMaterialInterface*> ExistingMaterials = InfoCache.GetAssetsForPrim<UMaterialInterface>(MaterialPrimPath);

					for (UMaterialInterface* ExistingMaterial : ExistingMaterials)
					{
						const bool bExistingIsTwoSided = ExistingMaterial->IsTwoSided();

						if (!bExistingIsTwoSided)
						{
							// Prefer sticking with a material instance that has as parent one of our reference materials.
							// The idea here being that we have two approaches when making TwoSided and compatible
							// materials: A) Make the material compatible first, and then a TwoSided version of the
							// compatible; B) Make the material TwoSided first, and then a compatible version of the
							// TwoSided; We're going to chose B), for the reason that at runtime we can only make a material
							// TwoSided if it is an instance of our reference materials (as we can't manually change the
							// material base property overrides at runtime)
							UMaterialInstance* ExistingInstance = Cast<UMaterialInstance>(ExistingMaterial);
							const bool bExistingIsInstanceOfReferencePreviewSurface = ExistingInstance
																					  && MeshTranslationImpl::IsReferencePreviewSurfaceMaterial(
																						  ExistingInstance->Parent
																					  );
							if (!OneSidedMat || (!bOneSidedMatIsInstanceOfReferencePreviewSurface && bExistingIsInstanceOfReferencePreviewSurface))
							{
								OneSidedMat = ExistingMaterial;
								bOneSidedMatIsInstanceOfReferencePreviewSurface = bExistingIsInstanceOfReferencePreviewSurface;
							}
						}

						if (Slot.bMeshIsDoubleSided == bExistingIsTwoSided)
						{
							Material = ExistingMaterial;
						}
					}

					FString PrefixedMaterialHash = Material ? AssetCache.GetHashForAsset(Material) : FString{};
					FString HashPrefix = UsdUtils::GetAssetHashPrefix(UsdPrim.GetStage()->GetPrimAtPath(MaterialPrimPath), bReuseIdenticalAssets);

					// Need to create a two-sided material on-demand, *before* we make it compatible:
					// This because at runtime we can't just set the base property overrides, and just instead create a new
					// MIC based on the TwoSided reference material, and the compatible material should be a MIC of that MIC
					if (Slot.bMeshIsDoubleSided && !Material)
					{
						// By now we parsed all materials so we must have the single-sided version of this material
						if (!OneSidedMat)
						{
							UE_LOG(
								LogUsd,
								Warning,
								TEXT("Failed to generate a two-sided material from the material prim at path '%s' as no "
									 "single-sided material was generated for it."),
								*Slot.MaterialSource
							);
							continue;
						}

						const FString PrefixedOneSidedHash = AssetCache.GetHashForAsset(OneSidedMat);
						const FString PrefixedTwoSidedHash = PrefixedOneSidedHash + UnrealIdentifiers::TwoSidedMaterialSuffix;

						// Check if for some reason we already have a two-sided material ready due to a complex scenario
						// related to the global cache
						UMaterialInterface* TwoSidedMat = Cast<UMaterialInterface>(AssetCache.GetCachedAsset(PrefixedTwoSidedHash));
						if (!TwoSidedMat)
						{
							TwoSidedMat = UE::MeshTranslationImplInternal::Private::CreateTwoSidedVersionOfMaterial(OneSidedMat);
						}

						if (TwoSidedMat)
						{
							// Update AssetUserData whether we generated a new material or reused one from the asset cache
							{
								UUsdMaterialAssetUserData* OneSidedUserData = OneSidedMat->GetAssetUserData<UUsdMaterialAssetUserData>();
								ensure(OneSidedUserData);

								UUsdMaterialAssetUserData* UserData = DuplicateObject(OneSidedUserData, TwoSidedMat, TEXT("USDAssetUserData"));
								UsdUtils::SetAssetUserData(TwoSidedMat, UserData);
							}

							TwoSidedMat->SetFlags(RF_Transient);
							Material = TwoSidedMat;
							PrefixedMaterialHash = PrefixedTwoSidedHash;
						}
						else
						{
							UE_LOG(
								LogUsd,
								Warning,
								TEXT("Failed to generate a two-sided material from the material prim at path '%s'. Falling "
									 "back to using the single-sided material '%s' instead."),
								*Slot.MaterialSource,
								*OneSidedMat->GetPathName()
							);
							Material = OneSidedMat;
							PrefixedMaterialHash = PrefixedOneSidedHash;
						}
					}

					if (Material)
					{
						// Cache the material to "ping it" as active, but also register two sided materials for the
						// first time
						AssetCache.CacheAsset(PrefixedMaterialHash, Material);
						InfoCache.LinkAssetToPrim(UE::FSdfPath{*Slot.MaterialSource}, Material);

						// Finally, try to make our generated material primvar-compatible. We do this last because this will
						// create another instance with the non-compatible material as reference material, which means we also
						// need that reference to be cached and linked for the asset cache to be able to handle dependencies
						// properly
						if (UMaterialInterface* AlreadyHandledMaterial = MaterialToCompatibleMaterial.FindRef(Material))
						{
							Material = AlreadyHandledMaterial;

							AssetCache.TouchAsset(Material);
							InfoCache.LinkAssetToPrim(UE::FSdfPath{*Slot.MaterialSource}, Material);
						}
						else
						{
							UMaterialInterface*
								CompatibleMaterial = UE::MeshTranslationImplInternal::Private::CreatePrimvarCompatibleVersionOfMaterial(
									*Material,
									MeshPrimvarToUVIndex,
									&AssetCache,
									&InfoCache,
									HashPrefix,
									bReuseIdenticalAssets
								);

							if (CompatibleMaterial)
							{
								MaterialToCompatibleMaterial.Add(Material, CompatibleMaterial);
								Material = CompatibleMaterial;
							}
						}
					}

					break;
				}
				case UsdUtils::EPrimAssignmentType::UnrealMaterial:
				{
					UObject* Object = FSoftObjectPath(Slot.MaterialSource).TryLoad();
					Material = Cast<UMaterialInterface>(Object);
					if (!Object)
					{
						UE_LOG(
							LogUsd,
							Warning,
							TEXT("UE material '%s' for prim '%s' could not be loaded or was not found."),
							*Slot.MaterialSource,
							*UsdToUnreal::ConvertPath(UsdPrim.GetPrimPath())
						);
					}
					else if (!Material)
					{
						UE_LOG(
							LogUsd,
							Warning,
							TEXT("Object '%s' assigned as an Unreal Material for prim '%s' is not actually a material (but instead a '%s') and will "
								 "not be used"),
							*Slot.MaterialSource,
							*UsdToUnreal::ConvertPath(UsdPrim.GetPrimPath()),
							*Object->GetClass()->GetName()
						);
					}
					else if (!Material->IsTwoSided() && Slot.bMeshIsDoubleSided)
					{
						UE_LOG(
							LogUsd,
							Warning,
							TEXT("Using one-sided UE material '%s' for doubleSided prim '%s'"),
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
	const FName& MaterialPurpose,
	bool bReuseIdenticalAssets
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
	const bool bProvideMaterialIndices = false;	   // We have no use for material indices and it can be slow to retrieve, as it will iterate all faces

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
			LODIndexToAssignments = {UsdUtils::GetPrimMaterialAssignments(
				ValidPrim,
				pxr::UsdTimeCode(Time),
				bProvideMaterialIndices,
				RenderContextToken,
				MaterialPurposeToken
			)};
		}
	}

	TMap<const UsdUtils::FUsdPrimMaterialSlot*, UMaterialInterface*> ResolvedMaterials;

	UUsdMeshAssetUserData* UserData = nullptr;
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(&MeshComponent))
	{
		if (UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh())
		{
			UserData = Mesh->GetAssetUserData<UUsdMeshAssetUserData>();
		}
	}
	else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(&MeshComponent))
	{
		if (USkeletalMesh* Mesh = SkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			UserData = Mesh->GetAssetUserData<UUsdMeshAssetUserData>();
		}
	}
	else if (UGeometryCacheComponent* GeometryCacheComponent = Cast<UGeometryCacheComponent>(&MeshComponent))
	{
		if (UGeometryCache* Mesh = GeometryCacheComponent->GetGeometryCache())
		{
			UserData = Mesh->GetAssetUserData<UUsdMeshAssetUserData>();
		}
	}
	else
	{
		ensureMsgf(
			false,
			TEXT("Unexpected component class '%s' encountered when setting material overrides for prim '%s'!"),
			*MeshComponent.GetClass()->GetName(),
			*UsdToUnreal::ConvertPath(Prim.GetPrimPath())
		);
	}

	ensureMsgf(
		UserData,
		TEXT("Mesh assigned to component '%s' generated for prim '%s' should have an UUsdMeshAssetUserData at this point!"),
		*MeshComponent.GetPathName(),
		*UsdToUnreal::ConvertPath(Prim.GetPrimPath())
	);

	if (UserData && LODIndexToAssignments.Num() > 0)
	{
		// Stash our PrimvarToUVIndex in here, as that's where ResolveMaterialAssignmentInfo will look for it
		LODIndexToAssignments[0].PrimvarToUVIndex = UserData->PrimvarToUVIndex;

		ResolvedMaterials = MeshTranslationImpl::ResolveMaterialAssignmentInfo(
			ValidPrim,
			LODIndexToAssignments,
			AssetCache,
			InfoCache,
			Flags,
			bReuseIdenticalAssets
		);
	}

	// Compare resolved materials with existing assignments, and create overrides if we need to
	uint32 StaticMeshSlotIndex = 0;
	for (int32 LODIndex = 0; LODIndex < LODIndexToAssignments.Num(); ++LODIndex)
	{
		const TArray<UsdUtils::FUsdPrimMaterialSlot>& LODSlots = LODIndexToAssignments[LODIndex].Slots;
		for (int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++StaticMeshSlotIndex)
		{
			// If we don't even have as many existing assignments as we have overrides just stop here.
			// This should happen often now because we'll always at least attempt at setting overrides on every
			// component (but only ever set anything if we really need to).
			// Previously we only attempted setting overrides in case the component didn't "own" the mesh prim,
			// but now it is not feasible to do that given the global asset cache and how assets may have come
			// from an entirely new stage/session.
			if (!ExistingAssignments.IsValidIndex(StaticMeshSlotIndex))
			{
				break;
			}

			const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[LODSlotIndex];

			UMaterialInterface* Material = nullptr;
			if (UMaterialInterface** FoundMaterial = ResolvedMaterials.Find(&Slot))
			{
				Material = *FoundMaterial;
			}
			else
			{
				UE_LOG(
					LogUsd,
					Error,
					TEXT("Lost track of resolved material for slot '%d' of LOD '%d' for mesh '%s'"),
					LODSlotIndex,
					LODIndex,
					*UsdToUnreal::ConvertPath(Prim.GetPath())
				);
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

void MeshTranslationImpl::RecordSourcePrimsForMaterialSlots(
	const TArray<UsdUtils::FUsdPrimMaterialAssignmentInfo>& LODIndexToMaterialInfo,
	UUsdMeshAssetUserData* UserData
)
{
	if (!UserData)
	{
		return;
	}

	uint32 SlotIndex = 0;
	for (int32 LODIndex = 0; LODIndex < LODIndexToMaterialInfo.Num(); ++LODIndex)
	{
		const TArray<UsdUtils::FUsdPrimMaterialSlot>& LODSlots = LODIndexToMaterialInfo[LODIndex].Slots;

		for (int32 LODSlotIndex = 0; LODSlotIndex < LODSlots.Num(); ++LODSlotIndex, ++SlotIndex)
		{
			const UsdUtils::FUsdPrimMaterialSlot& Slot = LODSlots[LODSlotIndex];
			UserData->MaterialSlotToPrimPaths.FindOrAdd(SlotIndex).PrimPaths.Append(Slot.PrimPaths.Array());
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

	return Cast<UMaterialInterface>(TargetMaterialPath->TryLoad());
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
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTwoSidedMaterial)
	{
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceTwoSidedVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentMaterial)
	{
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceTranslucentVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial)
	{
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceTranslucentTwoSidedVTMaterial.TryLoad());
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
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceTwoSidedMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentMaterial)
	{
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceVTMaterial)
	{
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceTwoSidedVTMaterial.TryLoad());
	}
	else if (PathName == Settings->ReferencePreviewSurfaceTranslucentVTMaterial)
	{
		return Cast<UMaterialInterface>(Settings->ReferencePreviewSurfaceTranslucentTwoSidedVTMaterial.TryLoad());
	}

	// We should only ever call this function with a ReferenceMaterial that matches one of the above paths
	ensure(false);
	return nullptr;
}

bool MeshTranslationImpl::IsReferencePreviewSurfaceMaterial(UMaterialInterface* Material)
{
	if (!Material)
	{
		return false;
	}

	const FSoftObjectPath PathName = Material->GetPathName();

	const UUsdProjectSettings* Settings = GetDefault<UUsdProjectSettings>();
	if (!Settings)
	{
		return false;
	}

	TSet<FSoftObjectPath> ReferenceMaterials = {
		Settings->ReferencePreviewSurfaceMaterial,
		Settings->ReferencePreviewSurfaceTranslucentMaterial,
		Settings->ReferencePreviewSurfaceTwoSidedMaterial,
		Settings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial,
		Settings->ReferencePreviewSurfaceVTMaterial,
		Settings->ReferencePreviewSurfaceTranslucentVTMaterial,
		Settings->ReferencePreviewSurfaceTwoSidedVTMaterial,
		Settings->ReferencePreviewSurfaceTranslucentTwoSidedVTMaterial};

	return ReferenceMaterials.Contains(PathName);
}

#endif	  // #if USE_USD_SDK
