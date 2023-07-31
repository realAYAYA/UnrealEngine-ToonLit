// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelExporterUSD.h"

#include "LevelExporterUSDOptions.h"
#include "UnrealUSDWrapper.h"
#include "USDErrorUtils.h"
#include "USDLayerUtils.h"
#include "USDOptionsWindow.h"
#include "USDSkeletalDataConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#include "AssetExportTask.h"
#include "EditorLevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "IPythonScriptPlugin.h"

ULevelExporterUSD::ULevelExporterUSD()
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
	SupportedClass = UWorld::StaticClass();
	bText = false;
#endif // #if USE_USD_SDK
}

bool ULevelExporterUSD::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags )
{
#if USE_USD_SDK
	UWorld* World = Cast< UWorld >( Object );
	if ( !World )
	{
		return false;
	}

	ULevelExporterUSDOptions* Options = nullptr;
	if ( ExportTask )
	{
		Options = Cast<ULevelExporterUSDOptions>( ExportTask->Options );
	}

	if ( !Options )
	{
		Options = GetMutableDefault<ULevelExporterUSDOptions>();
	}
	if ( !Options )
	{
		return false;
	}

	if ( ExportTask )
	{
		// Set this early so that we can use the Object from it (the world/level to export) when showing
		// the level export options dialog.
		// Note that we must also consider how LevelSequenceExporterUSD may call down to us and provide the
		// export task that we must use. In that case we will be an automated export too
		Options->CurrentTask = ExportTask;

		// There is a dedicated "Export selected" option that sets this, so let's sync to it
		Options->Inner.bSelectionOnly = ExportTask->bSelected;
	}

	// Manual export: Show dialog options
	if ( !ExportTask || !ExportTask->bAutomated )
	{
		Options->Inner.AssetFolder.Path = FPaths::Combine( FPaths::GetPath( UExporter::CurrentFilename ), TEXT( "Assets" ) );

		const bool bContinue = SUsdOptionsWindow::ShowExportOptions( *Options );
		if ( !bContinue )
		{
			return false;
		}
	}

	// Note how we don't explicitly pass the Options down to Python here: We stash our desired export options on the CDO, and
	// those are read from Python by executing export_with_cdo_options().
	if ( IPythonScriptPlugin::Get()->IsPythonAvailable() )
	{
		IPythonScriptPlugin::Get()->ExecPythonCommand( TEXT( "import usd_unreal.level_exporter; usd_unreal.level_exporter.export_with_cdo_options()" ) );
	}
	Options->CurrentTask = nullptr;

	return true;
#else
	return false;
#endif // #if USE_USD_SDK
}
