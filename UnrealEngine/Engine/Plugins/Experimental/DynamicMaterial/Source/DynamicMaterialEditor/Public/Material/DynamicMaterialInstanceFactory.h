// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "DynamicMaterialInstanceFactory.generated.h"

UCLASS(MinimalAPI)
class UDynamicMaterialInstanceFactory : public UFactory
{
	GENERATED_BODY()

public:
	UDynamicMaterialInstanceFactory();

	//~ Begin UFactory
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual FText GetDisplayName() const override;
	virtual FText GetToolTip() const override;
	//~ End UFactory
};
