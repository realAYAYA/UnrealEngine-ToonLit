// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "CameraModeFactory.generated.h"

class UCameraMode;

/**
 * Implements a factory for UCameraMode objects.
 */
UCLASS(hidecategories=Object)
class UCameraModeFactory : public UFactory
{
	GENERATED_BODY()

	UCameraModeFactory(const FObjectInitializer& ObjectInitializer);

	// UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool ConfigureProperties() override;
	virtual bool ShouldShowInNewMenu() const override;
};

