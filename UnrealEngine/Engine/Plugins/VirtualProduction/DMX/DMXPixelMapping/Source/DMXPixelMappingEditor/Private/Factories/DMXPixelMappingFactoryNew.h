// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "DMXPixelMappingFactoryNew.generated.h"

UCLASS(MinimalAPI)
class UDMXPixelMappingFactoryNew
	: public UFactory
{
	GENERATED_BODY()

public:
	UDMXPixelMappingFactoryNew();

private:
	//~ Begin UFactory Interface
	virtual FName GetNewAssetThumbnailOverride() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;
	//~ End UFactory Interface
};