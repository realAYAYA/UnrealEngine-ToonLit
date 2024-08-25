// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_SubUVAnimation.generated.h"

UENUM()
enum class ENSMSubUVAnimation_Mode
{
	InfiniteLoop,
	Linear,
	Random,
};

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Sub UV Animation"))
class UNiagaraStatelessModule_SubUVAnimation : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FParameters = NiagaraStateless::FSubUVAnimationModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	int32	NumFrames = 16;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool	bStartFrameRangeOverride_Enabled = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool	bEndFrameRangeOverride_Enabled = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "bStartFrameRangeOverride_Enabled"))
	int32	StartFrameRangeOverride = 0;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "bEndFrameRangeOverride_Enabled"))
	int32	EndFrameRangeOverride = 0;
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	ENSMSubUVAnimation_Mode AnimationMode = ENSMSubUVAnimation_Mode::Linear;

	//-Note: Main module has PlaybackMode (Loops / FPS) to choose between loops or frames per second
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "AnimationMode == ENSMSubUVAnimation_Mode::InfiniteLoop"))
	float LoopsPerSecond = 1.0f;

	//-Note: Main module has a few more options
	//UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "AnimationMode == ENSMSubUVAnimation_Mode::Linear"))
	//bool bRandomStartFrame = false;
	//int32 StartFrameOffset = 0;
	//float LoopupIndexScale = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "AnimationMode == ENSMSubUVAnimation_Mode::Random"))
	float RandomChangeInterval = 0.1f;

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		if (IsModuleEnabled())
		{
			const float FrameRangeStart	= bStartFrameRangeOverride_Enabled ? FMath::Clamp(float(StartFrameRangeOverride) / float(NumFrames - 1), 0.0f, 1.0f) : 0.0f;
			const float FrameRangeEnd	= bEndFrameRangeOverride_Enabled   ? FMath::Clamp(float(EndFrameRangeOverride)   / float(NumFrames - 1), 0.0f, 1.0f) : 1.0f;

			Parameters->SubUVAnimation_Mode			= (int)AnimationMode;
			Parameters->SubUVAnimation_NumFrames	= float(NumFrames);
			switch (AnimationMode)
			{
				case ENSMSubUVAnimation_Mode::InfiniteLoop:
					Parameters->SubUVAnimation_InitialFrameScale		= 0.0f;
					Parameters->SubUVAnimation_InitialFrameBias			= 0.0f;
					Parameters->SubUVAnimation_InitialFrameRateChange	= 0.0f;
					Parameters->SubUVAnimation_AnimFrameStart			= FrameRangeStart;
					Parameters->SubUVAnimation_AnimFrameRange			= FrameRangeEnd - FrameRangeStart;
					Parameters->SubUVAnimation_RateScale				= LoopsPerSecond;
					break;

				case ENSMSubUVAnimation_Mode::Linear:
					Parameters->SubUVAnimation_InitialFrameScale		= 0.0f;
					Parameters->SubUVAnimation_InitialFrameBias			= 0.0f;
					Parameters->SubUVAnimation_InitialFrameRateChange	= 0.0f;
					Parameters->SubUVAnimation_AnimFrameStart			= FrameRangeStart;
					Parameters->SubUVAnimation_AnimFrameRange			= FrameRangeEnd - FrameRangeStart;
					Parameters->SubUVAnimation_RateScale				= 1.0f;
					break;

				case ENSMSubUVAnimation_Mode::Random:
					Parameters->SubUVAnimation_InitialFrameScale		= FrameRangeEnd - FrameRangeStart;
					Parameters->SubUVAnimation_InitialFrameBias			= FrameRangeStart;
					Parameters->SubUVAnimation_InitialFrameRateChange	= RandomChangeInterval > 0.0f ? 1.0f / RandomChangeInterval : 0.0f;
					Parameters->SubUVAnimation_AnimFrameStart			= 0.0f;
					Parameters->SubUVAnimation_AnimFrameRange			= 0.0f;
					Parameters->SubUVAnimation_RateScale				= 0.0f;
					break;
			}
		}
		else
		{
			Parameters->SubUVAnimation_Mode						= 0;
			Parameters->SubUVAnimation_InitialFrameScale		= 0.0f;
			Parameters->SubUVAnimation_InitialFrameBias			= 0.0f;
			Parameters->SubUVAnimation_InitialFrameRateChange	= 0.0f;
			Parameters->SubUVAnimation_AnimFrameStart			= 0.0f;
			Parameters->SubUVAnimation_AnimFrameRange			= 0.0f;
			Parameters->SubUVAnimation_RateScale				= 0.0f;
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.SubImageIndexVariable);
	}
#endif
};
