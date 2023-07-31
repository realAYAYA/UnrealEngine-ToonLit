//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#pragma once

#include "Factories/Factory.h"
#include "AssetTypeActions_Base.h"
#include "ResonanceAudioReverbPluginPresetFactory.generated.h"

class FAssetTypeActions_ResonanceAudioReverbPluginPreset : public FAssetTypeActions_Base
{
public:
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual const TArray<FText>& GetSubMenus() const override;
};

UCLASS(MinimalAPI, hidecategories = Object)
class UResonanceAudioReverbPluginPresetFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
};
