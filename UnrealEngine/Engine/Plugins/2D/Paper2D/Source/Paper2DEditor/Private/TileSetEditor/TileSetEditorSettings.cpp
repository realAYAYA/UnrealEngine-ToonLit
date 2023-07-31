// Copyright Epic Games, Inc. All Rights Reserved.

#include "TileSetEditor/TileSetEditorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TileSetEditorSettings)

//////////////////////////////////////////////////////////////////////////
// UTileSetEditorSettings

UTileSetEditorSettings::UTileSetEditorSettings()
	: DefaultBackgroundColor(0, 0, 127)
	, bShowGridByDefault(true)
	, ExtrusionAmount(2)
	, bPadToPowerOf2(true)
	, bFillWithTransparentBlack(true)
{
}

