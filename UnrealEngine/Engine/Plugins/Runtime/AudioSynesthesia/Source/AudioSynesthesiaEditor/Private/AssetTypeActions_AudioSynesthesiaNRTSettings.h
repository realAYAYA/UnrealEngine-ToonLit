// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UAudioSynesthesiaNRTSettings;

/** FAssetTypeActions_AudioSynesthesiaNRTSettings */
class FAssetTypeActions_AudioSynesthesiaNRTSettings : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_AudioSynesthesiaNRTSettings(UAudioSynesthesiaNRTSettings* InSynesthesiaSettings);

	//~ Begin FAssetTypeActions_Base
	virtual bool CanFilter() override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual const TArray<FText>& GetSubMenus() const override;
	//~ End FAssetTypeActions_Base

private:
	UAudioSynesthesiaNRTSettings* SynesthesiaSettings;
};
