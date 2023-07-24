// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraValidationRule.h"
#include "NiagaraValidationRuleSet.generated.h"

/** A set of reusable validation rules. */
UCLASS(BlueprintType)
class NIAGARA_API UNiagaraValidationRuleSet : public UObject
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Validation", Instanced)
	TArray<TObjectPtr<UNiagaraValidationRule>> ValidationRules;
};
