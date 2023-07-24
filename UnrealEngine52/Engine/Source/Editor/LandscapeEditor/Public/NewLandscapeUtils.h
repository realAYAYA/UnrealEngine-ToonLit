// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class ALandscape;
struct FLandscapeFileResolution;
struct FLandscapeImportLayerInfo;
class ULandscapeEditorObject;

enum class ENewLandscapePreviewMode : uint8;

// This class has been replaced by FLandscapeImportHelper & ULandscapeEditorObject methods.
class LANDSCAPEEDITOR_API UE_DEPRECATED(5.1, "This class has been replaced by FLandscapeImportHelper & ULandscapeEditorObject methods. Use the these instead.") FNewLandscapeUtils
{
public:
	static void ChooseBestComponentSizeForImport( ULandscapeEditorObject* UISettings );
	static void ImportLandscapeData( ULandscapeEditorObject* UISettings, TArray< FLandscapeFileResolution >& ImportResolutions );
	static TOptional< TArray< FLandscapeImportLayerInfo > > CreateImportLayersInfo( ULandscapeEditorObject* UISettings, ENewLandscapePreviewMode NewLandscapePreviewMode );
	static TArray<uint16> ComputeHeightData( ULandscapeEditorObject* UISettings, TArray< FLandscapeImportLayerInfo >& ImportLayers, ENewLandscapePreviewMode NewLandscapePreviewMode );
};
