// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_InitialMeshOrientation.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Initial Mesh Orientation"))
class UNiagaraStatelessModule_InitialMeshOrientation : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FParameters = NiagaraStateless::FInitialMeshOrientationModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f	Rotation = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FVector3f	RandomRotationRange = FVector3f(360.0f, 360.0f, 360.0f);

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		if (IsModuleEnabled())
		{
			Parameters->InitialMeshOrientation_Rotation			= Rotation / 360.0f;
			Parameters->InitialMeshOrientation_RandomRangeScale	= RandomRotationRange / 360.0f;
		}
		else
		{
			Parameters->InitialMeshOrientation_Rotation			= FVector3f::ZeroVector;
			Parameters->InitialMeshOrientation_RandomRangeScale	= FVector3f::ZeroVector;
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.MeshOrientationVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousMeshOrientationVariable);
	}
#endif
};
