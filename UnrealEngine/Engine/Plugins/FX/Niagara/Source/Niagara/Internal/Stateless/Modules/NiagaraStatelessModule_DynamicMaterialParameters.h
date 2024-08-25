// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_DynamicMaterialParameters.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Dynamic Material Parameters"))
class UNiagaraStatelessModule_DynamicMaterialParameters : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	static constexpr int32 NumParameters			= 1;
	static constexpr int32 NumChannelPerParameter	= 4;

	struct FModuleBuiltData
	{
		FModuleBuiltData()
		{
			for (FUintVector3& v : ParameterDistributions)
			{
				v = FUintVector3::ZeroValue;
			}
		}

		uint32			ChannelMask = 0;
		FUintVector3	ParameterDistributions[NumParameters * NumChannelPerParameter];
	};

public:
	using FParameters = NiagaraStateless::FDynamicMaterialParametersModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	uint32 bParameter0XEnabled : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	uint32 bParameter0YEnabled : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	uint32 bParameter0ZEnabled : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	uint32 bParameter0WEnabled : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Parameter0.X", EditCondition = "bParameter0XEnabled"))
	FNiagaraDistributionFloat Parameter0XDistribution = FNiagaraDistributionFloat(FNiagaraStatelessGlobals::GetDefaultDynamicMaterialParameters0Value().X);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Parameter0.Y", EditCondition = "bParameter0YEnabled"))
	FNiagaraDistributionFloat Parameter0YDistribution = FNiagaraDistributionFloat(FNiagaraStatelessGlobals::GetDefaultDynamicMaterialParameters0Value().Y);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Parameter0.Z", EditCondition = "bParameter0ZEnabled"))
	FNiagaraDistributionFloat Parameter0ZDistribution = FNiagaraDistributionFloat(FNiagaraStatelessGlobals::GetDefaultDynamicMaterialParameters0Value().Z);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Parameter0.W", EditCondition = "bParameter0WEnabled"))
	FNiagaraDistributionFloat Parameter0WDistribution = FNiagaraDistributionFloat(FNiagaraStatelessGlobals::GetDefaultDynamicMaterialParameters0Value().W);

	uint32 GetParameterChannelMask(int32 ParameterIndex) const
	{
		uint32 Mask = 0;
		switch (ParameterIndex)
		{
			case 0:
				Mask |= bParameter0XEnabled ? 1 << 0 : 0;
				Mask |= bParameter0YEnabled ? 1 << 1 : 0;
				Mask |= bParameter0ZEnabled ? 1 << 2 : 0;
				Mask |= bParameter0WEnabled ? 1 << 3 : 0;
				break;
		}
		return Mask;
	}

	const FNiagaraDistributionFloat& GetParameterDistributions(int32 ParameterIndex, int32 ChannelIndex) const
	{
		switch (ParameterIndex)
		{
			case 0:
				switch (ChannelIndex)
				{
					case 0:		return Parameter0XDistribution;
					case 1:		return Parameter0YDistribution;
					case 2:		return Parameter0ZDistribution;
					case 3:		return Parameter0WDistribution;
					default:	checkNoEntry();	return Parameter0XDistribution;
				}
				break;

			default:
				checkNoEntry();
				return Parameter0XDistribution;
		}
	}

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();

		if (IsModuleEnabled())
		{
			for (int32 iParameter=0; iParameter < NumParameters; ++iParameter)
			{
				const uint32 ParameterChannelMask = GetParameterChannelMask(iParameter);
				if (ParameterChannelMask == 0)
				{
					continue;
				}
				BuiltData->ChannelMask |= ParameterChannelMask << (iParameter * 4);
				for (int32 iChannel = 0; iChannel < NumChannelPerParameter; ++iChannel)
				{
					BuiltData->ParameterDistributions[iParameter * NumParameters + iChannel] = BuildContext.AddDistribution(GetParameterDistributions(iParameter, iChannel), true);
				}
			}
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->DynamicMaterialParameters_ChannelMask = ModuleBuiltData->ChannelMask;
		Parameters->DynamicMaterialParameters_Parameter0X = ModuleBuiltData->ParameterDistributions[0];
		Parameters->DynamicMaterialParameters_Parameter0Y = ModuleBuiltData->ParameterDistributions[1];
		Parameters->DynamicMaterialParameters_Parameter0Z = ModuleBuiltData->ParameterDistributions[2];
		Parameters->DynamicMaterialParameters_Parameter0W = ModuleBuiltData->ParameterDistributions[3];
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		//-TODO: Channel masking, etc
		if (GetParameterChannelMask(0) != 0)
		{
			OutVariables.AddUnique(StatelessGlobals.DynamicMaterialParameters0Variable);
		}
	}
#endif
};
