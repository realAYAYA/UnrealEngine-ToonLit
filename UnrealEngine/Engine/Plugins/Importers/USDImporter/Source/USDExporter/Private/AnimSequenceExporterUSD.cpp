// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimSequenceExporterUSD.h"

#include "AnimSequenceExporterUSDOptions.h"
#include "Engine/SkeletalMesh.h"
#include "EngineAnalytics.h"
#include "MaterialExporterUSD.h"
#include "SkeletalMeshExporterUSDOptions.h"
#include "UnrealUSDWrapper.h"
#include "USDClassesModule.h"
#include "USDErrorUtils.h"
#include "USDLayerUtils.h"
#include "USDLog.h"
#include "USDOptionsWindow.h"
#include "USDSkeletalDataConversion.h"
#include "USDUnrealAssetInfo.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "Animation/AnimSequence.h"
#include "AssetExportTask.h"
#include "UObject/GCObjectScopeGuard.h"

namespace UE::AnimSequenceExporterUSD::Private
{
	void SendAnalytics(
		UObject* Asset,
		UAnimSequenceExporterUSDOptions* Options,
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

UAnimSequenceExporterUSD::UAnimSequenceExporterUSD()
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
	SupportedClass = UAnimSequence::StaticClass();
	bText = false;
#endif // #if USE_USD_SDK
}

bool UAnimSequenceExporterUSD::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
#if USE_USD_SDK
	UAnimSequence* AnimSequence = CastChecked< UAnimSequence >( Object );
	if ( !AnimSequence )
	{
		return false;
	}

	FScopedUsdMessageLog UsdMessageLog;

	// We may dispatch another export task in between, so lets cache this for ourselves as it may
	// be overwritten
	const FString AnimSequenceFile = UExporter::CurrentFilename;

	UAnimSequenceExporterUSDOptions* Options = nullptr;
	if ( ExportTask )
	{
		Options = Cast<UAnimSequenceExporterUSDOptions>( ExportTask->Options );
	}
	if ( !Options )
	{
		Options = GetMutableDefault<UAnimSequenceExporterUSDOptions>();

		// Prompt with an options dialog if we can
		if ( Options && ( !ExportTask || !ExportTask->bAutomated ) )
		{
			Options->PreviewMeshOptions.MaterialBakingOptions.TexturesDir.Path = FPaths::Combine( FPaths::GetPath( AnimSequenceFile ), TEXT( "Textures" ) );

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

	// Export preview mesh if needed
	USkeletalMesh* SkeletalMesh = nullptr;
	FString MeshAssetFile;
	if ( Options && Options->bExportPreviewMesh )
	{
		SkeletalMesh = AnimSequence->GetPreviewMesh();
		USkeleton* AnimSkeleton = SkeletalMesh ? SkeletalMesh->GetSkeleton() : nullptr;

		if ( !AnimSkeleton && !SkeletalMesh )
		{
			AnimSkeleton = AnimSequence->GetSkeleton();
			SkeletalMesh = AnimSkeleton ? AnimSkeleton->GetAssetPreviewMesh( AnimSequence ) : nullptr;
		}

		if ( AnimSkeleton && !SkeletalMesh )
		{
			SkeletalMesh = AnimSkeleton->FindCompatibleMesh();
		}

		if ( SkeletalMesh )
		{
			FString PathPart;
			FString FilenamePart;
			FString ExtensionPart;
			FPaths::Split( AnimSequenceFile, PathPart, FilenamePart, ExtensionPart );
			MeshAssetFile = FPaths::Combine( PathPart, FilenamePart + TEXT( "_SkeletalMesh." ) + ExtensionPart );

			USkeletalMeshExporterUSDOptions* SkeletalMeshOptions = GetMutableDefault<USkeletalMeshExporterUSDOptions>();
			SkeletalMeshOptions->StageOptions = Options->StageOptions;
			SkeletalMeshOptions->MeshAssetOptions = Options->PreviewMeshOptions;
			SkeletalMeshOptions->bReExportIdenticalAssets = Options->bReExportIdenticalAssets;

			UAssetExportTask* LevelExportTask = NewObject<UAssetExportTask>();
			FGCObjectScopeGuard ExportTaskGuard( LevelExportTask );
			LevelExportTask->Object = SkeletalMesh;
			LevelExportTask->Options = SkeletalMeshOptions;
			LevelExportTask->Exporter = nullptr;
			LevelExportTask->Filename = MeshAssetFile;
			LevelExportTask->bSelected = false;
			LevelExportTask->bReplaceIdentical = ExportTask->bReplaceIdentical;
			LevelExportTask->bPrompt = false;
			LevelExportTask->bUseFileArchive = false;
			LevelExportTask->bWriteEmptyFiles = false;
			LevelExportTask->bAutomated = true; // Pretend this is an automated task so it doesn't pop the options dialog

			const bool bSucceeded = UExporter::RunAssetExportTask( LevelExportTask );
			if ( !bSucceeded )
			{
				MeshAssetFile = {};
			}
		}
		else
		{
			FUsdLogManager::LogMessage(
				EMessageSeverity::Warning,
				FText::Format( NSLOCTEXT( "AnimSequenceExporterUSD", "InvalidSkelMesh", "Couldn't find the skeletal mesh to export for anim sequence {0}." ), FText::FromName( AnimSequence->GetFName() ) ) );
		}
	}

	// Collect the target paths for our SkelAnimation prim and its SkelRoot, if any
	UE::FSdfPath SkelRootPath;
	UE::FSdfPath SkelAnimPath;
	if ( MeshAssetFile.IsEmpty() )
	{
		SkelAnimPath = UE::FSdfPath::AbsoluteRootPath().AppendChild(
			*UsdUtils::SanitizeUsdIdentifier( *AnimSequence->GetName() )
		);
	}
	else
	{
		SkelRootPath = UE::FSdfPath::AbsoluteRootPath().AppendChild(
			*UsdUtils::SanitizeUsdIdentifier( *SkeletalMesh->GetName() )
		);

		SkelAnimPath = SkelRootPath.AppendChild(
			*UsdUtils::SanitizeUsdIdentifier( *AnimSequence->GetName() )
		);
	}

	FString AnimSequenceVersion;
	if ( UAnimDataModel* DataModel = AnimSequence->GetDataModel() )
	{
		FSHA1 SHA1;

		FGuid DataModelGuid = DataModel->GenerateGuid();
		SHA1.Update( reinterpret_cast< uint8* >( &DataModelGuid ), sizeof( DataModelGuid ) );

		UsdUtils::HashForAnimSequenceExport( *Options, SHA1 );

		SHA1.Final();
		FSHAHash Hash;
		SHA1.GetHash( &Hash.Hash[ 0 ] );
		AnimSequenceVersion = Hash.ToString();
	}

	// Check if we already have exported what we plan on exporting anyway
	if ( FPaths::FileExists( AnimSequenceFile ) && !AnimSequenceVersion.IsEmpty() )
	{
		if ( !ExportTask->bReplaceIdentical )
		{
			UE_LOG( LogUsd, Log,
				TEXT( "Skipping export of asset '%s' as the target file '%s' already exists." ),
				*Object->GetPathName(),
				*AnimSequenceFile
			);
			return false;
		}
		// If we don't want to re-export this asset we need to check if its the same version
		else if ( !Options->bReExportIdenticalAssets )
		{
			// Don't use the stage cache here as we want this stage to close within this scope in case
			// we have to overwrite its files due to e.g. missing payload or anything like that
			const bool bUseStageCache = false;
			const EUsdInitialLoadSet InitialLoadSet = EUsdInitialLoadSet::LoadNone;
			if ( UE::FUsdStage TempStage = UnrealUSDWrapper::OpenStage( *AnimSequenceFile, InitialLoadSet, bUseStageCache ) )
			{
				if ( UE::FUsdPrim SkelAnimPrim = TempStage.GetPrimAtPath( SkelAnimPath ) )
				{
					FUsdUnrealAssetInfo Info = UsdUtils::GetPrimAssetInfo( SkelAnimPrim );

					const bool bVersionMatches = !Info.Version.IsEmpty() && Info.Version == AnimSequenceVersion;

					const bool bAssetTypeMatches = !Info.UnrealAssetType.IsEmpty()
						&& Info.UnrealAssetType == Object->GetClass()->GetName();

					if ( bVersionMatches && bAssetTypeMatches )
					{
						UE_LOG( LogUsd, Log,
							TEXT( "Skipping export of asset '%s' as the target file '%s' already contains up-to-date exported data." ),
							*Object->GetPathName(),
							*AnimSequenceFile
						);
						return true;
					}
				}
			}
		}
	}

	double StartTime = FPlatformTime::Cycles64();

	UE::FUsdStage AnimationStage = UnrealUSDWrapper::NewStage( *AnimSequenceFile );
	if ( !AnimationStage )
	{
		return false;
	}

	UE::FUsdPrim SkelRootPrim;
	UE::FUsdPrim SkelAnimPrim;

	// Haven't exported the SkeletalMesh, just make a stage with a SkelAnimation prim
	if( MeshAssetFile.IsEmpty() )
	{
		SkelAnimPrim = AnimationStage.DefinePrim( SkelAnimPath, TEXT( "SkelAnimation" ) );
		if ( !SkelAnimPrim )
		{
			return false;
		}

		AnimationStage.SetDefaultPrim( SkelAnimPrim );
	}
	// Exported a SkeletalMesh prim elsewhere, create a SkelRoot containing this SkelAnimation prim
	else
	{
		SkelRootPrim = AnimationStage.DefinePrim( SkelRootPath, TEXT( "SkelRoot" ) );
		if ( !SkelRootPrim )
		{
			return false;
		}

		SkelAnimPrim = AnimationStage.DefinePrim( SkelAnimPath, TEXT( "SkelAnimation" ) );
		if ( !SkelAnimPrim )
		{
			return false;
		}

		AnimationStage.SetDefaultPrim( SkelRootPrim );
		UsdUtils::BindAnimationSource( SkelRootPrim, SkelAnimPrim );

		// Add a reference to the SkelRoot of the static mesh, which will compose in the Mesh and Skeleton prims
		UsdUtils::AddReference( SkelRootPrim, *MeshAssetFile );
	}

	// Configure stage metadata
	{
		if ( Options )
		{
			UsdUtils::SetUsdStageMetersPerUnit( AnimationStage, Options->StageOptions.MetersPerUnit );
			UsdUtils::SetUsdStageUpAxis( AnimationStage, Options->StageOptions.UpAxis );
		}

		const double StartTimeCode = 0.0;
		const double EndTimeCode = AnimSequence->GetNumberOfSampledKeys() - 1;
		UsdUtils::AddTimeCodeRangeToLayer( AnimationStage.GetRootLayer(), StartTimeCode, EndTimeCode );

		AnimationStage.SetTimeCodesPerSecond( AnimSequence->GetSamplingFrameRate().AsDecimal() );
	}

	UnrealToUsd::ConvertAnimSequence( AnimSequence, SkelAnimPrim );

	// Write asset info now that we finished exporting
	{
		FUsdUnrealAssetInfo Info;
		Info.Name = AnimSequence->GetName();
		Info.Identifier = AnimSequenceFile;
		Info.Version = AnimSequenceVersion;
		Info.UnrealContentPath = AnimSequence->GetPathName();
		Info.UnrealAssetType = AnimSequence->GetClass()->GetName();
		Info.UnrealExportTime = FDateTime::Now().ToString();
		Info.UnrealEngineVersion = FEngineVersion::Current().ToString();

		UsdUtils::SetPrimAssetInfo( SkelAnimPrim, Info );
	}

	AnimationStage.GetRootLayer().Save();

	// Analytics
	{
		bool bAutomated = ExportTask ? ExportTask->bAutomated : false;
		double ElapsedSeconds = FPlatformTime::ToSeconds64( FPlatformTime::Cycles64() - StartTime );
		FString Extension = FPaths::GetExtension( AnimSequenceFile );

		UE::AnimSequenceExporterUSD::Private::SendAnalytics(
			Object,
			Options,
			bAutomated,
			ElapsedSeconds,
			UsdUtils::GetUsdStageNumFrames( AnimationStage ),
			Extension
		);
	}

	return true;
#else
	return false;
#endif // #if USE_USD_SDK
}