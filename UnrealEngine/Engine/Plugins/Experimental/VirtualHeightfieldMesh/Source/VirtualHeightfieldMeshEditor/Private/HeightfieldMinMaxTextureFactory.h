// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "HeightfieldMinMaxTextureFactory.generated.h"

/** Factory for UHeightfieldMinMaxTexture */
UCLASS(hidecategories = (Object))
class UHeightfieldMinMaxTextureFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual bool ShouldShowInNewMenu() const { return false; }
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface	
};
