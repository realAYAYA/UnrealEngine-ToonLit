// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDShadeMaterialTranslator.h"

#include "MeshTranslationImpl.h"
#include "USDAssetUserData.h"
#include "USDClassesModule.h"
#include "USDLog.h"
#include "USDPrimConversion.h"
#include "USDProjectSettings.h"
#include "USDShadeConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"

#include "EditorFramework/AssetImportData.h"
#include "Engine/Level.h"
#include "Engine/Texture.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "Misc/SecureHash.h"

#if USE_USD_SDK

#include "USDIncludesStart.h"
#include "pxr/base/tf/token.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/tokens.h"
#include "USDIncludesEnd.h"

namespace UE::UsdShadeTranslator::Private
{
	void RecursiveUpgradeMaterialsAndTexturesToVT(
		const TSet<UTexture*>& TexturesToUpgrade,
		const TSharedRef<FUsdSchemaTranslationContext>& Context,
		TSet<UMaterialInterface*>& VisitedMaterials,
		TSet<UMaterialInterface*>& NewMaterials
	)
	{
		for (UTexture* Texture : TexturesToUpgrade)
		{
			if (Texture->VirtualTextureStreaming)
			{
				continue;
			}

			UE_LOG(LogUsd, Log, TEXT("Upgrading texture '%s' to VT as it is used by a material that must be VT"), *Texture->GetName());
			FPropertyChangedEvent PropertyChangeEvent(
				UTexture::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UTexture, VirtualTextureStreaming))
			);
			Texture->Modify();
			Texture->VirtualTextureStreaming = true;

#if WITH_EDITOR
			Texture->PostEditChangeProperty(PropertyChangeEvent);
#endif	  // WITH_EDITOR

			// Now that our texture is VT, all materials that use the texture must be VT
			if (const TSet<UMaterialInterface*>* UserMaterials = Context->TextureToUserMaterials.Find(Texture))
			{
				for (UMaterialInterface* UserMaterial : *UserMaterials)
				{
					if (VisitedMaterials.Contains(UserMaterial))
					{
						continue;
					}

					if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(UserMaterial))
					{
						// Important to not use GetBaseMaterial() here because if our parent is the translucent we'll
						// get the reference UsdPreviewSurface instead, as that is also *its* reference
						UMaterialInterface* ReferenceMaterial = MaterialInstance->Parent.Get();
						UMaterialInterface* ReferenceMaterialVT = MeshTranslationImpl::GetVTVersionOfReferencePreviewSurfaceMaterial(ReferenceMaterial
						);
						if (ReferenceMaterial == ReferenceMaterialVT)
						{
							// Material is already VT, we're good
							continue;
						}

						// Visit it before we start recursing. We need this because we must convert textures to VT
						// before materials (or else we get a warning) but we'll only actually swap the reference material
						// at the end of this scope
						VisitedMaterials.Add(UserMaterial);

						// If we're going to update this material to VT, all of *its* textures need to be VT too
						TSet<UTexture*> OtherUsedTextures;
						for (const FTextureParameterValue& TextureValue : MaterialInstance->TextureParameterValues)
						{
							if (UTexture* OtherTexture = TextureValue.ParameterValue)
							{
								OtherUsedTextures.Add(OtherTexture);
							}
						}

						RecursiveUpgradeMaterialsAndTexturesToVT(OtherUsedTextures, Context, VisitedMaterials, NewMaterials);

						// We can't blindly recreate all component render states when a level is being added, because
						// we may end up first creating render states for some components, and UWorld::AddToWorld
						// calls FScene::AddPrimitive which expects the component to not have primitives yet
						FMaterialUpdateContext::EOptions::Type Options = FMaterialUpdateContext::EOptions::Default;
						if (Context->Level->bIsAssociatingLevel)
						{
							Options = (FMaterialUpdateContext::EOptions::Type)(Options & ~FMaterialUpdateContext::EOptions::RecreateRenderStates);
						}

						UE_LOG(
							LogUsd,
							Log,
							TEXT("Upgrading material instance '%s' to having a VT reference as texture '%s' requires it"),
							*MaterialInstance->GetName(),
							*Texture->GetName()
						);

#if WITH_EDITOR
						UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInstance);
						if (GIsEditor && MIC)
						{
							FMaterialUpdateContext UpdateContext(Options, GMaxRHIShaderPlatform);
							UpdateContext.AddMaterialInstance(MIC);
							MIC->PreEditChange(nullptr);
							MIC->SetParentEditorOnly(ReferenceMaterialVT);
							MIC->PostEditChange();
						}
						else
#endif	  // WITH_EDITOR
							if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(UserMaterial))
							{
								if (Context->AssetCache && Context->InfoCache)
								{
									UE::FSdfPath PrimPath;
									for (const UE::FSdfPath& Path : Context->InfoCache->GetPrimsForAsset(MID))
									{
										PrimPath = Path;
										break;
									}
									const FString Hash = Context->AssetCache->GetHashForAsset(MID);

									const FName NewInstanceName = MakeUniqueObjectName(
										GetTransientPackage(),
										UMaterialInstance::StaticClass(),
										PrimPath.IsEmpty() ? TEXT("MaterialInstance")
														   : *IUsdClassesModule::SanitizeObjectName(FPaths::GetBaseFilename(PrimPath.GetString()))
									);

									// For MID we can't swap the reference material, so we need to create a brand new one and copy
									// the overrides
									UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(
										ReferenceMaterialVT,
										GetTransientPackage(),
										NewInstanceName
									);
									if (!ensure(NewMID))
									{
										continue;
									}

									NewMID->CopyParameterOverrides(MID);

									UUsdMaterialAssetUserData* OldUserData = UserMaterial->GetAssetUserData<UUsdMaterialAssetUserData>();
									if (OldUserData)
									{
										UUsdMaterialAssetUserData* NewUserData = DuplicateObject(OldUserData, NewMID);
										NewMID->AddAssetUserData(NewUserData);
									}

									if (Context->AssetCache->CanRemoveAsset(Hash) && Context->AssetCache->RemoveAsset(Hash))
									{
										Context->AssetCache->CacheAsset(Hash, NewMID);
										if (!PrimPath.IsEmpty())
										{
											Context->InfoCache->LinkAssetToPrim(PrimPath, NewMID);
										}
										NewMaterials.Add(NewMID);
									}
								}
							}
							else
							{
								// This should never happen
								ensure(false);
							}
					}
				}
			}
		}
	}

	void UpgradeMaterialsAndTexturesToVT(TSet<UTexture*> TexturesToUpgrade, TSharedRef<FUsdSchemaTranslationContext>& Context)
	{
		TSet<UMaterialInterface*> VisitedMaterials;
		TSet<UMaterialInterface*> NewMaterials;
		RecursiveUpgradeMaterialsAndTexturesToVT(TexturesToUpgrade, Context, VisitedMaterials, NewMaterials);

		// When we "visit" a MID we'll create a brand new instance of it and discard the old one, so let's drop the old ones
		// from Context->TextureToUserMaterials too
		for (UMaterialInterface* Material : VisitedMaterials)
		{
			if (UMaterialInstanceDynamic* OldMID = Cast<UMaterialInstanceDynamic>(Material))
			{
				for (TPair<UTexture*, TSet<UMaterialInterface*>>& Pair : Context->TextureToUserMaterials)
				{
					Pair.Value.Remove(Material);
				}
			}
		}

		// Additionally now we need to add those new MIDs we created back into Context->TextureToUserMaterials
		for (UMaterialInterface* Material : NewMaterials)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
			if (ensure(MID))
			{
				for (const FTextureParameterValue& TextureValue : MID->TextureParameterValues)
				{
					if (UTexture* Texture = TextureValue.ParameterValue)
					{
						Context->TextureToUserMaterials.FindOrAdd(Texture).Add(MID);
					}
				}
			}
		}
	}

	// We need to hash the reference material that we'll use, so that if this is changed we regenerate a new instance.
	// However, unlike for displayColor materials, we can't really know *which* reference material we'll end up using
	// until after we've already created it (which doesn't sound like it makes any sense but it's part of why we have those
	// VT and double-sided "upgrade" mechanisms).
	//
	// If all we want is a hash, the solution can be simple though: Hash them all. Yea we may end up unnecessarily regenerating
	// materials sometimes but changing the reference materials on the project settings should be rare.
	void HashPreviewSurfaceReferences(FSHA1& InOutHash)
	{
		const UUsdProjectSettings* Settings = GetDefault<UUsdProjectSettings>();
		if (!Settings)
		{
			return;
		}

		TArray<const FSoftObjectPath*> ReferenceMaterials = {
			&Settings->ReferencePreviewSurfaceMaterial,
			&Settings->ReferencePreviewSurfaceTranslucentMaterial,
			&Settings->ReferencePreviewSurfaceTwoSidedMaterial,
			&Settings->ReferencePreviewSurfaceTranslucentTwoSidedMaterial,
			&Settings->ReferencePreviewSurfaceVTMaterial,
			&Settings->ReferencePreviewSurfaceTranslucentVTMaterial,
			&Settings->ReferencePreviewSurfaceTwoSidedVTMaterial,
			&Settings->ReferencePreviewSurfaceTranslucentTwoSidedVTMaterial};

		for (const FSoftObjectPath* ReferencePath : ReferenceMaterials)
		{
			if (!ReferencePath)
			{
				continue;
			}

			FString ReferencePathStr = ReferencePath->ToString();
			InOutHash.UpdateWithString(*ReferencePathStr, ReferencePathStr.Len());
		}
	}
}	 // namespace UE::UsdShadeTranslator::Private

void FUsdShadeMaterialTranslator::CreateAssets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FUsdShadeMaterialTranslator::CreateAssets);

	pxr::UsdShadeMaterial ShadeMaterial(GetPrim());

	if (!ShadeMaterial)
	{
		return;
	}

	if (Context->bTranslateOnlyUsedMaterials && Context->InfoCache)
	{
		if (!Context->InfoCache->IsMaterialUsed(PrimPath))
		{
			UE_LOG(
				LogUsd,
				Verbose,
				TEXT("Skipping creating assets for material prim '%s' as it is not currently bound by any prim."),
				*PrimPath.GetString()
			);
			return;
		}
	}

	const pxr::TfToken RenderContextToken = Context->RenderContext.IsNone() ? pxr::UsdShadeTokens->universalRenderContext
																			: UnrealToUsd::ConvertToken(*Context->RenderContext.ToString()).Get();

	// If this material has a valid surface output for the 'unreal' render context and we're using it, don't bother
	// generating any new UMaterialInterface asset because when resolving material assignments for this material
	// all consumers will just use the referenced UAsset anyway
	if (RenderContextToken == UnrealIdentifiers::Unreal)
	{
		TOptional<FString> UnrealMaterial = UsdUtils::GetUnrealSurfaceOutput(ShadeMaterial.GetPrim());
		if (UnrealMaterial.IsSet())
		{
			UE_LOG(
				LogUsd,
				Log,
				TEXT(
					"Skipping generation of assets for material prim '%s' as all prims that bind this material will use its referenced Unreal material '%s' instead."
				),
				*PrimPath.GetString(),
				*UnrealMaterial.GetValue()
			);

			UObject* Object = FSoftObjectPath(UnrealMaterial.GetValue()).TryLoad();
			if (!Object)
			{
				UE_LOG(
					LogUsd,
					Warning,
					TEXT("Failed to find the Unreal material '%s' referenced by material prim '%s'."),
					*UnrealMaterial.GetValue(),
					*PrimPath.GetString()
				);
			}

			return;
		}
	}

	FString MaterialHash;
	{
		FSHAHash OutHash;

		FSHA1 SHA1;

		UsdUtils::HashShadeMaterial(ShadeMaterial, SHA1, RenderContextToken);
		UE::UsdShadeTranslator::Private::HashPreviewSurfaceReferences(SHA1);

		SHA1.Final();
		SHA1.GetHash(&OutHash.Hash[0]);
		MaterialHash = OutHash.ToString();
	}
	const FString PrefixedMaterialHash = UsdUtils::GetAssetHashPrefix(GetPrim(), Context->bReuseIdenticalAssets) + MaterialHash;

	UMaterialInterface* ConvertedMaterial = nullptr;

	if (Context->AssetCache)
	{
		ConvertedMaterial = Cast<UMaterialInterface>(Context->AssetCache->GetCachedAsset(PrefixedMaterialHash));
	}

	if (!ConvertedMaterial)
	{
		const FString PrimPathString = PrimPath.GetString();
		const bool bIsTranslucent = UsdUtils::IsMaterialTranslucent(ShadeMaterial);
		const FName InstanceName = MakeUniqueObjectName(
			GetTransientPackage(),
			UMaterialInstance::StaticClass(),
			*IUsdClassesModule::SanitizeObjectName(FPaths::GetBaseFilename(PrimPathString))
		);

#if WITH_EDITOR
		if (GIsEditor)	  // Also have to prevent Standalone game from going with MaterialInstanceConstants
		{
			if (UMaterialInstanceConstant* NewMaterial = NewObject<UMaterialInstanceConstant>(
					GetTransientPackage(),
					InstanceName,
					Context->ObjectFlags | EObjectFlags::RF_Transient
				))
			{
				UUsdMaterialAssetUserData* UserData = NewObject<UUsdMaterialAssetUserData>(NewMaterial, TEXT("USDAssetUserData"));
				UserData->PrimPaths = {PrimPath.GetString()};
				NewMaterial->AddAssetUserData(UserData);

				const bool bSuccess = UsdToUnreal::ConvertMaterial(
					ShadeMaterial,
					*NewMaterial,
					Context->AssetCache.Get(),
					*Context->RenderContext.ToString(),
					Context->bReuseIdenticalAssets
				);
				if (!bSuccess)
				{
					NewMaterial->MarkAsGarbage();
					return;
				}

				TSet<UTexture*> VTTextures;
				TSet<UTexture*> NonVTTextures;
				for (const FTextureParameterValue& TextureValue : NewMaterial->TextureParameterValues)
				{
					if (UTexture* Texture = TextureValue.ParameterValue)
					{
						if (Texture->VirtualTextureStreaming)
						{
							UsdUtils::NotifyIfVirtualTexturesNeeded(Texture);
							VTTextures.Add(Texture);
						}
						else
						{
							NonVTTextures.Add(Texture);
						}
					}
				}

				// Our VT material only has VT texture samplers, so *all* of its textures must be VT
				if (VTTextures.Num() && NonVTTextures.Num())
				{
					UE_LOG(
						LogUsd,
						Log,
						TEXT("Upgrading textures used by material instance '%s' to VT as the material must be VT"),
						*NewMaterial->GetName()
					);
					UE::UsdShadeTranslator::Private::UpgradeMaterialsAndTexturesToVT(NonVTTextures, Context);
				}

				MeshTranslationImpl::EUsdReferenceMaterialProperties Properties = MeshTranslationImpl::EUsdReferenceMaterialProperties::None;
				if (bIsTranslucent)
				{
					Properties |= MeshTranslationImpl::EUsdReferenceMaterialProperties::Translucent;
				}
				if (VTTextures.Num() > 0)
				{
					Properties |= MeshTranslationImpl::EUsdReferenceMaterialProperties::VT;
				}
				UMaterialInterface* ReferenceMaterial = MeshTranslationImpl::GetReferencePreviewSurfaceMaterial(Properties);

				if (ensure(ReferenceMaterial))
				{
					NewMaterial->SetParentEditorOnly(ReferenceMaterial);

					// We can't blindly recreate all component render states when a level is being added, because we may end up first creating
					// render states for some components, and UWorld::AddToWorld calls FScene::AddPrimitive which expects the component to not have
					// primitives yet
					FMaterialUpdateContext::EOptions::Type Options = FMaterialUpdateContext::EOptions::Default;
					if (Context->Level && Context->Level->bIsAssociatingLevel)
					{
						Options = (FMaterialUpdateContext::EOptions::Type)(Options & ~FMaterialUpdateContext::EOptions::RecreateRenderStates);
					}

					FMaterialUpdateContext UpdateContext(Options, GMaxRHIShaderPlatform);
					UpdateContext.AddMaterialInstance(NewMaterial);
					NewMaterial->PreEditChange(nullptr);
					NewMaterial->PostEditChange();

					ConvertedMaterial = NewMaterial;

					for (UTexture* Texture : VTTextures.Union(NonVTTextures))
					{
						Context->TextureToUserMaterials.FindOrAdd(Texture).Add(NewMaterial);
					}
				}
			}
		}
		else
#endif	  // WITH_EDITOR
		{
			// At runtime we always start with a non-VT reference and if we discover we need one we just create a new MID
			// using the VT reference and copy the overrides. Not much else we can do as we need a reference to call
			// UMaterialInstanceDynamic::Create and get our instance, but we an instance to call UsdToUnreal::ConvertMaterial
			// to create our textures and decide on our reference.
			MeshTranslationImpl::EUsdReferenceMaterialProperties Properties = MeshTranslationImpl::EUsdReferenceMaterialProperties::None;
			if (bIsTranslucent)
			{
				Properties |= MeshTranslationImpl::EUsdReferenceMaterialProperties::Translucent;
			}
			UMaterialInterface* ReferenceMaterial = MeshTranslationImpl::GetReferencePreviewSurfaceMaterial(Properties);

			if (ensure(ReferenceMaterial))
			{
				if (UMaterialInstanceDynamic* NewMaterial = UMaterialInstanceDynamic::Create(ReferenceMaterial, GetTransientPackage(), InstanceName))
				{
					UUsdMaterialAssetUserData* UserData = NewObject<UUsdMaterialAssetUserData>(NewMaterial, TEXT("USDAssetUserData"));
					UserData->PrimPaths = {PrimPath.GetString()};
					NewMaterial->AddAssetUserData(UserData);

					NewMaterial->SetFlags(RF_Transient);

					if (UsdToUnreal::ConvertMaterial(
							ShadeMaterial,
							*NewMaterial,
							Context->AssetCache.Get(),
							*Context->RenderContext.ToString(),
							Context->bReuseIdenticalAssets
						))
					{
						TSet<UTexture*> VTTextures;
						TSet<UTexture*> NonVTTextures;
						for (const FTextureParameterValue& TextureValue : NewMaterial->TextureParameterValues)
						{
							if (UTexture* Texture = TextureValue.ParameterValue)
							{
								if (Texture->VirtualTextureStreaming)
								{
									VTTextures.Add(Texture);
								}
								else
								{
									NonVTTextures.Add(Texture);
								}
							}
						}

						// We must stash our material and textures *before* we call UpgradeMaterialsAndTexturesToVT, as that
						// is what will actually swap our reference with a VT one if needed
						if (Context->AssetCache && Context->InfoCache)
						{
							Context->AssetCache->CacheAsset(PrefixedMaterialHash, NewMaterial);
							Context->InfoCache->LinkAssetToPrim(PrimPath, NewMaterial);
						}
						for (UTexture* Texture : VTTextures.Union(NonVTTextures))
						{
							Context->TextureToUserMaterials.FindOrAdd(Texture).Add(NewMaterial);
						}

						// Our VT material only has VT texture samplers, so *all* of its textures must be VT
						if (VTTextures.Num() && NonVTTextures.Num())
						{
							UE_LOG(
								LogUsd,
								Log,
								TEXT("Upgrading textures used by material instance '%s' to VT as the material must be VT"),
								*NewMaterial->GetName()
							);
							UE::UsdShadeTranslator::Private::UpgradeMaterialsAndTexturesToVT(NonVTTextures, Context);
						}

						// We must go through the cache to fetch our result material here as UpgradeMaterialsAndTexturesToVT
						// may have created a new MID for this material with a VT reference
						ConvertedMaterial = Cast<UMaterialInterface>(Context->AssetCache->GetCachedAsset(PrefixedMaterialHash));
					}
				}
			}
		}
	}

	PostImportMaterial(PrefixedMaterialHash, ConvertedMaterial);
}

bool FUsdShadeMaterialTranslator::CollapsesChildren(ECollapsingType CollapsingType) const
{
	return false;
}

bool FUsdShadeMaterialTranslator::CanBeCollapsed(ECollapsingType CollapsingType) const
{
	return false;
}

void FUsdShadeMaterialTranslator::PostImportMaterial(const FString& PrefixedMaterialHash, UMaterialInterface* ImportedMaterial)
{
	if (!ImportedMaterial || !Context->InfoCache || !Context->AssetCache)
	{
		return;
	}

	if (UUsdMaterialAssetUserData* UserData = UsdUtils::GetOrCreateAssetUserData<UUsdMaterialAssetUserData>(ImportedMaterial))
	{
		UserData->PrimPaths.AddUnique(PrimPath.GetString());

		if (Context->MetadataOptions.bCollectMetadata)
		{
			UsdToUnreal::ConvertMetadata(
				GetPrim(),
				UserData,
				Context->MetadataOptions.BlockedPrefixFilters,
				Context->MetadataOptions.bInvertFilters,
				Context->MetadataOptions.bCollectFromEntireSubtrees
			);
		}
		else
		{
			// Strip the metadata from this prim, so that if we uncheck "Collect Metadata" it actually disappears on the AssetUserData
			UserData->StageIdentifierToMetadata.Remove(GetPrim().GetStage().GetRootLayer().GetIdentifier());
		}
	}

	// Note that this needs to run even if we found this material in the asset cache already, otherwise we won't
	// re-register the prim asset links when we reload a stage
	Context->AssetCache->CacheAsset(PrefixedMaterialHash, ImportedMaterial);
	Context->InfoCache->LinkAssetToPrim(PrimPath, ImportedMaterial);

	// Also link the textures to the same material prim.
	// This is important because it lets the stage actor drop its references to old unused textures in the
	// asset cache if they aren't being used by any other material
	TSet<UObject*> Dependencies = IUsdClassesModule::GetAssetDependencies(ImportedMaterial);
	for (UObject* Object : Dependencies)
	{
		if (UTexture* Texture = Cast<UTexture>(Object))
		{
			// We don't use "GetOutermost()" here because it's also possible to be owned by an asset cache that
			// itself lives in the transient package... bIsOwnedByTransientPackage should be true just for new textures
			// dumped on the transient package
			const bool bIsOwnedByTransientPackage = Texture->GetOuter()->GetPackage() == GetTransientPackage();
			const bool bIsOwnedByCache = Context->AssetCache->IsAssetOwnedByCache(Texture->GetPathName());

			// Texture is already owned by the cache: Just touch it without recomputing its hash as that's expensive
			if (bIsOwnedByCache)
			{
				Context->AssetCache->TouchAsset(Texture);
			}
			// Texture is owned by the transient package, but not cached yet: Let's take it
			else if (bIsOwnedByTransientPackage)
			{
				FString FilePath;
#if WITH_EDITOR
				if (UAssetImportData* TextureImportData = Texture->AssetImportData.Get())
				{
					FilePath = TextureImportData->GetFirstFilename();
				}
				else
#endif	  // WITH_EDITOR
				{
					FilePath = Texture->GetName();
				}

				const FString HashPrefix = UsdUtils::GetAssetHashPrefix(GetPrim(), Context->bReuseIdenticalAssets);
				const FString PrefixedTextureHash = HashPrefix
													+ UsdUtils::GetTextureHash(
														FilePath,
														Texture->SRGB,
														Texture->CompressionSettings,
														Texture->GetTextureAddressX(),
														Texture->GetTextureAddressY()
													);

				// Some translators like FMaterialXUsdShadeMaterialTranslator will import many materials and textures
				// at once and preload them all in the cache. In some complex scenarios when some materials are updated
				// it is possible to arrive at a situation where the we need to reparse the source file again and
				// regenerate a bunch of materials and textures without access to the asset cache. In those cases we'll
				// unfortunately recreate identical textures, and if we try to store them in here we'll run into a hash
				// collision. These should be rare, however, and require repeatedly e.g. updating the Material prims
				// generated by MaterialX or updating the MaterialX import options
				UTexture* ExistingTexture = Cast<UTexture>(Context->AssetCache->GetCachedAsset(PrefixedTextureHash));
				if (!ExistingTexture)
				{
					Texture->SetFlags(RF_Transient);
					Context->AssetCache->CacheAsset(PrefixedTextureHash, Texture);
				}
			}

			if (bIsOwnedByCache || bIsOwnedByTransientPackage)
			{
				UUsdAssetUserData* TextureUserData = Texture->GetAssetUserData<UUsdAssetUserData>();
				if (!TextureUserData)
				{
					TextureUserData = NewObject<UUsdAssetUserData>(Texture, TEXT("USDAssetUserData"));
					Texture->AddAssetUserData(TextureUserData);
				}
				TextureUserData->PrimPaths.AddUnique(PrimPath.GetString());

				Context->InfoCache->LinkAssetToPrim(PrimPath, Texture);
			}
		}
		else if (UMaterialInterface* ReferenceMaterial = Cast<UMaterialInterface>(Object))
		{
			// Some scenarios can generate reference/instance material pairs, and reference materials are dependencies.
			// We won't handle these dependencies recursively though, the caller is responsible for calling this for
			// all individual materials as they need to also provide the hash to use for each
			UMaterialInstance* Instance = Cast<UMaterialInstance>(ImportedMaterial);
			ensure(Instance && Instance->Parent.Get() == ReferenceMaterial);
		}
		else
		{
			ensureMsgf(false, TEXT("Asset type unsupported!"));
		}
	}
}

TSet<UE::FSdfPath> FUsdShadeMaterialTranslator::CollectAuxiliaryPrims() const
{
	if (!Context->bIsBuildingInfoCache)
	{
		return Context->InfoCache->GetAuxiliaryPrims(PrimPath);
	}

	TSet<UE::FSdfPath> Result;
	{
		TFunction<void(const pxr::UsdShadeInput&)> TraverseShadeInput;
		TraverseShadeInput = [&TraverseShadeInput, &Result](const pxr::UsdShadeInput& ShadeInput)
		{
			if (!ShadeInput)
			{
				return;
			}

			pxr::UsdShadeConnectableAPI Source;
			pxr::TfToken SourceName;
			pxr::UsdShadeAttributeType AttributeType;
			if (pxr::UsdShadeConnectableAPI::GetConnectedSource(ShadeInput.GetAttr(), &Source, &SourceName, &AttributeType))
			{
				pxr::UsdPrim ConnectedPrim = Source.GetPrim();
				UE::FSdfPath ConnectedPrimPath = UE::FSdfPath{ConnectedPrim.GetPrimPath()};

				if (!Result.Contains(ConnectedPrimPath))
				{
					Result.Add(ConnectedPrimPath);

					for (const pxr::UsdShadeInput& ChildInput : Source.GetInputs())
					{
						TraverseShadeInput(ChildInput);
					}
				}
			}
		};

		FScopedUsdAllocs UsdAllocs;

		pxr::UsdPrim Prim = GetPrim();
		pxr::UsdShadeMaterial UsdShadeMaterial{Prim};
		if (!UsdShadeMaterial)
		{
			return {};
		}

		const pxr::TfToken RenderContextToken = Context->RenderContext.IsNone() ? pxr::UsdShadeTokens->universalRenderContext
																				: UnrealToUsd::ConvertToken(*Context->RenderContext.ToString()).Get();
		pxr::UsdShadeShader SurfaceShader = UsdShadeMaterial.ComputeSurfaceSource({RenderContextToken});
		if (!SurfaceShader)
		{
			return {};
		}

		Result.Add(UE::FSdfPath{SurfaceShader.GetPrim().GetPrimPath()});

		for (const pxr::UsdShadeInput& ShadeInput : SurfaceShader.GetInputs())
		{
			TraverseShadeInput(ShadeInput);
		}
	}
	return Result;
}

#endif	  // #if USE_USD_SDK
