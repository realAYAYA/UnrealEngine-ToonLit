// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_ScaleColor.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Color"))
class UNiagaraStatelessModule_ScaleColor : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FUintVector3	DistributionParameters = FUintVector3::ZeroValue;
	};

public:
	using FParameters = NiagaraStateless::FScaleColorModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale"))
	FNiagaraDistributionColor ScaleDistribution = FNiagaraDistributionColor(FLinearColor::White);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->DistributionParameters = BuildContext.AddDistribution(ScaleDistribution, IsModuleEnabled());
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleColor_Distribution = ModuleBuiltData->DistributionParameters;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.ColorVariable);
	}
#endif
};
