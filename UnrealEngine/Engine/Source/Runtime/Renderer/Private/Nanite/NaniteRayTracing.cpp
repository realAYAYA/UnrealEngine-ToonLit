// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteRayTracing.h"

#if RHI_RAYTRACING

#include "Rendering/NaniteStreamingManager.h"

#include "NaniteStreamOut.h"
#include "NaniteSceneProxy.h"
#include "NaniteShared.h"

#include "ShaderPrintParameters.h"

#include "PrimitiveSceneInfo.h"
#include "ScenePrivate.h"
#include "SceneInterface.h"

#include "RenderGraphUtils.h"

/*
* TODO:
* - StagingAuxiliaryDataBuffer
*	- Keep track of how many pages/clusters are streamed-in per resource
*		and allocate less staging memory than the very conservative (Data.NumClusters * NANITE_MAX_CLUSTER_TRIANGLES)
*	- Warn user if GNaniteRayTracingMaxStagingBufferSizeMB is not large enough for a specific mesh cut
* 
* - Defragment AuxiliaryDataBuffer
* 
* - VB/IB Buffers
*	- Resize VB/IB buffers dynamically instead of always allocating max size
*	- Warn user if max VB/IB buffer size are not large enough for a specific mesh cut
*	- Store vertices and indices in the same buffer in a single allocation
*/

static bool GNaniteRayTracingUpdate = true;
static FAutoConsoleVariableRef CVarNaniteRayTracingUpdate(
	TEXT("r.RayTracing.Nanite.Update"),
	GNaniteRayTracingUpdate,
	TEXT("Whether to process Nanite RayTracing update requests."),
	ECVF_RenderThreadSafe
);

static bool GNaniteRayTracingForceUpdateVisible = false;
static FAutoConsoleVariableRef CVarNaniteRayTracingForceUpdateVisible(
	TEXT("r.RayTracing.Nanite.ForceUpdateVisible"),
	GNaniteRayTracingForceUpdateVisible,
	TEXT("Force BLAS of visible primitives to be updated next frame."),
	ECVF_RenderThreadSafe
);

static float GNaniteRayTracingCutError = 0.0f;
static FAutoConsoleVariableRef CVarNaniteRayTracingCutError(
	TEXT("r.RayTracing.Nanite.CutError"),
	GNaniteRayTracingCutError,
	TEXT("Global target cut error to control quality when using procedural raytracing geometry for Nanite meshes."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteRayTracingMaxNumVertices = 16 * 1024 * 1024;
static FAutoConsoleVariableRef CVarNaniteRayTracingMaxNumVertices(
	TEXT("r.RayTracing.Nanite.StreamOut.MaxNumVertices"),
	GNaniteRayTracingMaxNumVertices,
	TEXT("Max number of vertices to stream out per frame."),
	ECVF_ReadOnly
);

static int32 GNaniteRayTracingMaxNumIndices = 64 * 1024 * 1024;
static FAutoConsoleVariableRef CVarNaniteRayTracingMaxNumIndices(
	TEXT("r.RayTracing.Nanite.StreamOut.MaxNumIndices"),
	GNaniteRayTracingMaxNumIndices,
	TEXT("Max number of indices to stream out per frame."),
	ECVF_ReadOnly
);

static int32 GNaniteRayTracingMaxBuiltPrimitivesPerFrame = 8 * 1024 * 1024;
static FAutoConsoleVariableRef CVarNaniteRayTracingMaxBuiltPrimitivesPerFrame(
	TEXT("r.RayTracing.Nanite.MaxBuiltPrimitivesPerFrame"),
	GNaniteRayTracingMaxBuiltPrimitivesPerFrame,
	TEXT("Limit number of BLAS built per frame based on a budget defined in terms of maximum number of triangles."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteRayTracingMaxStagingBufferSizeMB = 1024;
static FAutoConsoleVariableRef CVarNaniteRayTracingMaxStagingBufferSizeMB(
	TEXT("r.RayTracing.Nanite.MaxStagingBufferSizeMB"),
	GNaniteRayTracingMaxStagingBufferSizeMB,
	TEXT("Limit the size of the staging buffer used during stream out (lower values can cause updates to be throttled)\n")
	TEXT("Default   = 1024 MB.\n")
	TEXT("Max value = 2048 MB."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteRayTracingBLASScratchSizeMultipleMB = 64;
static FAutoConsoleVariableRef CVarNaniteRayTracingBLASScratchSizeMultipleMBMB(
	TEXT("r.RayTracing.Nanite.BLASScratchSizeMultipleMB"),
	GNaniteRayTracingBLASScratchSizeMultipleMB,
	TEXT("Round the size of the BLAS build scratch buffer to be a multiple of this value.\n")
	TEXT("This helps maintain consistent memory usage and prevent memory usage spikes.\n")
	TEXT("Default = 64 MB."),
	ECVF_RenderThreadSafe
);

static bool GNaniteRayTracingProfileStreamOut = false;
static FAutoConsoleVariableRef CVarNaniteRayTracingProfileStreamOut(
	TEXT("r.RayTracing.Nanite.ProfileStreamOut"),
	GNaniteRayTracingProfileStreamOut,
	TEXT("[Development only] Stream out pending requests every frame in order to measure performance."),
	ECVF_RenderThreadSafe
);

DECLARE_GPU_STAT(RebuildNaniteBLAS);

DECLARE_STATS_GROUP(TEXT("Nanite RayTracing"), STATGROUP_NaniteRayTracing, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("In-flight Updates"), STAT_NaniteRayTracingInFlightUpdates, STATGROUP_NaniteRayTracing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Stream Out Requests"), STAT_NaniteRayTracingStreamOutRequests, STATGROUP_NaniteRayTracing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Failed Stream Out Requests"), STAT_NaniteRayTracingFailedStreamOutRequests, STATGROUP_NaniteRayTracing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Scheduled Builds"), STAT_NaniteRayTracingScheduledBuilds, STATGROUP_NaniteRayTracing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Scheduled Builds - Num Primitives"), STAT_NaniteRayTracingScheduledBuildsNumPrimitives, STATGROUP_NaniteRayTracing);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Pending Builds"), STAT_NaniteRayTracingPendingBuilds, STATGROUP_NaniteRayTracing);
DECLARE_MEMORY_STAT(TEXT("Auxiliary Data Buffer"), STAT_NaniteRayTracingAuxiliaryDataBuffer, STATGROUP_NaniteRayTracing);
DECLARE_MEMORY_STAT(TEXT("Staging Auxiliary Data Buffer"), STAT_NaniteRayTracingStagingAuxiliaryDataBuffer, STATGROUP_NaniteRayTracing);

static const uint32 MinAuxiliaryBufferEntries = 4 * 1024 * 1024; // buffer size will be 16MB

namespace Nanite
{
	static FRDGBufferRef ResizeBufferIfNeeded(FRDGBuilder& GraphBuilder, TRefCountPtr<FRDGPooledBuffer>& ExternalBuffer, uint32 BytesPerElement, uint32 NumElements, const TCHAR* Name, bool bCopy, EAllowShrinking AllowShrinking)
	{
		FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(BytesPerElement, NumElements);

		FRDGBufferRef BufferRDG = GraphBuilder.RegisterExternalBuffer(ExternalBuffer);

		if (BufferDesc.GetSize() > BufferRDG->GetSize()) // grow
		{
			FRDGBufferRef SrcBufferRDG = BufferRDG;

			BufferRDG = GraphBuilder.CreateBuffer(BufferDesc, Name);

			if (bCopy)
			{
				AddCopyBufferPass(GraphBuilder, BufferRDG, SrcBufferRDG);
			}
		}
		else if (AllowShrinking == EAllowShrinking::Yes && BufferDesc.GetSize() / 2 < BufferRDG->GetSize()) // shrink
		{
			FRDGBufferRef SrcBufferRDG = BufferRDG;

			BufferRDG = GraphBuilder.CreateBuffer(BufferDesc, Name);

			if (bCopy)
			{
				const uint64 NumBytes = BufferDesc.NumElements * BufferDesc.BytesPerElement;
				AddCopyBufferPass(GraphBuilder, BufferRDG, 0, SrcBufferRDG, 0, NumBytes);
			}
		}

		return BufferRDG;
	}

	static uint32 CalculateAuxiliaryDataSizeInUints(uint32 NumTriangles)
	{
		return NumTriangles; // (one uint per triangle)
	}

	FRayTracingManager::FRayTracingManager()
	{

	}

	FRayTracingManager::~FRayTracingManager()
	{

	}

	void FRayTracingManager::InitRHI(FRHICommandListBase&)
	{
		if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
		{
			return;
		}

		AuxiliaryDataBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 8), TEXT("NaniteRayTracing.AuxiliaryDataBuffer"));
		SET_MEMORY_STAT(STAT_NaniteRayTracingAuxiliaryDataBuffer, AuxiliaryDataBuffer->GetSize());

		StagingAuxiliaryDataBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 8), TEXT("NaniteRayTracing.StagingAuxiliaryDataBuffer"));
		SET_MEMORY_STAT(STAT_NaniteRayTracingStagingAuxiliaryDataBuffer, StagingAuxiliaryDataBuffer->GetSize());

		ReadbackBuffers.SetNum(MaxReadbackBuffers);

		for (auto& ReadbackData : ReadbackBuffers)
		{
			ReadbackData.MeshDataReadbackBuffer = new FRHIGPUBufferReadback(TEXT("NaniteRayTracing.MeshDataReadbackBuffer"));
		}

		FNaniteRayTracingUniformParameters Params = {};
		// Use AuxiliaryDataBuffer as placeholder when creating the uniform buffer
		// This is later updated with the correct SRVs
		Params.ClusterPageData = AuxiliaryDataBuffer->GetSRV();
		Params.HierarchyBuffer = AuxiliaryDataBuffer->GetSRV();
		Params.RayTracingDataBuffer = AuxiliaryDataBuffer->GetSRV();

		UniformBuffer = TUniformBufferRef<FNaniteRayTracingUniformParameters>::CreateUniformBufferImmediate(Params, UniformBuffer_MultiFrame);

		bInitialized = true;
	}

	void FRayTracingManager::ReleaseRHI()
	{
		if (!bInitialized)
		{
			return;
		}

		bInitialized = false;

		VertexBuffer.SafeRelease();
		IndexBuffer.SafeRelease();

		UniformBuffer.SafeRelease();

		for (auto& ReadbackData : ReadbackBuffers)
		{
			delete ReadbackData.MeshDataReadbackBuffer;
			ReadbackData.MeshDataReadbackBuffer = nullptr;
		}

		ReadbackBuffers.Empty();
		StagingAuxiliaryDataBuffer.SafeRelease();
		AuxiliaryDataBuffer.SafeRelease();
	}

	void FRayTracingManager::Add(FPrimitiveSceneInfo* SceneInfo)
	{
		if (!IsRayTracingAllowed())
		{
			return;
		}

		auto NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(SceneInfo->Proxy);

		uint32 NaniteResourceID = INDEX_NONE;
		uint32 NaniteHierarchyOffset = INDEX_NONE;
		uint32 NaniteImposterIndex = INDEX_NONE;
		NaniteProxy->GetNaniteResourceInfo(NaniteResourceID, NaniteHierarchyOffset, NaniteImposterIndex);

		FUint32Vector2 NaniteMaterialMask;
		NaniteProxy->GetNaniteMaterialMask(NaniteMaterialMask); // TODO: Implement support

		// TODO: Should use both ResourceID and HierarchyOffset as identifier for raytracing geometry
		// For example, FNaniteGeometryCollectionSceneProxy can use the same ResourceID with different HierarchyOffsets
		// (FNaniteGeometryCollectionSceneProxy are not supported in raytracing yet)
		uint32& Id = ResourceToRayTracingIdMap.FindOrAdd(NaniteResourceID, INDEX_NONE);

		FInternalData* Data;

		if (Id == INDEX_NONE)
		{
			Nanite::FResourceMeshInfo MeshInfo = NaniteProxy->GetResourceMeshInfo();
			check(MeshInfo.NumClusters);

			Data = new FInternalData;

			Data->ResourceId = NaniteResourceID;
			Data->HierarchyOffset = NaniteHierarchyOffset;
			Data->NumClusters = MeshInfo.NumClusters;
			Data->NumNodes = MeshInfo.NumNodes;
			Data->NumVertices = MeshInfo.NumVertices;
			Data->NumTriangles = MeshInfo.NumTriangles;
			Data->NumMaterials = MeshInfo.NumMaterials;
			Data->NumSegments = MeshInfo.NumSegments;
			Data->SegmentMapping = MeshInfo.SegmentMapping;
			Data->DebugName = MeshInfo.DebugName;

			Data->NumResidentClusters = 0;
			Data->NumResidentClustersUpdate = MeshInfo.NumResidentClusters;

			Data->PrimitiveId = INDEX_NONE;

			Id = Geometries.Add(Data);

			if (Data->NumResidentClustersUpdate > 0)
			{
				// some clusters are already streamed in and RequestUpdates(...) is only called when new pages are streamed in/out
				// so request an update here to make sure we build ray tracing geometry with the currently available data
				UpdateRequests.Add(Id);
			}
		}
		else
		{
			Data = Geometries[Id];
		}

		Data->Primitives.Add(SceneInfo);

		if ((GetRayTracingMode() != ERayTracingMode::Fallback) && Data->RayTracingGeometryRHI)
		{
			// Patch CachedRayTracingInstance here since CacheRayTracingPrimitives(...) is called before Primitive is added to Nanite::FRayTracingManager
			SceneInfo->CachedRayTracingInstance.GeometryRHI = Data->RayTracingGeometryRHI;
		}

		PendingRemoves.Remove(Id);

		NaniteProxy->SetRayTracingId(Id);
		NaniteProxy->SetRayTracingDataOffset(Data->AuxiliaryDataOffset);
	}

	void FRayTracingManager::Remove(FPrimitiveSceneInfo* SceneInfo)
	{
		if (!IsRayTracingAllowed())
		{
			return;
		}

		auto NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(SceneInfo->Proxy);

		const uint32 Id = NaniteProxy->GetRayTracingId();
		check(Id != INDEX_NONE);

		FInternalData* Data = Geometries[Id];

		Data->Primitives.Remove(SceneInfo);
		if (Data->Primitives.IsEmpty())
		{
			PendingRemoves.Add(Id);
		}

		NaniteProxy->SetRayTracingId(INDEX_NONE);
		NaniteProxy->SetRayTracingDataOffset(INDEX_NONE);
	}

	void FRayTracingManager::RequestUpdates(const TMap<uint32, uint32>& InUpdateRequests)
	{
		if (!IsRayTracingAllowed())
		{
			return;
		}

		for (auto& Elem : InUpdateRequests)
		{
			uint32 RuntimeResourceID = Elem.Key;
			uint32* GeometryId = ResourceToRayTracingIdMap.Find(RuntimeResourceID);

			if (GeometryId != nullptr)
			{
				FInternalData& Data = *Geometries[*GeometryId];
				Data.NumResidentClustersUpdate = Elem.Value;
				check(Data.NumResidentClustersUpdate > 0);

				UpdateRequests.Add(*GeometryId);
			}
		}
	}

	void FRayTracingManager::AddVisiblePrimitive(const FPrimitiveSceneInfo* SceneInfo)
	{
		check(GetRayTracingMode() != ERayTracingMode::Fallback);

		auto NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(SceneInfo->Proxy);

		const uint32 Id = NaniteProxy->GetRayTracingId();
		check(Id != INDEX_NONE);

		FInternalData* Data = Geometries[Id];
		Data->PrimitiveId = SceneInfo->GetPersistentIndex().Index;

		VisibleGeometries.Add(Id);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FNaniteRayTracingPrimitivesParams, )
		RDG_BUFFER_ACCESS(Buffer0, ERHIAccess::SRVCompute)
		RDG_BUFFER_ACCESS(Buffer1, ERHIAccess::SRVCompute)
		RDG_BUFFER_ACCESS(ScratchBuffer, ERHIAccess::UAVCompute)
	END_SHADER_PARAMETER_STRUCT()

	void FRayTracingManager::ProcessUpdateRequests(FRDGBuilder& GraphBuilder, FSceneUniformBuffer &SceneUniformBuffer)
	{
		// D3D12 limits resources to 2048MB.
		GNaniteRayTracingMaxStagingBufferSizeMB = FMath::Min(GNaniteRayTracingMaxStagingBufferSizeMB, 2048);

		if (GNaniteRayTracingForceUpdateVisible)
		{
			UpdateRequests.Append(VisibleGeometries);
			GNaniteRayTracingForceUpdateVisible = false;
		}

		if (!GNaniteRayTracingUpdate || GetRayTracingMode() == ERayTracingMode::Fallback || bUpdating || UpdateRequests.IsEmpty())
		{
			VisibleGeometries.Empty();
			// TODO: shrink staging buffer
			return;
		}

		TSet<uint32> ToUpdate;

		uint32 NumMeshDataEntries = 0;
		uint32 NumAuxiliaryDataEntries = 0;
		uint32 NumSegmentMappingEntries = 0;

		for (uint32 GeometryId : VisibleGeometries)
		{
			if (UpdateRequests.Contains(GeometryId))
			{
				FInternalData& Data = *Geometries[GeometryId];

				check(Data.NumResidentClustersUpdate > 0);
 				//check(Data.NumResidentClustersUpdate <= Data.NumClusters); // Temporary workaround: NumClusters from cooked data is not always correct for Geometry Collections: UE-194917

				const uint64 NewNumAuxiliaryDataEntries = NumAuxiliaryDataEntries + CalculateAuxiliaryDataSizeInUints(Data.NumResidentClustersUpdate * NANITE_MAX_CLUSTER_TRIANGLES);
				const uint64 NewAuxiliaryDataBufferSize = NewNumAuxiliaryDataEntries * sizeof(uint32);

				if (NewAuxiliaryDataBufferSize >= (uint64)GNaniteRayTracingMaxStagingBufferSizeMB * 1024ull * 1024ull)
				{
					break;
				}

				check(NewAuxiliaryDataBufferSize <= (1u << 31)); // D3D12 limits resources to 2048MB.

				if (!GNaniteRayTracingProfileStreamOut) // don't remove request when profiling stream out
				{
					UpdateRequests.Remove(GeometryId);
				}
				ToUpdate.Add(GeometryId);

				Data.NumResidentClusters = Data.NumResidentClustersUpdate;

				check(!Data.bUpdating);
				Data.bUpdating = true;

				check(Data.BaseMeshDataOffset == -1);
				Data.BaseMeshDataOffset = NumMeshDataEntries;

				check(Data.StagingAuxiliaryDataOffset == INDEX_NONE);
				Data.StagingAuxiliaryDataOffset = NumAuxiliaryDataEntries;

				NumMeshDataEntries += (sizeof(FStreamOutMeshDataHeader) + sizeof(FStreamOutMeshDataSegment) * Data.NumSegments);
				NumAuxiliaryDataEntries = NewNumAuxiliaryDataEntries;
				NumSegmentMappingEntries += Data.SegmentMapping.Num();
			}
		}

		VisibleGeometries.Empty();

		if (ToUpdate.IsEmpty())
		{
			// TODO: shrink staging buffer
			return;
		}

		RDG_EVENT_SCOPE(GraphBuilder, "Nanite::FRayTracingManager::ProcessUpdateRequests");

		bUpdating = true;

		FReadbackData& ReadbackData = ReadbackBuffers[ReadbackBuffersWriteIndex];
		check(ReadbackData.Entries.IsEmpty());

		// Upload geometry data
		FRDGBufferRef RequestBuffer = nullptr;
		FRDGBufferRef SegmentMappingBuffer = nullptr;
		
		{
			FRDGUploadData<FStreamOutRequest> UploadData(GraphBuilder, ToUpdate.Num());
			FRDGUploadData<uint32> SegmentMappingUploadData(GraphBuilder, NumSegmentMappingEntries);

			uint32 Index = 0;
			uint32 SegmentMappingOffset = 0;

			for (auto GeometryId : ToUpdate)
			{
				const FInternalData& Data = *Geometries[GeometryId];

				FStreamOutRequest& Request = UploadData[Index];
				Request.PrimitiveId = Data.PrimitiveId;
				Request.NumMaterials = Data.NumMaterials;
				Request.NumSegments = Data.NumSegments;
				Request.SegmentMappingOffset = SegmentMappingOffset;
				Request.AuxiliaryDataOffset = Data.StagingAuxiliaryDataOffset;
				Request.MeshDataOffset = Data.BaseMeshDataOffset;

				for (uint32 SegmentIndex : Data.SegmentMapping)
				{
					SegmentMappingUploadData[SegmentMappingOffset] = SegmentIndex;
					++SegmentMappingOffset;
				}

				ReadbackData.Entries.Add(GeometryId);

				++Index;
			}

			INC_DWORD_STAT_BY(STAT_NaniteRayTracingInFlightUpdates, ToUpdate.Num());

			RequestBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("NaniteRayTracing.RequestBuffer"), UploadData);

			SegmentMappingBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("NaniteRayTracing.SegmentMappingBuffer"), SegmentMappingUploadData);
		}

		FRDGBufferDesc MeshDataBufferDesc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), FMath::Max(NumMeshDataEntries, 32U));
		MeshDataBufferDesc.Usage |= BUF_SourceCopy;

		FRDGBufferRef MeshDataBuffer = GraphBuilder.CreateBuffer(MeshDataBufferDesc, TEXT("NaniteRayTracing.MeshDataBuffer"));

		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(MeshDataBuffer), 0);

		FRDGBufferRef StagingAuxiliaryDataBufferRDG;

		{
			const uint32 BufferNumAuxiliaryDataEntries = FMath::Max(NumAuxiliaryDataEntries, MinAuxiliaryBufferEntries);
			const bool bCopy = false;
			StagingAuxiliaryDataBufferRDG = ResizeBufferIfNeeded(GraphBuilder, StagingAuxiliaryDataBuffer, sizeof(uint32), BufferNumAuxiliaryDataEntries, TEXT("NaniteRayTracing.StagingAuxiliaryDataBuffer"), bCopy, EAllowShrinking::Yes);
			StagingAuxiliaryDataBuffer = GraphBuilder.ConvertToExternalBuffer(StagingAuxiliaryDataBufferRDG);

			SET_MEMORY_STAT(STAT_NaniteRayTracingStagingAuxiliaryDataBuffer, StagingAuxiliaryDataBufferRDG->GetSize());
		}

		if (VertexBuffer == nullptr)
		{
			VertexBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(float), GNaniteRayTracingMaxNumVertices * 3), TEXT("NaniteRayTracing.VertexBuffer"));
		}

		if (IndexBuffer == nullptr)
		{
			IndexBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), GNaniteRayTracingMaxNumIndices), TEXT("NaniteRayTracing.IndexBuffer"));
		}

		FRDGBufferRef VertexBufferRDG = GraphBuilder.RegisterExternalBuffer(VertexBuffer);
		FRDGBufferRef IndexBufferRDG = GraphBuilder.RegisterExternalBuffer(IndexBuffer);

		StreamOutData(
			GraphBuilder,
			GetGlobalShaderMap(GetFeatureLevel()),
			SceneUniformBuffer,
			NodesAndClusterBatchesBuffer,
			GetCutError(),
			ToUpdate.Num(),
			RequestBuffer,
			SegmentMappingBuffer,
			MeshDataBuffer,
			StagingAuxiliaryDataBufferRDG,
			VertexBufferRDG,
			GNaniteRayTracingMaxNumVertices,
			IndexBufferRDG,
			GNaniteRayTracingMaxNumIndices);

		INC_DWORD_STAT_BY(STAT_NaniteRayTracingStreamOutRequests, ToUpdate.Num());

		if (!GNaniteRayTracingProfileStreamOut)
		{
			// readback
			{
				AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("NaniteRayTracing::Readback"), MeshDataBuffer,
					[MeshDataReadbackBuffer = ReadbackData.MeshDataReadbackBuffer, MeshDataBuffer](FRHICommandList& RHICmdList)
					{
						MeshDataReadbackBuffer->EnqueueCopy(RHICmdList, MeshDataBuffer->GetRHI(), 0u);
					});

				ReadbackData.NumMeshDataEntries = NumMeshDataEntries;

				ReadbackBuffersWriteIndex = (ReadbackBuffersWriteIndex + 1u) % MaxReadbackBuffers;
				ReadbackBuffersNumPending = FMath::Min(ReadbackBuffersNumPending + 1u, MaxReadbackBuffers);
			}
		}
		else
		{
			// if running profile mode, clear state for next frame

			bUpdating = false;

			for (auto GeometryId : ToUpdate)
			{
				FInternalData& Data = *Geometries[GeometryId];
				Data.bUpdating = false;
				Data.BaseMeshDataOffset = -1;
				Data.StagingAuxiliaryDataOffset = INDEX_NONE;
			}

			ReadbackData.Entries.Empty();
		}

		ToUpdate.Empty();
	}

	void FRayTracingManager::Update()
	{
		const bool bUsingNaniteRayTracing = GetRayTracingMode() != ERayTracingMode::Fallback;

		// process PendingRemoves
		{
			TSet<uint32> StillPendingRemoves;

			for (uint32 GeometryId : PendingRemoves)
			{
				FInternalData* Data = Geometries[GeometryId];

				if (Data->bUpdating)
				{
					// can't remove until update is finished, delay to next frame
					StillPendingRemoves.Add(GeometryId);
				}
				else
				{
					if (Data->AuxiliaryDataOffset != INDEX_NONE)
					{
						AuxiliaryDataAllocator.Free(Data->AuxiliaryDataOffset, Data->AuxiliaryDataSize);
					}
					ResourceToRayTracingIdMap.Remove(Data->ResourceId);
					Geometries.RemoveAt(GeometryId);
					delete (Data);
				}
			}

			Swap(PendingRemoves, StillPendingRemoves);
		}

		int32 NumPrimitivesScheduled = 0;

		// scheduling pending builds
		{
			checkf(ScheduledBuilds.IsEmpty(), TEXT("Scheduled builds were not dispatched last frame."));

			for (const FPendingBuild& PendingBuild : PendingBuilds)
			{
				if (NumPrimitivesScheduled >= GNaniteRayTracingMaxBuiltPrimitivesPerFrame)
				{
					break;
				}

				FInternalData& Data = *Geometries[PendingBuild.GeometryId];
				Data.RayTracingGeometryRHI = PendingBuild.RayTracingGeometryRHI;

				const FRayTracingGeometryInitializer& Initializer = Data.RayTracingGeometryRHI->GetInitializer();

				NumPrimitivesScheduled += Initializer.TotalPrimitiveCount;

				if (Data.AuxiliaryDataOffset != INDEX_NONE)
				{
					AuxiliaryDataAllocator.Free(Data.AuxiliaryDataOffset, Data.AuxiliaryDataSize);
				}
				Data.AuxiliaryDataSize = Initializer.TotalPrimitiveCount;
				Data.AuxiliaryDataOffset = AuxiliaryDataAllocator.Allocate(Data.AuxiliaryDataSize);

				for (auto& Primitive : Data.Primitives)
				{
					if (bUsingNaniteRayTracing)
					{
						Primitive->CachedRayTracingInstance.GeometryRHI = Data.RayTracingGeometryRHI;
					}

					auto NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(Primitive->Proxy);
					NaniteProxy->SetRayTracingDataOffset(Data.AuxiliaryDataOffset);

					Primitive->Scene->GPUScene.AddPrimitiveToUpdate(Primitive->GetPersistentIndex(), EPrimitiveDirtyState::ChangedOther);
				}

				ScheduledBuilds.Add(PendingBuild.GeometryId);
			}

			// not using RemoveAtSwap to avoid starving requests in the middle
			// not expecting significant number of elements remaining anyway
			PendingBuilds.RemoveAt(0, ScheduledBuilds.Num());

			DEC_DWORD_STAT_BY(STAT_NaniteRayTracingPendingBuilds, ScheduledBuilds.Num());
		}

		while (ReadbackBuffersNumPending > 0)
		{
			uint32 Index = (ReadbackBuffersWriteIndex + MaxReadbackBuffers - ReadbackBuffersNumPending) % MaxReadbackBuffers;
			FReadbackData& ReadbackData = ReadbackBuffers[Index];
			if (ReadbackData.MeshDataReadbackBuffer->IsReady())
			{
				ReadbackBuffersNumPending--;

				auto MeshDataReadbackBufferPtr = (const uint32*)ReadbackData.MeshDataReadbackBuffer->Lock(ReadbackData.NumMeshDataEntries * sizeof(uint32));

				for (int32 GeometryIndex = 0; GeometryIndex < ReadbackData.Entries.Num(); ++GeometryIndex)
				{
					uint32 GeometryId = ReadbackData.Entries[GeometryIndex];
					FInternalData& Data = *Geometries[GeometryId];

					auto Header = (const FStreamOutMeshDataHeader*)(MeshDataReadbackBufferPtr + Data.BaseMeshDataOffset);
					auto Segments = (const FStreamOutMeshDataSegment*)(Header + 1);

					check(Header->NumClusters <= Data.NumResidentClusters);

					const uint32 VertexBufferOffset = Header->VertexBufferOffset;
					const uint32 IndexBufferOffset = Header->IndexBufferOffset;
					const uint32 NumVertices = Header->NumVertices;

					if (VertexBufferOffset == 0xFFFFFFFFu || IndexBufferOffset == 0xFFFFFFFFu)
					{
						// ran out of space in StreamOut buffers
						Data.bUpdating = false;
						Data.BaseMeshDataOffset = -1;

						check(Data.StagingAuxiliaryDataOffset != INDEX_NONE);
						Data.StagingAuxiliaryDataOffset = INDEX_NONE;

						UpdateRequests.Add(GeometryId); // request update again

						DEC_DWORD_STAT_BY(STAT_NaniteRayTracingInFlightUpdates, 1);
						INC_DWORD_STAT_BY(STAT_NaniteRayTracingFailedStreamOutRequests, 1);

						continue;
					}

					FRayTracingGeometryInitializer Initializer;
					Initializer.DebugName = Data.DebugName;
// 					Initializer.bFastBuild = false;
// 					Initializer.bAllowUpdate = false;
					Initializer.bAllowCompaction = false;

					Initializer.IndexBuffer = IndexBuffer->GetRHI();
					Initializer.IndexBufferOffset = IndexBufferOffset * sizeof(uint32);

					Initializer.TotalPrimitiveCount = 0;

					Initializer.Segments.SetNum(Data.NumSegments);

					for (uint32 SegmentIndex = 0; SegmentIndex < Data.NumSegments; ++SegmentIndex)
					{
						const uint32 NumIndices = Segments[SegmentIndex].NumIndices;
						const uint32 FirstIndex = Segments[SegmentIndex].FirstIndex;

						FRayTracingGeometrySegment& Segment = Initializer.Segments[SegmentIndex];
						Segment.FirstPrimitive = FirstIndex / 3;
						Segment.NumPrimitives = NumIndices / 3;
						Segment.VertexBuffer = VertexBuffer->GetRHI();
						Segment.VertexBufferOffset = VertexBufferOffset * sizeof(FVector3f);
						Segment.MaxVertices = NumVertices;

						Initializer.TotalPrimitiveCount += Segment.NumPrimitives;
					}

					FRayTracingGeometryRHIRef RayTracingGeometryRHI = RHICreateRayTracingGeometry(Initializer);

					if (NumPrimitivesScheduled < GNaniteRayTracingMaxBuiltPrimitivesPerFrame)
					{
						NumPrimitivesScheduled += RayTracingGeometryRHI->GetInitializer().TotalPrimitiveCount;

						Data.RayTracingGeometryRHI = MoveTemp(RayTracingGeometryRHI);

						if (Data.AuxiliaryDataOffset != INDEX_NONE)
						{
							AuxiliaryDataAllocator.Free(Data.AuxiliaryDataOffset, Data.AuxiliaryDataSize);
						}
						// allocate persistent auxiliary range
						Data.AuxiliaryDataSize = CalculateAuxiliaryDataSizeInUints(Initializer.TotalPrimitiveCount);
						Data.AuxiliaryDataOffset = AuxiliaryDataAllocator.Allocate(Data.AuxiliaryDataSize);

						for (auto& Primitive : Data.Primitives)
						{
							if (bUsingNaniteRayTracing)
							{
								Primitive->CachedRayTracingInstance.GeometryRHI = Data.RayTracingGeometryRHI;
							}

							auto NaniteProxy = static_cast<Nanite::FSceneProxyBase*>(Primitive->Proxy);
							NaniteProxy->SetRayTracingDataOffset(Data.AuxiliaryDataOffset);

							Primitive->Scene->GPUScene.AddPrimitiveToUpdate(Primitive->GetPersistentIndex(), EPrimitiveDirtyState::ChangedOther);
						}

						ScheduledBuilds.Add(GeometryId);
					}
					else
					{
						FPendingBuild PendingBuild;
						PendingBuild.GeometryId = GeometryId;
						PendingBuild.RayTracingGeometryRHI = MoveTemp(RayTracingGeometryRHI);
						PendingBuilds.Add(MoveTemp(PendingBuild));

						INC_DWORD_STAT_BY(STAT_NaniteRayTracingPendingBuilds, 1);
					}
				}

				ReadbackData.Entries.Empty();
				ReadbackData.MeshDataReadbackBuffer->Unlock();
			}
			else
			{
				break;
			}
		}

		INC_DWORD_STAT_BY(STAT_NaniteRayTracingScheduledBuildsNumPrimitives, NumPrimitivesScheduled);
	}

	bool FRayTracingManager::ProcessBuildRequests(FRDGBuilder& GraphBuilder)
	{
		if (!bInitialized)
		{
			return false;
		}

		// resize AuxiliaryDataBuffer if necessary
		FRDGBufferRef AuxiliaryDataBufferRDG;
		{
			const uint32 NumAuxiliaryDataEntries = FMath::Max((uint32)AuxiliaryDataAllocator.GetMaxSize(), MinAuxiliaryBufferEntries);
			const bool bCopy = true;
			AuxiliaryDataBufferRDG = ResizeBufferIfNeeded(GraphBuilder, AuxiliaryDataBuffer, sizeof(uint32), NumAuxiliaryDataEntries, TEXT("NaniteRayTracing.AuxiliaryDataBuffer"), bCopy, EAllowShrinking::No);
			AuxiliaryDataBuffer = GraphBuilder.ConvertToExternalBuffer(AuxiliaryDataBufferRDG);

			SET_MEMORY_STAT(STAT_NaniteRayTracingAuxiliaryDataBuffer, AuxiliaryDataBufferRDG->GetSize());
		}

		FRDGBufferRef StagingAuxiliaryDataBufferRDG = GraphBuilder.RegisterExternalBuffer(StagingAuxiliaryDataBuffer);

		TArray<FRayTracingGeometryBuildParams> BuildParams;
		uint32 BLASScratchSize = 0;
		
		for (uint32 GeometryId : ScheduledBuilds)
		{
			FInternalData& Data = *Geometries[GeometryId];

			const FRayTracingGeometryInitializer& Initializer = Data.RayTracingGeometryRHI->GetInitializer();

			FRayTracingGeometryBuildParams Params;
			Params.Geometry = Data.RayTracingGeometryRHI;
			Params.BuildMode = EAccelerationStructureBuildMode::Build;

			BuildParams.Add(Params);

			FRayTracingAccelerationStructureSize SizeInfo = GraphBuilder.RHICmdList.CalcRayTracingGeometrySize(Initializer);
			BLASScratchSize = Align(BLASScratchSize + SizeInfo.BuildScratchSize, GRHIRayTracingScratchBufferAlignment);

			Data.bUpdating = false;
			Data.BaseMeshDataOffset = -1;

			DEC_DWORD_STAT_BY(STAT_NaniteRayTracingInFlightUpdates, 1);

			// copy from staging to persistent auxiliary data buffer
			AddCopyBufferPass(GraphBuilder, AuxiliaryDataBufferRDG, Data.AuxiliaryDataOffset * sizeof(uint32), StagingAuxiliaryDataBufferRDG, Data.StagingAuxiliaryDataOffset * sizeof(uint32), Data.AuxiliaryDataSize * sizeof(uint32));
			Data.StagingAuxiliaryDataOffset = INDEX_NONE;
		}

		const uint32 BLASScratchSizeMultiple = FMath::Max(GNaniteRayTracingBLASScratchSizeMultipleMB, 1) * 1024 * 1024;
		BLASScratchSize = FMath::DivideAndRoundUp(BLASScratchSize, BLASScratchSizeMultiple) * BLASScratchSizeMultiple;

		INC_DWORD_STAT_BY(STAT_NaniteRayTracingScheduledBuilds, ScheduledBuilds.Num());

		ScheduledBuilds.Empty();

		bool bAnyBlasRebuilt = false;

		if (BuildParams.Num() > 0)
		{
			RDG_GPU_STAT_SCOPE(GraphBuilder, RebuildNaniteBLAS);

			FRDGBufferDesc ScratchBufferDesc;
			ScratchBufferDesc.Usage = EBufferUsageFlags::RayTracingScratch | EBufferUsageFlags::StructuredBuffer;
			ScratchBufferDesc.BytesPerElement = GRHIRayTracingScratchBufferAlignment;
			ScratchBufferDesc.NumElements = FMath::DivideAndRoundUp(BLASScratchSize, GRHIRayTracingScratchBufferAlignment);

			FRDGBufferRef ScratchBuffer = GraphBuilder.CreateBuffer(ScratchBufferDesc, TEXT("NaniteRayTracing.BLASSharedScratchBuffer"));

			FNaniteRayTracingPrimitivesParams* PassParams = GraphBuilder.AllocParameters<FNaniteRayTracingPrimitivesParams>();
			PassParams->Buffer0 = nullptr;
			PassParams->Buffer1 = nullptr;
			PassParams->ScratchBuffer = ScratchBuffer;

			GraphBuilder.AddPass(RDG_EVENT_NAME("NaniteRayTracing::UpdateBLASes"), PassParams, ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				[PassParams, BuildParams = MoveTemp(BuildParams)](FRHIComputeCommandList& RHICmdList)
			{
				FRHIBufferRange ScratchBufferRange;
				ScratchBufferRange.Buffer = PassParams->ScratchBuffer->GetRHI();
				ScratchBufferRange.Offset = 0;
				RHICmdList.BuildAccelerationStructures(BuildParams, ScratchBufferRange);
			});

			bAnyBlasRebuilt = true;
		}

		if (ReadbackBuffersNumPending == 0 && PendingBuilds.IsEmpty())
		{
			bUpdating = false;
		}

		return bAnyBlasRebuilt;
	}

	FRHIRayTracingGeometry* FRayTracingManager::GetRayTracingGeometry(FPrimitiveSceneInfo* SceneInfo) const
	{
		auto NaniteProxy = static_cast<const Nanite::FSceneProxyBase*>(SceneInfo->Proxy);

		const uint32 Id = NaniteProxy->GetRayTracingId();

		if (Id == INDEX_NONE)
		{
			return nullptr;
		}

		const FInternalData* Data = Geometries[Id];

		return Data->RayTracingGeometryRHI;
	}

	bool FRayTracingManager::CheckModeChanged()
	{
		bPrevMode = bCurrentMode;
		bCurrentMode = GetRayTracingMode();
		return bPrevMode != bCurrentMode;
	}

	float FRayTracingManager::GetCutError() const
	{
		return GNaniteRayTracingCutError;
	}

	TGlobalResource<FRayTracingManager> GRayTracingManager;
} // namespace Nanite

#endif // RHI_RAYTRACING
