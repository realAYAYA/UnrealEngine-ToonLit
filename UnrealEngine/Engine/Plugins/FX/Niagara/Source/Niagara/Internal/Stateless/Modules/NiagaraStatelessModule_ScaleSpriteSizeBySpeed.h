// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_ScaleSpriteSizeBySpeed.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Sprite Size By Speed"))
class UNiagaraStatelessModule_ScaleSpriteSizeBySpeed : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		float			VelocityNorm = 0.0f;
		FUintVector2	ScaleDistribution = FUintVector2::ZeroValue;
	};

public:
	using FParameters = NiagaraStateless::FScaleSpriteSizeBySpeedModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float VelocityThreshold = 1000.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale", DisableRangeDistribution, DisableBindingDistribution))
	FNiagaraDistributionVector2 ScaleDistribution = FNiagaraDistributionVector2(1.0f);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (IsModuleEnabled())
		{
			BuiltData->VelocityNorm			= VelocityThreshold > 0.0f ? 1.0f / (VelocityThreshold * VelocityThreshold) : 0.0f;
			if (ScaleDistribution.IsCurve() && ScaleDistribution.Values.Num() > 1)
			{
				BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(ScaleDistribution.Values);
				BuiltData->ScaleDistribution.Y = ScaleDistribution.Values.Num() - 1;
			}
			else
			{
				const FVector2f Values[] = { FVector2f::One(), ScaleDistribution.Values.Num() > 0 ? ScaleDistribution.Values[0] : FVector2f::One() };
				BuiltData->ScaleDistribution.X = BuildContext.AddStaticData(Values);
				BuiltData->ScaleDistribution.Y = 1;
			}
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleSpriteSizeBySpeed_VelocityNorm			= ModuleBuiltData->VelocityNorm;
		Parameters->ScaleSpriteSizeBySpeed_ScaleDistribution	= ModuleBuiltData->ScaleDistribution;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.SpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteSizeVariable);
	}
#endif
};
