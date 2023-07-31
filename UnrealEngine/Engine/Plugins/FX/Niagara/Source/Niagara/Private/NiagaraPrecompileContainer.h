// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraSystem.h"
#include "NiagaraScript.h"
#include "NiagaraPrecompileContainer.generated.h"


UCLASS()
class NIAGARA_API UNiagaraPrecompileContainer : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraScript>> Scripts;

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> System;
};