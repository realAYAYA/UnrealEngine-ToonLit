// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessEmitterInstance.h"
#include "Stateless/NiagaraStatelessEmitter.h"
#include "Stateless/NiagaraStatelessEmitterData.h"
#include "Stateless/NiagaraStatelessComputeManager.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"

#include "Shader.h"

FNiagaraDataBuffer* NiagaraStateless::FEmitterInstance_RT::GetDataToRender(bool bIsLowLatencyTranslucent) const
{
	return ComputeManager ? ComputeManager->GetDataBuffer(uintptr_t(this), this) : nullptr;
}

FNiagaraStatelessEmitterInstance::FNiagaraStatelessEmitterInstance(FNiagaraSystemInstance* InParentSystemInstance)
	: Super(InParentSystemInstance)
{
	// Setup base properties
	bLocalSpace = true;
	SimTarget = ENiagaraSimTarget::GPUComputeSim;
	bNeedsPartialDepthTexture = false;
	ParticleDataSet = new FNiagaraDataSet();
}

FNiagaraStatelessEmitterInstance::~FNiagaraStatelessEmitterInstance()
{
	//-TODO: Should we move this into the base class?
	UnbindParameters(false);

	NiagaraStateless::FEmitterInstance_RT* RenderThreadData = RenderThreadDataPtr.Release();
	if (RenderThreadData || ParticleDataSet)
	{
		ENQUEUE_RENDER_COMMAND(FReleaseStatelessEmitter)(
			[RenderThreadData, ParticleDataSet=ParticleDataSet](FRHICommandListImmediate& RHICmdList)
			{
				if (RenderThreadData != nullptr)
				{
					delete RenderThreadData;
				}
				if (ParticleDataSet != nullptr )
				{
					delete ParticleDataSet;
				}
			}
		);
		ParticleDataSet = nullptr;
	}
}

void FNiagaraStatelessEmitterInstance::Init(int32 InEmitterIndex)
{
	Super::Init(InEmitterIndex);

	// Initialize the EmitterData ptr if this is invalid the emitter is not allowed to run
	InitEmitterData();
	if (!bCanEverExecute)
	{
		ExecutionState = ENiagaraExecutionState::Disabled;
		return;
	}

	// Pull out information
	RandomSeed = EmitterData->RandomSeed + ParentSystemInstance->GetRandomSeedOffset();
	if (!EmitterData->bDeterministic)
	{
		RandomSeed ^= FPlatformTime::Cycles();
	}
	RandomStream.Initialize(RandomSeed);
	//CompletionAge = EmitterData->CalculateCompletionAge(RandomSeed);

	// Initialize data set
	ParticleDataSet->Init(&EmitterData->ParticleDataSetCompiledData);

	// Prepare our parameters
	RendererBindings = EmitterData->RendererBindings;

	// Allocate and fill shader parameters
	RenderThreadDataPtr.Reset(new NiagaraStateless::FEmitterInstance_RT());
	RenderThreadDataPtr->EmitterData = EmitterData;
	RenderThreadDataPtr->RandomSeed = RandomSeed;
	RenderThreadDataPtr->Age = 0.0f;
	RenderThreadDataPtr->ExecutionState = ENiagaraExecutionState::Active;
	RenderThreadDataPtr->ShaderParameters.Reset(WeakStatelessEmitter->AllocateShaderParameters(RendererBindings));
	RenderThreadDataPtr->ShaderParameters->Common_RandomSeed = RandomSeed;

	ENQUEUE_RENDER_COMMAND(FInitStatelessEmitter)(
		[RenderThreadData=RenderThreadDataPtr.Get(), ComputeInterface = ParentSystemInstance->GetComputeDispatchInterface()](FRHICommandListImmediate& RHICmdList)
		{
			RenderThreadData->ComputeManager = &ComputeInterface->GetOrCreateDataManager<FNiagaraStatelessComputeManager>();
		}
	);

	GPUDataBufferInterfaces = RenderThreadDataPtr.Get();

	InitEmitterState();
	InitSpawnInfos();
}

void FNiagaraStatelessEmitterInstance::ResetSimulation(bool bKillExisting)
{
	if (!bCanEverExecute)
	{
		return;
	}

	if (bKillExisting)
	{
		SpawnInfos.Empty();
		bSpawnInfosDirty = true;
	}
	else
	{
		for (FNiagaraStatelessRuntimeSpawnInfo& SpawnInfo : SpawnInfos)
		{
			SpawnInfo.SpawnTimeStart -= Age;
			SpawnInfo.SpawnTimeEnd -= Age;
		}
	}
	ActiveSpawnRates.Empty();

	RandomStream.Initialize(RandomSeed);

	Age = 0.0f;
	UniqueIndexOffset = 0;
	bEmitterEnabled_CNC = bEmitterEnabled_GT;

	InitEmitterState();
	InitSpawnInfos();

	ExecutionState = ENiagaraExecutionState::Active;
	if (NiagaraStateless::FEmitterInstance_RT* RenderThreadData = RenderThreadDataPtr.Get())
	{
		ENQUEUE_RENDER_COMMAND(UpdateStatelessAge)(
			[RenderThreadData](FRHICommandListImmediate& RHICmdList)
			{
				RenderThreadData->Age = 0.0f;
				RenderThreadData->ExecutionState = ENiagaraExecutionState::Active;
			}
		);
	}
}

void FNiagaraStatelessEmitterInstance::SetEmitterEnable(bool bNewEnableState)
{
	bEmitterEnabled_GT = bNewEnableState;
}

bool FNiagaraStatelessEmitterInstance::HandleCompletion(bool bForce)
{
	bool bIsComplete = IsComplete();
	if (!bIsComplete && bForce)
	{
		ExecutionState = ENiagaraExecutionState::Complete;
		bIsComplete = true;

		if (NiagaraStateless::FEmitterInstance_RT* RenderThreadData = RenderThreadDataPtr.Get())
		{
			ENQUEUE_RENDER_COMMAND(CompleteStatelessEmitter)(
				[RenderThreadData](FRHICommandListImmediate& RHICmdList)
				{
					RenderThreadData->ExecutionState = ENiagaraExecutionState::Complete;
				}
			);
		}
	}

	return bIsComplete;
}

int32 FNiagaraStatelessEmitterInstance::GetNumParticles() const
{
	return bCanEverExecute ? EmitterData->CalculateActiveParticles(RandomSeed, SpawnInfos, Age) : 0;
}

TConstArrayView<UNiagaraRendererProperties*> FNiagaraStatelessEmitterInstance::GetRenderers() const
{
	return EmitterData ? EmitterData->RendererProperties : TConstArrayView<UNiagaraRendererProperties*>();
}

void FNiagaraStatelessEmitterInstance::BindParameters(bool bExternalOnly)
{
	if (!RendererBindings.IsEmpty())
	{
		if (ParentSystemInstance)
		{
			ParentSystemInstance->BindToParameterStore(RendererBindings);
		}
	}
}

void FNiagaraStatelessEmitterInstance::UnbindParameters(bool bExternalOnly)
{
	if (!RendererBindings.IsEmpty())
	{
		if (ParentSystemInstance)
		{
			ParentSystemInstance->UnbindFromParameterStore(RendererBindings);
		}
	}
}

void FNiagaraStatelessEmitterInstance::Tick(float DeltaSeconds)
{
	Age += DeltaSeconds;

	TickSpawnInfos();
	TickEmitterState();
	CalculateBounds();
	SendRenderData();
}

void FNiagaraStatelessEmitterInstance::InitEmitterState()
{
	const FNiagaraEmitterStateData& EmitterState = EmitterData->EmitterState;
	LoopCount			= 0;
	CurrentLoopDuration = RandomStream.FRandRange(EmitterState.LoopDuration.Min, EmitterState.LoopDuration.Max);
	CurrentLoopDelay	= RandomStream.FRandRange(EmitterState.LoopDelay.Min, EmitterState.LoopDelay.Max);
	CurrentLoopAgeStart	= 0.0f;
	CurrentLoopAgeEnd	= CurrentLoopAgeStart + CurrentLoopDelay + CurrentLoopDuration;
}

void FNiagaraStatelessEmitterInstance::TickEmitterState()
{
	if (ParentSystemInstance)
	{
		const ENiagaraExecutionState ParentExecutionState = ParentSystemInstance->GetActualExecutionState();
		if (ParentExecutionState > ExecutionState)
		{
			SetExecutionStateInternal(ParentExecutionState);
		}
	}

	if (ExecutionState == ENiagaraExecutionState::Active)
	{
		if (Age < CurrentLoopAgeEnd)
		{
			return;
		}

		const FNiagaraEmitterStateData& EmitterState = EmitterData->EmitterState;
		if (EmitterState.LoopBehavior == ENiagaraLoopBehavior::Once)
		{
			SetExecutionStateInternal(ENiagaraExecutionState::Inactive);
			return;
		}

		// Keep looping until we find out which loop we are in as a small loop age + large DT could result in crossing multiple loops
		do
		{
			++LoopCount;
			if (EmitterState.LoopBehavior == ENiagaraLoopBehavior::Multiple && LoopCount >= EmitterState.LoopCount)
			{
				SetExecutionStateInternal(ENiagaraExecutionState::Inactive);
				return;
			}

			if (EmitterState.bRecalculateDurationEachLoop)
			{
				CurrentLoopDuration = RandomStream.FRandRange(EmitterState.LoopDuration.Min, EmitterState.LoopDuration.Max);
			}

			if (EmitterState.bDelayFirstLoopOnly)
			{
				CurrentLoopDelay	= 0.0f;
			}
			else if ( EmitterState.bRecalculateDelayEachLoop )
			{
				CurrentLoopDelay	= RandomStream.FRandRange(EmitterState.LoopDelay.Min, EmitterState.LoopDelay.Max);
			}

			CurrentLoopAgeStart	= CurrentLoopAgeEnd;
			CurrentLoopAgeEnd	= CurrentLoopAgeStart + CurrentLoopDelay + CurrentLoopDuration;
			InitSpawnInfosForLoop();
		} while ( Age >= CurrentLoopAgeEnd );
	}
	else if (ExecutionState == ENiagaraExecutionState::Inactive)
	{
		if (SpawnInfos.Num() == 0 || GetNumParticles() == 0)
		{
			SetExecutionStateInternal(ENiagaraExecutionState::Complete);
		}
		return;
	}
}

void FNiagaraStatelessEmitterInstance::CalculateBounds()
{
	CachedBounds.Init();
	FRWScopeLock ScopeLock(FixedBoundsGuard, SLT_ReadOnly);
	if (FixedBounds.IsValid)
	{
		CachedBounds = FixedBounds;
	}
	else if (CachedSystemFixedBounds.IsValid)
	{
		CachedBounds = CachedSystemFixedBounds;
	}
	else
	{
		CachedBounds = EmitterData->FixedBounds;
	}
}

void FNiagaraStatelessEmitterInstance::SendRenderData()
{
	NiagaraStateless::FEmitterInstance_RT* RenderThreadData = RenderThreadDataPtr.Get();
	if (RenderThreadData == nullptr)
	{
		return;
	}

	struct FDataForRenderThread
	{
		float Age = 0.0f;
		ENiagaraExecutionState ExecutionState = ENiagaraExecutionState::Disabled;

		NiagaraStateless::FCommonShaderParameters* ShaderParameters = nullptr;

		bool bHasBindingBufferData = false;
		TArray<uint8> BindingBufferData;

		bool bHasSpawnInfoData = false;
		TArray<FNiagaraStatelessRuntimeSpawnInfo>	SpawnInfos;
	};

	FDataForRenderThread DataForRenderThread;
	DataForRenderThread.Age				= Age;
	DataForRenderThread.ExecutionState	= ExecutionState;

	if ( EmitterData->bModulesHaveRendererBindings && RendererBindings.GetParametersDirty() )
	{
		RendererBindings.Tick();
		DataForRenderThread.bHasBindingBufferData = true;
		DataForRenderThread.BindingBufferData = RendererBindings.GetParameterDataArray();
		check((DataForRenderThread.BindingBufferData.Num() % sizeof(uint32)) == 0);

		DataForRenderThread.ShaderParameters = WeakStatelessEmitter->AllocateShaderParameters(RendererBindings);
		DataForRenderThread.ShaderParameters->Common_RandomSeed = RandomSeed;
	}

	if (bSpawnInfosDirty)
	{
		DataForRenderThread.bHasSpawnInfoData = true;
		DataForRenderThread.SpawnInfos = SpawnInfos;
		bSpawnInfosDirty = false;
	}

	ENQUEUE_RENDER_COMMAND(UpdateStatelessAge)(
		[RenderThreadData, EmitterData=MoveTemp(DataForRenderThread)](FRHICommandListImmediate& RHICmdList) mutable
		{
			RenderThreadData->Age				= EmitterData.Age;
			RenderThreadData->ExecutionState	= EmitterData.ExecutionState;

			if (EmitterData.ShaderParameters)
			{
				RenderThreadData->ShaderParameters.Reset(EmitterData.ShaderParameters);
			}

			if (EmitterData.bHasBindingBufferData)
			{
				RenderThreadData->BindingBufferData = MoveTemp(EmitterData.BindingBufferData);
			}

			if (EmitterData.bHasSpawnInfoData)
			{
				RenderThreadData->SpawnInfos = MoveTemp(EmitterData.SpawnInfos);
			}
		}
	);
}

void FNiagaraStatelessEmitterInstance::InitSpawnInfos()
{
	UniqueIndexOffset = 0;

	if (bEmitterEnabled_CNC)
	{
		for (const FNiagaraStatelessSpawnInfo& SpawnInfo : EmitterData->SpawnInfos)
		{
			if (SpawnInfo.Type == ENiagaraStatelessSpawnInfoType::Rate)
			{
				const float SpawnRate = RandomStream.FRandRange(SpawnInfo.Rate.Min, SpawnInfo.Rate.Max);
				if (SpawnRate > 0.0f)
				{
					FActiveSpawnRate& ActiveSpawnRate = ActiveSpawnRates.AddDefaulted_GetRef();
					ActiveSpawnRate.Rate = SpawnRate;
					ActiveSpawnRate.SpawnTime = CurrentLoopDelay;
				}
			}
		}

		InitSpawnInfosForLoop();
	}
}

void FNiagaraStatelessEmitterInstance::InitSpawnInfosForLoop()
{
	if (bEmitterEnabled_CNC == false)
	{
		return;
	}

	// Add the next chunk for any active spawn rates
	for (const FActiveSpawnRate& SpawnInfo : ActiveSpawnRates)
	{
		float SpawnTime = FMath::Max(CurrentLoopAgeStart - SpawnInfo.SpawnTime, 0.0f);
		SpawnTime = FMath::CeilToFloat(SpawnTime * SpawnInfo.Rate) / SpawnInfo.Rate;
		SpawnTime += SpawnInfo.SpawnTime;
		if (SpawnTime >= CurrentLoopAgeEnd)
		{
			continue;
		}

		FNiagaraStatelessRuntimeSpawnInfo& NewSpawnInfo = SpawnInfos.AddDefaulted_GetRef();
		NewSpawnInfo.Type			= ENiagaraStatelessSpawnInfoType::Rate;
		NewSpawnInfo.UniqueOffset	= UniqueIndexOffset;
		NewSpawnInfo.SpawnTimeStart	= SpawnTime;
		NewSpawnInfo.SpawnTimeEnd	= CurrentLoopAgeEnd;
		NewSpawnInfo.Rate			= SpawnInfo.Rate;

		const float ActiveDuration	= CurrentLoopAgeEnd - SpawnTime;
		const int32 NumSpawned		= FMath::FloorToInt(ActiveDuration * SpawnInfo.Rate);

		UniqueIndexOffset += NumSpawned;
		bSpawnInfosDirty = true;
	}

	// Add bursts that fit within the loop duration (due to loop random they might not)
	for (const FNiagaraStatelessSpawnInfo& SpawnInfo : EmitterData->SpawnInfos)
	{
		if (SpawnInfo.Type == ENiagaraStatelessSpawnInfoType::Rate || !SpawnInfo.IsValid(CurrentLoopDuration))
		{
			continue;
		}

		if (SpawnInfo.bSpawnProbabilityEnabled && SpawnInfo.SpawnProbability < RandomStream.FRand())
		{
			continue;
		}

		const int32 SpawnAmount = RandomStream.RandRange(SpawnInfo.Amount.Min, SpawnInfo.Amount.Max);
		if (SpawnAmount <= 0)
		{
			continue;
		}

		FNiagaraStatelessRuntimeSpawnInfo& NewSpawnInfo = SpawnInfos.AddDefaulted_GetRef();
		NewSpawnInfo.Type			= ENiagaraStatelessSpawnInfoType::Burst;
		NewSpawnInfo.UniqueOffset	= UniqueIndexOffset;
		NewSpawnInfo.SpawnTimeStart	= CurrentLoopAgeStart + CurrentLoopDelay + SpawnInfo.SpawnTime;
		NewSpawnInfo.SpawnTimeEnd	= NewSpawnInfo.SpawnTimeStart;
		NewSpawnInfo.Amount			= SpawnAmount;

		UniqueIndexOffset += SpawnAmount;
		bSpawnInfosDirty = true;
	}
}

void FNiagaraStatelessEmitterInstance::TickSpawnInfos()
{
	if (bEmitterEnabled_CNC != bEmitterEnabled_GT)
	{
		bEmitterEnabled_CNC = bEmitterEnabled_GT;

		if (bEmitterEnabled_CNC)
		{
			if ( ActiveSpawnRates.Num() > 0 )
			{
				for (const FActiveSpawnRate& SpawnInfo : ActiveSpawnRates)
				{
					float SpawnTime = FMath::Max(Age - SpawnInfo.SpawnTime, 0.0f);
					SpawnTime = FMath::CeilToFloat(SpawnTime * SpawnInfo.Rate) / SpawnInfo.Rate;
					SpawnTime += SpawnInfo.SpawnTime;
					if (SpawnTime >= CurrentLoopAgeEnd)
					{
						continue;
					}

					FNiagaraStatelessRuntimeSpawnInfo& NewSpawnInfo = SpawnInfos.AddDefaulted_GetRef();
					NewSpawnInfo.Type = ENiagaraStatelessSpawnInfoType::Rate;
					NewSpawnInfo.UniqueOffset = UniqueIndexOffset;
					NewSpawnInfo.SpawnTimeStart = SpawnTime;
					NewSpawnInfo.SpawnTimeEnd = CurrentLoopAgeEnd;
					NewSpawnInfo.Rate = SpawnInfo.Rate;

					const float ActiveDuration = CurrentLoopAgeEnd - SpawnTime;
					const int32 NumSpawned = FMath::FloorToInt(ActiveDuration * SpawnInfo.Rate);

					UniqueIndexOffset += NumSpawned;
					bSpawnInfosDirty = true;
				}
			}
		}
		else
		{
			for (auto SpawnInfoIt=SpawnInfos.CreateIterator(); SpawnInfoIt; ++SpawnInfoIt)
			{
				FNiagaraStatelessRuntimeSpawnInfo& SpawnInfo = *SpawnInfoIt;
				if (SpawnInfo.SpawnTimeStart > Age)
				{
					SpawnInfoIt.RemoveCurrent();
				}
				else
				{
					SpawnInfo.SpawnTimeEnd = FMath::Min(SpawnInfo.SpawnTimeEnd, Age);
				}
			}
			bSpawnInfosDirty = true;
		}
	}

	const float MaxLifetime = EmitterData->LifetimeRange.Max;
	SpawnInfos.RemoveAll(
		[this, &MaxLifetime](const FNiagaraStatelessRuntimeSpawnInfo& SpawnInfo)
		{
			return Age >= SpawnInfo.SpawnTimeEnd + MaxLifetime;
		}
	);
}

void FNiagaraStatelessEmitterInstance::SetExecutionStateInternal(ENiagaraExecutionState RequestedExecutionState)
{
	if (ExecutionState == RequestedExecutionState)
	{
		return;
	}
	
	switch (RequestedExecutionState)
	{
		case ENiagaraExecutionState::Active:
			UE_LOG(LogNiagara, Error, TEXT("Lightweight Emitter: Was requested to go Active and we do not supoprt that."));
			break;

		case ENiagaraExecutionState::Inactive:
			if (EmitterData->EmitterState.InactiveResponse != ENiagaraEmitterInactiveResponse::Kill)
			{
				ActiveSpawnRates.Empty();

				// Crop & Remove Spawn Infos
				const float MaxLifetime = EmitterData->LifetimeRange.Max;
				for (auto it=SpawnInfos.CreateIterator(); it; ++it)
				{
					FNiagaraStatelessRuntimeSpawnInfo& SpawnInfo = *it;
					if (SpawnInfo.Type == ENiagaraStatelessSpawnInfoType::Rate)
					{
						SpawnInfo.SpawnTimeEnd = FMath::Min(SpawnInfo.SpawnTimeEnd, Age);
					}
					if (Age < SpawnInfo.SpawnTimeStart || Age >= SpawnInfo.SpawnTimeEnd + MaxLifetime)
					{
						it.RemoveCurrent();
					}
				}

				if (SpawnInfos.Num() > 0)
				{
					//-TODO: Better way to send data to renderer
					if (NiagaraStateless::FEmitterInstance_RT* RenderThreadData = RenderThreadDataPtr.Get())
					{
						ENQUEUE_RENDER_COMMAND(FInitStatelessEmitter)(
							[RenderThreadData=RenderThreadDataPtr.Get(), SpawnInfos_RT=SpawnInfos](FRHICommandListImmediate& RHICmdList) mutable
							{
								RenderThreadData->SpawnInfos = MoveTemp(SpawnInfos_RT);
							}
						);
					}

					ExecutionState = ENiagaraExecutionState::Inactive;
					break;
				}
			}
			// Intentional fall through as we are complete

		case ENiagaraExecutionState::InactiveClear:
		case ENiagaraExecutionState::Complete:
			SpawnInfos.Empty();
			ActiveSpawnRates.Empty();
			ExecutionState = ENiagaraExecutionState::Complete;
			break;
	}
}

void FNiagaraStatelessEmitterInstance::InitEmitterData()
{
	bCanEverExecute = false;
	EmitterData = nullptr;
	WeakStatelessEmitter = nullptr;

	const FNiagaraEmitterHandle& EmitterHandle = GetEmitterHandle();
	UNiagaraStatelessEmitter* StatelessEmitter = GetEmitterHandle().GetStatelessEmitter();
	WeakStatelessEmitter = StatelessEmitter;
	if (!StatelessEmitter)
	{
		return;
	}
	EmitterData = StatelessEmitter->GetEmitterData();

	bCanEverExecute =
		EmitterData != nullptr &&
		EmitterData->bCanEverExecute &&
		EmitterHandle.GetIsEnabled();
}
