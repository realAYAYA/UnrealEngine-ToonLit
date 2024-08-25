// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_CalculateAccurateVelocity.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Rotate Around Point"))
class UNiagaraStatelessModule_CalculateAccurateVelocity : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.VelocityVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousVelocityVariable);
	}
#endif
};
