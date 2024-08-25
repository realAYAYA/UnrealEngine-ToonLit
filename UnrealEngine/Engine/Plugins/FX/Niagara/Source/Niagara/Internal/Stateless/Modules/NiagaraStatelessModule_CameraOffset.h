// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_CameraOffset.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Camera Offset"))
class UNiagaraStatelessModule_CameraOffset : public UNiagaraStatelessModule
{
public:
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FUintVector3	DistributionParameters = FUintVector3::ZeroValue;
	};

public:
	using FParameters = NiagaraStateless::FCameraOffsetModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "CameraOffset"))
	FNiagaraDistributionFloat CameraOffsetDistribution = FNiagaraDistributionFloat(0.0f);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->DistributionParameters = BuildContext.AddDistribution(CameraOffsetDistribution, IsModuleEnabled());
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->CameraOffset_Distribution = ModuleBuiltData->DistributionParameters;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.CameraOffsetVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousCameraOffsetVariable);
	}
#endif
};
