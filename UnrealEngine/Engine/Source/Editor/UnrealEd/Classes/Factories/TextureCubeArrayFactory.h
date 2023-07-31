// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "TextureCubeArrayFactory.generated.h"

class UTextureCube;

/** Factory for creating volume texture */
UCLASS(hidecategories = Object, MinimalAPI)
class UTextureCubeArrayFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UTextureCube>> InitialTextures;

	//~ Begin UFactory Interface
	virtual bool CanCreateNew() const override;
	virtual FText GetDisplayName() const override;
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface

	bool CheckArrayTexturesCompatibility();
};



