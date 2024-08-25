// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_ScaleMeshSizeBySpeed.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Mesh Size By Speed"))
class UNiagaraStatelessModule_ScaleMeshSizeBySpeed : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		float			VelocityNorm = 0.0f;
		FUintVector2	ScaleDistribution = FUintVector2::ZeroValue;
	};

public:
	using FParameters = NiagaraStateless::FScaleMeshSizeBySpeedModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float VelocityThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale", DisableRangeDistribution, DisableBindingDistribution))
	FNiagaraDistributionVector3 ScaleDistribution = FNiagaraDistributionVector3(1.0f);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (IsModuleEnabled())
		{
			BuiltData->VelocityNorm = VelocityThreshold > 0.0f ? 1.0f / (VelocityThreshold * VelocityThreshold) : 0.0f;
			if (ScaleDistribution.IsCurve() && ScaleDistribution.Values.Num() > 1)
			{
				BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(ScaleDistribution.Values);
				BuiltData->ScaleDistribution.Y = ScaleDistribution.Values.Num() - 1;
			}
			else
			{
				const FVector3f Values[] = { FVector3f::One(), ScaleDistribution.Values.Num() > 0 ? ScaleDistribution.Values[0] : FVector3f::One() };
				BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(Values);
				BuiltData->ScaleDistribution.Y = 1;
			}
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleMeshSizeBySpeed_VelocityNorm		= ModuleBuiltData->VelocityNorm;
		Parameters->ScaleMeshSizeBySpeed_ScaleDistribution	= ModuleBuiltData->ScaleDistribution;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.ScaleVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousScaleVariable);
	}
#endif
};
