// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStatelessCommon.h"
#include "NiagaraStatelessSpawnInfo.h"
#include "NiagaraDataSet.h"
#include "NiagaraParameterStore.h"
#include "NiagaraSystemEmitterState.h"

#include "Shader.h"

class UNiagaraRendererProperties;
class UNiagaraStatelessEmitterTemplate;
namespace NiagaraStateless
{
	class FSimulationShader;
	class FSpawnInfoShaderParameters;
}

struct FNiagaraStatelessEmitterData
{
	struct FDeleter
	{
		void operator()(FNiagaraStatelessEmitterData* EmitterData) const;
	};

	UE_NONCOPYABLE(FNiagaraStatelessEmitterData);

	FNiagaraStatelessEmitterData() = default;
	~FNiagaraStatelessEmitterData();

	FNiagaraDataSetCompiledData						ParticleDataSetCompiledData;
	TArray<int32>									ComponentOffsets;

	bool											bCanEverExecute = false;
	bool											bDeterministic = false;
	int32											RandomSeed = 0;
	FNiagaraStatelessRangeFloat						LifetimeRange = FNiagaraStatelessRangeFloat(0.0f, 0.0f);
	FBox											FixedBounds = FBox(ForceInit);

	FNiagaraEmitterStateData						EmitterState;
	TArray<FNiagaraStatelessSpawnInfo>				SpawnInfos;

	TArray<TObjectPtr<UNiagaraRendererProperties>>	RendererProperties;

	bool											bModulesHaveRendererBindings = false;
	FNiagaraParameterStore							RendererBindings;			// Contains all bindings for modules & renderers

	const UNiagaraStatelessEmitterTemplate*			EmitterTemplate = nullptr;	// Used to access shader information

	TArray<uint8>									BuiltData;					// Built data, generally allocated by modules if any
	TArray<float>									StaticFloatData;			// Transient data used in build process, do not access directly
	FReadBuffer										StaticFloatBuffer;

	void InitRenderResources();

	TShaderRef<NiagaraStateless::FSimulationShader>	GetShader() const;
	const FShaderParametersMetadata* GetShaderParametersMetadata() const;

	// Calculates the completion age based on the spawn infos / maximum potential lifetime
	//float CalculateCompletionAge(int32 InRandomSeed, TConstArrayView<FNiagaraStatelessRuntimeSpawnInfo> RuntimeSpawnInfos) const;

	// Calculcate the active particle count for all the spawn infos
	// Optionally fills out GPU spawning data into SpawnParameters
	// If no Age is provided we are calculating the maximum number of particles we could ever spawn
	uint32 CalculateActiveParticles(int32 InRandomSeed, TConstArrayView<FNiagaraStatelessRuntimeSpawnInfo> RuntimeSpawnInfos, TOptional<float> Age = TOptional<float>(), NiagaraStateless::FSpawnInfoShaderParameters* SpawnParameters = nullptr) const;
};

using FNiagaraStatelessEmitterDataPtr = TSharedPtr<const FNiagaraStatelessEmitterData, ESPMode::ThreadSafe>;
