// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraComputeExecutionContext.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraStatelessEmitter.h"
#include "Math/RandomStream.h"

class FNiagaraStatelessComputeManager;
class FShaderParametersMetadata;

namespace NiagaraStateless
{
	class FEmitterInstance_RT : public INiagaraComputeDataBufferInterface
	{
	public:
		virtual ~FEmitterInstance_RT() = default;

		FNiagaraStatelessComputeManager*						ComputeManager = nullptr;
		FNiagaraStatelessEmitterDataPtr							EmitterData;

		int32													RandomSeed = 0;
		float													Age = 0.0f;
		ENiagaraExecutionState									ExecutionState = ENiagaraExecutionState::Active;
		TArray<FNiagaraStatelessRuntimeSpawnInfo>				SpawnInfos;
		TUniquePtr<NiagaraStateless::FCommonShaderParameters>	ShaderParameters;

		mutable TOptional<TArray<uint8>>						BindingBufferData;
		mutable FReadBuffer										BindingBuffer;

		// Begin: INiagaraComputeDataBufferInterface
		virtual bool HasTranslucentDataToRender() const override { return false; }
		virtual FNiagaraDataBuffer* GetDataToRender(bool bIsLowLatencyTranslucent) const override;
		// End: INiagaraComputeDataBufferInterface
	};
}

/**
* Implementation of a stateless Niagara particle simulation
*/
class FNiagaraStatelessEmitterInstance final : public FNiagaraEmitterInstance
{
	using Super = FNiagaraEmitterInstance;

	struct FActiveSpawnRate
	{
		float	Rate = 0.0f;
		float	SpawnTime = 0.0f;
	};

public:
	explicit FNiagaraStatelessEmitterInstance(FNiagaraSystemInstance* InParentSystemInstance);
	virtual ~FNiagaraStatelessEmitterInstance();

	// FNiagaraEmitterInstance Impl
	virtual void Init(int32 InEmitterIndex) override;
	virtual void ResetSimulation(bool bKillExisting) override;
	virtual void SetEmitterEnable(bool bNewEnableState) override;
	virtual void OnPooledReuse() override {}
	virtual bool HandleCompletion(bool bForce) override;
	virtual int32 GetNumParticles() const override;
	virtual FNiagaraStatelessEmitterInstance* AsStateless() override { return this; }
	virtual TConstArrayView<UNiagaraRendererProperties*> GetRenderers() const override;
	virtual void BindParameters(bool bExternalOnly) override;
	virtual void UnbindParameters(bool bExternalOnly) override;
	virtual void Tick(float DeltaSeconds) override;
	// FNiagaraEmitterInstance Impl

	int32 GetRandomSeed() const { return RandomSeed; }

	const FNiagaraStatelessEmitterData* GetEmitterData() const { check(EmitterData.IsValid()); return EmitterData.Get(); }

	const NiagaraStateless::FEmitterInstance_RT* GetRenderInstance() const { return RenderThreadDataPtr.Get(); }

private:
	void InitEmitterData();

	void InitEmitterState();
	void TickEmitterState();
	void CalculateBounds();
	void SendRenderData();

	void InitSpawnInfos();
	void InitSpawnInfosForLoop();
	void TickSpawnInfos();

	//-TODO: This can be shared perhaps?
	void SetExecutionStateInternal(ENiagaraExecutionState InExecutionState);

private:
	uint32										bCanEverExecute : 1 = false;
	uint32										bEmitterEnabled_GT : 1 = true;
	uint32										bEmitterEnabled_CNC : 1 = true;
	uint32										bSpawnInfosDirty : 1 = false;

	int32										RandomSeed = 0;
	FRandomStream								RandomStream;

	FNiagaraStatelessEmitterDataPtr				EmitterData;
	TWeakObjectPtr<UNiagaraStatelessEmitter>	WeakStatelessEmitter;

	float										Age = 0.0f;

	uint32										UniqueIndexOffset = 0;
	TArray<FNiagaraStatelessRuntimeSpawnInfo>	SpawnInfos;
	TArray<FActiveSpawnRate>					ActiveSpawnRates;

	int32										LoopCount = 0;
	float										CurrentLoopDuration = 0.0f;
	float										CurrentLoopDelay = 0.0f;
	float										CurrentLoopAgeStart = 0.0f;
	float										CurrentLoopAgeEnd = 0.0f;

	TUniquePtr<NiagaraStateless::FEmitterInstance_RT>	RenderThreadDataPtr;
};
