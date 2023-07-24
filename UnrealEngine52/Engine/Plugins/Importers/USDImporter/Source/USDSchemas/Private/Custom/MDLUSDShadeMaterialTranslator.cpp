// Copyright Epic Games, Inc. All Rights Reserved.

#include "MDLUSDShadeMaterialTranslator.h"
#include "Engine/Level.h"

#if USE_USD_SDK && WITH_EDITOR

#include "USDAssetCache.h"
#include "USDAssetImportData.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDLog.h"
#include "USDShadeConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"

#include "Engine/Texture.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialShared.h"
#include "Misc/Paths.h"
#include "MDLImporterOptions.h"
#include "MDLMaterialImporter.h"
#include "UObject/StrongObjectPtr.h"

#include "USDIncludesStart.h"
	#include "pxr/usd/sdf/assetPath.h"
	#include "pxr/usd/usdShade/material.h"
#include "USDIncludesEnd.h"

#define LOCTEXT_NAMESPACE "MDLUSDShadeMaterialTranslator"

FName FMdlUsdShadeMaterialTranslator::MdlRenderContext = TEXT("mdl");

namespace UE
{
	namespace MDLShadeTranslatorImpl
	{
		namespace Private
		{
			void NotifyIfMaterialNeedsVirtualTextures( UMaterialInterface* MaterialInterface )
			{
				if ( UMaterial* Material = Cast<UMaterial>( MaterialInterface ) )
				{
					TArray<UTexture*> UsedTextures;
					const bool bAllQualityLevels = true;
					const bool bAllFeatureLevels = true;
					Material->GetUsedTextures( UsedTextures, EMaterialQualityLevel::High, bAllQualityLevels, ERHIFeatureLevel::SM5, bAllFeatureLevels );

					for ( UTexture* UsedTexture : UsedTextures )
					{
						UsdUtils::NotifyIfVirtualTexturesNeeded( UsedTexture );
					}
				}
				else if ( UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>( MaterialInterface ) )
				{
					for ( const FTextureParameterValue& TextureValue : MaterialInstance->TextureParameterValues )
					{
						if ( UTexture* Texture = TextureValue.ParameterValue )
						{
							UsdUtils::NotifyIfVirtualTexturesNeeded( Texture );
						}
					}
				}
			}
		}
	}
}

void FMdlUsdShadeMaterialTranslator::CreateAssets()
{
	// MDL USD Schema:
	//   info:mdl:sourceAsset -> Path to the MDL file
	//   info:mdl:sourceAsset:subIdentifier -> Name of the material in the MDL file
	//   inputs -> material parameters

	if ( Context->RenderContext != MdlRenderContext )
	{
		Super::CreateAssets();
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

	FScopedUsdAllocs UsdAllocs;

	pxr::UsdShadeMaterial ShadeMaterial( GetPrim() );

	if ( !ShadeMaterial )
	{
		return;
	}

	const pxr::TfToken MdlToken = UnrealToUsd::ConvertToken( *MdlRenderContext.ToString() ).Get();

	pxr::UsdShadeShader SurfaceShader = ShadeMaterial.ComputeSurfaceSource( MdlToken );

	if ( !SurfaceShader )
	{
		Super::CreateAssets();
		return;
	}

	const FString MdlRootPath = Context->Stage.GetRootLayer().GetRealPath();

	pxr::SdfAssetPath SurfaceSourceAssetPath;
	SurfaceShader.GetSourceAsset( &SurfaceSourceAssetPath, MdlToken );

	if ( SurfaceSourceAssetPath.GetAssetPath().empty() )
	{
		// Old Mdl Schema
		const pxr::TfToken ModuleToken( "module" );
		const pxr::UsdAttribute& MDLModule = SurfaceShader.GetPrim().GetAttribute( ModuleToken );

		if ( MDLModule.GetTypeName().GetAsToken() == pxr::SdfValueTypeNames->Asset )
		{
			SurfaceSourceAssetPath = UsdUtils::GetUsdValue< pxr::SdfAssetPath >( MDLModule, Context->Time );
		}
	}

	const FString MdlAbsoluteAssetPath = UsdToUnreal::ConvertString( SurfaceSourceAssetPath.GetResolvedPath() );

	const FString MdlModuleName = [ &MdlAbsoluteAssetPath, &MdlRootPath ]()
	{
		FString ModuleRelativePath = MdlAbsoluteAssetPath;
		FPaths::MakePathRelativeTo( ModuleRelativePath, *MdlRootPath );

		FString ModuleName = UE::Mdl::Util::ConvertFilePathToModuleName( *ModuleRelativePath );
		return ModuleName;
	}();

	if (!MdlModuleName.IsEmpty() && Context->AssetCache)
	{
		pxr::TfToken MdlDefinitionToken;
		SurfaceShader.GetSourceAssetSubIdentifier( &MdlDefinitionToken, MdlToken );

		const FString MdlDefinitionName = UsdToUnreal::ConvertToken( MdlDefinitionToken );

		const FString MdlFullName = MdlModuleName + TEXT("::") + MdlDefinitionName;
		const FString MdlFullInstanceName = MdlFullName + TEXT("_Instance");

		UMaterialInterface* MdlMaterial = Cast< UMaterialInterface >( Context->AssetCache->GetCachedAsset( MdlFullName ) );
		if ( !MdlMaterial )
		{
			FScopedUnrealAllocs UnrealAllocs;

			// Add the USD root as a search path for MDL
			FMdlMaterialImporter::FScopedSearchPath UsdDirMdlSearchPath( FPaths::GetPath( Context->Stage.GetRootLayer().GetRealPath() ) );

			TStrongObjectPtr< UMDLImporterOptions > ImportOptions( NewObject< UMDLImporterOptions >() );

			MdlMaterial = FMdlMaterialImporter::ImportMaterialFromModule( GetTransientPackage(), Context->ObjectFlags | RF_Transient, MdlModuleName, MdlDefinitionName, *ImportOptions.Get() );

			if ( MdlMaterial )
			{
				UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( MdlMaterial, TEXT( "USDAssetImportData" ) );
				ImportData->PrimPath = PrimPath.GetString();
				MdlMaterial->AssetImportData = ImportData;

				Context->AssetCache->CacheAsset( MdlFullName, MdlMaterial );

				UE::MDLShadeTranslatorImpl::Private::NotifyIfMaterialNeedsVirtualTextures( MdlMaterial );
			}
			else
			{
				FUsdLogManager::LogMessage(
					EMessageSeverity::Warning,
					FText::Format( LOCTEXT("UsdMdlConversionFailed", "Failed to create MDL material for prim {0}."), FText::FromString( PrimPath.GetString() ) ) );
			}
		}

		UMaterialInstanceConstant* MdlMaterialInstance = Cast< UMaterialInstanceConstant >(Context->AssetCache->GetCachedAsset(MdlFullInstanceName));
		if ( !MdlMaterialInstance && MdlMaterial )
		{
			MdlMaterialInstance = NewObject< UMaterialInstanceConstant >(GetTransientPackage(), NAME_None, Context->ObjectFlags | RF_Transient);

			UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( MdlMaterialInstance, TEXT( "USDAssetImportData" ) );
			ImportData->PrimPath = PrimPath.GetString();
			MdlMaterialInstance->AssetImportData = ImportData;

			MdlMaterialInstance->SetParentEditorOnly( MdlMaterial );

			UsdToUnreal::ConvertShadeInputsToParameters( ShadeMaterial, *MdlMaterialInstance, Context->AssetCache.Get(), *Context->RenderContext.ToString() );

			// We can't blindly recreate all component render states when a level is being added, because we may end up first creating
			// render states for some components, and UWorld::AddToWorld calls FScene::AddPrimitive which expects the component to not have
			// primitives yet
			FMaterialUpdateContext::EOptions::Type Options = FMaterialUpdateContext::EOptions::Default;
			if ( Context->Level->bIsAssociatingLevel )
			{
				Options = ( FMaterialUpdateContext::EOptions::Type ) ( Options & ~FMaterialUpdateContext::EOptions::RecreateRenderStates );
			}

			FMaterialUpdateContext UpdateContext( Options, GMaxRHIShaderPlatform );
			UpdateContext.AddMaterialInstance( MdlMaterialInstance );
			MdlMaterialInstance->PreEditChange( nullptr );
			MdlMaterialInstance->PostEditChange();

			Context->AssetCache->CacheAsset(MdlFullInstanceName, MdlMaterialInstance);

			UE::MDLShadeTranslatorImpl::Private::NotifyIfMaterialNeedsVirtualTextures( MdlMaterialInstance );
		}

		if (Context->InfoCache)
		{
			Context->InfoCache->LinkAssetToPrim(PrimPath, MdlMaterial);
			Context->InfoCache->LinkAssetToPrim(PrimPath, MdlMaterialInstance);

			if (UMaterial* MdlReference = Cast<UMaterial>(MdlMaterial))
			{
				TArray<UTexture*> UsedTextures;
				const bool bAllQualityLevels = true;
				const bool bAllFeatureLevels = true;
				MdlReference->GetUsedTextures(
					UsedTextures,
					EMaterialQualityLevel::High,
					bAllQualityLevels,
					ERHIFeatureLevel::SM5,
					bAllFeatureLevels
				);

				for (UTexture* Texture : UsedTextures)
				{
					if (Texture->GetOutermost() == GetTransientPackage())
					{
						Context->InfoCache->LinkAssetToPrim(PrimPath, Texture);

						const FString FilePath = Texture->AssetImportData ? Texture->AssetImportData->GetFirstFilename() : Texture->GetName();
						const FString TextureHash = UsdUtils::GetTextureHash(
							FilePath,
							Texture->SRGB,
							Texture->CompressionSettings,
							Texture->GetTextureAddressX(),
							Texture->GetTextureAddressY()
						);

						Texture->SetFlags(RF_Transient);
						Context->AssetCache->CacheAsset(TextureHash, Texture);
					}
				}
			}

			if (MdlMaterialInstance)
			{
				for (const FTextureParameterValue& TextureValue : MdlMaterialInstance->TextureParameterValues)
				{
					if (UTexture* Texture = TextureValue.ParameterValue)
					{
						if (Texture->GetOutermost() == GetTransientPackage())
						{
							Context->InfoCache->LinkAssetToPrim(PrimPath, Texture);

							const FString FilePath = Texture->AssetImportData ? Texture->AssetImportData->GetFirstFilename() : Texture->GetName();
							const FString TextureHash = UsdUtils::GetTextureHash(
								FilePath,
								Texture->SRGB,
								Texture->CompressionSettings,
								Texture->GetTextureAddressX(),
								Texture->GetTextureAddressY()
							);

							Texture->SetFlags(RF_Transient);
							Context->AssetCache->CacheAsset(TextureHash, Texture);
						}
					}
				}
			}
		}
	}
	else
	{
		Super::CreateAssets();
	}
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK && WITH_EDITOR
