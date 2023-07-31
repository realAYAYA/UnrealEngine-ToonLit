// Copyright Epic Games, Inc. All Rights Reserved.

#include "NewLandscapeUtils.h"

#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorModule.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeEdMode.h"
#include "LandscapeConfigHelper.h"

#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Templates/UnrealTemplate.h"

#define LOCTEXT_NAMESPACE "LandscapeEditor.NewLandscape"

void FNewLandscapeUtils::ChooseBestComponentSizeForImport(ULandscapeEditorObject* UISettings)
{
	UISettings->ChooseBestComponentSizeForImport();
}

void FNewLandscapeUtils::ImportLandscapeData( ULandscapeEditorObject* UISettings, TArray< FLandscapeFileResolution >& ImportResolutions )
{
	if ( !UISettings )
	{
		return;
	}

	ImportResolutions.Reset(1);
	UISettings->ImportLandscape_Width = 0;
	UISettings->ImportLandscape_Height = 0;
	UISettings->ClearImportLandscapeData();
	UISettings->ImportLandscape_HeightmapImportResult = ELandscapeImportResult::Success;
	UISettings->ImportLandscape_HeightmapErrorMessage = FText();
	int32 IndexToUse = 0;
	if(!UISettings->ImportLandscape_HeightmapFilename.IsEmpty())
	{
		FLandscapeImportDescriptor OutImportDescriptor;
		UISettings->ImportLandscape_HeightmapImportResult =
			FLandscapeImportHelper::GetHeightmapImportDescriptor(UISettings->ImportLandscape_HeightmapFilename, UISettings->UseSingleFileImport(), UISettings->bFlipYAxis, OutImportDescriptor, UISettings->ImportLandscape_HeightmapErrorMessage);

		if (UISettings->ImportLandscape_HeightmapImportResult != ELandscapeImportResult::Error)
		{
			IndexToUse = OutImportDescriptor.FileResolutions.Num() / 2;
			// In a single file import the region should match the file resolution
			check(OutImportDescriptor.ImportResolutions[IndexToUse].Width == OutImportDescriptor.FileResolutions[IndexToUse].Width &&
				OutImportDescriptor.ImportResolutions[IndexToUse].Height == OutImportDescriptor.FileResolutions[IndexToUse].Height);
			ImportResolutions = MoveTemp(OutImportDescriptor.FileResolutions);
		}
	}

	if(ImportResolutions.Num() > 0)
	{
		UISettings->ImportLandscape_Width = ImportResolutions[IndexToUse].Width;
		UISettings->ImportLandscape_Height = ImportResolutions[IndexToUse].Height;
		UISettings->ChooseBestComponentSizeForImport();
		UISettings->ImportLandscapeData();
	}
}

TOptional< TArray< FLandscapeImportLayerInfo > > FNewLandscapeUtils::CreateImportLayersInfo( ULandscapeEditorObject* UISettings, ENewLandscapePreviewMode NewLandscapePreviewMode )
{
	TArray<FLandscapeImportLayerInfo> ImportLayers;
	ELandscapeImportResult Result = ELandscapeImportResult::Success;
	if(NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape)
	{
		Result = UISettings->CreateNewLayersInfo(ImportLayers);
	}
	else if(NewLandscapePreviewMode == ENewLandscapePreviewMode::ImportLandscape)
	{
		Result = UISettings->CreateImportLayersInfo(ImportLayers);
	}

	if (Result == ELandscapeImportResult::Error)
	{
		return TOptional<TArray<FLandscapeImportLayerInfo>>();
	}

	return MoveTemp(ImportLayers);
}

TArray<uint16> FNewLandscapeUtils::ComputeHeightData(ULandscapeEditorObject* UISettings, TArray< FLandscapeImportLayerInfo >& ImportLayers, ENewLandscapePreviewMode NewLandscapePreviewMode)
{
	TArray<uint16> HeightData;
	if (NewLandscapePreviewMode == ENewLandscapePreviewMode::NewLandscape)
	{
		UISettings->InitializeDefaultHeightData(HeightData);
	}
	else
	{
		UISettings->ExpandImportData(HeightData, ImportLayers);
	}

	return MoveTemp(HeightData);
}

#undef LOCTEXT_NAMESPACE
