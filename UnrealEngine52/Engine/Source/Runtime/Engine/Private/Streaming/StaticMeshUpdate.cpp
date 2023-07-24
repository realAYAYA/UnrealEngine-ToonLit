// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
StaticMeshUpdate.cpp: Helpers to stream in and out static mesh LODs.
=============================================================================*/

#include "Streaming/StaticMeshUpdate.h"
#include "HAL/PlatformFile.h"
#include "RenderUtils.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Serialization/MemoryReader.h"
#include "StaticMeshResources.h"
#include "Streaming/RenderAssetUpdate.inl"
#include "ContentStreaming.h"
#include "RHIResourceUpdates.h"

int32 GStreamingMaxReferenceChecks = 2;
static FAutoConsoleVariableRef CVarStreamingMaxReferenceChecksBeforeStreamOut(
	TEXT("r.Streaming.MaxReferenceChecksBeforeStreamOut"),
	GStreamingMaxReferenceChecks,
	TEXT("Number of times the engine wait for references to be released before forcing streamout. (default=2)"),
	ECVF_Default
);

int32 GStreamingStaticMeshIOPriority = (int32)AIOP_Low;
static FAutoConsoleVariableRef CVarStreamingStaticMeshIOPriority(
	TEXT("r.Streaming.StaticMeshIOPriority"),
	GStreamingStaticMeshIOPriority,
	TEXT("Base I/O priority for loading static mesh LODs"),
	ECVF_Default);

// Instantiate TRenderAssetUpdate for FStaticMeshUpdateContext
template class TRenderAssetUpdate<FStaticMeshUpdateContext>;

static constexpr uint32 GStaticMeshMaxNumResourceUpdatesPerLOD = 14;
static constexpr uint32 GStaticMeshMaxNumResourceUpdatesPerBatch = (MAX_STATIC_MESH_LODS - 1) * GStaticMeshMaxNumResourceUpdatesPerLOD;

FStaticMeshUpdateContext::FStaticMeshUpdateContext(const UStaticMesh* InMesh, EThreadType InCurrentThread)
	: Mesh(InMesh)
	, CurrentThread(InCurrentThread)
{
	check(InMesh);
	checkSlow(InCurrentThread != FStaticMeshUpdate::TT_Render || IsInRenderingThread());
	// #TODO RenderData under a const UStaticMesh should stay const
	RenderData = const_cast<UStaticMesh*>(InMesh)->GetRenderData();
	if (RenderData)
	{
		LODResourcesView = TArrayView<FStaticMeshLODResources*>(RenderData->LODResources.GetData() + InMesh->GetStreamableResourceState().AssetLODBias, InMesh->GetStreamableResourceState().MaxNumLODs);
	}
}

FStaticMeshUpdateContext::FStaticMeshUpdateContext(const UStreamableRenderAsset* InMesh, EThreadType InCurrentThread)
#if UE_BUILD_SHIPPING
	: FStaticMeshUpdateContext(static_cast<const UStaticMesh*>(InMesh), InCurrentThread)
#else
	: FStaticMeshUpdateContext(Cast<UStaticMesh>(InMesh), InCurrentThread)
#endif
{}

FStaticMeshUpdate::FStaticMeshUpdate(const UStaticMesh* InMesh)
	: TRenderAssetUpdate<FStaticMeshUpdateContext>(InMesh)
{
}

void FStaticMeshStreamIn::FIntermediateBuffers::CreateFromCPUData_RenderThread(FStaticMeshLODResources& LODResource)
{
	FStaticMeshVertexBuffers& VBs = LODResource.VertexBuffers;
	TangentsVertexBuffer = VBs.StaticMeshVertexBuffer.CreateTangentsRHIBuffer_RenderThread();
	TexCoordVertexBuffer = VBs.StaticMeshVertexBuffer.CreateTexCoordRHIBuffer_RenderThread();
	PositionVertexBuffer = VBs.PositionVertexBuffer.CreateRHIBuffer_RenderThread();
	ColorVertexBuffer = VBs.ColorVertexBuffer.CreateRHIBuffer_RenderThread();
	IndexBuffer = LODResource.IndexBuffer.CreateRHIBuffer_RenderThread();
	DepthOnlyIndexBuffer = LODResource.DepthOnlyIndexBuffer.CreateRHIBuffer_RenderThread();


	if (LODResource.AdditionalIndexBuffers)
	{
		ReversedIndexBuffer = LODResource.AdditionalIndexBuffers->ReversedIndexBuffer.CreateRHIBuffer_RenderThread();
		ReversedDepthOnlyIndexBuffer = LODResource.AdditionalIndexBuffers->ReversedDepthOnlyIndexBuffer.CreateRHIBuffer_RenderThread();
		WireframeIndexBuffer = LODResource.AdditionalIndexBuffers->WireframeIndexBuffer.CreateRHIBuffer_RenderThread();
	}
}

void FStaticMeshStreamIn::FIntermediateBuffers::CreateFromCPUData_Async(FStaticMeshLODResources& LODResource)
{
	FStaticMeshVertexBuffers& VBs = LODResource.VertexBuffers;
	TangentsVertexBuffer = VBs.StaticMeshVertexBuffer.CreateTangentsRHIBuffer_Async();
	TexCoordVertexBuffer = VBs.StaticMeshVertexBuffer.CreateTexCoordRHIBuffer_Async();
	PositionVertexBuffer = VBs.PositionVertexBuffer.CreateRHIBuffer_Async();
	ColorVertexBuffer = VBs.ColorVertexBuffer.CreateRHIBuffer_Async();
	IndexBuffer = LODResource.IndexBuffer.CreateRHIBuffer_Async();
	DepthOnlyIndexBuffer = LODResource.DepthOnlyIndexBuffer.CreateRHIBuffer_Async();

	if (LODResource.AdditionalIndexBuffers)
	{
		ReversedIndexBuffer = LODResource.AdditionalIndexBuffers->ReversedIndexBuffer.CreateRHIBuffer_Async();
		ReversedDepthOnlyIndexBuffer = LODResource.AdditionalIndexBuffers->ReversedDepthOnlyIndexBuffer.CreateRHIBuffer_Async();
		WireframeIndexBuffer = LODResource.AdditionalIndexBuffers->WireframeIndexBuffer.CreateRHIBuffer_Async();
	}
}

void FStaticMeshStreamIn::FIntermediateBuffers::SafeRelease()
{
	TangentsVertexBuffer.SafeRelease();
	TexCoordVertexBuffer.SafeRelease();
	PositionVertexBuffer.SafeRelease();
	ColorVertexBuffer.SafeRelease();
	IndexBuffer.SafeRelease();
	ReversedIndexBuffer.SafeRelease();
	DepthOnlyIndexBuffer.SafeRelease();
	ReversedDepthOnlyIndexBuffer.SafeRelease();
	WireframeIndexBuffer.SafeRelease();
}

void FStaticMeshStreamIn::FIntermediateBuffers::TransferBuffers(FStaticMeshLODResources& LODResource, FRHIResourceUpdateBatcher& Batcher)
{
	FStaticMeshVertexBuffers& VBs = LODResource.VertexBuffers;
	VBs.StaticMeshVertexBuffer.InitRHIForStreaming(TangentsVertexBuffer, TexCoordVertexBuffer, Batcher);
	VBs.PositionVertexBuffer.InitRHIForStreaming(PositionVertexBuffer, Batcher);
	VBs.ColorVertexBuffer.InitRHIForStreaming(ColorVertexBuffer, Batcher);
	LODResource.IndexBuffer.InitRHIForStreaming(IndexBuffer, Batcher);
	LODResource.DepthOnlyIndexBuffer.InitRHIForStreaming(DepthOnlyIndexBuffer, Batcher);

	if (LODResource.AdditionalIndexBuffers)
	{
		LODResource.AdditionalIndexBuffers->ReversedIndexBuffer.InitRHIForStreaming(ReversedIndexBuffer, Batcher);
		LODResource.AdditionalIndexBuffers->ReversedDepthOnlyIndexBuffer.InitRHIForStreaming(ReversedDepthOnlyIndexBuffer, Batcher);
		LODResource.AdditionalIndexBuffers->WireframeIndexBuffer.InitRHIForStreaming(WireframeIndexBuffer, Batcher);
	}
	SafeRelease();
}

void FStaticMeshStreamIn::FIntermediateBuffers::CheckIsNull() const
{
	check(!TangentsVertexBuffer
		&& !TexCoordVertexBuffer
		&& !PositionVertexBuffer
		&& !ColorVertexBuffer
		&& !IndexBuffer
		&& !ReversedIndexBuffer
		&& !DepthOnlyIndexBuffer
		&& !ReversedDepthOnlyIndexBuffer
		&& !WireframeIndexBuffer);
}

FStaticMeshStreamIn::FStaticMeshStreamIn(const UStaticMesh* InMesh)
	: FStaticMeshUpdate(InMesh)
{}

FStaticMeshStreamIn::~FStaticMeshStreamIn()
{
#if DO_CHECK
	for (int32 Idx = 0; Idx < MAX_MESH_LOD_COUNT; ++Idx)
	{
		IntermediateBuffersArray[Idx].CheckIsNull();
	}
#endif
}

template <bool bRenderThread>
void FStaticMeshStreamIn::CreateBuffers_Internal(const FContext& Context)
{
	LLM_SCOPE(ELLMTag::StaticMesh);
	
	FStaticMeshRenderData* RenderData = Context.RenderData;
	if (!IsCancelled() && RenderData)
	{
		for (int32 LODIdx = PendingFirstLODIdx; LODIdx < CurrentFirstLODIdx; ++LODIdx)
		{
			FStaticMeshLODResources& LODResource = *Context.LODResourcesView[LODIdx];
			if (bRenderThread)
			{
				IntermediateBuffersArray[LODIdx].CreateFromCPUData_RenderThread(LODResource);
			}
			else
			{
				IntermediateBuffersArray[LODIdx].CreateFromCPUData_Async(LODResource);
			}

#if RHI_RAYTRACING
			if (IsRayTracingEnabled() && Context.Mesh->bSupportRayTracing &&
				LODResource.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() > 0)
			{
				const FName OwnerName = UStaticMesh::GetLODPathName(Context.Mesh, LODIdx);

				FRayTracingGeometryInitializer Initializer;
				if (Context.Mesh->HasValidNaniteData() && Nanite::GetSupportsRayTracingProceduralPrimitive(GMaxRHIShaderPlatform))
				{
					FStaticMeshLODResources::SetupRayTracingProceduralGeometryInitializer(Initializer, Context.Mesh->GetFName(), OwnerName);
				}
				else
				{
					Context.LODResourcesView[LODIdx]->SetupRayTracingGeometryInitializer(Initializer, Context.Mesh->GetFName(), OwnerName);
				}
				Initializer.Type = ERayTracingGeometryInitializerType::StreamingSource;
				IntermediateRayTracingGeometry[LODIdx].SetInitializer(Initializer);
				IntermediateRayTracingGeometry[LODIdx].CreateRayTracingGeometryFromCPUData(LODResource.RayTracingGeometry.RawData);
			}
#endif
		}
	}
}

void FStaticMeshStreamIn::CreateBuffers_RenderThread(const FContext& Context)
{
	check(Context.CurrentThread == TT_Render);
	CreateBuffers_Internal<true>(Context);
}

void FStaticMeshStreamIn::CreateBuffers_Async(const FContext& Context)
{
	check(Context.CurrentThread == TT_Async);
	CreateBuffers_Internal<false>(Context);
}

void FStaticMeshStreamIn::DiscardNewLODs(const FContext& Context)
{
	FStaticMeshRenderData* RenderData = Context.RenderData;
	if (RenderData)
	{
		for (int32 LODIdx = PendingFirstLODIdx; LODIdx < CurrentFirstLODIdx; ++LODIdx)
		{
			FStaticMeshLODResources& LODResource = *Context.LODResourcesView[LODIdx];
			LODResource.DiscardCPUData();
		}
	}
}

void FStaticMeshStreamIn::DoFinishUpdate(const FContext& Context)
{
	FStaticMeshRenderData* RenderData = Context.RenderData;
	if (!IsCancelled() && RenderData)
	{
		check(Context.CurrentThread == TT_Render);
		// Use a scope to flush the batcher before updating CurrentFirstLODIdx
		{
			TRHIResourceUpdateBatcher<GStaticMeshMaxNumResourceUpdatesPerBatch> Batcher;

			for (int32 LODIdx = PendingFirstLODIdx; LODIdx < CurrentFirstLODIdx; ++LODIdx)
			{
				FStaticMeshLODResources& LODResource = *Context.LODResourcesView[LODIdx];
				LODResource.IncrementMemoryStats();
				IntermediateBuffersArray[LODIdx].TransferBuffers(LODResource, Batcher);

#if RHI_RAYTRACING
				if (IsRayTracingEnabled() && Context.Mesh->bSupportRayTracing &&
					LODResource.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() > 0)
				{					
					check(IntermediateRayTracingGeometry[LODIdx].RayTracingGeometryRHI != nullptr);
					LODResource.RayTracingGeometry.InitRHIForStreaming(IntermediateRayTracingGeometry[LODIdx].RayTracingGeometryRHI, Batcher);

					LODResource.RayTracingGeometry.SetRequiresBuild(IntermediateRayTracingGeometry[LODIdx].GetRequiresBuild());

					IntermediateRayTracingGeometry[LODIdx].Initializer = {};
					IntermediateRayTracingGeometry[LODIdx].RayTracingGeometryRHI.SafeRelease();				
				}
#endif
			}
		}

#if RHI_RAYTRACING
		// Must happen after the batched updates have been flushed
		if (IsRayTracingAllowed() && Context.Mesh->bSupportRayTracing)
		{
			for (int32 LODIndex = PendingFirstLODIdx; LODIndex < CurrentFirstLODIdx; ++LODIndex)
			{
				FStaticMeshLODResources& LODResource = *Context.LODResourcesView[LODIndex];

				// Skip LODs that have their render data stripped
				if (LODResource.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() > 0)
				{
					const FName OwnerName = UStaticMesh::GetLODPathName(Context.Mesh, LODIndex);

					// Rebuild the initializer because it could have been reset during a previous release
					FRayTracingGeometryInitializer Initializer;
					if (Context.Mesh->HasValidNaniteData() && Nanite::GetSupportsRayTracingProceduralPrimitive(GMaxRHIShaderPlatform))
					{
						FStaticMeshLODResources::SetupRayTracingProceduralGeometryInitializer(Initializer, Context.Mesh->GetFName(), OwnerName);
					}
					else
					{
						LODResource.SetupRayTracingGeometryInitializer(Initializer, Context.Mesh->GetFName(), OwnerName);
					}
					LODResource.RayTracingGeometry.SetInitializer(Initializer);
					LODResource.RayTracingGeometry.SetAsStreamedIn();			

					// Under very rare circumstances that we switch ray tracing on/off right in the middle of streaming RayTracingGeometryRHI might not be valid.
					if (IsRayTracingEnabled() && LODResource.RayTracingGeometry.RayTracingGeometryRHI.IsValid())
					{
						LODResource.RayTracingGeometry.RequestBuildIfNeeded(ERTAccelerationStructureBuildPriority::Normal);
					}
				}
			}

		}
#endif
				
		Context.Mesh->RequestUpdateCachedRenderState();
		RenderData->CurrentFirstLODIdx = ResourceState.LODCountToAssetFirstLODIdx(ResourceState.NumRequestedLODs);
		MarkAsSuccessfullyFinished();
	}
	else
	{
		for (int32 LODIdx = PendingFirstLODIdx; LODIdx < CurrentFirstLODIdx; ++LODIdx)
		{
			IntermediateBuffersArray[LODIdx].SafeRelease();

#if RHI_RAYTRACING
			IntermediateRayTracingGeometry[LODIdx].Initializer = {};
			IntermediateRayTracingGeometry[LODIdx].RayTracingGeometryRHI.SafeRelease();
#endif
		}
	}
}

void FStaticMeshStreamIn::DoCancel(const FContext& Context)
{
	// TODO: support streaming CPU data for editor builds
	if (!FPlatformProperties::HasEditorOnlyData())
	{
		DiscardNewLODs(Context);
	}
	DoFinishUpdate(Context);
}

FStaticMeshStreamOut::FStaticMeshStreamOut(const UStaticMesh* InMesh, bool InDiscardCPUData)
	: FStaticMeshUpdate(InMesh)
	, bDiscardCPUData(InDiscardCPUData)
{
	check(InMesh);

	// Immediately change CurrentFirstLODIdx to prevent new references from being made to the streamed out lods.
	// #TODO RenderData under a const UStaticMesh should stay const
	FStaticMeshRenderData* RenderData = const_cast<UStaticMesh*>(InMesh)->GetRenderData();
	if (RenderData)
	{
		RenderData->CurrentFirstLODIdx = ResourceState.LODCountToAssetFirstLODIdx(ResourceState.NumRequestedLODs);
	}

	if (InDiscardCPUData)
	{
		PushTask(FContext(InMesh, TT_None), TT_Async, SRA_UPDATE_CALLBACK(CheckReferencesAndDiscardCPUData), TT_Async, SRA_UPDATE_CALLBACK(Cancel));
	}
	else
	{
		PushTask(FContext(InMesh, TT_None), TT_Render, SRA_UPDATE_CALLBACK(ReleaseRHIBuffers), TT_Async, SRA_UPDATE_CALLBACK(Cancel));
	}
}

void FStaticMeshStreamOut::CheckReferencesAndDiscardCPUData(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FStaticMeshStreamOut::CheckReferencesAndDiscardCPUData"), STAT_StaticMeshStreamOut_CheckReferencesAndDiscardCPUData, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	const UStaticMesh* Mesh = Context.Mesh;
	FStaticMeshRenderData* RenderData = Context.RenderData;
	uint32 NumExternalReferences = 0;

	if (Mesh && RenderData)
	{
		for (int32 LODIdx = CurrentFirstLODIdx; LODIdx < PendingFirstLODIdx; ++LODIdx)
		{
			// Minus 1 since the LODResources reference is not considered external
			NumExternalReferences += Context.LODResourcesView[LODIdx]->GetRefCount() - 1;
		}

		if (NumExternalReferences > PreviousNumberOfExternalReferences && NumReferenceChecks > 0)
		{
			PreviousNumberOfExternalReferences = NumExternalReferences;
			UE_LOG(LogContentStreaming, Warning, TEXT("[%s] Streamed out LODResources got referenced while in pending stream out."), *Mesh->GetName());
		}
	}

	if (!NumExternalReferences || NumReferenceChecks >= GStreamingMaxReferenceChecks)
	{
		if (RenderData)
		{
			for (int32 LODIdx = CurrentFirstLODIdx; LODIdx < PendingFirstLODIdx; ++LODIdx)
			{
				Context.LODResourcesView[LODIdx]->DiscardCPUData();
			}
		}

		// Because we discarded the CPU data, the stream out can not be cancelled anymore.
		PushTask(Context, TT_Render, SRA_UPDATE_CALLBACK(ReleaseRHIBuffers), TT_Render, SRA_UPDATE_CALLBACK(ReleaseRHIBuffers));
	}
	else
	{
		++NumReferenceChecks;
		if (NumReferenceChecks >= GStreamingMaxReferenceChecks)
		{
			UE_LOG(LogContentStreaming, Warning, TEXT("[%s] Streamed out LODResources references are not getting released."), *Mesh->GetName());
		}

		bDeferExecution = true;
		PushTask(Context, TT_Async, SRA_UPDATE_CALLBACK(CheckReferencesAndDiscardCPUData), TT_Async, SRA_UPDATE_CALLBACK(Cancel));
	}
}

void FStaticMeshStreamOut::ReleaseRHIBuffers(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FStaticMeshStreamOut::ReleaseRHIBuffers"), STAT_StaticMeshStreamOut_ReleaseRHIBuffers, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Render);

	FStaticMeshRenderData* RenderData = Context.RenderData;
	if (RenderData)
	{
		TRHIResourceUpdateBatcher<GStaticMeshMaxNumResourceUpdatesPerBatch> Batcher;
		for (int32 LODIdx = CurrentFirstLODIdx; LODIdx < PendingFirstLODIdx; ++LODIdx)
		{
			FStaticMeshLODResources& LODResource = *Context.LODResourcesView[LODIdx];
			LODResource.DecrementMemoryStats();
			LODResource.ReleaseRHIForStreaming(Batcher);
			
#if RHI_RAYTRACING
			if (IsRayTracingAllowed())
			{
				LODResource.RayTracingGeometry.ReleaseRHIForStreaming(Batcher);
			}
#endif
		}

		Context.Mesh->RequestUpdateCachedRenderState();
	}
	MarkAsSuccessfullyFinished();
}

void FStaticMeshStreamOut::Cancel(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FStaticMeshStreamOut::Cancel"), STAT_StaticMeshStreamOut_Cancel, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	if (Context.RenderData)
	{
		Context.RenderData->CurrentFirstLODIdx = ResourceState.LODCountToAssetFirstLODIdx(ResourceState.NumResidentLODs);
	}
}

void FStaticMeshStreamIn_IO::FCancelIORequestsTask::DoWork()
{
	check(PendingUpdate);
	// Acquire the lock of this object in order to cancel any pending IO.
	// If the object is currently being ticked, wait.
	const ETaskState PreviousTaskState = PendingUpdate->DoLock();
	PendingUpdate->CancelIORequest();
	PendingUpdate->DoUnlock(PreviousTaskState);
}

FStaticMeshStreamIn_IO::FStaticMeshStreamIn_IO(const UStaticMesh* InMesh, bool bHighPrio)
	: FStaticMeshStreamIn(InMesh)
	, bHighPrioIORequest(bHighPrio)
{}

void FStaticMeshStreamIn_IO::Abort()
{
	if (!IsCancelled() && !IsCompleted())
	{
		FStaticMeshStreamIn::Abort();

		if (BulkDataRequest.IsPending())
		{
			// Prevent the update from being considered done before this is finished.
			// By checking that it was not already cancelled, we make sure this doesn't get called twice.
			(new FAsyncCancelIORequestsTask(this))->StartBackgroundTask();
		}
	}
}

void FStaticMeshStreamIn_IO::SetIORequest(const FContext& Context)
{
	if (IsCancelled())
	{
		return;
	}

	check(BulkDataRequest.IsNone() && PendingFirstLODIdx < CurrentFirstLODIdx);

	const UStaticMesh* Mesh = Context.Mesh;
	FStaticMeshRenderData* RenderData = Context.RenderData;
	if (Mesh && RenderData)
	{
		const int32 BatchCount = CurrentFirstLODIdx - PendingFirstLODIdx;
		FBulkDataBatchRequest::FScatterGatherBuilder Batch = FBulkDataBatchRequest::ScatterGather(BatchCount);
		for (int32 Index = PendingFirstLODIdx; Index < CurrentFirstLODIdx; ++Index)
		{
			Batch.Read(Context.LODResourcesView[Index]->StreamingBulkData);
		}
		
		// Increment as we push the request. If a request complete immediately, then it will call the callback
		// but that won't do anything because the tick would not try to acquire the lock since it is already locked.
		TaskSynchronization.Increment();

		const EAsyncIOPriorityAndFlags Priority = (EAsyncIOPriorityAndFlags)FMath::Clamp<int32>(GStreamingStaticMeshIOPriority + (bHighPrioIORequest ? 1 : 0), AIOP_Low, AIOP_High);

		Batch.Issue(BulkData, Priority, [this](FBulkDataRequest::EStatus Status)
		{
			TaskSynchronization.Decrement();

			if (FBulkDataRequest::EStatus::Ok != Status)
			{
				// If IO requests was cancelled but the streaming request wasn't, this is an IO error.
				if (!bIsCancelled)
				{
					bFailedOnIOError = true;
				}
				MarkAsCancelled();
			}

#if !UE_BUILD_SHIPPING
			// On some platforms the IO is too fast to test cancelation requests timing issues.
			if (FRenderAssetStreamingSettings::ExtraIOLatency > 0 && TaskSynchronization.GetValue() == 0)
			{
				FPlatformProcess::Sleep(FRenderAssetStreamingSettings::ExtraIOLatency * .001f); // Slow down the streaming.
			}
#endif
			// The tick here is intended to schedule the success or cancel callback.
			// Using TT_None ensure gets which could create a dead lock.
			Tick(FStaticMeshUpdate::TT_None);
		},
		BulkDataRequest);
	}
	else
	{
		MarkAsCancelled();
	}
} 

void FStaticMeshStreamIn_IO::ClearIORequest(const FContext& Context)
{
	if (BulkDataRequest.IsPending())
	{
		BulkDataRequest.Cancel();
		BulkDataRequest.Wait();
	}
	
	BulkDataRequest = FBulkDataBatchRequest();
	BulkData = FIoBuffer();
}

void FStaticMeshStreamIn_IO::ReportIOError(const FContext& Context)
{
	// Invalidate the cache state of all initial mips (note that when using FIoChunkId each mip has a different value).
	if (bFailedOnIOError && Context.Mesh)
	{
		IRenderAssetStreamingManager& StreamingManager = IStreamingManager::Get().GetTextureStreamingManager();
		for (int32 MipIndex = 0; MipIndex < CurrentFirstLODIdx; ++MipIndex)
		{
			StreamingManager.MarkMountedStateDirty(Context.Mesh->GetMipIoFilenameHash(MipIndex));
		}

		UE_LOG(LogContentStreaming, Warning, TEXT("[%s] StaticMesh stream in request failed due to IO error (LOD %d-%d)."), *Context.Mesh->GetName(), PendingFirstLODIdx, CurrentFirstLODIdx - 1);
	}
}

void FStaticMeshStreamIn_IO::SerializeLODData(const FContext& Context)
{
	LLM_SCOPE(ELLMTag::StaticMesh);

	check(!TaskSynchronization.GetValue());
	const UStaticMesh* Mesh = Context.Mesh;
	FStaticMeshRenderData* RenderData = Context.RenderData;
	if (!IsCancelled() && Mesh && RenderData)
	{
		check(BulkData.GetSize() >= 0 && BulkData.GetSize() <= TNumericLimits<uint32>::Max());

		FMemoryReaderView Ar(BulkData.GetView(), true);
		for (int32 LODIdx = PendingFirstLODIdx; LODIdx < CurrentFirstLODIdx; ++LODIdx)
		{
			FStaticMeshLODResources& LODResource = *Context.LODResourcesView[LODIdx];
			constexpr uint8 DummyStripFlags = 0;
			typename FStaticMeshLODResources::FStaticMeshBuffersSize DummyBuffersSize;
			LODResource.SerializeBuffers(Ar, const_cast<UStaticMesh*>(Mesh), DummyStripFlags, DummyBuffersSize);
			check(DummyBuffersSize.CalcBuffersSize() == LODResource.BuffersSize);
		}

		BulkData = FIoBuffer();
	}
}

void FStaticMeshStreamIn_IO::Cancel(const FContext& Context)
{
	DoCancel(Context);
	ReportIOError(Context);
}

void FStaticMeshStreamIn_IO::CancelIORequest()
{
	if (BulkDataRequest.IsPending())
	{
		BulkDataRequest.Cancel();
	}
}

template <bool bRenderThread>
TStaticMeshStreamIn_IO<bRenderThread>::TStaticMeshStreamIn_IO(const UStaticMesh* InMesh, bool bHighPrio)
	: FStaticMeshStreamIn_IO(InMesh, bHighPrio)
{
	PushTask(FContext(InMesh, TT_None), TT_Async, SRA_UPDATE_CALLBACK(DoInitiateIO), TT_None, nullptr);
}

template <bool bRenderThread>
void TStaticMeshStreamIn_IO<bRenderThread>::DoInitiateIO(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TStaticMeshStreamIn_IO::DoInitiateIO"), STAT_StaticMeshStreamInIO_DoInitiateIO, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);

	SetIORequest(Context);

	PushTask(Context, TT_Async, SRA_UPDATE_CALLBACK(DoSerializeLODData), TT_Async, SRA_UPDATE_CALLBACK(DoCancelIO));
}

template <bool bRenderThread>
void TStaticMeshStreamIn_IO<bRenderThread>::DoSerializeLODData(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TStaticMeshStreamIn_IO::DoSerializeLODData"), STAT_StaticMeshStreamInIO_DoSerializeLODData, STATGROUP_StreamingDetails);
	check(Context.CurrentThread == TT_Async);
	SerializeLODData(Context);
	ClearIORequest(Context);
	const EThreadType TThread = bRenderThread ? TT_Render : TT_Async;
	const EThreadType CThread = (EThreadType)Context.CurrentThread;
	PushTask(Context, TThread, SRA_UPDATE_CALLBACK(DoCreateBuffers), CThread, SRA_UPDATE_CALLBACK(Cancel));
}

template <bool bRenderThread>
void TStaticMeshStreamIn_IO<bRenderThread>::DoCreateBuffers(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TStaticMeshStreamIn_IO::DoCreateBuffers"), STAT_StaticMeshStreamInIO_DoCreateBuffers, STATGROUP_StreamingDetails);
	if (bRenderThread)
	{
		CreateBuffers_RenderThread(Context);
	}
	else
	{
		CreateBuffers_Async(Context);
	}
	check(!TaskSynchronization.GetValue());
	PushTask(Context, TT_Render, SRA_UPDATE_CALLBACK(DoFinishUpdate), (EThreadType)Context.CurrentThread, SRA_UPDATE_CALLBACK(Cancel));
}

template <bool bRenderThread>
void TStaticMeshStreamIn_IO<bRenderThread>::DoCancelIO(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TStaticMeshStreamIn_IO::DoCancelIO"), STAT_StaticMeshStreamInIO_DoCancelIO, STATGROUP_StreamingDetails);
	ClearIORequest(Context);
	PushTask(Context, TT_None, nullptr, (EThreadType)Context.CurrentThread, SRA_UPDATE_CALLBACK(Cancel));
}

template class TStaticMeshStreamIn_IO<true>;
template class TStaticMeshStreamIn_IO<false>;

#if WITH_EDITOR
FStaticMeshStreamIn_DDC::FStaticMeshStreamIn_DDC(const UStaticMesh* InMesh)
	: FStaticMeshStreamIn(InMesh)
{}

void FStaticMeshStreamIn_DDC::LoadNewLODsFromDDC(const FContext& Context)
{
	check(Context.CurrentThread == TT_Async);
	// TODO: support streaming CPU data for editor builds
}

template <bool bRenderThread>
TStaticMeshStreamIn_DDC<bRenderThread>::TStaticMeshStreamIn_DDC(const UStaticMesh* InMesh)
	: FStaticMeshStreamIn_DDC(InMesh)
{
	PushTask(FContext(InMesh, TT_None), TT_Async, SRA_UPDATE_CALLBACK(DoLoadNewLODsFromDDC), TT_None, nullptr);
}

template <bool bRenderThread>
void TStaticMeshStreamIn_DDC<bRenderThread>::DoLoadNewLODsFromDDC(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TStaticMeshStreamInDDC::DoLoadNewLODsFromDDC"), STAT_StaticMeshStreamInDDC_DoLoadNewLODsFromDDC, STATGROUP_StreamingDetails);
	LoadNewLODsFromDDC(Context);
	check(!TaskSynchronization.GetValue());
	const EThreadType TThread = bRenderThread ? TT_Render : TT_Async;
	const EThreadType CThread = (EThreadType)Context.CurrentThread;
	PushTask(Context, TThread, SRA_UPDATE_CALLBACK(DoCreateBuffers), CThread, SRA_UPDATE_CALLBACK(DoCancel));
}

template <bool bRenderThread>
void TStaticMeshStreamIn_DDC<bRenderThread>::DoCreateBuffers(const FContext& Context)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("TStaticMeshStreamInDDC::DoCreateBuffers"), STAT_StaticMeshStreamInDDC_DoCreateBuffers, STATGROUP_StreamingDetails);
	if (bRenderThread)
	{
		CreateBuffers_RenderThread(Context);
	}
	else
	{
		CreateBuffers_Async(Context);
	}
	check(!TaskSynchronization.GetValue());
	PushTask(Context, TT_Render, SRA_UPDATE_CALLBACK(DoFinishUpdate), (EThreadType)Context.CurrentThread, SRA_UPDATE_CALLBACK(DoCancel));
}

template class TStaticMeshStreamIn_DDC<true>;
template class TStaticMeshStreamIn_DDC<false>;
#endif
