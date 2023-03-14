// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_AudioSynesthesiaNRTSettings.h"
#include "AssetTypeCategories.h"
#include "AudioSynesthesiaNRT.h"

FAssetTypeActions_AudioSynesthesiaNRTSettings::FAssetTypeActions_AudioSynesthesiaNRTSettings(UAudioSynesthesiaNRTSettings* InSynesthesiaSettings)
	: SynesthesiaSettings(InSynesthesiaSettings)
{
}

bool FAssetTypeActions_AudioSynesthesiaNRTSettings::CanFilter()
{
	// If no paired settings pointer provided, we filter as its a base class.
	// Otherwise, we do not as this bloats the filter list.
	return SynesthesiaSettings == nullptr;
}

FText FAssetTypeActions_AudioSynesthesiaNRTSettings::GetName() const
{
	if (SynesthesiaSettings)
	{
		const FText AssetActionName = SynesthesiaSettings->GetAssetActionName();
		if (AssetActionName.IsEmpty())
		{
			FString ClassName;
			SynesthesiaSettings->GetClass()->GetName(ClassName);
			return FText::FromString(ClassName);
		}

		return AssetActionName;
	}

	static const FText DefaultAssetActionName = NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetSoundSynesthesiaNRTSettings", "Synesthesia NRT Settings");
	return DefaultAssetActionName;
}

FColor FAssetTypeActions_AudioSynesthesiaNRTSettings::GetTypeColor() const 
{
	if (!SynesthesiaSettings)
	{
		return FColor(100.0f, 100.0f, 100.0f);
	}
	return SynesthesiaSettings->GetTypeColor();
}

const TArray<FText>& FAssetTypeActions_AudioSynesthesiaNRTSettings::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetSoundAnalysisSubmenu", "Analysis")
	};

	if (!SynesthesiaSettings)
	{
		return SubMenus;
	}

	return SynesthesiaSettings->GetAssetActionSubmenus();
}

UClass* FAssetTypeActions_AudioSynesthesiaNRTSettings::GetSupportedClass() const
{
	if (SynesthesiaSettings)
	{
		if (UClass* SupportedClass = SynesthesiaSettings->GetSupportedClass())
		{
			return SupportedClass;
		}

		return SynesthesiaSettings->GetClass();
	}

	return UAudioSynesthesiaNRTSettings::StaticClass();
}

uint32 FAssetTypeActions_AudioSynesthesiaNRTSettings::GetCategories() 
{
	return EAssetTypeCategories::Sounds; 
}

