// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraParameterBinding.h"

#include "NiagaraStatelessModule_ScaleMeshSize.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Mesh Size"))
class UNiagaraStatelessModule_ScaleMeshSize : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FUintVector3	DistributionParameters = FUintVector3::ZeroValue;
		FVector3f		CurveScale = FVector3f::OneVector;
		int32			CurveScaleOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FScaleMeshSizeModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale"))
	FNiagaraDistributionVector3 ScaleDistribution = FNiagaraDistributionVector3(FVector3f::OneVector);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "UseScaleCurveRange()"))
	FNiagaraParameterBindingWithValue ScaleCurveRange;

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();

	#if WITH_EDITORONLY_DATA
		if (HasAnyFlags(RF_ClassDefaultObject) == false)
		{
			ScaleCurveRange.SetUsage(ENiagaraParameterBindingUsage::NotParticle);
			ScaleCurveRange.SetAllowedTypeDefinitions({ FNiagaraTypeDefinition::GetVec3Def() });
			ScaleCurveRange.SetDefaultParameter(FNiagaraTypeDefinition::GetVec3Def(), FVector3f::OneVector);
		}
	#endif
	}

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->DistributionParameters = BuildContext.AddDistribution(ScaleDistribution, IsModuleEnabled());
		if (IsModuleEnabled() && UseScaleCurveRange())
		{
			BuiltData->CurveScaleOffset = BuildContext.AddRendererBinding(ScaleCurveRange.ResolvedParameter);
			BuiltData->CurveScale = ScaleCurveRange.GetDefaultValue<FVector3f>();
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters						= SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleMeshSize_Distribution		= ModuleBuiltData->DistributionParameters;
		Parameters->ScaleMeshSize_CurveScale		= ModuleBuiltData->CurveScale;
		Parameters->ScaleMeshSize_CurveScaleOffset	= ModuleBuiltData->CurveScaleOffset;
	}

	UFUNCTION()
	bool UseScaleCurveRange() const { return ScaleDistribution.IsCurve(); }

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
