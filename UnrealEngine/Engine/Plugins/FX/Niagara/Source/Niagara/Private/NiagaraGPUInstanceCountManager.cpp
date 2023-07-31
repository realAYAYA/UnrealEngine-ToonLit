// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraGPUInstanceCountManager.h"
#include "NiagaraEmptyUAVPool.h"
#include "NiagaraGpuComputeDispatch.h"
#include "NiagaraRenderer.h"
#include "NiagaraStats.h"

#include "Containers/DynamicRHIResourceArray.h"
#include "GPUSortManager.h" // CopyUIntBufferToTargets
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

int32 GNiagaraMinGPUInstanceCount = 2048;
static FAutoConsoleVariableRef CVarNiagaraMinGPUInstanceCount(
	TEXT("Niagara.MinGPUInstanceCount"),
	GNiagaraMinGPUInstanceCount,
	TEXT("Minimum number of instance count entries allocated in the global buffer. (default=2048)"),
	ECVF_Default
);

int32 GNiagaraGPUCountManagerAllocateIncrement = 64;
static FAutoConsoleVariableRef CVarNiagaraGPUCountManagerAllocateIncrement(
	TEXT("Niagara.GPUCountManager.AllocateIncrement"),
	GNiagaraGPUCountManagerAllocateIncrement,
	TEXT("If we run out of space for allocations this is how many allocate rather than a single entry. (default=64)"),
	ECVF_Default
);

int32 GNiagaraMinCulledGPUInstanceCount = 2048;
static FAutoConsoleVariableRef CVarNiagaraMinCulledGPUInstanceCount(
	TEXT("Niagara.MinCulledGPUInstanceCount"),
	GNiagaraMinCulledGPUInstanceCount,
	TEXT("Minimum number of culled (per-view) instance count entries allocated in the global buffer. (default=2048)"),
	ECVF_Default
);

float GNiagaraGPUCountBufferSlack = 1.5f;
static FAutoConsoleVariableRef CVarNiagaraGPUCountBufferSlack(
	TEXT("Niagara.GPUCountBufferSlack"),
	GNiagaraGPUCountBufferSlack,
	TEXT("Multiplier of the GPU count buffer size to prevent frequent re-allocation."),
	ECVF_Default
);

int32 GNiagaraIndirectArgsPoolMinSize = 256;
static FAutoConsoleVariableRef CVarNiagaraIndirectArgsPoolMinSize(
	TEXT("fx.Niagara.IndirectArgsPool.MinSize"),
	GNiagaraIndirectArgsPoolMinSize,
	TEXT("Minimum number of draw indirect args allocated into the pool. (default=256)"),
	ECVF_Default
);

float GNiagaraIndirectArgsPoolBlockSizeFactor = 2.0f;
static FAutoConsoleVariableRef CNiagaraIndirectArgsPoolBlockSizeFactor(
	TEXT("fx.Niagara.IndirectArgsPool.BlockSizeFactor"),
	GNiagaraIndirectArgsPoolBlockSizeFactor,
	TEXT("Multiplier on the indirect args pool size when needing to increase it from running out of space. (default=2.0)"),
	ECVF_Default
);

int32 GNiagaraIndirectArgsPoolAllowShrinking = 1;
static FAutoConsoleVariableRef CVarNiagaraIndirectArgsPoolAllowShrinking(
	TEXT("fx.Niagara.IndirectArgsPool.AllowShrinking"),
	GNiagaraIndirectArgsPoolAllowShrinking,
	TEXT("Allow the indirect args pool to shrink after a number of frames below a low water mark."),
	ECVF_Default
);

float GNiagaraIndirectArgsPoolLowWaterAmount = 0.5f;
static FAutoConsoleVariableRef CVarNiagaraIndirectArgsPoolLowWaterAmount(
	TEXT("fx.Niagara.IndirectArgsPool.LowWaterAmount"),
	GNiagaraIndirectArgsPoolLowWaterAmount,
	TEXT("Percentage (0-1) of the indirect args pool that is considered low and worthy of shrinking"),
	ECVF_Default
);

int32 GNiagaraIndirectArgsPoolLowWaterFrames = 150;
static FAutoConsoleVariableRef CVarNiagaraIndirectArgsPoolLowWaterFrames(
	TEXT("fx.Niagara.IndirectArgsPool.LowWaterFrames"),
	GNiagaraIndirectArgsPoolLowWaterFrames,
	TEXT("The number of frames to wait to shrink the indirect args pool for being below the low water mark. (default=150)"),
	ECVF_Default
);

DECLARE_DWORD_COUNTER_STAT(TEXT("Used GPU Instance Counters"), STAT_NiagaraUsedGPUInstanceCounters, STATGROUP_Niagara);
DECLARE_DWORD_COUNTER_STAT(TEXT("Indirect Draw Calls"), STAT_NiagaraIndirectDraws, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("GPU Readback Lock"), STAT_NiagaraGPUReadbackLock, STATGROUP_Niagara);

#ifndef ENABLE_NIAGARA_INDIRECT_ARG_POOL_LOG
#define ENABLE_NIAGARA_INDIRECT_ARG_POOL_LOG 0
#endif

#if ENABLE_NIAGARA_INDIRECT_ARG_POOL_LOG
#define INDIRECT_ARG_POOL_LOG(Format, ...) UE_LOG(LogNiagara, Log, TEXT("NIAGARA INDIRECT ARG POOL: ") TEXT(Format), __VA_ARGS__)
#else
#define INDIRECT_ARG_POOL_LOG(Format, ...) do {} while(0)
#endif

//*****************************************************************************

const ERHIAccess FNiagaraGPUInstanceCountManager::kCountBufferDefaultState = ERHIAccess::SRVMask | ERHIAccess::CopySrc;
const ERHIAccess FNiagaraGPUInstanceCountManager::kIndirectArgsDefaultState = ERHIAccess::IndirectArgs | ERHIAccess::SRVMask;

FNiagaraGPUInstanceCountManager::FNiagaraGPUInstanceCountManager(ERHIFeatureLevel::Type InFeatureLevel)
	: FeatureLevel(InFeatureLevel)
{
}

FNiagaraGPUInstanceCountManager::~FNiagaraGPUInstanceCountManager()
{
	ReleaseRHI();
}

void FNiagaraGPUInstanceCountManager::InitRHI()
{
}

void FNiagaraGPUInstanceCountManager::ReleaseRHI()
{
	ReleaseCounts();

	for (auto& PoolEntry : DrawIndirectPool)
	{
		PoolEntry->Buffer.Release();
	}
	DrawIndirectPool.Empty();
}

void FNiagaraGPUInstanceCountManager::ReleaseCounts()
{
	CountBuffer.Release();
	CulledCountBuffer.Release();

	AllocatedInstanceCounts = 0;
	AllocatedCulledCounts = 0;

	if (CountReadback)
	{
		delete CountReadback;
		CountReadback = nullptr;
		CountReadbackSize = 0;
	}
}

uint32 FNiagaraGPUInstanceCountManager::AcquireEntry()
{
	checkSlow(IsInRenderingThread());

	if (FreeEntries.Num())
	{
		return FreeEntries.Pop();
	}
	else if (UsedInstanceCounts < AllocatedInstanceCounts)
	{
		// We can't reallocate on the fly, the buffer must be correctly resized before any tick gets scheduled.
		return UsedInstanceCounts++;
	}
	else
	{
		// @TODO : add realloc the buffer and copy the current content to it. Might require reallocating the readback in FNiagaraGPUInstanceCountManager::EnqueueGPUReadback()
		ensure(UsedInstanceCounts < AllocatedInstanceCounts);
		//UE_LOG(LogNiagara, Error, TEXT("Niagara.MinGPUInstanceCount too small. UsedInstanceCounts: %d < AllocatedInstanceCounts: %d"), UsedInstanceCounts, AllocatedInstanceCounts);
		return INDEX_NONE;
	}
}

uint32 FNiagaraGPUInstanceCountManager::AcquireOrAllocateEntry(FRHICommandListImmediate& RHICmdList)
{
	checkSlow(IsInRenderingThread());

	// Free entries?
	if (FreeEntries.Num())
	{
		return FreeEntries.Pop();
	}
	else if (UsedInstanceCounts < AllocatedInstanceCounts)
	{
		return UsedInstanceCounts++;
	}

	// We need to resize
	ResizeBuffers(RHICmdList, AllocatedInstanceCounts + GNiagaraGPUCountManagerAllocateIncrement);

	check(UsedInstanceCounts < AllocatedInstanceCounts);
	return UsedInstanceCounts++;
}

void FNiagaraGPUInstanceCountManager::FreeEntry(uint32& BufferOffset)
{
	checkSlow(IsInRenderingThread());

	if (BufferOffset != INDEX_NONE)
	{
		checkf(!FreeEntries.Contains(BufferOffset), TEXT("BufferOffset %u exists in FreeEntries"), BufferOffset);
		checkf(!InstanceCountClearTasks.Contains(BufferOffset), TEXT("BufferOffset %u exists in InstanceCountClearTasks"), BufferOffset);

		InstanceCountClearTasks.Add(BufferOffset);
		BufferOffset = INDEX_NONE;
	}
}

void FNiagaraGPUInstanceCountManager::FreeEntryArray(TConstArrayView<uint32> EntryArray)
{
	checkSlow(IsInRenderingThread());

	const int32 NumToFree = EntryArray.Num();
	if (NumToFree > 0)
	{
#if DO_CHECK
		for (uint32 BufferOffset : EntryArray)
		{
			checkf(!FreeEntries.Contains(BufferOffset), TEXT("BufferOffset %u exists in FreeEntries"), BufferOffset);
			checkf(!InstanceCountClearTasks.Contains(BufferOffset), TEXT("BufferOffset %u exists in InstanceCountClearTasks"), BufferOffset);
		}
#endif
		InstanceCountClearTasks.Append(EntryArray.GetData(), NumToFree);
	}
}

FRWBuffer* FNiagaraGPUInstanceCountManager::AcquireCulledCountsBuffer(FRHICommandListImmediate& RHICmdList)
{
	if (RequiredCulledCounts > 0)
	{
		if (!bAcquiredCulledCounts)
		{
			const int32 RecommendedCulledCounts = FMath::Max(GNiagaraMinCulledGPUInstanceCount, (int32)(RequiredCulledCounts * GNiagaraGPUCountBufferSlack));
			if (RecommendedCulledCounts > AllocatedCulledCounts)
			{
				// We need a bigger buffer
				CulledCountBuffer.Release();

				AllocatedCulledCounts = RecommendedCulledCounts;
				CulledCountBuffer.Initialize(TEXT("NiagaraCulledGPUInstanceCounts"), sizeof(uint32), AllocatedCulledCounts, EPixelFormat::PF_R32_UINT, ERHIAccess::SRVCompute);
			}

			// Initialize the buffer by clearing it to zero then transition it to be ready to write to
			RHICmdList.Transition(FRHITransitionInfo(CulledCountBuffer.UAV, ERHIAccess::SRVCompute, ERHIAccess::UAVCompute));
			RHICmdList.ClearUAVUint(CulledCountBuffer.UAV, FUintVector4(EForceInit::ForceInitToZero));
			RHICmdList.Transition(FRHITransitionInfo(CulledCountBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));

			bAcquiredCulledCounts = true;
		}

		return &CulledCountBuffer;
	}

	return nullptr;
}

void FNiagaraGPUInstanceCountManager::ResizeBuffers(FRHICommandListImmediate& RHICmdList, int32 ReservedInstanceCounts)
{
	const int32 RequiredInstanceCounts = UsedInstanceCounts + FMath::Max<int32>(ReservedInstanceCounts - FreeEntries.Num(), 0);
	if (RequiredInstanceCounts > 0)
	{
		const int32 RecommendedInstanceCounts = FMath::Max(GNiagaraMinGPUInstanceCount, (int32)(RequiredInstanceCounts * GNiagaraGPUCountBufferSlack));
		// If the buffer is not allocated, allocate it to the recommended size.
		if (!AllocatedInstanceCounts)
		{
			AllocatedInstanceCounts = RecommendedInstanceCounts;
			TResourceArray<uint32> InitData;
			InitData.AddZeroed(AllocatedInstanceCounts);
			CountBuffer.Initialize(TEXT("NiagaraGPUInstanceCounts"), sizeof(uint32), AllocatedInstanceCounts, EPixelFormat::PF_R32_UINT, kCountBufferDefaultState, BUF_Static | BUF_SourceCopy, &InitData);
			//UE_LOG(LogNiagara, Log, TEXT("FNiagaraGPUInstanceCountManager::ResizeBuffers Alloc AllocatedInstanceCounts: %d ReservedInstanceCounts: %d"), AllocatedInstanceCounts, ReservedInstanceCounts);
		}
		// If we need to increase the buffer size to RecommendedInstanceCounts because the buffer is too small.
		else if (RequiredInstanceCounts > AllocatedInstanceCounts)
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResizeNiagaraGPUCounts);

			// Init a bigger buffer filled with 0.
			TResourceArray<uint32> InitData;
			InitData.AddZeroed(RecommendedInstanceCounts);
			FRWBuffer NextCountBuffer;
			NextCountBuffer.Initialize(TEXT("NiagaraGPUInstanceCounts"), sizeof(uint32), RecommendedInstanceCounts, EPixelFormat::PF_R32_UINT, ERHIAccess::UAVCompute, BUF_Static | BUF_SourceCopy, &InitData);

			// Copy the current buffer in the next buffer. We don't need to transition any of the buffers, because the current buffer is transitioned to readable after
			// the simulation, and the new buffer is created in the UAVCompute state.
			FRHIUnorderedAccessView* UAVs[] = { NextCountBuffer.UAV };
			int32 UsedIndexCounts[] = { AllocatedInstanceCounts };
			CopyUIntBufferToTargets(RHICmdList, FeatureLevel, CountBuffer.SRV, UAVs, UsedIndexCounts, 0, UE_ARRAY_COUNT(UAVs));

			// FNiagaraGpuComputeDispatch expects the count buffer to be readable and copyable before running the sim.
			RHICmdList.Transition(FRHITransitionInfo(NextCountBuffer.UAV, ERHIAccess::UAVCompute, kCountBufferDefaultState));

			// Swap the buffers
			AllocatedInstanceCounts = RecommendedInstanceCounts;
			FMemory::Memswap(&NextCountBuffer, &CountBuffer, sizeof(NextCountBuffer));
			//UE_LOG(LogNiagara, Log, TEXT("FNiagaraGPUInstanceCountManager::ResizeBuffers Resize AllocatedInstanceCounts: %d ReservedInstanceCounts: %d"), AllocatedInstanceCounts, ReservedInstanceCounts);
		}
		// If we need to shrink the buffer size because use way to much buffer size.
		else if ((int32)(RecommendedInstanceCounts * GNiagaraGPUCountBufferSlack) < AllocatedInstanceCounts)
		{
			// possibly shrink but hard to do because of sparse array allocation.
		}
	}
	else
	{
		ReleaseCounts();
	}

	INC_DWORD_STAT_BY(STAT_NiagaraUsedGPUInstanceCounters, RequiredInstanceCounts);
}

void FNiagaraGPUInstanceCountManager::FlushIndirectArgsPool()
{
	// Cull indirect draw pool entries so that we only keep the last pool
	while (DrawIndirectPool.Num() > 1)
	{
		FIndirectArgsPoolEntryPtr& PoolEntry = DrawIndirectPool[0];
		PoolEntry->Buffer.Release();

		DrawIndirectPool.RemoveAt(0, 1, false);
	}

	// If shrinking is allowed and we've been under the low water mark
	if (GNiagaraIndirectArgsPoolAllowShrinking && DrawIndirectPool.Num() > 0 && DrawIndirectLowWaterFrames >= uint32(GNiagaraIndirectArgsPoolLowWaterFrames))
	{
		FIndirectArgsPoolEntryPtr& PoolEntry = DrawIndirectPool[0];
		const uint32 NewSize = FMath::Max<uint32>(GNiagaraIndirectArgsPoolMinSize, PoolEntry->AllocatedEntries / GNiagaraIndirectArgsPoolBlockSizeFactor);

		INDIRECT_ARG_POOL_LOG("Shrinking pool from size %d to %d", PoolEntry->AllocatedEntries, NewSize);

		PoolEntry->Buffer.Release();
		PoolEntry->AllocatedEntries = NewSize;

		TResourceArray<uint32> InitData;
		InitData.AddZeroed(PoolEntry->AllocatedEntries * NIAGARA_DRAW_INDIRECT_ARGS_SIZE);
		PoolEntry->Buffer.Initialize(TEXT("NiagaraGPUDrawIndirectArgs"), sizeof(uint32), PoolEntry->AllocatedEntries * NIAGARA_DRAW_INDIRECT_ARGS_SIZE, EPixelFormat::PF_R32_UINT, kIndirectArgsDefaultState, BUF_Static | BUF_DrawIndirect, &InitData);

		// Reset the timer
		DrawIndirectLowWaterFrames = 0;
	}
}

FNiagaraGPUInstanceCountManager::FIndirectArgSlot FNiagaraGPUInstanceCountManager::AddDrawIndirect(uint32 InstanceCountBufferOffset, uint32 NumIndicesPerInstance, uint32 StartIndexLocation, bool bIsInstancedStereoEnabled, bool bCulled, ENiagaraGpuComputeTickStage::Type ReadyTickStage)
{
	checkSlow(IsInRenderingThread());

	const ENiagaraDrawIndirectArgGenTaskFlags TaskFlags =
		(bIsInstancedStereoEnabled ? ENiagaraDrawIndirectArgGenTaskFlags::InstancedStereo : ENiagaraDrawIndirectArgGenTaskFlags::None)
		| (bCulled ? ENiagaraDrawIndirectArgGenTaskFlags::UseCulledCounts : ENiagaraDrawIndirectArgGenTaskFlags::None)
		| (ReadyTickStage == ENiagaraGpuComputeTickStage::PostOpaqueRender ? ENiagaraDrawIndirectArgGenTaskFlags::PostOpaque : ENiagaraDrawIndirectArgGenTaskFlags::None);
	FNiagaraDrawIndirectArgGenTaskInfo Info(InstanceCountBufferOffset, NumIndicesPerInstance, StartIndexLocation, TaskFlags);

	FNiagaraDrawIndirectArgGenSlotInfo* SlotInfo = DrawIndirectArgMap.Find(Info);
	if ( SlotInfo == nullptr )
	{
		// Attempt to allocate a new slot from the pool, or add to the pool if it's full
		FIndirectArgsPoolEntry* PoolEntry = DrawIndirectPool.Num() > 0 ? DrawIndirectPool.Last().Get() : nullptr;
		if (PoolEntry == nullptr || PoolEntry->UsedEntriesTotal >= PoolEntry->AllocatedEntries)
		{
			FIndirectArgsPoolEntryPtr NewEntry = MakeUnique<FIndirectArgsPoolEntry>();
			NewEntry->AllocatedEntries = PoolEntry ? uint32(PoolEntry->AllocatedEntries * GNiagaraIndirectArgsPoolBlockSizeFactor) : uint32(GNiagaraIndirectArgsPoolMinSize);

			INDIRECT_ARG_POOL_LOG("Increasing pool from size %d to %d", PoolEntry ? PoolEntry->AllocatedEntries : 0, NewEntry->AllocatedEntries);

			TResourceArray<uint32> InitData;
			InitData.AddZeroed(NewEntry->AllocatedEntries * NIAGARA_DRAW_INDIRECT_ARGS_SIZE);
			NewEntry->Buffer.Initialize(TEXT("NiagaraGPUDrawIndirectArgs"), sizeof(uint32), NewEntry->AllocatedEntries * NIAGARA_DRAW_INDIRECT_ARGS_SIZE, EPixelFormat::PF_R32_UINT, kIndirectArgsDefaultState, BUF_Static | BUF_DrawIndirect, &InitData);

			PoolEntry = NewEntry.Get();
			DrawIndirectPool.Emplace(MoveTemp(NewEntry));
		}

		Info.IndirectArgsBufferOffset = PoolEntry->UsedEntriesTotal * NIAGARA_DRAW_INDIRECT_ARGS_SIZE;
		++PoolEntry->UsedEntriesTotal;

		SlotInfo = &DrawIndirectArgMap.Add(Info);
		SlotInfo->PoolIndex = DrawIndirectPool.Num() - 1;
		SlotInfo->BufferOffset = Info.IndirectArgsBufferOffset * sizeof(uint32);

		const ENiagaraGPUCountUpdatePhase::Type CountPhase = ReadyTickStage == ENiagaraGpuComputeTickStage::PostOpaqueRender ? ENiagaraGPUCountUpdatePhase::PostOpaque : ENiagaraGPUCountUpdatePhase::PreOpaque;
		DrawIndirectArgGenTasks[CountPhase].Add(Info);
		++PoolEntry->UsedEntries[CountPhase];
	}

	return FIndirectArgSlot(DrawIndirectPool[SlotInfo->PoolIndex]->Buffer.Buffer, DrawIndirectPool[SlotInfo->PoolIndex]->Buffer.SRV, SlotInfo->BufferOffset);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FNiagaraGPUInstanceCountManager::UpdateDrawIndirectBuffers(FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface, FRHICommandList& RHICmdList, ENiagaraGPUCountUpdatePhase::Type CountPhase)
{
	// Anything to process?
	TArray<FNiagaraDrawIndirectArgGenTaskInfo>& ArgTasks = DrawIndirectArgGenTasks[CountPhase];
	const bool bClearCounts = (CountPhase == ENiagaraGPUCountUpdatePhase::PreOpaque) && (InstanceCountClearTasks.Num() > 0);
	if ( (ArgTasks.Num() > 0 || bClearCounts) && FNiagaraUtilities::AllowComputeShaders(GShaderPlatformForFeatureLevel[FeatureLevel]) )
	{
		INC_DWORD_STAT_BY(STAT_NiagaraIndirectDraws, ArgTasks.Num());

		SCOPED_DRAW_EVENT(RHICmdList, NiagaraUpdateDrawIndirectBuffers);

		// Allocate task buffer
		FReadBuffer TaskInfosBuffer;
		{
			const uint32 ArgGenSize = ArgTasks.Num() * sizeof(FNiagaraDrawIndirectArgGenTaskInfo);
			const uint32 InstanceCountClearSize = InstanceCountClearTasks.Num() * sizeof(uint32);
			const uint32 TaskBufferSize = ArgGenSize + InstanceCountClearSize;
			TaskInfosBuffer.Initialize(TEXT("NiagaraTaskInfosBuffer"), sizeof(uint32), TaskBufferSize / sizeof(uint32), EPixelFormat::PF_R32_UINT, BUF_Volatile);

			uint8* TaskBufferData = (uint8*)RHILockBuffer(TaskInfosBuffer.Buffer, 0, TaskBufferSize, RLM_WriteOnly);
			FMemory::Memcpy(TaskBufferData, ArgTasks.GetData(), ArgGenSize);
			FMemory::Memcpy(TaskBufferData + ArgGenSize, InstanceCountClearTasks.GetData(), InstanceCountClearSize);
			RHIUnlockBuffer(TaskInfosBuffer.Buffer);
		}

		FNiagaraEmptyUAVPoolScopedAccess UAVPoolAccessScope(ComputeDispatchInterface->GetEmptyUAVPool());
		TArray<FRHITransitionInfo, TInlineAllocator<10>> Transitions;
		Transitions.Reserve(DrawIndirectPool.Num() + 2);

		// Get counts buffer
		FUnorderedAccessViewRHIRef CountsUAV = nullptr;
		const bool bCountBufferIsValid = CountBuffer.UAV.IsValid();
		if (bCountBufferIsValid)
		{
			// treat the incoming UAV as being unknown to be sure a barrier is inserted in the case
			// where the preceding dispatch wrote to the counts buffer
			Transitions.Emplace(CountBuffer.UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute);
			CountsUAV = CountBuffer.UAV;
		}
		else
		{
			// This can happen if there are no InstanceCountClearTasks and all DrawIndirectArgGenTasks_PreOpaque are using culled counts
			CountsUAV = ComputeDispatchInterface->GetEmptyUAVFromPool(RHICmdList, PF_R32_UINT, ENiagaraEmptyUAVType::Buffer);
		}

		// Get culled counts buffer
		FShaderResourceViewRHIRef CulledCountsSRV = nullptr;
		if (CulledCountBuffer.SRV.IsValid())
		{
			if (bAcquiredCulledCounts)
			{
				Transitions.Emplace(CulledCountBuffer.UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVCompute);
			}
			CulledCountsSRV = CulledCountBuffer.SRV.GetReference();
		}
		else
		{
			CulledCountsSRV = FNiagaraRenderer::GetDummyUIntBuffer();
		}

		// Execute transitions
		for (auto& PoolEntry : DrawIndirectPool)
		{
			Transitions.Emplace(PoolEntry->Buffer.UAV, kIndirectArgsDefaultState, ERHIAccess::UAVCompute);
		}
		RHICmdList.Transition(Transitions);

		// Execute tasks
		FNiagaraDrawIndirectArgsGenCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FNiagaraDrawIndirectArgsGenCS::FSupportsTextureRW>(GRHISupportsRWTextureBuffers ? 1 : 0);
		TShaderMapRef<FNiagaraDrawIndirectArgsGenCS> DrawIndirectArgsGenCS(GetGlobalShaderMap(FeatureLevel), PermutationVector);

		if (bCountBufferIsValid)
		{
			RHICmdList.BeginUAVOverlap(CountsUAV);
		}

		const int32 NumDispatches = FMath::Max(DrawIndirectPool.Num(), 1);
		uint32 ArgGenTaskOffset = 0;
		for (int32 DispatchIdx = 0; DispatchIdx < NumDispatches; ++DispatchIdx)
		{
			// Get draw indirect pool UAV
			// Note: If we have counts to clear but no indirect args we won't have a DrawIndirectPool entry
			FRHIUnorderedAccessView* ArgsUAV = nullptr;
			int32 NumArgGenTasks = 0;
			if (DrawIndirectPool.IsValidIndex(DispatchIdx))
			{
				FIndirectArgsPoolEntryPtr& PoolEntry = DrawIndirectPool[DispatchIdx];
				ArgsUAV = PoolEntry->Buffer.UAV;
				NumArgGenTasks = PoolEntry->UsedEntries[CountPhase];
			}
			else
			{
				ArgsUAV = ComputeDispatchInterface->GetEmptyUAVFromPool(RHICmdList, PF_R32_UINT, ENiagaraEmptyUAVType::Buffer);
			}

			const bool bIsLastDispatch = DispatchIdx == (NumDispatches - 1);
			const int32 NumInstanceCountClearTasks = bIsLastDispatch ? InstanceCountClearTasks.Num() : 0;

			// Do we have anything to do for this pool?
			if (NumArgGenTasks + NumInstanceCountClearTasks == 0)
			{
				continue;
			}

			SetComputePipelineState(RHICmdList, DrawIndirectArgsGenCS.GetComputeShader());
			DrawIndirectArgsGenCS->SetOutput(RHICmdList, ArgsUAV, CountsUAV);
			DrawIndirectArgsGenCS->SetParameters(RHICmdList, TaskInfosBuffer.SRV, CulledCountsSRV, ArgGenTaskOffset, NumArgGenTasks, NumInstanceCountClearTasks);

			// If the device supports RW Texture buffers then we can use a single compute pass, otherwise we need to split into two passes
			if (GRHISupportsRWTextureBuffers)
			{
				DispatchComputeShader(RHICmdList, DrawIndirectArgsGenCS.GetShader(), FMath::DivideAndRoundUp(NumArgGenTasks + NumInstanceCountClearTasks, NIAGARA_DRAW_INDIRECT_ARGS_GEN_THREAD_COUNT), 1, 1);
				DrawIndirectArgsGenCS->UnbindBuffers(RHICmdList);
			}
			else
			{
				if (NumArgGenTasks > 0)
				{
					DispatchComputeShader(RHICmdList, DrawIndirectArgsGenCS.GetShader(), FMath::DivideAndRoundUp(NumArgGenTasks, NIAGARA_DRAW_INDIRECT_ARGS_GEN_THREAD_COUNT), 1, 1);
					DrawIndirectArgsGenCS->UnbindBuffers(RHICmdList);
				}

				if (NumInstanceCountClearTasks > 0)
				{
					FNiagaraDrawIndirectResetCountsCS::FPermutationDomain PermutationVectorResetCounts;
					TShaderMapRef<FNiagaraDrawIndirectResetCountsCS> DrawIndirectResetCountsArgsGenCS(GetGlobalShaderMap(FeatureLevel), PermutationVectorResetCounts);
					SetComputePipelineState(RHICmdList, DrawIndirectResetCountsArgsGenCS.GetComputeShader());
					DrawIndirectResetCountsArgsGenCS->SetOutput(RHICmdList, CountBuffer.UAV);
					DrawIndirectResetCountsArgsGenCS->SetParameters(RHICmdList, TaskInfosBuffer.SRV, ArgTasks.Num(), NumInstanceCountClearTasks);
					DispatchComputeShader(RHICmdList, DrawIndirectResetCountsArgsGenCS.GetShader(), FMath::DivideAndRoundUp(NumInstanceCountClearTasks, NIAGARA_DRAW_INDIRECT_ARGS_GEN_THREAD_COUNT), 1, 1);
					DrawIndirectResetCountsArgsGenCS->UnbindBuffers(RHICmdList);
				}
			}

			ArgGenTaskOffset += NumArgGenTasks;
		}

		if (bCountBufferIsValid)
		{
			RHICmdList.EndUAVOverlap(CountsUAV);
		}

		// Generate and execute transitions
		Transitions.Reset();
		for (auto& PoolEntry : DrawIndirectPool)
		{
			Transitions.Emplace(PoolEntry->Buffer.UAV, ERHIAccess::UAVCompute, kIndirectArgsDefaultState);
		}
		Transitions.Emplace(CountBuffer.UAV, ERHIAccess::UAVCompute, kCountBufferDefaultState);
		RHICmdList.Transition(Transitions);
	}

	// Add free counts back to list as we have cleared them
	if ( bClearCounts )
	{
		FreeEntries.Append(InstanceCountClearTasks);
		InstanceCountClearTasks.Empty();
	}

	DrawIndirectArgGenTasks[CountPhase].Empty();

	// Final phase we can do some booking keeping / clearing of data
	if ( CountPhase == ENiagaraGPUCountUpdatePhase::PostOpaque )
	{
		// Clear indirect arg map
		DrawIndirectArgMap.Empty();

		// Release culled count buffers
		bAcquiredCulledCounts = false;
		RequiredCulledCounts = 0;

		// Optionally shrink the indirect arg pool
		if (GNiagaraIndirectArgsPoolAllowShrinking)
		{
			if (DrawIndirectPool.Num() == 1 && DrawIndirectPool[0]->AllocatedEntries > uint32(GNiagaraIndirectArgsPoolMinSize))
			{
				// See if this was a low water mark frame
				FIndirectArgsPoolEntryPtr& PoolEntry = DrawIndirectPool[0];
				const uint32 LowWaterCount = FMath::Max<uint32>(GNiagaraIndirectArgsPoolMinSize, PoolEntry->AllocatedEntries * GNiagaraIndirectArgsPoolLowWaterAmount);
				if (PoolEntry->UsedEntriesTotal < LowWaterCount)
				{
					++DrawIndirectLowWaterFrames;
				}
				else
				{
					// We've allocated above the low water amount, reset the timer
					DrawIndirectLowWaterFrames = 0;
				}
			}
			else
			{
				// Either the pool is empty, at the min size, or we had to increase the pool size this frame. Either way, reset the shrink timer
				DrawIndirectLowWaterFrames = 0;
			}
		}

		// Clear indirect args pool counts
		for (auto& Pool : DrawIndirectPool)
		{
			Pool->UsedEntriesTotal = 0;
			for ( uint32& UsedEntry : Pool->UsedEntries )
			{
				UsedEntry = 0;
			}
		}
	}
}

const uint32* FNiagaraGPUInstanceCountManager::GetGPUReadback()
{
	if (CountReadback && CountReadbackSize && CountReadback->IsReady())
	{
		SCOPE_CYCLE_COUNTER(STAT_NiagaraGPUReadbackLock);
		return (uint32*)(CountReadback->Lock(CountReadbackSize * sizeof(uint32)));
	}
	else
	{
		return nullptr;
	}
}

void FNiagaraGPUInstanceCountManager::ReleaseGPUReadback()
{
	check(CountReadback && CountReadbackSize);
	CountReadback->Unlock();
	// Readback can only ever be done once, to prevent misusage with index lifetime
	CountReadbackSize = 0;
}

void FNiagaraGPUInstanceCountManager::EnqueueGPUReadback(FRHICommandListImmediate& RHICmdList)
{
	if (UsedInstanceCounts > 0 && (UsedInstanceCounts != FreeEntries.Num()))
	{
		if (!CountReadback)
		{
			CountReadback = new FRHIGPUBufferReadback(TEXT("Niagara GPU Instance Count Readback"));
		}
		CountReadbackSize = UsedInstanceCounts;

		// No need for a transition, FNiagaraGpuComputeDispatch ensures that the buffer is left in the correct state after the simulation.
		CountReadback->EnqueueCopy(RHICmdList, CountBuffer.Buffer);
	}
}

bool FNiagaraGPUInstanceCountManager::HasPendingGPUReadback() const
{
	return CountReadback && CountReadbackSize;
}

