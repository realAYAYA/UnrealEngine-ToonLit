﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraValidationRule.h"
#include "NiagaraPlatformSet.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraValidationRules.generated.h"

class UNiagaraScript;

namespace NiagaraValidation
{
	NIAGARAEDITOR_API bool HasValidationRules(UNiagaraSystem* NiagaraSystem);
	NIAGARAEDITOR_API void ValidateAllRulesInSystem(TSharedPtr<FNiagaraSystemViewModel> ViewModel, TFunction<void(const FNiagaraValidationResult& Result)> ResultCallback);
}

/** This validation rule ensures that no Systems have a warm up time set. */
UCLASS(Category = "Validation", DisplayName = "No Warmup Time")
class UNiagaraValidationRule_NoWarmupTime : public UNiagaraValidationRule
{
	GENERATED_BODY()
public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule ensures that GPU emitters have fixed bounds set. */
UCLASS(Category = "Validation", DisplayName = "Fixed GPU Bounds Set")
class UNiagaraValidationRule_FixedGPUBoundsSet : public UNiagaraValidationRule
{
	GENERATED_BODY()
public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule can ban the use of certain renderers on all or a subset of platforms. */
UCLASS(Category = "Validation", DisplayName = "Banned Renderers")
class UNiagaraValidationRule_BannedRenderers : public UNiagaraValidationRule
{
	GENERATED_BODY()

public:

	//Platforms this validation rule will apply to.
	UPROPERTY(EditAnywhere, Category = Validation)
	FNiagaraPlatformSet Platforms;

	UPROPERTY(EditAnywhere, Category = Validation)
	TArray<TSubclassOf<UNiagaraRendererProperties>> BannedRenderers;

	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule can ban the use of certain modules on all or a subset of platforms. */
UCLASS(Category = "Validation", DisplayName = "Banned Modules")
class UNiagaraValidationRule_BannedModules : public UNiagaraValidationRule
{
	GENERATED_BODY()

public:

	//Platforms this validation rule will apply to.
	UPROPERTY(EditAnywhere, Category=Validation)
	FNiagaraPlatformSet Platforms;

	UPROPERTY(EditAnywhere, Category = Validation)
	TArray<TObjectPtr<UNiagaraScript>> BannedModules;

	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule can ban the use of certain datainterfaces on all or a subset of platforms. */
UCLASS(Category = "Validation", DisplayName = "Banned DataInterfaces")
class UNiagaraValidationRule_BannedDataInterfaces : public UNiagaraValidationRule
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Validation)
	ENiagaraValidationSeverity Severity = ENiagaraValidationSeverity::Warning;

	UPROPERTY(EditAnywhere, Category = Validation)
	bool bBanOnGpu = true;

	UPROPERTY(EditAnywhere, Category = Validation)
	bool bBanOnCpu = true;

	UPROPERTY(EditAnywhere, Category = Validation)
	FNiagaraPlatformSet Platforms;

	UPROPERTY(EditAnywhere, Category = Validation)
	TArray<TSubclassOf<UNiagaraDataInterface>> BannedDataInterfaces;

	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule can marks this effect type as invalid and so must be changed. Forces a choice of correct Effect Type for an System rather than. Leaving as the default. */
UCLASS(Category = "Validation", DisplayName = "Invalid Effect Type")
class UNiagaraValidationRule_InvalidEffectType : public UNiagaraValidationRule
{
	GENERATED_BODY()
public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule checks for various issue with Large World Coordinates. */
UCLASS(Category = "Validation", DisplayName = "Large World Coordinates")
class UNiagaraValidationRule_LWC : public UNiagaraValidationRule
{
	GENERATED_BODY()
public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule is used by the collision module to check that renderers don't use any opaque or masked materials when depth buffer collisions are used. */
UCLASS(Category = "Validation", DisplayName = "Validate GPU Depth Collision Module")
class UNiagaraValidationRule_NoOpaqueRenderMaterial : public UNiagaraValidationRule
{
	GENERATED_BODY()
public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule is used by modules or effect types to warn that they don't support systems with fixed delta time ticks. */
UCLASS(Category = "Validation", DisplayName = "No Fixed DT Tick Support")
class UNiagaraValidationRule_NoFixedDeltaTime : public UNiagaraValidationRule
{
	GENERATED_BODY()
public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;
};

/** This validation rule can be used to enforce a budget on the number of simulation stages and the iterations that may execute. */
UCLASS(Category = "Validation", DisplayName = "Simulation Stage Budget")
class UNiagaraValidationRule_SimulationStageBudget : public UNiagaraValidationRule
{
	GENERATED_BODY()

public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;

	UPROPERTY(EditAnywhere, Category = Validation)
	bool bMaxSimulationStagesEnabled = false;

	UPROPERTY(EditAnywhere, Category = Validation)
	bool bMaxIterationsPerStageEnabled = false;

	UPROPERTY(EditAnywhere, Category = Validation)
	bool bMaxTotalIterationsEnabled = false;

	/** How do we want to repro the error in the stack */
	UPROPERTY(EditAnywhere, Category = Validation)
	ENiagaraValidationSeverity Severity = ENiagaraValidationSeverity::Warning;

	/** Maximum number of simulation stages allowed, where 0 means no simulation stages. */
	UPROPERTY(EditAnywhere, Category = Validation, meta=(EditCondition="bMaxSimulationStagesEnabled"))
	int32 MaxSimulationStages = 0;

	/**
	Maximum number of iterations a single stage is allowed to execute.
	Note: Can only check across explicit counts, dynamic bindings will be ignored.
	*/
	UPROPERTY(EditAnywhere, Category = Validation, meta = (EditCondition = "bMaxIterationsPerStageEnabled"))
	int32 MaxIterationsPerStage = 1;

	/**
	Maximum total iterations across all the enabled simulation stages.
	Note: Can only check across explicit counts, dynamic bindings will be ignored.
	*/
	UPROPERTY(EditAnywhere, Category = Validation, meta = (EditCondition = "bMaxTotalIterationsEnabled"))
	int32 MaxTotalIterations = 1;
};

/** Validation rule to check that we don't have a tick dependency we don't want.  */
UCLASS(Category = "Validation", DisplayName = "Tick Dependency Check")
class UNiagaraValidationRule_TickDependencyCheck : public UNiagaraValidationRule
{
	GENERATED_BODY()

public:
	virtual void CheckValidity(const FNiagaraValidationContext& Context, TArray<FNiagaraValidationResult>& OutResults) const override;

	/** How do we want to repro the error in the stack */
	UPROPERTY(EditAnywhere, Category = Validation)
	ENiagaraValidationSeverity Severity = ENiagaraValidationSeverity::Info;

	/** Check that the actor component interface isn't adding a tick dependency on the CPU. */
	UPROPERTY(EditAnywhere, Category = Validation)
	bool bCheckActorComponentInterface = true;

	/** Check that the camera data interface isn't adding a tick dependency on the CPU. */
	UPROPERTY(EditAnywhere, Category = Validation)
	bool bCheckCameraDataInterface = true;

	/** Check that the skeletal mesh interface isn't adding a tick dependency on the CPU. */
	UPROPERTY(EditAnywhere, Category = Validation)
	bool bCheckSkeletalMeshInterface = true;
};
