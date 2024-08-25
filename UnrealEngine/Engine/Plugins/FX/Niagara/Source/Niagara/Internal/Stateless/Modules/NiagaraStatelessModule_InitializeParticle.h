// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraParameterBinding.h"
#include "NiagaraParameterStore.h"

#include "NiagaraStatelessModule_InitializeParticle.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Initialize Particle"))
class UNiagaraStatelessModule_InitializeParticle : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	static const uint32 EInitializeParticleModuleFlag_UniformSpriteSize	= 1 << 0;
	static const uint32 EInitializeParticleModuleFlag_UniformMeshScale	= 1 << 1;

	struct FModuleBuiltData
	{
		uint32							ModuleFlags = 0;
		FUintVector3					InitialPosition = FUintVector3::ZeroValue;
		FNiagaraStatelessRangeFloat		LifetimeRange;
		FNiagaraStatelessRangeColor		ColorRange;
		FNiagaraStatelessRangeFloat		MassRange;
		FNiagaraStatelessRangeVector2	SpriteSizeRange;
		FNiagaraStatelessRangeFloat		SpriteRotationRange;
		FNiagaraStatelessRangeVector3	MeshScaleRange;
		FNiagaraStatelessRangeFloat		RibbonWidthRange;
	};

public:
	using FParameters = NiagaraStateless::FInitializeParticleModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Lifetime"))
	FNiagaraDistributionRangeFloat LifetimeDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultLifetimeValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Color", DisableCurveDistribution))
	FNiagaraDistributionRangeColor ColorDistribution = FNiagaraDistributionRangeColor(FNiagaraStatelessGlobals::GetDefaultColorValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Mass"))
	FNiagaraDistributionRangeFloat MassDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultMassValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Sprite Size"))
	FNiagaraDistributionRangeVector2 SpriteSizeDistribution = FNiagaraDistributionRangeVector2(FNiagaraStatelessGlobals::GetDefaultSpriteSizeValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Sprite Rotation"))
	FNiagaraDistributionRangeFloat SpriteRotationDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultSpriteRotationValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Mesh Scale"))
	FNiagaraDistributionRangeVector3 MeshScaleDistribution = FNiagaraDistributionRangeVector3(FNiagaraStatelessGlobals::GetDefaultScaleValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bWriteRibbonWidth = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Ribbon Width", EditCondition="bWriteRibbonWidth"))
	FNiagaraDistributionRangeFloat RibbonWidthDistribution = FNiagaraDistributionRangeFloat(FNiagaraStatelessGlobals::GetDefaultRibbonWidthValue());

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableCurveDistribution))
	FNiagaraDistributionVector3	InitialPosition = FNiagaraDistributionVector3(FVector3f::ZeroVector);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData			= BuildContext.AllocateBuiltData<FModuleBuiltData>();
		BuiltData->ModuleFlags				 = SpriteSizeDistribution.IsUniform() ? EInitializeParticleModuleFlag_UniformSpriteSize : 0;
		BuiltData->ModuleFlags				|= MeshScaleDistribution.IsUniform() ? EInitializeParticleModuleFlag_UniformMeshScale : 0;

		BuiltData->InitialPosition			= BuildContext.AddDistribution(InitialPosition, true);
		BuiltData->LifetimeRange			= LifetimeDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultLifetimeValue());
		BuiltData->ColorRange				= BuildContext.ConvertDistributionToRange(ColorDistribution, FNiagaraStatelessGlobals::GetDefaultColorValue());
		BuiltData->MassRange				= BuildContext.ConvertDistributionToRange(MassDistribution, FNiagaraStatelessGlobals::GetDefaultMassValue());
		BuiltData->SpriteSizeRange			= BuildContext.ConvertDistributionToRange(SpriteSizeDistribution, FNiagaraStatelessGlobals::GetDefaultSpriteSizeValue());
		BuiltData->SpriteRotationRange		= BuildContext.ConvertDistributionToRange(SpriteRotationDistribution, FNiagaraStatelessGlobals::GetDefaultSpriteRotationValue());
		BuiltData->MeshScaleRange			= BuildContext.ConvertDistributionToRange(MeshScaleDistribution, FNiagaraStatelessGlobals::GetDefaultScaleValue());
		BuiltData->RibbonWidthRange			= BuildContext.ConvertDistributionToRange(RibbonWidthDistribution, FNiagaraStatelessGlobals::GetDefaultRibbonWidthValue());

		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
		PhysicsBuildData.MassRange			= MassDistribution.CalculateRange(FNiagaraStatelessGlobals::GetDefaultMassValue());
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		Parameters->InitializeParticle_ModuleFlags			= ModuleBuiltData->ModuleFlags;
		Parameters->InitializeParticle_InitialPosition		= ModuleBuiltData->InitialPosition;
		SetShaderParameterContext.ConvertRangeToScaleBias(ModuleBuiltData->ColorRange,			Parameters->InitializeParticle_ColorScale, Parameters->InitializeParticle_ColorBias);
		SetShaderParameterContext.ConvertRangeToScaleBias(ModuleBuiltData->SpriteSizeRange,		Parameters->InitializeParticle_SpriteSizeScale, Parameters->InitializeParticle_SpriteSizeBias);
		SetShaderParameterContext.ConvertRangeToScaleBias(ModuleBuiltData->SpriteRotationRange, Parameters->InitializeParticle_SpriteRotationScale, Parameters->InitializeParticle_SpriteRotationBias);
		SetShaderParameterContext.ConvertRangeToScaleBias(ModuleBuiltData->MeshScaleRange,		Parameters->InitializeParticle_MeshScaleScale, Parameters->InitializeParticle_MeshScaleBias);
		SetShaderParameterContext.ConvertRangeToScaleBias(ModuleBuiltData->RibbonWidthRange,	Parameters->InitializeParticle_RibbonWidthScale, Parameters->InitializeParticle_RibbonWidthBias);
	}

#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.UniqueIDVariable);
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.ColorVariable);
		OutVariables.AddUnique(StatelessGlobals.SpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.SpriteRotationVariable);
		OutVariables.AddUnique(StatelessGlobals.ScaleVariable);

		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteSizeVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousSpriteRotationVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousScaleVariable);

		if (bWriteRibbonWidth)
		{
			OutVariables.AddUnique(StatelessGlobals.RibbonWidthVariable);
			OutVariables.AddUnique(StatelessGlobals.PreviousRibbonWidthVariable);
		}
	}
#endif
};
