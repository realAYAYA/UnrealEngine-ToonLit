// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDShadeMaterialTranslator.h"

#include "Engine/Level.h"
#include "MeshTranslationImpl.h"
#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDLog.h"
#include "USDShadeConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfPath.h"

#include "Engine/Texture.h"
#include "Materials/Material.h"
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
		const TSharedRef< FUsdSchemaTranslationContext >& Context,
		TSet<UMaterialInterface*>& VisitedMaterials,
		TSet<UMaterialInterface*>& NewMaterials
	)
	{
		for ( UTexture* Texture : TexturesToUpgrade )
		{
			if ( Texture->VirtualTextureStreaming )
			{
				continue;
			}

			UE_LOG( LogUsd, Log, TEXT( "Upgrading texture '%s' to VT as it is used by a material that must be VT" ),
				*Texture->GetName()
			);
			FPropertyChangedEvent PropertyChangeEvent( UTexture::StaticClass()->FindPropertyByName( GET_MEMBER_NAME_CHECKED( UTexture, VirtualTextureStreaming ) ) );
			Texture->Modify();
			Texture->VirtualTextureStreaming = true;

#if WITH_EDITOR
			Texture->PostEditChangeProperty( PropertyChangeEvent );
#endif // WITH_EDITOR

			// Now that our texture is VT, all materials that use the texture must be VT
			if ( const TSet<UMaterialInterface*>* UserMaterials = Context->TextureToUserMaterials.Find( Texture ) )
			{
				for ( UMaterialInterface* UserMaterial : *UserMaterials )
				{
					if ( VisitedMaterials.Contains( UserMaterial ) )
					{
						continue;
					}

					if ( UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( UserMaterial ) )
					{
						// Important to not use GetBaseMaterial() here because if our parent is the translucent we'll
						// get the reference UsdPreviewSurface instead, as that is also *its* reference
						UMaterialInterface* ReferenceMaterial = MaterialInstance->Parent.Get();
						UMaterialInterface* ReferenceMaterialVT =
							MeshTranslationImpl::GetVTVersionOfReferencePreviewSurfaceMaterial( ReferenceMaterial );
						if ( ReferenceMaterial == ReferenceMaterialVT )
						{
							// Material is already VT, we're good
							continue;
						}

						// Visit it before we start recursing. We need this because we must convert textures to VT
						// before materials (or else we get a warning) but we'll only actually swap the reference material
						// at the end of this scope
						VisitedMaterials.Add( UserMaterial );

						// If we're going to update this material to VT, all of *its* textures need to be VT too
						TSet<UTexture*> OtherUsedTextures;
						for ( const FTextureParameterValue& TextureValue : MaterialInstance->TextureParameterValues )
						{
							if ( UTexture* OtherTexture = TextureValue.ParameterValue )
							{
								OtherUsedTextures.Add( OtherTexture );
							}
						}

						RecursiveUpgradeMaterialsAndTexturesToVT(
							OtherUsedTextures,
							Context,
							VisitedMaterials,
							NewMaterials
						);

						// We can't blindly recreate all component render states when a level is being added, because
						// we may end up first creating render states for some components, and UWorld::AddToWorld
						// calls FScene::AddPrimitive which expects the component to not have primitives yet
						FMaterialUpdateContext::EOptions::Type Options = FMaterialUpdateContext::EOptions::Default;
						if ( Context->Level->bIsAssociatingLevel )
						{
							Options = ( FMaterialUpdateContext::EOptions::Type ) (
								Options & ~FMaterialUpdateContext::EOptions::RecreateRenderStates
							);
						}

						UE_LOG( LogUsd, Log, TEXT( "Upgrading material instance '%s' to having a VT reference as texture '%s' requires it" ),
							*MaterialInstance->GetName(),
							*Texture->GetName()
						);

#if WITH_EDITOR
						UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>( MaterialInstance );
						if ( GIsEditor && MIC )
						{
							FMaterialUpdateContext UpdateContext( Options, GMaxRHIShaderPlatform );
							UpdateContext.AddMaterialInstance( MIC );
							MIC->PreEditChange( nullptr );
							MIC->SetParentEditorOnly( ReferenceMaterialVT );
							MIC->PostEditChange();
						}
						else
#endif // WITH_EDITOR
						if ( UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>( UserMaterial ) )
						{
							if ( Context->AssetCache && Context->InfoCache )
							{
								// This is super slow as it will essentially linear search the asset cache, but
								// not much we can do at runtime without asset import data
								const UE::FSdfPath PrimPath = Context->InfoCache->GetPrimForAsset( MID );
								const FString Hash = Context->AssetCache->GetHashForAsset( MID );

								const FName NewInstanceName = MakeUniqueObjectName(
									GetTransientPackage(),
									UMaterialInstance::StaticClass(),
									PrimPath.IsEmpty() ? TEXT("MaterialInstance") : *FPaths::GetBaseFilename(PrimPath.GetString())
								);

								// For MID we can't swap the reference material, so we need to create a brand new one and copy
								// the overrides
								UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(
									ReferenceMaterialVT,
									GetTransientPackage(),
									NewInstanceName
								);
								if ( !ensure( NewMID ) )
								{
									continue;
								}
								NewMID->CopyParameterOverrides( MID );

								if (Context->AssetCache->RemoveAsset(Hash))
								{
									Context->AssetCache->CacheAsset(Hash, NewMID);
									if (!PrimPath.IsEmpty())
									{
										Context->InfoCache->LinkAssetToPrim(PrimPath, NewMID);
									}
									NewMaterials.Add( NewMID );
								}
							}
						}
						else
						{
							// This should never happen
							ensure( false );
						}
					}
				}
			}
		}
	}

	void UpgradeMaterialsAndTexturesToVT( TSet<UTexture*> TexturesToUpgrade, TSharedRef< FUsdSchemaTranslationContext >& Context )
	{
		TSet<UMaterialInterface*> VisitedMaterials;
		TSet<UMaterialInterface*> NewMaterials;
		RecursiveUpgradeMaterialsAndTexturesToVT( TexturesToUpgrade, Context, VisitedMaterials, NewMaterials );

		// When we "visit" a MID we'll create a brand new instance of it and discard the old one, so let's drop the old ones
		// from Context->TextureToUserMaterials too
		for ( UMaterialInterface* Material : VisitedMaterials )
		{
			if ( UMaterialInstanceDynamic* OldMID = Cast<UMaterialInstanceDynamic>( Material ) )
			{
				for ( TPair<UTexture*, TSet<UMaterialInterface*>>& Pair : Context->TextureToUserMaterials )
				{
					Pair.Value.Remove( Material );
				}
			}
		}

		// Additionally now we need to add those new MIDs we created back into Context->TextureToUserMaterials
		for ( UMaterialInterface* Material : NewMaterials )
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>( Material );
			if ( ensure( MID ) )
			{
				for ( const FTextureParameterValue& TextureValue : MID->TextureParameterValues )
				{
					if ( UTexture* Texture = TextureValue.ParameterValue )
					{
						Context->TextureToUserMaterials.FindOrAdd( Texture ).Add( MID );
					}
				}
			}
		}
	}
}

void FUsdShadeMaterialTranslator::CreateAssets()
{
	pxr::UsdShadeMaterial ShadeMaterial( GetPrim() );

	if ( !ShadeMaterial )
	{
		return;
	}

	if (Context->bTranslateOnlyUsedMaterials && Context->InfoCache)
	{
		if (!Context->InfoCache->IsMaterialUsed(PrimPath))
		{
			UE_LOG(LogUsd, Verbose, TEXT("Skipping creating assets for material prim '%s' as it is not currently bound by any prim."), *PrimPath.GetString());
			return;
		}
	}

	TRACE_CPUPROFILER_EVENT_SCOPE( FUsdShadeMaterialTranslator::CreateAssets );

	const pxr::TfToken RenderContextToken =
		Context->RenderContext.IsNone() ?
			pxr::UsdShadeTokens->universalRenderContext :
			UnrealToUsd::ConvertToken( *Context->RenderContext.ToString() ).Get();
	FString MaterialHashString = UsdUtils::HashShadeMaterial( ShadeMaterial, RenderContextToken ).ToString();

	UMaterialInterface* ConvertedMaterial = nullptr;

	if ( Context->AssetCache )
	{
		ConvertedMaterial = Cast< UMaterialInterface >( Context->AssetCache->GetCachedAsset( MaterialHashString ) );
	}

	if ( !ConvertedMaterial )
	{
		const FString PrimPathString = PrimPath.GetString();
		const bool bIsTranslucent = UsdUtils::IsMaterialTranslucent( ShadeMaterial );
		const FName InstanceName = MakeUniqueObjectName(
			GetTransientPackage(),
			UMaterialInstance::StaticClass(),
			*FPaths::GetBaseFilename(PrimPathString)
		);

		// TODO: There is an issue here: Note how we're only ever going to write the material prim's primvars into
		// this PrimvarToUVIndex map when first creating the material, and not if we find it in the asset cache.
		// This because finding the primvars to use essentially involves parsing the entire material again, so we
		// likely shouldn't do it every time.
		// This means that a mesh with float2 UV sets and materials that use them will likely end up with no UVs once
		// the stage reloads...
		TMap<FString, int32> Unused;
		TMap<FString, int32>& PrimvarToUVIndex = Context->MaterialToPrimvarToUVIndex
			? Context->MaterialToPrimvarToUVIndex->FindOrAdd(PrimPathString)
			: Unused;

#if WITH_EDITOR
		if ( GIsEditor ) // Also have to prevent Standalone game from going with MaterialInstanceConstants
		{
			if ( UMaterialInstanceConstant* NewMaterial = NewObject<UMaterialInstanceConstant>( GetTransientPackage(), InstanceName, Context->ObjectFlags | EObjectFlags::RF_Transient ) )
			{
				UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( NewMaterial, TEXT( "USDAssetImportData" ) );
				ImportData->PrimPath = PrimPath.GetString();
				NewMaterial->AssetImportData = ImportData;

				const bool bSuccess = UsdToUnreal::ConvertMaterial(
					ShadeMaterial,
					*NewMaterial,
					Context->AssetCache.Get(),
					PrimvarToUVIndex,
					*Context->RenderContext.ToString()
				);
				if ( !bSuccess )
				{
					return;
				}

				TSet<UTexture*> VTTextures;
				TSet<UTexture*> NonVTTextures;
				for ( const FTextureParameterValue& TextureValue : NewMaterial->TextureParameterValues )
				{
					if ( UTexture* Texture = TextureValue.ParameterValue )
					{
						if ( Texture->VirtualTextureStreaming )
						{
							UsdUtils::NotifyIfVirtualTexturesNeeded( Texture );
							VTTextures.Add( Texture );
						}
						else
						{
							NonVTTextures.Add( Texture );
						}
					}
				}

				// Our VT material only has VT texture samplers, so *all* of its textures must be VT
				if ( VTTextures.Num() && NonVTTextures.Num() )
				{
					UE_LOG( LogUsd, Log, TEXT( "Upgrading textures used by material instance '%s' to VT as the material must be VT" ),
						*NewMaterial->GetName()
					);
					UE::UsdShadeTranslator::Private::UpgradeMaterialsAndTexturesToVT( NonVTTextures, Context );
				}

				MeshTranslationImpl::EUsdReferenceMaterialProperties Properties = MeshTranslationImpl::EUsdReferenceMaterialProperties::None;
				if ( bIsTranslucent )
				{
					Properties |= MeshTranslationImpl::EUsdReferenceMaterialProperties::Translucent;
				}
				if ( VTTextures.Num() > 0 )
				{
					Properties |= MeshTranslationImpl::EUsdReferenceMaterialProperties::VT;
				}
				UMaterialInterface* ReferenceMaterial = MeshTranslationImpl::GetReferencePreviewSurfaceMaterial( Properties );

				if ( ensure( ReferenceMaterial ) )
				{
					NewMaterial->SetParentEditorOnly( ReferenceMaterial );

					// We can't blindly recreate all component render states when a level is being added, because we may end up first creating
					// render states for some components, and UWorld::AddToWorld calls FScene::AddPrimitive which expects the component to not have
					// primitives yet
					FMaterialUpdateContext::EOptions::Type Options = FMaterialUpdateContext::EOptions::Default;
					if ( Context->Level->bIsAssociatingLevel )
					{
						Options = ( FMaterialUpdateContext::EOptions::Type ) ( Options & ~FMaterialUpdateContext::EOptions::RecreateRenderStates );
					}

					FMaterialUpdateContext UpdateContext( Options, GMaxRHIShaderPlatform );
					UpdateContext.AddMaterialInstance( NewMaterial );
					NewMaterial->PreEditChange( nullptr );
					NewMaterial->PostEditChange();

					ConvertedMaterial = NewMaterial;

					for ( UTexture* Texture : VTTextures.Union( NonVTTextures ) )
					{
						Context->TextureToUserMaterials.FindOrAdd( Texture ).Add( NewMaterial );
					}
				}
			}
		}
		else
#endif // WITH_EDITOR
		{
			// At runtime we always start with a non-VT reference and if we discover we need one we just create a new MID
			// using the VT reference and copy the overrides. Not much else we can do as we need a reference to call
			// UMaterialInstanceDynamic::Create and get our instance, but we an instance to call UsdToUnreal::ConvertMaterial
			// to create our textures and decide on our reference.
			MeshTranslationImpl::EUsdReferenceMaterialProperties Properties = MeshTranslationImpl::EUsdReferenceMaterialProperties::None;
			if ( bIsTranslucent )
			{
				Properties |= MeshTranslationImpl::EUsdReferenceMaterialProperties::Translucent;
			}
			UMaterialInterface* ReferenceMaterial = MeshTranslationImpl::GetReferencePreviewSurfaceMaterial( Properties );

			if ( ensure( ReferenceMaterial ) )
			{
				if ( UMaterialInstanceDynamic* NewMaterial = UMaterialInstanceDynamic::Create( ReferenceMaterial, GetTransientPackage(), InstanceName ) )
				{
					NewMaterial->SetFlags(RF_Transient);

					if ( UsdToUnreal::ConvertMaterial( ShadeMaterial, *NewMaterial, Context->AssetCache.Get(), PrimvarToUVIndex, *Context->RenderContext.ToString() ) )
					{
						TSet<UTexture*> VTTextures;
						TSet<UTexture*> NonVTTextures;
						for ( const FTextureParameterValue& TextureValue : NewMaterial->TextureParameterValues )
						{
							if ( UTexture* Texture = TextureValue.ParameterValue )
							{
								if ( Texture->VirtualTextureStreaming )
								{
									VTTextures.Add( Texture );
								}
								else
								{
									NonVTTextures.Add( Texture );
								}
							}
						}

						// We must stash our material and textures *before* we call UpgradeMaterialsAndTexturesToVT, as that
						// is what will actually swap our reference with a VT one if needed
						if ( Context->AssetCache && Context->InfoCache )
						{
							Context->AssetCache->CacheAsset(MaterialHashString, NewMaterial);
							Context->InfoCache->LinkAssetToPrim(PrimPath, NewMaterial);
						}
						for ( UTexture* Texture : VTTextures.Union( NonVTTextures ) )
						{
							Context->TextureToUserMaterials.FindOrAdd( Texture ).Add( NewMaterial );
						}

						// Our VT material only has VT texture samplers, so *all* of its textures must be VT
						if ( VTTextures.Num() && NonVTTextures.Num() )
						{
							UE_LOG( LogUsd, Log, TEXT( "Upgrading textures used by material instance '%s' to VT as the material must be VT" ),
								*NewMaterial->GetName()
							);
							UE::UsdShadeTranslator::Private::UpgradeMaterialsAndTexturesToVT( NonVTTextures, Context );
						}

						// We must go through the cache to fetch our result material here as UpgradeMaterialsAndTexturesToVT
						// may have created a new MID for this material with a VT reference
						ConvertedMaterial = Cast<UMaterialInterface>( Context->AssetCache->GetCachedAsset( MaterialHashString ) );
					}
				}
			}
		}
	}
	else if ( Context->MaterialToPrimvarToUVIndex && Context->InfoCache )
	{
		const UE::FSdfPath FoundPrimPath = Context->InfoCache->GetPrimForAsset(ConvertedMaterial);
		if (!FoundPrimPath.IsEmpty())
		{
			if (TMap<FString, int32>* PrimvarToUVIndex = Context->MaterialToPrimvarToUVIndex->Find(FoundPrimPath.GetString()))
			{
				// Copy the Material -> Primvar -> UV index mapping from the cached material prim path to this prim path
				Context->MaterialToPrimvarToUVIndex->FindOrAdd(PrimPath.GetString()) = *PrimvarToUVIndex;
			}
		}
	}

	// Note that this needs to run even if we do find the material from the asset cache, otherwise we won't
	// re-register the prim asset links when we reload a stage
	if (ConvertedMaterial)
	{
		if (Context->InfoCache)
		{
			Context->InfoCache->LinkAssetToPrim(PrimPath, ConvertedMaterial);

			// Also link the textures to the same material prim.
			// This is important because it lets the stage actor drop its references to old unused textures in the
			// asset cache if they aren't being used by any other material
			if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(ConvertedMaterial))
			{
				for (const FTextureParameterValue& TextureValue : MaterialInstance->TextureParameterValues)
				{
					if (UTexture* Texture = TextureValue.ParameterValue)
					{
						Context->InfoCache->LinkAssetToPrim(PrimPath, Texture);
					}
				}
			}
		}

		if (Context->AssetCache)
		{
			Context->AssetCache->CacheAsset(MaterialHashString, ConvertedMaterial);
		}
	}
}

bool FUsdShadeMaterialTranslator::CollapsesChildren( ECollapsingType CollapsingType ) const
{
	return false;
}

bool FUsdShadeMaterialTranslator::CanBeCollapsed( ECollapsingType CollapsingType ) const
{
	return false;
}

#endif // #if USE_USD_SDK
