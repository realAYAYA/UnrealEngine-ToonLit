// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraStatelessModule_AddVelocity.generated.h"

UENUM()
enum class ENSM_VelocityType
{
	Linear,			// Velocity Min / Max -> Velocity Speed Scale
	FromPoint,		// Min / Max / Origin
	InCone,			// Max, Min, Cone| Cone Axis, Cone Angle, Inner Cone
};

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Add Velocity"))
class UNiagaraStatelessModule_AddVelocity : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Parameters")
	ENSM_VelocityType VelocityType = ENSM_VelocityType::Linear;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Velocity", DisableUniformDistribution, DisableBindingDistribution, EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::Linear"))
	FNiagaraDistributionRangeVector3 LinearVelocityDistribution = FNiagaraDistributionRangeVector3(FVector3f(0.0f, 0.0f, 100.0f));
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Velocity Scale", EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::Linear"))
	float LinearVelocityScale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Cone Velocity", DisableBindingDistribution, EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::InCone"))
	FNiagaraDistributionRangeFloat ConeVelocityDistribution = FNiagaraDistributionRangeFloat(500.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::InCone"))
	FRotator ConeRotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::InCone", ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0"))
	float ConeAngle = 45.0f;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::InCone", ClampMin = "0.0", ClampMax = "360.0", UIMin = "0.0", UIMax = "360.0"))
	float InnerCone = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Point Velocity", DisableBindingDistribution, EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::FromPoint"))
	FNiagaraDistributionRangeFloat PointVelocityDistribution = FNiagaraDistributionRangeFloat(100.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "VelocityType == ENSM_VelocityType::FromPoint"))
	FVector3f PointOrigin = FVector3f::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "VelocityType == ENSM_VelocityType::InCone"))
	bool bSpeedFalloffFromConeAxisEnabled = false;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditCondition = "bSpeedFalloffFromConeAxisEnabled && VelocityType == ENSM_VelocityType::InCone", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SpeedFalloffFromConeAxis = 0.0f;

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override;

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }

	virtual bool CanDebugDraw() const { return true; }
	virtual void DrawDebug(const FNiagaraStatelessDrawDebugContext& DrawDebugContext) const override;
#endif
};
