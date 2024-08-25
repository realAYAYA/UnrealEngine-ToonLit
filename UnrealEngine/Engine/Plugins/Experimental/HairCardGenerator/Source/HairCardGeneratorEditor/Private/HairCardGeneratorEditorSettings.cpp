// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairCardGeneratorEditorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(HairCardGeneratorEditorSettings)

UHairCardGeneratorEditorSettings::UHairCardGeneratorEditorSettings()
	: HairCardAssetNameFormat(TEXT("{groom_name}_LOD{lod}"))
	, HairCardAssetPathFormat(TEXT("{groom_dir}"))
{
}
