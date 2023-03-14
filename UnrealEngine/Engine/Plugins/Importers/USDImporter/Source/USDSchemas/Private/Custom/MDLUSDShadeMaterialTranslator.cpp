// Copyright Epic Games, Inc. All Rights Reserved.

#include "MDLUSDShadeMaterialTranslator.h"

#if USE_USD_SDK && WITH_EDITOR

#include "USDAssetImportData.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDShadeConversion.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"

#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
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

	if ( !MdlModuleName.IsEmpty() )
	{
		pxr::TfToken MdlDefinitionToken;
		SurfaceShader.GetSourceAssetSubIdentifier( &MdlDefinitionToken, MdlToken );

		const FString MdlDefinitionName = UsdToUnreal::ConvertToken( MdlDefinitionToken );

		const FString MdlFullname = MdlModuleName + TEXT("::") + MdlDefinitionName;

		UMaterialInterface* MdlMaterial = Cast< UMaterialInterface >( Context->AssetCache->GetCachedAsset( MdlFullname ) );

		if ( !MdlMaterial )
		{
			FScopedUnrealAllocs UnrealAllocs;

			// Add the USD root as a search path for MDL
			FMdlMaterialImporter::FScopedSearchPath UsdDirMdlSearchPath( FPaths::GetPath( Context->Stage.GetRootLayer().GetRealPath() ) );

			TStrongObjectPtr< UMDLImporterOptions > ImportOptions( NewObject< UMDLImporterOptions >() );

			MdlMaterial = FMdlMaterialImporter::ImportMaterialFromModule( GetTransientPackage(), Context->ObjectFlags, MdlModuleName, MdlDefinitionName, *ImportOptions.Get() );

			if ( MdlMaterial )
			{
				UUsdAssetImportData* ImportData = NewObject< UUsdAssetImportData >( MdlMaterial, TEXT( "USDAssetImportData" ) );
				ImportData->PrimPath = PrimPath.GetString();
				MdlMaterial->AssetImportData = ImportData;

				Context->AssetCache->CacheAsset( MdlFullname, MdlMaterial );

				UE::MDLShadeTranslatorImpl::Private::NotifyIfMaterialNeedsVirtualTextures( MdlMaterial );
			}
			else
			{
				FUsdLogManager::LogMessage(
					EMessageSeverity::Warning,
					FText::Format( LOCTEXT("UsdMdlConversionFailed", "Failed to create MDL material for prim {0}."), FText::FromString( PrimPath.GetString() ) ) );
			}
		}

		UMaterialInstanceConstant* MdlMaterialInstance = nullptr;
		if ( MdlMaterial )
		{
			MdlMaterialInstance = NewObject< UMaterialInstanceConstant >( GetTransientPackage() );

			if ( MdlMaterialInstance )
			{
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

				UE::MDLShadeTranslatorImpl::Private::NotifyIfMaterialNeedsVirtualTextures( MdlMaterialInstance );
			}
		}

		const FString PrimPathString = PrimPath.GetString();
		Context->AssetCache->CacheAsset( PrimPathString, MdlMaterialInstance );
		Context->AssetCache->LinkAssetToPrim( PrimPathString, MdlMaterialInstance );
	}
	else
	{
		Super::CreateAssets();
	}
}

#undef LOCTEXT_NAMESPACE

#endif // #if USE_USD_SDK && WITH_EDITOR
