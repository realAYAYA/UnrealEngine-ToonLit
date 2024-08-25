// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessCommon.h"
#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_ScaleSpriteSize.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Sprite Size"))
class UNiagaraStatelessModule_ScaleSpriteSize : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FUintVector3	DistributionParameters = FUintVector3::ZeroValue;
		FVector2f		CurveScale = FVector2f::One();
		int32			CurveScaleOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FScaleSpriteSizeModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale"))
	FNiagaraDistributionVector2 ScaleDistribution = FNiagaraDistributionVector2(FVector2f::One());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "UseScaleCurveRange()"))
	FNiagaraParameterBindingWithValue ScaleCurveRange;

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();

	#if WITH_EDITORONLY_DATA
		if (HasAnyFlags(RF_ClassDefaultObject) == false)
		{
			ScaleCurveRange.SetUsage(ENiagaraParameterBindingUsage::NotParticle);
			ScaleCurveRange.SetAllowedTypeDefinitions({ FNiagaraTypeDefinition::GetVec2Def() });
			ScaleCurveRange.SetDefaultParameter(FNiagaraTypeDefinition::GetVec2Def(), FVector2f::One());
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
			BuiltData->CurveScale = ScaleCurveRange.GetDefaultValue<FVector2f>();
		}
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters						= SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleSpriteSize_Distribution	= ModuleBuiltData->DistributionParameters;
		Parameters->ScaleSpriteSize_CurveScale		= ModuleBuiltData->CurveScale;
		Parameters->ScaleSpriteSize_CurveScaleOffset= ModuleBuiltData->CurveScaleOffset;
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
		OutVariables.AddUnique(StatelessGlobals.SpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteSizeVariable);
	}
#endif
};
