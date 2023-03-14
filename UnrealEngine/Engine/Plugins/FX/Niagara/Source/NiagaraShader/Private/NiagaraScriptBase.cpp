// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraScriptBase)

UNiagaraScriptBase::UNiagaraScriptBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FSimulationStageMetaData::FSimulationStageMetaData()
	: bWritesParticles(0)
	, bPartialParticleUpdate(0)
	, bParticleIterationStateEnabled(0)
	, bOverrideElementCount(0)
	, GpuDispatchType(ENiagaraGpuDispatchType::OneD)
	, GpuDispatchNumThreads(0, 0, 0)
{
}

