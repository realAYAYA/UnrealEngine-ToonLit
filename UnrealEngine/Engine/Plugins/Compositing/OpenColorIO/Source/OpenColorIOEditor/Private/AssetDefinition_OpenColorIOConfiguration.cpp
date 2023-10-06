// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_OpenColorIOConfiguration.h"

#include "OpenColorIOConfiguration.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_OpenColorIOConfiguration"


/* FAssetTypeActions_Base interface
*****************************************************************************/

FText UAssetDefinition_OpenColorIOConfiguration::GetAssetDescription(const struct FAssetData& AssetData) const
{
	return FText::FromString(AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UOpenColorIOConfiguration, ConfigurationFile)));
}


TSoftClassPtr<UObject> UAssetDefinition_OpenColorIOConfiguration::GetAssetClass() const
{
	return UOpenColorIOConfiguration::StaticClass();
}


FText UAssetDefinition_OpenColorIOConfiguration::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeActions_OpenColorIOConfiguration", "OpenColorIO Configuration");
}

FLinearColor UAssetDefinition_OpenColorIOConfiguration::GetAssetColor() const
{
	return FLinearColor::White;
}


#undef LOCTEXT_NAMESPACE
