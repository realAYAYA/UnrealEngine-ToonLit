// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlPreferences.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SourceControlPreferences)

bool USourceControlPreferences::IsValidationTagEnabled()
{
	return GetDefault<USourceControlPreferences>()->bEnableValidationTag;
}

bool USourceControlPreferences::ShouldDeleteNewFilesOnRevert()
{
	return GetDefault<USourceControlPreferences>()->bShouldDeleteNewFilesOnRevert;
}

bool USourceControlPreferences::AreUncontrolledChangelistsEnabled()
{
	return GetDefault<USourceControlPreferences>()->bEnableUncontrolledChangelists;
}

