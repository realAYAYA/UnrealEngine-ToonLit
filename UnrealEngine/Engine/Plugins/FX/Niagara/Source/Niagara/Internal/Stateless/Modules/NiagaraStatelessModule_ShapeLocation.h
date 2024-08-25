// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_ShapeLocation.generated.h"

UENUM()
enum class ENSM_ShapePrimitive
{
	Box,
	Cylinder,
	Plane,
	Ring,
	Sphere,
};

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Shape Location"))
class UNiagaraStatelessModule_ShapeLocation : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FParameters = NiagaraStateless::FShapeLocationModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ShowInStackItemHeader, StackItemHeaderAlignment = "Left"))
	ENSM_ShapePrimitive ShapePrimitive = ENSM_ShapePrimitive::Sphere;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box"))
	FVector3f BoxSize = FVector3f(100.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box"))
	bool bBoxSurfaceOnly = false;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box"))
	float BoxSurfaceThicknessMin = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box"))
	float BoxSurfaceThicknessMax = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Plane"))
	FVector2f PlaneSize = FVector2f(100.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Cylinder"))
	float CylinderHeight = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Cylinder"))
	float CylinderRadius = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Cylinder"))
	float CylinderHeightMidpoint = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Ring"))
	float RingRadius = 100.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Ring", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DiscCoverage = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Ring", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RingUDistribution = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(EditConditionHides, EditCondition="ShapePrimitive == ENSM_ShapePrimitive::Sphere"))
	float SphereMin = 0.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(EditConditionHides, EditCondition="ShapePrimitive == ENSM_ShapePrimitive::Sphere"))
	float SphereMax = 100.0f;

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override;

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }

	virtual bool CanDebugDraw() const { return true; }
	virtual void DrawDebug(const FNiagaraStatelessDrawDebugContext& DrawDebugContext) const override;
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
	}
#endif
};
