// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraWeaponDebugSettings.h"
#include "Misc/App.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraWeaponDebugSettings)

ULyraWeaponDebugSettings::ULyraWeaponDebugSettings()
{
}

FName ULyraWeaponDebugSettings::GetCategoryName() const
{
	return FApp::GetProjectName();
}

