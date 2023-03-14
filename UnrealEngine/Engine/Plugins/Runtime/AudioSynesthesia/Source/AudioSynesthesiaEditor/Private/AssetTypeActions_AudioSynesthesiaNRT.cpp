// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_AudioSynesthesiaNRT.h"
#include "AssetTypeCategories.h"
#include "AudioSynesthesiaNRT.h"

FAssetTypeActions_AudioSynesthesiaNRT::FAssetTypeActions_AudioSynesthesiaNRT(UAudioSynesthesiaNRT* InSynesthesia)
	: Synesthesia(InSynesthesia)
{
}

bool FAssetTypeActions_AudioSynesthesiaNRT::CanFilter()
{
	// If no paired synesthesia pointer provided, we filter as its a base class.
	// Otherwise, we do not as this bloats the filter list.
	return Synesthesia == nullptr;
}

FText FAssetTypeActions_AudioSynesthesiaNRT::GetName() const
{
	if (Synesthesia)
	{
		const FText AssetActionName = Synesthesia->GetAssetActionName();
		if (AssetActionName.IsEmpty())
		{
			FString ClassName;
			Synesthesia->GetClass()->GetName(ClassName);
			return FText::FromString(ClassName);
		}

		return AssetActionName;
	}

	static const FText DefaultAssetActionName = NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetSoundSynesthesiaNRT", "Synesthesia NRT");
	return DefaultAssetActionName;
}

const TArray<FText>& FAssetTypeActions_AudioSynesthesiaNRT::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AssetSoundAnalysisSubmenu", "Analysis")
	};

	if (!Synesthesia)
	{
		return SubMenus;
	}

	return Synesthesia->GetAssetActionSubmenus();
}

FColor FAssetTypeActions_AudioSynesthesiaNRT::GetTypeColor() const 
{
	if (!Synesthesia)
	{
		return FColor(200.0f, 150.0f, 200.0f);
	}
	return Synesthesia->GetTypeColor(); 
}

UClass* FAssetTypeActions_AudioSynesthesiaNRT::GetSupportedClass() const
{
	if (Synesthesia)
	{
		if (UClass* SupportedClass = Synesthesia->GetSupportedClass())
		{
			return SupportedClass;
		}

		return Synesthesia->GetClass();
	}

	return UAudioSynesthesiaNRT::StaticClass();
}

uint32 FAssetTypeActions_AudioSynesthesiaNRT::GetCategories() 
{
	return EAssetTypeCategories::Sounds; 
}
