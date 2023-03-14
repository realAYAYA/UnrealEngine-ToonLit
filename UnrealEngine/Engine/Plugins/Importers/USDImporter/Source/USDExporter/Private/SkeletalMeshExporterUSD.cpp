// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshExporterUSD.h"

#include "EngineAnalytics.h"
#include "MaterialExporterUSD.h"
#include "SkeletalMeshExporterUSDOptions.h"
#include "USDClassesModule.h"
#include "USDConversionUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDOptionsWindow.h"
#include "USDSkeletalDataConversion.h"
#include "USDTypesConversion.h"
#include "USDUnrealAssetInfo.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "AssetExportTask.h"
#include "Engine/SkeletalMesh.h"

namespace UE::SkeletalMeshExporterUSD::Private
{
	void SendAnalytics(
		UObject* Asset,
		USkeletalMeshExporterUSDOptions* Options,
		bool bAutomated,
		double ElapsedSeconds,
		double NumberOfFrames,
		const FString& Extension
	)
	{
		if ( !Asset || !FEngineAnalytics::IsAvailable() )
		{
			return;
		}

		FString ClassName = Asset->GetClass()->GetName();

		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Emplace( TEXT( "AssetType" ), ClassName );

		if ( Options )
		{
			UsdUtils::AddAnalyticsAttributes( *Options, EventAttributes );
		}

		IUsdClassesModule::SendAnalytics(
			MoveTemp( EventAttributes ),
			FString::Printf( TEXT( "Export.%s" ), *ClassName ),
			bAutomated,
			ElapsedSeconds,
			NumberOfFrames,
			Extension
		);
	}
}

USkeletalMeshExporterUsd::USkeletalMeshExporterUsd()
{
#if USE_USD_SDK
	for ( const FString& Extension : UnrealUSDWrapper::GetNativeFileFormats() )
	{
		// USDZ is not supported for writing for now
		if ( Extension.Equals( TEXT( "usdz" ) ) )
		{
			continue;
		}

		FormatExtension.Add(Extension);
		FormatDescription.Add(TEXT("Universal Scene Description file"));
	}
	SupportedClass = USkeletalMesh::StaticClass();
	bText = false;
#endif // #if USE_USD_SDK
}

bool USkeletalMeshExporterUsd::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
#if USE_USD_SDK
	USkeletalMesh* SkeletalMesh = CastChecked< USkeletalMesh >( Object );
	if ( !SkeletalMesh )
	{
		return false;
	}

	USkeletalMeshExporterUSDOptions* Options = nullptr;
	if ( ExportTask )
	{
		Options = Cast<USkeletalMeshExporterUSDOptions>( ExportTask->Options );
	}
	if ( !Options )
	{
		Options = GetMutableDefault<USkeletalMeshExporterUSDOptions>();

		// Prompt with an options dialog if we can
		if ( Options && ( !ExportTask || !ExportTask->bAutomated ) )
		{
			Options->MeshAssetOptions.MaterialBakingOptions.TexturesDir.Path = FPaths::Combine( FPaths::GetPath( UExporter::CurrentFilename ), TEXT( "Textures" ) );

			const bool bContinue = SUsdOptionsWindow::ShowExportOptions( *Options );
			if ( !bContinue )
			{
				return false;
			}
		}
	}
	if ( !Options )
	{
		return false;
	}

	// See comment on the analogous line within StaticMeshExporterUSD.cpp
	ExportTask->bPrompt = false;

	// If bUsePayload is true, we'll intercept the filename so that we write the mesh data to
	// "C:/MyFolder/file_payload.usda" and create an "asset" file "C:/MyFolder/file.usda" that uses it
	// as a payload, pointing at the default prim
	FString PayloadFilename = UExporter::CurrentFilename;
	if ( Options && Options->MeshAssetOptions.bUsePayload )
	{
		FString PathPart;
		FString FilenamePart;
		FString ExtensionPart;
		FPaths::Split( PayloadFilename, PathPart, FilenamePart, ExtensionPart );

		if ( FormatExtension.Contains( Options->MeshAssetOptions.PayloadFormat ) )
		{
			ExtensionPart = Options->MeshAssetOptions.PayloadFormat;
		}

		PayloadFilename = FPaths::Combine( PathPart, FilenamePart + TEXT( "_payload." ) + ExtensionPart );
	}

	// Get a simple GUID hash/identifier of our mesh
	FString DDCKeyHash;
	if( FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering() )
	{
		const FString& DDCKey = RenderData->DerivedDataKey;
		FSHA1 SHA1;
		SHA1.UpdateWithString( *DDCKey, DDCKey.Len() );
		UsdUtils::HashForSkeletalMeshExport( *Options, SHA1 );
		SHA1.Final();
		FSHAHash Hash;
		SHA1.GetHash( &Hash.Hash[ 0 ] );
		DDCKeyHash = Hash.ToString();
	}

	// Check if we already have exported what we plan on exporting anyway
	if ( FPaths::FileExists( UExporter::CurrentFilename ) && FPaths::FileExists( PayloadFilename ) )
	{
		if ( !ExportTask->bReplaceIdentical )
		{
			UE_LOG( LogUsd, Log,
				TEXT( "Skipping export of asset '%s' as the target file '%s' already exists." ),
				*Object->GetPathName(),
				*UExporter::CurrentFilename
			);
			return false;
		}
		// If we don't want to re-export this asset we need to check if its the same version
		else if ( !Options->bReExportIdenticalAssets )
		{
			bool bSkipMeshExport = false;

			// Don't use the stage cache here as we want this stage to close within this scope in case
			// we have to overwrite its files due to e.g. missing payload or anything like that
			const bool bUseStageCache = false;
			const EUsdInitialLoadSet InitialLoadSet = EUsdInitialLoadSet::LoadNone;
			if ( UE::FUsdStage TempStage = UnrealUSDWrapper::OpenStage( *UExporter::CurrentFilename, InitialLoadSet, bUseStageCache ) )
			{
				if ( UE::FUsdPrim DefaultPrim = TempStage.GetDefaultPrim() )
				{
					FUsdUnrealAssetInfo Info = UsdUtils::GetPrimAssetInfo( DefaultPrim );

					const bool bVersionMatches = !Info.Version.IsEmpty() && Info.Version == DDCKeyHash;

					const bool bAssetTypeMatches = !Info.UnrealAssetType.IsEmpty()
						&& Info.UnrealAssetType == SkeletalMesh->GetClass()->GetName();

					if ( bVersionMatches && bAssetTypeMatches )
					{
						UE_LOG( LogUsd, Log,
							TEXT( "Skipping export of asset '%s' as the target file '%s' already contains up-to-date exported data." ),
							*SkeletalMesh->GetPathName(),
							*UExporter::CurrentFilename
						);

						bSkipMeshExport = true;
					}
				}
			}

			if ( bSkipMeshExport )
			{
				// Even if we're not going to export the mesh, we may still need to re-bake materials
				if ( Options->MeshAssetOptions.bBakeMaterials )
				{
					TSet<UMaterialInterface*> MaterialsToBake;
					for ( const FSkeletalMaterial& SkeletalMaterial : SkeletalMesh->GetMaterials() )
					{
						MaterialsToBake.Add( SkeletalMaterial.MaterialInterface );
					}

					const bool bIsAssetLayer = true;
					UMaterialExporterUsd::ExportMaterialsForStage(
						MaterialsToBake.Array(),
						Options->MeshAssetOptions.MaterialBakingOptions,
						UExporter::CurrentFilename,
						bIsAssetLayer,
						Options->MeshAssetOptions.bUsePayload,
						Options->MeshAssetOptions.bRemoveUnrealMaterials,
						ExportTask->bReplaceIdentical,
						Options->bReExportIdenticalAssets,
						ExportTask->bAutomated
					);
				}

				return true;
			}
		}
	}

	double StartTime = FPlatformTime::Cycles64();

	// UsdStage is the payload stage when exporting with payloads, or just the single stage otherwise
	UE::FUsdStage UsdStage = UnrealUSDWrapper::NewStage( *PayloadFilename );
	if ( !UsdStage )
	{
		return false;
	}

	if ( Options )
	{
		UsdUtils::SetUsdStageMetersPerUnit( UsdStage, Options->StageOptions.MetersPerUnit );
		UsdUtils::SetUsdStageUpAxis( UsdStage, Options->StageOptions.UpAxis );
	}

	FString RootPrimPath = ( TEXT( "/" ) + UsdUtils::SanitizeUsdIdentifier( *SkeletalMesh->GetName() ) );

	UE::FUsdPrim RootPrim = UsdStage.DefinePrim( UE::FSdfPath( *RootPrimPath ), TEXT("SkelRoot") );
	if ( !RootPrim )
	{
		return false;
	}

	UsdStage.SetDefaultPrim( RootPrim );

	// Asset stage always the stage where we write the material assignments
	UE::FUsdStage AssetStage;

	// Using payload: Convert mesh data through the asset stage (that references the payload) so that we can
	// author mesh data on the payload layer and material data on the asset layer
	if ( Options && Options->MeshAssetOptions.bUsePayload )
	{
		AssetStage = UnrealUSDWrapper::NewStage( *UExporter::CurrentFilename );
		if ( AssetStage )
		{
			UsdUtils::SetUsdStageMetersPerUnit( AssetStage, Options->StageOptions.MetersPerUnit );
			UsdUtils::SetUsdStageUpAxis( AssetStage, Options->StageOptions.UpAxis );

			if ( UE::FUsdPrim AssetRootPrim = AssetStage.DefinePrim( UE::FSdfPath( *RootPrimPath ) ) )
			{
				AssetStage.SetDefaultPrim( AssetRootPrim );
				UsdUtils::AddPayload( AssetRootPrim, *PayloadFilename );
			}
		}
	}
	// Not using payload: Just author everything on the current edit target of the single stage
	else
	{
		AssetStage = UsdStage;
	}

	UnrealToUsd::ConvertSkeletalMesh( SkeletalMesh, RootPrim, UsdUtils::GetDefaultTimeCode(), &AssetStage, Options->MeshAssetOptions.LowestMeshLOD, Options->MeshAssetOptions.HighestMeshLOD );

	// Write asset info now that we finished exporting
	if ( UE::FUsdPrim AssetDefaultPrim = AssetStage.GetDefaultPrim() )
	{
		FUsdUnrealAssetInfo Info;
		Info.Name = SkeletalMesh->GetName();
		Info.Identifier = UExporter::CurrentFilename;
		Info.Version = DDCKeyHash;
		Info.UnrealContentPath = SkeletalMesh->GetPathName();
		Info.UnrealAssetType = SkeletalMesh->GetClass()->GetName();
		Info.UnrealExportTime = FDateTime::Now().ToString();
		Info.UnrealEngineVersion = FEngineVersion::Current().ToString();

		UsdUtils::SetPrimAssetInfo( AssetDefaultPrim, Info );
	}

	// Bake materials and replace unrealMaterials with references to the baked files.
	if ( Options->MeshAssetOptions.bBakeMaterials )
	{
		TSet<UMaterialInterface*> MaterialsToBake;
		for ( const FSkeletalMaterial& SkeletalMaterial : SkeletalMesh->GetMaterials() )
		{
			MaterialsToBake.Add( SkeletalMaterial.MaterialInterface );
		}

		const bool bIsAssetLayer = true;
		UMaterialExporterUsd::ExportMaterialsForStage(
			MaterialsToBake.Array(),
			Options->MeshAssetOptions.MaterialBakingOptions,
			AssetStage.GetRootLayer().GetRealPath(),
			bIsAssetLayer,
			Options->MeshAssetOptions.bUsePayload,
			Options->MeshAssetOptions.bRemoveUnrealMaterials,
			ExportTask->bReplaceIdentical,
			Options->bReExportIdenticalAssets,
			ExportTask->bAutomated
		);
	}

	if ( AssetStage && UsdStage != AssetStage )
	{
		AssetStage.GetRootLayer().Save();
	}
	UsdStage.GetRootLayer().Save();

	// Analytics
	{
		bool bAutomated = ExportTask ? ExportTask->bAutomated : false;
		double ElapsedSeconds = FPlatformTime::ToSeconds64( FPlatformTime::Cycles64() - StartTime );
		FString Extension = FPaths::GetExtension( UExporter::CurrentFilename );
		double NumberOfFrames = 1 + UsdStage.GetEndTimeCode() - UsdStage.GetStartTimeCode();

		UE::SkeletalMeshExporterUSD::Private::SendAnalytics(
			Object,
			Options,
			bAutomated,
			ElapsedSeconds,
			UsdUtils::GetUsdStageNumFrames( AssetStage ),
			Extension
		);
	}

	return true;
#else
	return false;
#endif // #if USE_USD_SDK
}
