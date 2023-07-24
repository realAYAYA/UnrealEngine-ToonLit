// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "UObject/ObjectMacros.h"

#include "MetasoundFactory.generated.h"

class UMetaSoundPatch;
class UMetaSoundSource;

UCLASS(abstract)
class UMetaSoundBaseFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// Set to initialize MetaSound type as Preset,
	// using the provided MetaSound as a Reference
	UPROPERTY(EditAnywhere, Transient, Category = Factory)
	TObjectPtr<UObject> ReferencedMetaSoundObject;
};

UCLASS(hidecategories=Object, MinimalAPI)
class UMetaSoundFactory : public UMetaSoundBaseFactory
{
	GENERATED_UCLASS_BODY()
 
	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InFeedbackContext) override;
	//~ Begin UFactory Interface
 };

UCLASS(hidecategories = Object, MinimalAPI)
class UMetaSoundSourceFactory : public UMetaSoundBaseFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InFeedbackContext) override;
	//~ Begin UFactory Interface
};
