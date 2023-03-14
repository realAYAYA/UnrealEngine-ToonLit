// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_AudioSynesthesiaSettings.h"
#include "AssetTypeCategories.h"
#include "AudioSynesthesia.h"

FAssetTypeActions_AudioSynesthesiaSettings::FAssetTypeActions_AudioSynesthesiaSettings(UAudioSynesthesiaSettings* InSynesthesiaSettings)
	: SynesthesiaSettings(InSynesthesiaSettings)
{
}

bool FAssetTypeActions_AudioSynesthesiaSettings::CanFilter()
{
	// If no paired settings pointer provided, we filter as its a base class.
	// Otherwise, we do not as this bloats the filter list.
	return SynesthesiaSettings == nullptr;
}

FText FAssetTypeActions_AudioSynesthesiaSettings::GetName() const
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

	static const FText DefaultAssetActionName = NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetSoundSynesthesiaSettings", "Synesthesia Settings");
	return DefaultAssetActionName;
}

FColor FAssetTypeActions_AudioSynesthesiaSettings::GetTypeColor() const 
{
	if (!SynesthesiaSettings)
	{
		return FColor(100.0f, 50.0f, 100.0f);
	}
	return SynesthesiaSettings->GetTypeColor();
}

const TArray<FText>& FAssetTypeActions_AudioSynesthesiaSettings::GetSubMenus() const
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

UClass* FAssetTypeActions_AudioSynesthesiaSettings::GetSupportedClass() const
{
	if (SynesthesiaSettings)
	{
		if (UClass* SupportedClass = SynesthesiaSettings->GetSupportedClass())
		{
			return SupportedClass;
		}

		return SynesthesiaSettings->GetClass();
	}

	return UAudioSynesthesiaSettings::StaticClass();
}

uint32 FAssetTypeActions_AudioSynesthesiaSettings::GetCategories() 
{
	return EAssetTypeCategories::Sounds; 
}

