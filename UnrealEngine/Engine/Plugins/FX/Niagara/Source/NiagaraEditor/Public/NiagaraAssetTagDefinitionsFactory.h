// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "NiagaraAssetTagDefinitionsFactory.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraAssetTagDefinitionsFactory : public UFactory
{
	GENERATED_BODY()

	UNiagaraAssetTagDefinitionsFactory();
	
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
};
