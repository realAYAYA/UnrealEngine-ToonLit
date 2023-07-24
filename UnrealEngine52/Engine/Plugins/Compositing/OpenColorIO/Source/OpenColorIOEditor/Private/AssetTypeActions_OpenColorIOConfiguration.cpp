// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_OpenColorIOConfiguration.h"

#include "OpenColorIOConfiguration.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_OpenColorIOConfiguration"


/* FAssetTypeActions_Base interface
*****************************************************************************/

FText FAssetTypeActions_OpenColorIOConfiguration::GetAssetDescription(const struct FAssetData& AssetData) const
{
	return FText::FromString(AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, ConfigurationFile)));
}


UClass* FAssetTypeActions_OpenColorIOConfiguration::GetSupportedClass() const
{
	return UOpenColorIOConfiguration::StaticClass();
}


FText FAssetTypeActions_OpenColorIOConfiguration::GetName() const
{
	return LOCTEXT("AssetTypeActions_OpenColorIOConfiguration", "OpenColorIO Configuration");
}

FColor FAssetTypeActions_OpenColorIOConfiguration::GetTypeColor() const
{
	return FColor::White;
}


#undef LOCTEXT_NAMESPACE
