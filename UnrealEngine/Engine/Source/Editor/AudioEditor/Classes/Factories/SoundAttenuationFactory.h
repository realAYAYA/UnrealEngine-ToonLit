// Copyright Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// SoundAttenuationFactory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "SoundAttenuationFactory.generated.h"

class FFeedbackContext;
class UClass;
class UObject;

UCLASS(hidecategories=Object, MinimalAPI)
class USoundAttenuationFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};



