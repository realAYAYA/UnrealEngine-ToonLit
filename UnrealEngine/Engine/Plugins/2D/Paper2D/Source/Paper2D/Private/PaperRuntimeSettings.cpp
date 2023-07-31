// Copyright Epic Games, Inc. All Rights Reserved.

#include "PaperRuntimeSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PaperRuntimeSettings)

//////////////////////////////////////////////////////////////////////////
// UPaperRuntimeSettings

UPaperRuntimeSettings::UPaperRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnableSpriteAtlasGroups(false)
	, bEnableTerrainSplineEditing(false)
	, bResizeSpriteDataToMatchTextures(true)
{
}

