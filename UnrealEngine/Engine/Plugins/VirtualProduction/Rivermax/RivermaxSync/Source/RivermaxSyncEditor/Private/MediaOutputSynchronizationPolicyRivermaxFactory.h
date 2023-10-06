// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"

#include "MediaOutputSynchronizationPolicyRivermaxFactory.generated.h"


/**
 * Factory for UMediaOutputSynchronizationPolicyRivermax objects.
 */
UCLASS(hidecategories = Object)
class UMediaOutputSynchronizationPolicyRivermaxFactory
	: public UFactory
{
	GENERATED_UCLASS_BODY()

public:
	//~ UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	virtual bool ShouldShowInNewMenu() const override;
};
