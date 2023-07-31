// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Sound/DialogueVoice.h"

#define LOCTEXT_NAMESPACE "Dialogue"

class FAssetTypeActions_DialogueVoice : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueVoice", "Dialogue Voice"); }
	virtual FColor GetTypeColor() const override { return FColor(97, 85, 212); }
	virtual UClass* GetSupportedClass() const override { return UDialogueVoice::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	virtual const TArray<FText>& GetSubMenus() const override
	{
		static const TArray<FText> SubMenus
		{
			FText(LOCTEXT("AssetDialogueSubMenu", "Dialogue"))
		};

		return SubMenus;
	}
	virtual bool CanFilter() override { return true; }
};

#undef LOCTEXT_NAMESPACE