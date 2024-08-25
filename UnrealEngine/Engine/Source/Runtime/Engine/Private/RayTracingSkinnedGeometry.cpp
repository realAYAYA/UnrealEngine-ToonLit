// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingSkinnedGeometry.h"

#if RHI_RAYTRACING

#include "RenderGraphBuilder.h"
#include "RayTracingGeometry.h"

DECLARE_GPU_STAT(SkinnedGeometryBuildBLAS);
DECLARE_GPU_STAT(SkinnedGeometryUpdateBLAS);

static int32 GMemoryLimitForBatchedRayTracingGeometryUpdates = 512;
FAutoConsoleVariableRef CVarSkinnedGeometryMemoryLimitForBatchedRayTracingGeometryUpdates(
	TEXT("r.SkinCache.MemoryLimitForBatchedRayTracingGeometryUpdates"),
	GMemoryLimitForBatchedRayTracingGeometryUpdates,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingUseTransientForScratch = 0;
FAutoConsoleVariableRef CVarSkinnedGeometryRayTracingUseTransientForScratch(
	TEXT("r.SkinCache.RayTracingUseTransientForScratch"),
	GRayTracingUseTransientForScratch,
	TEXT("Use Transient memory for BLAS scratch allocation to reduce memory footprint and allocation overhead."),
	ECVF_RenderThreadSafe
);

static int32 GMaxRayTracingPrimitivesPerCmdList = -1;
FAutoConsoleVariableRef CVarSkinnedGeometryMaxRayTracingPrimitivesPerCmdList(
	TEXT("r.SkinCache.MaxRayTracingPrimitivesPerCmdList"),
	GMaxRayTracingPrimitivesPerCmdList,
	TEXT("Maximum amount of primitives which are batched together into a single command list to fix potential TDRs."),
	ECVF_RenderThreadSafe
);

void FRayTracingSkinnedGeometryUpdateQueue::Add(FRayTracingGeometry* InRayTracingGeometry, const FRayTracingAccelerationStructureSize& StructureSize)
{
	FScopeLock Lock(&CS);
	FRayTracingUpdateInfo* CurrentUpdateInfo = ToUpdate.Find(InRayTracingGeometry);
	if (CurrentUpdateInfo == nullptr)
	{
		FRayTracingUpdateInfo UpdateInfo;
		UpdateInfo.BuildMode = InRayTracingGeometry->GetRequiresBuild() ? EAccelerationStructureBuildMode::Build : EAccelerationStructureBuildMode::Update;
		UpdateInfo.ScratchSize = InRayTracingGeometry->GetRequiresBuild() ? StructureSize.BuildScratchSize : StructureSize.UpdateScratchSize;
		ToUpdate.Add(InRayTracingGeometry, UpdateInfo);
	}
	// If currently updating but need full rebuild then update the stored build mode
	else if (CurrentUpdateInfo->BuildMode == EAccelerationStructureBuildMode::Update && InRayTracingGeometry->GetRequiresBuild())
	{
		CurrentUpdateInfo->BuildMode = EAccelerationStructureBuildMode::Build;
		CurrentUpdateInfo->ScratchSize = StructureSize.BuildScratchSize;
	}

	InRayTracingGeometry->SetRequiresBuild(false);
}

void FRayTracingSkinnedGeometryUpdateQueue::Remove(FRayTracingGeometry* RayTracingGeometry, uint32 EstimatedMemory)
{
	FScopeLock Lock(&CS);
	if (ToUpdate.Find(RayTracingGeometry) != nullptr)
	{
		ToUpdate.Remove(RayTracingGeometry);
		EstimatedMemoryPendingRelease += EstimatedMemory;
	}
}

uint32 FRayTracingSkinnedGeometryUpdateQueue::ComputeScratchBufferSize() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingSkinnedGeometryUpdateQueue::ComputeScratchBufferSize);

	const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
	uint32 ScratchBLASSize = 0;

	if (ToUpdate.Num() && GRayTracingUseTransientForScratch > 0)
	{
		for (TMap<FRayTracingGeometry*, FRayTracingUpdateInfo>::TRangedForConstIterator Iter = ToUpdate.begin(); Iter != ToUpdate.end(); ++Iter)
		{			
			FRayTracingUpdateInfo const& UpdateInfo = Iter.Value();
			ScratchBLASSize = Align(ScratchBLASSize + UpdateInfo.ScratchSize, ScratchAlignment);
		}
	}

	return ScratchBLASSize;
}

void FRayTracingSkinnedGeometryUpdateQueue::Commit(FRHICommandListImmediate & RHICmdList, FRHIBuffer * ScratchBuffer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingSkinnedGeometryUpdateQueue::Commit);

	if (ToUpdate.Num())
	{
		FScopeLock Lock(&CS);

		// If we have more deferred deleted data than set limit then force flush to make sure all pending releases have actually been freed
		// before reallocating a lot of new BLAS data
		if (EstimatedMemoryPendingRelease >= GMemoryLimitForBatchedRayTracingGeometryUpdates * 1024ull * 1024ull)
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
			//UE_LOG(LogRenderer, Display, TEXT("Flushing RHI resource pending deletes due to %d MB limit"), GMemoryLimitForBatchedRayTracingGeometryUpdates);
		}
				
		// Track the amount of primitives which need to be build/updated in a single batch
		uint64 PrimitivesToUpdates = 0;
		TArray<FRayTracingGeometryBuildParams> BatchedBuildParams;
		TArray<FRayTracingGeometryBuildParams> BatchedUpdateParams;

		BatchedBuildParams.Reserve(ToUpdate.Num());
		BatchedUpdateParams.Reserve(ToUpdate.Num());

		auto KickBatch = [&RHICmdList, ScratchBuffer, &BatchedBuildParams, &BatchedUpdateParams, &PrimitivesToUpdates]()
		{
			// TODO compute correct offset and increment for the next call or just always use 0 as the offset since we know that 
			// 2 calls to BuildAccelerationStructures won't overlap due to UAV barrier inside RHIBuildAccelerationStructures so scratch memory can be reused by the next call already
			uint32 ScratchBLASOffset = 0;

			if (BatchedBuildParams.Num())
			{
				SCOPED_GPU_STAT(RHICmdList, SkinnedGeometryBuildBLAS);
				SCOPED_DRAW_EVENT(RHICmdList, SkinnedGeometryBuildBLAS);
				
				if (ScratchBuffer)
				{
					FRHIBufferRange ScratchBufferRange;
					ScratchBufferRange.Buffer = ScratchBuffer;
					ScratchBufferRange.Offset = ScratchBLASOffset;
					RHICmdList.BuildAccelerationStructures(BatchedBuildParams, ScratchBufferRange);
				}
				else
				{
					RHICmdList.BuildAccelerationStructures(BatchedBuildParams);
				}
				
				BatchedBuildParams.Empty(BatchedBuildParams.Max());
			}

			if (BatchedUpdateParams.Num())
			{
				SCOPED_GPU_STAT(RHICmdList, SkinnedGeometryUpdateBLAS);
				SCOPED_DRAW_EVENT(RHICmdList, SkinnedGeometryUpdateBLAS);

				if (ScratchBuffer)
				{
					FRHIBufferRange ScratchBufferRange;
					ScratchBufferRange.Buffer = ScratchBuffer;
					ScratchBufferRange.Offset = ScratchBLASOffset;
					RHICmdList.BuildAccelerationStructures(BatchedUpdateParams, ScratchBufferRange);
				}
				else
				{
					RHICmdList.BuildAccelerationStructures(BatchedUpdateParams);
				}
				BatchedUpdateParams.Empty(BatchedUpdateParams.Max());
			}

			PrimitivesToUpdates = 0;
		};

		const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
		uint32 ScratchBLASCurrentOffset = 0;
		uint32 ScratchBLASNextOffset = 0;

		// Iterate all the geometries which need an update
		for (TMap<FRayTracingGeometry*, FRayTracingUpdateInfo>::TRangedForIterator Iter = ToUpdate.begin(); Iter != ToUpdate.end(); ++Iter)
		{
			FRayTracingGeometry* RayTracingGeometry = Iter.Key();
			FRayTracingUpdateInfo& UpdateInfo = Iter.Value();

			FRayTracingGeometryBuildParams BuildParams;
			BuildParams.Geometry = RayTracingGeometry->RayTracingGeometryRHI;
			BuildParams.BuildMode = UpdateInfo.BuildMode;
			BuildParams.Segments = RayTracingGeometry->Initializer.Segments;

			// Update the offset
			ScratchBLASNextOffset = Align(ScratchBLASNextOffset + UpdateInfo.ScratchSize, ScratchAlignment);

			// Make 'Build' 10 times more expensive than 1 'Update' of the BVH
			uint32 PrimitiveCount = RayTracingGeometry->Initializer.TotalPrimitiveCount;
			if (BuildParams.BuildMode == EAccelerationStructureBuildMode::Build)
			{
				PrimitiveCount *= 10;
				BatchedBuildParams.Add(BuildParams);
			}
			else
			{
				BatchedUpdateParams.Add(BuildParams);
			}

			PrimitivesToUpdates += PrimitiveCount;

			// Flush batch when limit is reached
			if (GMaxRayTracingPrimitivesPerCmdList > 0 && PrimitivesToUpdates >= GMaxRayTracingPrimitivesPerCmdList)
			{
				KickBatch();
				RHICmdList.SubmitCommandsHint();
			}
		}

		// Enqueue the last batch
		KickBatch();

		// Clear working data
		ToUpdate.Reset();
		EstimatedMemoryPendingRelease = 0;
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FSkinnedGeometryBLASUpdateParams, )
	RDG_BUFFER_ACCESS(SharedScratchBuffer, ERHIAccess::UAVCompute)
END_SHADER_PARAMETER_STRUCT()

void FRayTracingSkinnedGeometryUpdateQueue::Commit(FRDGBuilder& GraphBuilder)
{
	// Find out the total BLAS scratch size and allocate transient RDG buffer.
	FRDGBufferRef SharedScratchBuffer = nullptr;
	
	const uint32 BLASScratchSize = ComputeScratchBufferSize();
	if (BLASScratchSize > 0)
	{
		const uint32 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
		
		FRDGBufferDesc ScratchBufferDesc;
		ScratchBufferDesc.Usage = EBufferUsageFlags::RayTracingScratch | EBufferUsageFlags::StructuredBuffer;
		ScratchBufferDesc.BytesPerElement = ScratchAlignment;
		ScratchBufferDesc.NumElements = FMath::DivideAndRoundUp(BLASScratchSize, ScratchAlignment);

		SharedScratchBuffer = GraphBuilder.CreateBuffer(ScratchBufferDesc, TEXT("SkinnedGeometry.BLASSharedScratchBuffer"));
	}

	FSkinnedGeometryBLASUpdateParams* BLASUpdateParams = GraphBuilder.AllocParameters<FSkinnedGeometryBLASUpdateParams>();
	BLASUpdateParams->SharedScratchBuffer = SharedScratchBuffer;

	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	GraphBuilder.AddPass(RDG_EVENT_NAME("CommitRayTracingSkinnedGeometryUpdates"), BLASUpdateParams, ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[this, SharedScratchBuffer](FRHICommandListImmediate& RHICmdList)
		{
			Commit(RHICmdList, SharedScratchBuffer ? SharedScratchBuffer->GetRHI() : nullptr);
		});
}

#endif // RHI_RAYTRACING
