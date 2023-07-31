// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_OpenColorIOConfiguration.h"

#include "OpenColorIOConfiguration.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_OpenColorIOConfiguration"


/* FAssetTypeActions_Base interface
*****************************************************************************/

FText FAssetTypeActions_OpenColorIOConfiguration::GetAssetDescription(const struct FAssetData& AssetData) const
{
	UOpenColorIOConfiguration* Asset = Cast<UOpenColorIOConfiguration>(AssetData.GetAsset());

	FString Description;
	if (Asset != nullptr)
	{
		const FString AssociatedFile = Asset->ConfigurationFile.FilePath;

		if (AssociatedFile.IsEmpty())
		{
			Description = TEXT("No configuration file selected.");
		}
		else if (!Asset->Validate())
		{
			Description = TEXT("Warning: Configuration asset is invalid. Verify the selected configuration file.");
		}
		else
		{
			Description = TEXT("Configuration file: ") + AssociatedFile;
		}

		
	}

	return FText::Format(LOCTEXT("AssetTypeActions_OpenColorIOConfiguration_Description", "{0}"), FText::FromString(MoveTemp(Description)));
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
