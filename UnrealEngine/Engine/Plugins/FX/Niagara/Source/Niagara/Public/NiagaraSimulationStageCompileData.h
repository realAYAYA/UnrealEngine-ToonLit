// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScriptBase.h"

struct FNiagaraSimulationStageCompilationData
{
	FGuid                           StageGuid;
	FName                           StageName;
	FName                           EnabledBinding;
	FName                           ElementCountXBinding;
	FName                           ElementCountYBinding;
	FName                           ElementCountZBinding;
	uint32                          NumIterations = 1;
	FName                           NumIterationsBinding;
	FName                           IterationSource;
	ENiagaraSimStageExecuteBehavior ExecuteBehavior = ENiagaraSimStageExecuteBehavior::Always;
	mutable bool                    PartialParticleUpdate = false;
	bool                            bParticleIterationStateEnabled = false;
	FName                           ParticleIterationStateBinding;
	FIntPoint                       ParticleIterationStateRange = FIntPoint::ZeroValue;
	bool                            bGpuDispatchForceLinear = false;
	bool                            bOverrideGpuDispatchType = false;
	ENiagaraGpuDispatchType         OverrideGpuDispatchType = ENiagaraGpuDispatchType::OneD;
	bool                            bOverrideGpuDispatchNumThreads = false;
	FIntVector                      OverrideGpuDispatchNumThreads = FIntVector(1, 1, 1);
};
