// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOSettings.h"

UOpenColorIOSettings::UOpenColorIOSettings()
	: bUseLegacyProcessor(false)
	, bUse32fLUT(false)
{

}

FName UOpenColorIOSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText UOpenColorIOSettings::GetSectionText() const
{
	return NSLOCTEXT("OpenColorIOSettings", "OpenColorIOSettingsSection", "OpenColorIO");
}

FName UOpenColorIOSettings::GetSectionName() const
{
	return TEXT("OpenColorIO");
}
#endif
