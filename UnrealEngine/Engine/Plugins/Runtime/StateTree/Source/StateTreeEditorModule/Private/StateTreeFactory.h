// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "StateTreeFactory.generated.h"

class UStateTreeSchema;

/**
 * Factory for UStateTree
 */

UCLASS()
class STATETREEEDITORMODULE_API UStateTreeFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	// UFactory interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface

protected:
	
	UPROPERTY(Transient)
	TObjectPtr<UClass> StateTreeSchemaClass;
};
