// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_SpriteFacingAndAlignment.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Sprite Facing Alignment"))
class UNiagaraStatelessModule_SpriteFacingAndAlignment : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FParameters = NiagaraStateless::FSpriteFacingAndAlignmentModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bSpriteFacingEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bSpriteAlignmentEnabled = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "bSpriteFacingEnabled"))
	FVector3f	SpriteFacing = FVector3f::XAxisVector;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "bSpriteAlignmentEnabled"))
	FVector3f	SpriteAlignment = FVector3f::YAxisVector;

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->SpriteFacingAndAlignment_SpriteFacing		= SpriteFacing;
		Parameters->SpriteFacingAndAlignment_SpriteAlignment	= SpriteAlignment;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		if (bSpriteFacingEnabled)
		{
			OutVariables.AddUnique(StatelessGlobals.SpriteFacingVariable);
			OutVariables.AddUnique(StatelessGlobals.PreviousSpriteFacingVariable);
		}
		if (bSpriteAlignmentEnabled)
		{
			OutVariables.AddUnique(StatelessGlobals.SpriteAlignmentVariable);
			OutVariables.AddUnique(StatelessGlobals.PreviousSpriteAlignmentVariable);
		}
	}
#endif
};
