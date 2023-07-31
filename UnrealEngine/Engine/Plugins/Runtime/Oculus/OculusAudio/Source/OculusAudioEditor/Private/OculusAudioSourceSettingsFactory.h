// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioExtensionPlugin.h"
#include "Factories/Factory.h"
#include "AssetTypeActions_Base.h"
#include "AssetToolsModule.h"
#include "OculusAudioSourceSettingsFactory.generated.h"

class FAssetTypeActions_OculusAudioSourceSettings : public FAssetTypeActions_Base
{
public:
    virtual FText GetName() const override;
    virtual FColor GetTypeColor() const override;
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override;
    virtual uint32 GetCategories() override;
};

UCLASS(hidecategories = Object, MinimalAPI)
class UOculusAudioSourceSettingsFactory : public UFactory
{
    GENERATED_UCLASS_BODY()

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
		FFeedbackContext* Warn) override;

	virtual uint32 GetMenuCategories() const override;
};
