// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Factory for LandscapeLayerInfoObject assets
 */

#pragma once

#include "Factories/Factory.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectGlobals.h"

#include "LandscapeLayerInfoObjectFactory.generated.h"

class FFeedbackContext;
class UClass;
class UObject;

UCLASS()
class ULandscapeLayerInfoObjectFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};
