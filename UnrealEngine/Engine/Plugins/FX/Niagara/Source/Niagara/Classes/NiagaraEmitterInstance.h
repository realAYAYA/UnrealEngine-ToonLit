// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraEmitterInstance.h: Niagara emitter simulation class
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptExecutionContext.h"

class FNiagaraSystemInstance;
struct FNiagaraEmitterHandle;
class UNiagaraParameterCollection;
class UNiagaraParameterCollectionInstance;
class FNiagaraGpuComputeDispatchInterface;
struct FNiagaraEmitterCompiledData;

/**
* A Niagara particle simulation.
*/
class FNiagaraEmitterInstance
{
	friend class UNiagaraSimCache;

private:
	struct FEventInstanceData
	{
		TArray<FNiagaraScriptExecutionContext> EventExecContexts;
		TArray<FNiagaraParameterDirectBinding<int32>> EventExecCountBindings;

		TArray<FNiagaraDataSet*> UpdateScriptEventDataSets;
		TArray<FNiagaraDataSet*> SpawnScriptEventDataSets;

		TArray<bool> UpdateEventGeneratorIsSharedByIndex;
		TArray<bool> SpawnEventGeneratorIsSharedByIndex;

		/** Data required for handling events. */
		TArray<FNiagaraEventHandlingInfo> EventHandlingInfo;
		int32 EventSpawnTotal = 0;
	};

public:
	explicit FNiagaraEmitterInstance(FNiagaraSystemInstance* InParentSystemInstance);
	virtual ~FNiagaraEmitterInstance();

	void Init(int32 InEmitterIdx, FNiagaraSystemInstanceID SystemInstanceID);

	void ResetSimulation(bool bKillExisting = true);

	void OnPooledReuse();

	void DirtyDataInterfaces();

	/** Replaces the binding for a single parameter collection instance. If for example the component begins to override the global instance. */
	//void RebindParameterCollection(UNiagaraParameterCollectionInstance* OldInstance, UNiagaraParameterCollectionInstance* NewInstance);
	void BindParameters(bool bExternalOnly);
	void UnbindParameters(bool bExternalOnly);

	bool IsAllowedToExecute() const;

#if WITH_EDITOR
	void TickRapidIterationParameters();
#endif

	void PreTick();
	void Tick(float DeltaSeconds);
	void PostTick();
	bool HandleCompletion(bool bForce = false);

	bool RequiresPersistentIDs() const;

	FORCEINLINE bool ShouldTick()const { return ExecutionState == ENiagaraExecutionState::Active || GetNumParticles() > 0; }

	uint32 CalculateEventSpawnCount(const FNiagaraEventScriptProperties &EventHandlerProps, TArray<int32, TInlineAllocator<16>>& EventSpawnCounts, FNiagaraDataSet *EventSet);

#if WITH_EDITOR
	/** Potentially reads back data from the GPU which will introduce a stall and should only be used for debug purposes. */
	NIAGARA_API void CalculateFixedBounds(const FTransform& ToWorldSpace);
#endif
	
	bool GetBoundRendererValue_GT(const FNiagaraVariableBase& InBaseVar, const FNiagaraVariableBase& InSubVar, void* OutValueData) const;

	FNiagaraDataSet& GetData()const { return *ParticleDataSet; }

	FORCEINLINE bool IsDisabled()const { return ExecutionState == ENiagaraExecutionState::Disabled; }
	FORCEINLINE bool IsInactive()const { return ExecutionState == ENiagaraExecutionState::Inactive; }
	FORCEINLINE bool IsComplete()const { return ExecutionState == ENiagaraExecutionState::Complete || ExecutionState == ENiagaraExecutionState::Disabled; }

private:
	NIAGARA_API int32 GetNumParticlesGPUInternal() const;
public:
	FORCEINLINE int32 GetNumParticles() const
	{
		// Note: For GPU simulations the data is latent we can not read directly from GetCurrentData() until we have passed a fence
		// which guarantees that at least one tick has occurred inside the batcher.  The count will still technically be incorrect
		// but hopefully adequate for a system script update.
		if (GPUExecContext)
		{
			return GetNumParticlesGPUInternal();
		}

		if ( ParticleDataSet->GetCurrentData() )
		{
			return ParticleDataSet->GetCurrentData()->GetNumInstances();
		}
		return 0;
	}

	FORCEINLINE int32 GetTotalSpawnedParticles()const { return TotalSpawnedParticles; }
	FORCEINLINE const FNiagaraEmitterScalabilitySettings& GetScalabilitySettings()const { return GetCachedEmitterData()->GetScalabilitySettings(); }

	NIAGARA_API const FNiagaraEmitterHandle& GetEmitterHandle() const;

	FNiagaraSystemInstance* GetParentSystemInstance()const { return ParentSystemInstance; }

	float NIAGARA_API GetTotalCPUTimeMS();
	int64 NIAGARA_API GetTotalBytesUsed();

	ENiagaraExecutionState NIAGARA_API GetExecutionState() { return ExecutionState; }
	void NIAGARA_API SetExecutionState(ENiagaraExecutionState InState);

	bool AreBoundsDynamic() const { return bCachedBoundsDynamic; }
	FBox GetBounds() const { return CachedBounds; }

	FNiagaraScriptExecutionContext& GetSpawnExecutionContext() { return SpawnExecContext; }
	FNiagaraScriptExecutionContext& GetUpdateExecutionContext() { return UpdateExecContext; }
	TArrayView<FNiagaraScriptExecutionContext> GetEventExecutionContexts();

	FORCEINLINE FName GetCachedIDName()const { return CachedIDName; }
	FORCEINLINE FVersionedNiagaraEmitter GetCachedEmitter()const { return CachedEmitter; }
	FORCEINLINE FVersionedNiagaraEmitterData* GetCachedEmitterData()const { return CachedEmitter.GetEmitterData(); }

	TArray<FNiagaraSpawnInfo>& GetSpawnInfo() { return SpawnInfos; }

	NIAGARA_API bool IsReadyToRun() const;

	void Dump()const;

	bool WaitForDebugInfo();

	FNiagaraComputeExecutionContext* GetGPUContext()const
	{
		return GPUExecContext;
	}

	void SetSystemFixedBoundsOverride(FBox SystemFixedBounds);
	FORCEINLINE void SetFixedBounds(const FBox& InLocalBounds)
	{
		FRWScopeLock ScopeLock(FixedBoundsGuard, SLT_Write);
		FixedBounds = InLocalBounds;
	}
	FBox GetFixedBounds() const;

	UObject* FindBinding(const FNiagaraVariable& InVariable) const;
	UNiagaraDataInterface* FindDataInterface(const FNiagaraVariable& InVariable) const;

	bool HasTicked() const { return TickCount > 0;  }

	const FNiagaraParameterStore& GetRendererBoundVariables() const { return RendererBindings; }
	FNiagaraParameterStore& GetRendererBoundVariables() { return RendererBindings; }

	int32 GetRandomSeed() const { return RandomSeed; }
	int32 GetInstanceSeed() const { return InstanceSeed; }

	void SetParticleComponentActive(FObjectKey ComponentKey, int32 ParticleID) const;

	bool IsParticleComponentActive(FObjectKey ComponentKey, int32 ParticleID) const;

private:
	void CheckForErrors();

	void BuildConstantBufferTable(
		const FNiagaraScriptExecutionContext& ExecContext,
		FScriptExecutionConstantBufferTable& ConstantBufferTable) const;

	/** Generate emitter bounds */
	FBox InternalCalculateDynamicBounds(int32 ParticleCount) const;

	/** Array of all spawn info driven by our owning emitter script. */
	TArray<FNiagaraSpawnInfo> SpawnInfos;

	FNiagaraScriptExecutionContext SpawnExecContext;
	FNiagaraScriptExecutionContext UpdateExecContext;
	FNiagaraComputeExecutionContext* GPUExecContext = nullptr;

	FNiagaraParameterDirectBinding<float> SpawnIntervalBinding;
	FNiagaraParameterDirectBinding<float> InterpSpawnStartBinding;
	FNiagaraParameterDirectBinding<int32> SpawnGroupBinding;

	FNiagaraParameterDirectBinding<int32> SpawnExecCountBinding;
	FNiagaraParameterDirectBinding<int32> UpdateExecCountBinding;

	TSharedPtr<const FNiagaraEmitterCompiledData> CachedEmitterCompiledData;
	FNiagaraParameterStore RendererBindings;

	TUniquePtr<FEventInstanceData> EventInstanceData;

	/** A parameter store which contains the data interfaces parameters which were defined by the scripts. */
	FNiagaraParameterStore ScriptDefinedDataInterfaceParameters;

	/* Are the cached emitter bounds dynamic */
	bool bCachedBoundsDynamic = false;

	/* Emitter bounds */
	FBox CachedBounds;

	/** Optional user or VM specified bounds. */
	mutable FRWLock FixedBoundsGuard;
	FBox FixedBounds;

	/** Cached fixed bounds of the parent system which override this Emitter Instances bounds if set. Whenever we initialize the owning SystemInstance we will reconstruct this
	 ** EmitterInstance and the cached bounds will be unset. */
	FBox CachedSystemFixedBounds;

	FNiagaraSystemInstanceID OwnerSystemInstanceID = 0;

	FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = nullptr;

	/** particle simulation data. Must be a shared ref as various things on the RT can have direct ref to it. */
	FNiagaraDataSet* ParticleDataSet = nullptr;

	FNiagaraSystemInstance *ParentSystemInstance = nullptr;

	/** Raw pointer to the emitter that we're instanced from. Raw ptr should be safe here as we check for the validity of the system and it's emitters higher up before any ticking. */
	FVersionedNiagaraEmitter CachedEmitter;
	FName CachedIDName;

	/** The index of our emitter in our parent system instance. */
	int32 EmitterIdx = INDEX_NONE;

	/* The age of the emitter*/
	float EmitterAge = 0.0f;

	int32 RandomSeed = 0;
	int32 InstanceSeed = FGenericPlatformMath::Rand();
	int32 TickCount = 0;

	int32 TotalSpawnedParticles = 0;
	
	/* Cycles taken to process the tick. */
	uint32 CPUTimeCycles = 0;

	uint32 MaxRuntimeAllocation = 0;

	int32 MaxAllocationCount = 0;
	int32 MinOverallocation = -1;
	int32 ReallocationCount = 0;

	uint32 MaxInstanceCount = 0;

	/* Emitter tick state */
	ENiagaraExecutionState ExecutionState = ENiagaraExecutionState::Inactive;

	/** Typical resets must be deferred until the tick as the RT could still be using the current buffer. */
	uint32 bResetPending : 1;
	/** Allows event spawn to be combined into a single spawn.  This is only safe when not using things like ExecIndex(). */
	uint32 bCombineEventSpawn : 1;

	// This is used to keep track which particles have spawned a component. This is needed when the bOnlyCreateComponentsOnParticleSpawn flag is set in the renderer.
	// Without this bookkeeping, the particles would lose their components when the render state is recreated or the visibility tag flips them off and on again.
	mutable TMap<FObjectKey, TSet<int32>> ParticlesWithComponents;
};
