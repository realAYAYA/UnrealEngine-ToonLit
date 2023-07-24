// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/PlatformMediaSourceActions.h"
#include "PlatformMediaSource.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FPlatformMediaSourceActions::CanFilter()
{
	return true;
}


FText FPlatformMediaSourceActions::GetName() const
{
	return LOCTEXT("AssetTypeActions_PlatformMediaSource", "Platform Media Source");
}


UClass* FPlatformMediaSourceActions::GetSupportedClass() const
{
	return UPlatformMediaSource::StaticClass();
}


#undef LOCTEXT_NAMESPACE
