// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_RotateAroundPoint.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Rotate Around Point"))
class UNiagaraStatelessModule_RotateAroundPoint : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FParameters = NiagaraStateless::FRotateAroundPointModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float RateMin = 360.f;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float RateMax = 360.f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float RadiusMin = 50.f;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float RadiusMax = 50.f;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	float InitialPhaseMin = 0.f;
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float InitialPhaseMax = 0.f;

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		if (IsModuleEnabled())
		{
			Parameters->RotateAroundPoint_RateScale			= FMath::DegreesToRadians(RateMax - RateMin);
			Parameters->RotateAroundPoint_RateBias			= FMath::DegreesToRadians(RateMin);
			Parameters->RotateAroundPoint_RadiusScale		= RadiusMax - RadiusMin;
			Parameters->RotateAroundPoint_RadiusBias		= RadiusMin;
			Parameters->RotateAroundPoint_InitialPhaseScale	= InitialPhaseMax - InitialPhaseMin;
			Parameters->RotateAroundPoint_InitialPhaseBias	= InitialPhaseMin;
		}
		else
		{
			Parameters->RotateAroundPoint_RateScale			= 0.0f;
			Parameters->RotateAroundPoint_RateBias			= 0.0f;
			Parameters->RotateAroundPoint_RadiusScale		= 0.0f;
			Parameters->RotateAroundPoint_RadiusBias		= 0.0f;
			Parameters->RotateAroundPoint_InitialPhaseScale	= 0.0f;
			Parameters->RotateAroundPoint_InitialPhaseBias	= 0.0f;
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
	}
#endif
};
