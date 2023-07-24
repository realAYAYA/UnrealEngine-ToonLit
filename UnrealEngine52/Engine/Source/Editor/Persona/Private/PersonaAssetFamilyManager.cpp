// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaAssetFamilyManager.h"
#include "AssetRegistry/AssetData.h"
#include "IAssetFamily.h"
#include "PersonaAssetFamily.h"

FPersonaAssetFamilyManager& FPersonaAssetFamilyManager::Get()
{
	static FPersonaAssetFamilyManager TheManager;
	return TheManager;
}

TSharedRef<IAssetFamily> FPersonaAssetFamilyManager::CreatePersonaAssetFamily(const UObject* InAsset)
{
	// compact any invalid entries
	AssetFamilies.RemoveAll([](const TWeakPtr<IAssetFamily>& InAssetFamily) { return !InAssetFamily.IsValid(); });

	// Create new asset family
	TSharedRef<FPersonaAssetFamily> NewAssetFamily = MakeShared<FPersonaAssetFamily>(InAsset);
	NewAssetFamily->Initialize();
	AssetFamilies.Add(NewAssetFamily);
	return NewAssetFamily;
}

void FPersonaAssetFamilyManager::BroadcastAssetFamilyChange()
{
	// Create copy as delegate can modify the AssetFamilies array
	TArray<TWeakPtr<IAssetFamily>> AssetFamiliesCopy = AssetFamilies;
	for (TWeakPtr<IAssetFamily>& AssetFamily : AssetFamiliesCopy)
	{
		if (TSharedPtr<IAssetFamily> PinnedAssetFamily = AssetFamily.Pin())
		{
			PinnedAssetFamily->GetOnAssetFamilyChanged().Broadcast();
		}
	}
}

void FPersonaAssetFamilyManager::RecordAssetOpened(const FAssetData& InAssetData) const
{
	for (const TWeakPtr<IAssetFamily>& AssetFamily : AssetFamilies)
	{
		if (TSharedPtr<IAssetFamily> PinnedAssetFamily = AssetFamily.Pin())
		{
			if(PinnedAssetFamily->IsAssetCompatible(InAssetData))
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				PinnedAssetFamily->RecordAssetOpened(InAssetData);
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}
	}
}