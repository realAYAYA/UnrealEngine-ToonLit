// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "RemoteControlPresetFactory.generated.h"

/**
 * Implements a factory for URemoteControlPreset objects.
 */
UCLASS(BlueprintType, hidecategories=Object)
class URemoteControlPresetFactory
	: public UFactory
{
	GENERATED_BODY()
public:
	URemoteControlPresetFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override;
	//~ End UFactory Interface
};
