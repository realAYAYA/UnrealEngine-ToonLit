// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshMaterialShader.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ScenePrivate.h"
#include "RayTracingDynamicGeometryCollection.h"
#include "RayTracingInstance.h"
#include "RayTracingGeometry.h"

#if RHI_RAYTRACING

#include "Materials/MaterialRenderProxy.h"

static int32 GRTDynGeomSharedVertexBufferSizeInMB = 4;
static FAutoConsoleVariableRef CVarRTDynGeomSharedVertexBufferSizeInMB(
	TEXT("r.RayTracing.DynamicGeometry.SharedVertexBufferSizeInMB"),
	GRTDynGeomSharedVertexBufferSizeInMB,
	TEXT("Size of the a single shared vertex buffer used during the BLAS update of dynamic geometries (default 4MB)"),
	ECVF_RenderThreadSafe
);

static int32 GRTDynGeomSharedVertexBufferGarbageCollectLatency = 30;
static FAutoConsoleVariableRef CVarRTDynGeomSharedVertexBufferGarbageCollectLatency(
	TEXT("r.RayTracing.DynamicGeometry.SharedVertexBufferGarbageCollectLatency"),
	GRTDynGeomSharedVertexBufferGarbageCollectLatency,
	TEXT("Amount of update cycles before a heap is deleted when not used (default 30)."),
	ECVF_RenderThreadSafe
);

DECLARE_CYCLE_STAT(TEXT("RTDynGeomDispatch"), STAT_CLM_RTDynGeomDispatch, STATGROUP_ParallelCommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RTDynGeomBuild"), STAT_CLM_RTDynGeomBuild, STATGROUP_ParallelCommandListMarkers);

// Workaround for outstanding memory corruption on some platforms when parallel command list translation is used.
#define USE_RAY_TRACING_DYNAMIC_GEOMETRY_PARALLEL_COMMAND_LISTS 0

class FRayTracingDynamicGeometryConverterCS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRayTracingDynamicGeometryConverterCS, MeshMaterial);
public:
	FRayTracingDynamicGeometryConverterCS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());

		RWVertexPositions.Bind(Initializer.ParameterMap, TEXT("RWVertexPositions"));
		UsingIndirectDraw.Bind(Initializer.ParameterMap, TEXT("UsingIndirectDraw"));
		NumVertices.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
		MinVertexIndex.Bind(Initializer.ParameterMap, TEXT("MinVertexIndex"));
		PrimitiveId.Bind(Initializer.ParameterMap, TEXT("PrimitiveId"));
		OutputVertexBaseIndex.Bind(Initializer.ParameterMap, TEXT("OutputVertexBaseIndex"));
		bApplyWorldPositionOffset.Bind(Initializer.ParameterMap, TEXT("bApplyWorldPositionOffset"));
		InstanceId.Bind(Initializer.ParameterMap, TEXT("InstanceId"));
		WorldToInstance.Bind(Initializer.ParameterMap, TEXT("WorldToInstance"));
	}

	FRayTracingDynamicGeometryConverterCS() = default;

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return Parameters.VertexFactoryType->SupportsRayTracingDynamicGeometry() && IsRayTracingEnabledForProject(Parameters.Platform) && RHISupportsRayTracing(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), 1);
		OutEnvironment.SetDefine(TEXT("USE_INSTANCE_CULLING_DATA"), 0);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);
	}

	void GetElementShaderBindings(
		const FShaderMapPointerTable& PointerTable,
		const FScene* Scene,
		const FSceneView* ViewIfDynamicMeshCommand,
		const FVertexFactory* VertexFactory,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& MeshBatch, 
		const FMeshBatchElement& BatchElement,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		FMeshMaterialShader::GetElementShaderBindings(PointerTable, Scene, ViewIfDynamicMeshCommand, VertexFactory, InputStreamType, FeatureLevel, PrimitiveSceneProxy, MeshBatch, BatchElement, ShaderElementData, ShaderBindings, VertexStreams);
	}

	LAYOUT_FIELD(FShaderResourceParameter, RWVertexPositions);
	LAYOUT_FIELD(FShaderParameter, UsingIndirectDraw);
	LAYOUT_FIELD(FShaderParameter, NumVertices);
	LAYOUT_FIELD(FShaderParameter, MinVertexIndex);
	LAYOUT_FIELD(FShaderParameter, PrimitiveId);
	LAYOUT_FIELD(FShaderParameter, bApplyWorldPositionOffset);
	LAYOUT_FIELD(FShaderParameter, OutputVertexBaseIndex);
	LAYOUT_FIELD(FShaderParameter, InstanceId);
	LAYOUT_FIELD(FShaderParameter, WorldToInstance);
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FRayTracingDynamicGeometryConverterCS, TEXT("/Engine/Private/RayTracing/RayTracingDynamicMesh.usf"), TEXT("RayTracingDynamicGeometryConverterCS"), SF_Compute);

FRayTracingDynamicGeometryCollection::FRayTracingDynamicGeometryCollection() 
{
}

FRayTracingDynamicGeometryCollection::~FRayTracingDynamicGeometryCollection()
{
	for (FVertexPositionBuffer* Buffer : VertexPositionBuffers)
	{
		delete Buffer;
	}
	VertexPositionBuffers.Empty();
}

void FRayTracingDynamicGeometryCollection::Clear()
{
	// Clear working arrays - keep max size allocated
	DispatchCommands.Empty(DispatchCommands.Max());
	BuildParams.Empty(BuildParams.Max());
	Segments.Empty(Segments.Max());
}

int64 FRayTracingDynamicGeometryCollection::BeginUpdate()
{
	check(DispatchCommands.IsEmpty());
	check(BuildParams.IsEmpty());
	check(Segments.IsEmpty());
	check(ReferencedUniformBuffers.IsEmpty());

	// Vertex buffer data can be immediatly reused the next frame, because it's already 'consumed' for building the AccelerationStructure data
	// Garbage collect unused buffers for n generations
	for (int32 BufferIndex = 0; BufferIndex < VertexPositionBuffers.Num(); ++BufferIndex)
	{
		FVertexPositionBuffer* Buffer = VertexPositionBuffers[BufferIndex];
		Buffer->UsedSize = 0;

		if (Buffer->LastUsedGenerationID + GRTDynGeomSharedVertexBufferGarbageCollectLatency <= SharedBufferGenerationID)
		{
			VertexPositionBuffers.RemoveAtSwap(BufferIndex);
			delete Buffer;
			BufferIndex--;
		}
	}

	// Increment generation ID used for validation
	SharedBufferGenerationID++;

	return SharedBufferGenerationID;
}

void FRayTracingDynamicGeometryCollection::AddDynamicMeshBatchForGeometryUpdate(
	const FScene* Scene,
	const FSceneView* View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FRayTracingDynamicGeometryUpdateParams& UpdateParams,
	uint32 PrimitiveId
)
{
	AddDynamicMeshBatchForGeometryUpdate(FRHICommandListImmediate::Get(), Scene, View, PrimitiveSceneProxy, UpdateParams, PrimitiveId);
}

void FRayTracingDynamicGeometryCollection::AddDynamicMeshBatchForGeometryUpdate(
	FRHICommandListBase& RHICmdList,
	const FScene* Scene,
	const FSceneView* View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FRayTracingDynamicGeometryUpdateParams& UpdateParams,
	uint32 PrimitiveId
)
{
	FRayTracingGeometry& Geometry = *UpdateParams.Geometry;
	bool bUsingIndirectDraw = UpdateParams.bUsingIndirectDraw;
	uint32 NumMaxVertices = UpdateParams.NumVertices;

	FRWBuffer* RWBuffer = UpdateParams.Buffer;
	uint32 VertexBufferOffset = 0;
	bool bUseSharedVertexBuffer = false;

	if (ReferencedUniformBuffers.Num() == 0 || ReferencedUniformBuffers.Last() != View->ViewUniformBuffer)
	{
		// Keep ViewUniformBuffer alive until EndUpdate()
		ReferencedUniformBuffers.Add(View->ViewUniformBuffer);
	}

	// If update params didn't provide a buffer then use a shared vertex position buffer
	if (RWBuffer == nullptr)
	{
		FVertexPositionBuffer* VertexPositionBuffer = nullptr;
		for (FVertexPositionBuffer* Buffer : VertexPositionBuffers)
		{
			if ((Buffer->RWBuffer.NumBytes - Buffer->UsedSize) >= UpdateParams.VertexBufferSize)
			{
				VertexPositionBuffer = Buffer;
				break;
			}
		}

		// Allocate a new buffer?
		if (VertexPositionBuffer == nullptr)
		{
			VertexPositionBuffer = new FVertexPositionBuffer;
			VertexPositionBuffers.Add(VertexPositionBuffer);

			static const uint32 VertexBufferCacheSize = GRTDynGeomSharedVertexBufferSizeInMB * 1024 * 1024;
			uint32 AllocationSize = FMath::Max(VertexBufferCacheSize, UpdateParams.VertexBufferSize);

			VertexPositionBuffer->RWBuffer.Initialize(RHICmdList, TEXT("FRayTracingDynamicGeometryCollection::RayTracingDynamicVertexBuffer"), sizeof(float), AllocationSize / sizeof(float), PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);
			VertexPositionBuffer->UsedSize = 0;
		}

		// Update the last used generation ID
		VertexPositionBuffer->LastUsedGenerationID = SharedBufferGenerationID;

		// Get the offset and update used size
		VertexBufferOffset = VertexPositionBuffer->UsedSize;
		VertexPositionBuffer->UsedSize += UpdateParams.VertexBufferSize;

		bUseSharedVertexBuffer = true;
		RWBuffer = &VertexPositionBuffer->RWBuffer;
	}

	for (const FMeshBatch& MeshBatch : UpdateParams.MeshBatches)
	{
		if (!ensureMsgf(MeshBatch.VertexFactory->GetType()->SupportsRayTracingDynamicGeometry(),
			TEXT("FRayTracingDynamicGeometryConverterCS doesn't support %s. Skipping rendering of %s.  This can happen when the skinning cache runs out of space and falls back to GPUSkinVertexFactory."),
			MeshBatch.VertexFactory->GetType()->GetName(), *PrimitiveSceneProxy->GetOwnerName().ToString()))
		{
			continue;
		}

		const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = nullptr;
		const FMaterial& Material = MeshBatch.MaterialRenderProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), FallbackMaterialRenderProxyPtr);
		auto* MaterialInterface = Material.GetMaterialInterface();
		const FMaterialRenderProxy& MaterialRenderProxy = FallbackMaterialRenderProxyPtr ? *FallbackMaterialRenderProxyPtr : *MeshBatch.MaterialRenderProxy;

		FMeshComputeDispatchCommand DispatchCmd;
		
		FMaterialShaderTypes ShaderTypes;
		ShaderTypes.AddShaderType<FRayTracingDynamicGeometryConverterCS>();

		FMaterialShaders MaterialShaders;
		if (!Material.TryGetShaders(ShaderTypes, MeshBatch.VertexFactory->GetType(), MaterialShaders))
		{
			continue;
		}

		TShaderRef<FRayTracingDynamicGeometryConverterCS> Shader;
		MaterialShaders.TryGetShader(SF_Compute, Shader);

		FMeshProcessorShaders MeshProcessorShaders;
		MeshProcessorShaders.ComputeShader = Shader;

		DispatchCmd.MaterialShader = Shader;
		FMeshDrawShaderBindings& ShaderBindings = DispatchCmd.ShaderBindings;
		ShaderBindings.Initialize(MeshProcessorShaders);

		FMeshMaterialShaderElementData ShaderElementData;
		ShaderElementData.InitializeMeshMaterialData(View, PrimitiveSceneProxy, MeshBatch, -1, false);

		FMeshDrawSingleShaderBindings SingleShaderBindings = ShaderBindings.GetSingleShaderBindings(SF_Compute);
		Shader->GetShaderBindings(Scene, Scene->GetFeatureLevel(), PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, SingleShaderBindings);

		FVertexInputStreamArray DummyArray;
		FMeshMaterialShader::GetElementShaderBindings(Shader, Scene, View, MeshBatch.VertexFactory, EVertexInputStreamType::Default, Scene->GetFeatureLevel(), PrimitiveSceneProxy, MeshBatch, MeshBatch.Elements[0], ShaderElementData, SingleShaderBindings, DummyArray);

		DispatchCmd.TargetBuffer = RWBuffer;
		DispatchCmd.NumMaxVertices = UpdateParams.NumVertices;

		// Setup the loose parameters directly on the binding
		uint32 OutputVertexBaseIndex = VertexBufferOffset / sizeof(float);
		uint32 MinVertexIndex = MeshBatch.Elements[0].MinVertexIndex;
		uint32 NumCPUVertices = UpdateParams.NumVertices;
		if (MeshBatch.Elements[0].MinVertexIndex < MeshBatch.Elements[0].MaxVertexIndex)
		{
			NumCPUVertices = 1 + MeshBatch.Elements[0].MaxVertexIndex - MeshBatch.Elements[0].MinVertexIndex;
		}

		const uint32 VertexBufferNumElements = UpdateParams.VertexBufferSize / sizeof(FVector3f) - MinVertexIndex;
		if (!ensureMsgf(NumCPUVertices <= VertexBufferNumElements, 
			TEXT("Vertex buffer contains %d vertices, but RayTracingDynamicGeometryConverterCS dispatch command expects at least %d."),
			VertexBufferNumElements, NumCPUVertices))
		{
			NumCPUVertices = VertexBufferNumElements;
		}

		SingleShaderBindings.Add(Shader->UsingIndirectDraw, bUsingIndirectDraw ? 1 : 0);
		SingleShaderBindings.Add(Shader->NumVertices, NumCPUVertices);
		SingleShaderBindings.Add(Shader->MinVertexIndex, MinVertexIndex);
		SingleShaderBindings.Add(Shader->PrimitiveId, PrimitiveId);
		SingleShaderBindings.Add(Shader->OutputVertexBaseIndex, OutputVertexBaseIndex);
		SingleShaderBindings.Add(Shader->bApplyWorldPositionOffset, UpdateParams.bApplyWorldPositionOffset ? 1 : 0);
		SingleShaderBindings.Add(Shader->InstanceId, UpdateParams.InstanceId);
		SingleShaderBindings.Add(Shader->WorldToInstance, UpdateParams.WorldToInstance);

#if MESH_DRAW_COMMAND_DEBUG_DATA
		ShaderBindings.Finalize(&MeshProcessorShaders);
#endif

		DispatchCommands.Add(DispatchCmd);
	}

	bool bRefit = true;

	// Optionally resize the buffer when not shared (could also be lazy allocated and still empty)
	if (!bUseSharedVertexBuffer && RWBuffer->NumBytes != UpdateParams.VertexBufferSize)
	{
		RWBuffer->Initialize(RHICmdList, TEXT("FRayTracingDynamicGeometryCollection::RayTracingDynamicVertexBuffer"), sizeof(float), UpdateParams.VertexBufferSize / sizeof(float), PF_R32_FLOAT, BUF_UnorderedAccess | BUF_ShaderResource);
		bRefit = false;
	}

	if (!Geometry.RayTracingGeometryRHI.IsValid())
	{
		bRefit = false;
	}

	if (!Geometry.Initializer.bAllowUpdate)
	{
		bRefit = false;
	}

	check(Geometry.IsInitialized());

	if (Geometry.Initializer.TotalPrimitiveCount != UpdateParams.NumTriangles)
	{
		check(Geometry.Initializer.Segments.Num() <= 1);
		Geometry.Initializer.TotalPrimitiveCount = UpdateParams.NumTriangles;
		Geometry.Initializer.Segments.Empty();
		FRayTracingGeometrySegment Segment;
		Segment.NumPrimitives = UpdateParams.NumTriangles;
		Segment.MaxVertices = UpdateParams.NumVertices;
		Geometry.Initializer.Segments.Add(Segment);
		bRefit = false;
	}

	for (FRayTracingGeometrySegment& Segment : Geometry.Initializer.Segments)
	{
		Segment.VertexBuffer = RWBuffer->Buffer;
		Segment.VertexBufferOffset = VertexBufferOffset;
	}

	if (!bRefit)
	{
		checkf(Geometry.Initializer.OfflineData == nullptr, TEXT("Dynamic geometry is not expected to have offline acceleration structure data"));
		Geometry.RayTracingGeometryRHI = RHICmdList.CreateRayTracingGeometry(Geometry.Initializer);
		Geometry.SetRequiresBuild(true);
	}

	FRayTracingGeometryBuildParams Params;
	Params.Geometry = Geometry.RayTracingGeometryRHI;
	Params.BuildMode = Geometry.GetRequiresBuild()
		? EAccelerationStructureBuildMode::Build
		: EAccelerationStructureBuildMode::Update;

	Geometry.SetRequiresBuild(false);

	if (bUseSharedVertexBuffer)
	{
		Segments.Append(Geometry.Initializer.Segments);

		// Cache the count of segments so final views can be made when all segments are collected (Segments array could still be reallocated)
		Params.Segments = MakeArrayView((FRayTracingGeometrySegment*)nullptr, Geometry.Initializer.Segments.Num());
	}

	BuildParams.Add(Params);
	
	if (bUseSharedVertexBuffer)
	{
		Geometry.DynamicGeometrySharedBufferGenerationID = SharedBufferGenerationID;
	}
	else
	{
		Geometry.DynamicGeometrySharedBufferGenerationID = FRayTracingGeometry::NonSharedVertexBuffers;
	}
}

void FRayTracingDynamicGeometryCollection::DispatchUpdates(FRHICommandListImmediate& ParentCmdList, FRHIBuffer* ScratchBuffer)
{
#if WANTS_DRAW_MESH_EVENTS
#define SCOPED_DRAW_OR_COMPUTE_EVENT(ParentCmdList, Name) FDrawEvent PREPROCESSOR_JOIN(Event_##Name,__LINE__); if(GetEmitDrawEvents()) PREPROCESSOR_JOIN(Event_##Name,__LINE__).Start(&ParentCmdList, FColor(0), TEXT(#Name));
#else
#define SCOPED_DRAW_OR_COMPUTE_EVENT(...)
#endif

	if (DispatchCommands.Num() > 0)
	{
		SCOPED_DRAW_OR_COMPUTE_EVENT(ParentCmdList, RayTracingDynamicGeometryUpdate)

		{
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SortDispatchCommands);

				// This can be optimized by using sorted insert or using map on shaders
				// There are only a handful of unique shaders and a few target buffers so we want to swap state as little as possible
				// to reduce RHI thread overhead
				DispatchCommands.Sort([](const FMeshComputeDispatchCommand& InLHS, const FMeshComputeDispatchCommand& InRHS)
					{
						if (InLHS.MaterialShader.GetComputeShader() != InRHS.MaterialShader.GetComputeShader())
							return InLHS.MaterialShader.GetComputeShader() < InRHS.MaterialShader.GetComputeShader();

						return InLHS.TargetBuffer < InRHS.TargetBuffer;
					});
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SetupSegmentData);

				// Setup the array views on final allocated segments array
				FRayTracingGeometrySegment* SegmentData = Segments.GetData();
				for (FRayTracingGeometryBuildParams& Param : BuildParams)
				{
					uint32 SegmentCount = Param.Segments.Num();
					if (SegmentCount > 0)
					{
						Param.Segments = MakeArrayView(SegmentData, SegmentCount);
						SegmentData += SegmentCount;
					}
				}
			}

			FMemMark Mark(FMemStack::Get());

			TArray<FRHITransitionInfo, TMemStackAllocator<>> TransitionsBefore, TransitionsAfter;
			TArray<FRHIUnorderedAccessView*, TMemStackAllocator<>> OverlapUAVs;
			TransitionsBefore.Reserve(DispatchCommands.Num());
			TransitionsAfter.Reserve(DispatchCommands.Num());
			OverlapUAVs.Reserve(DispatchCommands.Num());
			const FRWBuffer* LastBuffer = nullptr;
			TSet<const FRWBuffer*> TransitionedBuffers;
			for (FMeshComputeDispatchCommand& Cmd : DispatchCommands)
			{
				if (Cmd.TargetBuffer == nullptr)
				{
					continue;
				}
				FRHIUnorderedAccessView* UAV = Cmd.TargetBuffer->UAV.GetReference();

				// The list is sorted by TargetBuffer, so we can remove duplicates by simply looking at the previous value we've processed.
				if (LastBuffer == Cmd.TargetBuffer)
				{
					// This UAV is used by more than one dispatch, so tell the RHI it's OK to overlap the dispatches, because
					// we're updating disjoint regions.
					if (OverlapUAVs.Num() == 0 || OverlapUAVs.Last() != UAV)
					{
						OverlapUAVs.Add(UAV);
					}
					continue;
				}

				LastBuffer = Cmd.TargetBuffer;

				// In case different shaders use different TargetBuffer we want to add transition only once
				bool bAlreadyInSet = false;
				TransitionedBuffers.FindOrAdd(LastBuffer, &bAlreadyInSet);
				if (!bAlreadyInSet)
				{
					// Looks like the resource can get here in either UAVCompute or SRVMask mode, so we'll have to use Unknown until we can have better tracking.
					TransitionsBefore.Add(FRHITransitionInfo(UAV, ERHIAccess::Unknown, ERHIAccess::UAVCompute));
					TransitionsAfter.Add(FRHITransitionInfo(UAV, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
				}
			}

			TArray<FRHICommandListImmediate::FQueuedCommandList, TInlineAllocator<1>> QueuedCommandLists;
			auto AllocateCommandList = [&ParentCmdList, &QueuedCommandLists](uint32 ExpectedNumDraws, TStatId StatId) -> FRHIComputeCommandList&
			{
			#if USE_RAY_TRACING_DYNAMIC_GEOMETRY_PARALLEL_COMMAND_LISTS
				if (ParentCmdList.Bypass())
				{
					return ParentCmdList;
				}
				else
				{
					FRHIComputeCommandList* RHICmdList = new FRHIComputeCommandList(ParentCmdList.GetGPUMask());
					RHICmdList->SwitchPipeline(ERHIPipeline::Graphics);
					RHICmdList->SetExecuteStat(StatId);

					QueuedCommandLists.Emplace(RHICmdList, ExpectedNumDraws);

					return *RHICmdList;
				}
			#else // USE_RAY_TRACING_DYNAMIC_GEOMETRY_PARALLEL_COMMAND_LISTS
				return ParentCmdList;
			#endif // USE_RAY_TRACING_DYNAMIC_GEOMETRY_PARALLEL_COMMAND_LISTS
			};

			{
				FRHIComputeCommandList& RHICmdList = AllocateCommandList(DispatchCommands.Num(), GET_STATID(STAT_CLM_RTDynGeomDispatch));

				FRHIComputeShader* CurrentShader = nullptr;
				FRWBuffer* CurrentBuffer = nullptr;

				// Transition to writeable for each cmd list and enable UAV overlap, because several dispatches can update non-overlapping portions of the same buffer.
				RHICmdList.Transition(TransitionsBefore);
				RHICmdList.BeginUAVOverlap(OverlapUAVs);

				// Cache the bound uniform buffers because a lot are the same between dispatches
				FShaderBindingState ShaderBindingState;

				for (FMeshComputeDispatchCommand& Cmd : DispatchCommands)
				{
					const TShaderRef<FRayTracingDynamicGeometryConverterCS>& Shader = Cmd.MaterialShader;
					FRHIComputeShader* ComputeShader = Shader.GetComputeShader();
					if (CurrentShader != ComputeShader)
					{
						SetComputePipelineState(RHICmdList, ComputeShader);
						CurrentBuffer = nullptr;
						CurrentShader = ComputeShader;

						// Reset binding state
						ShaderBindingState = FShaderBindingState();
					}

					FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();

					FRWBuffer* TargetBuffer = Cmd.TargetBuffer;
					if (CurrentBuffer != TargetBuffer)
					{
						CurrentBuffer = TargetBuffer;

						SetUAVParameter(BatchedParameters, Shader->RWVertexPositions, Cmd.TargetBuffer->UAV);
					}

					Cmd.ShaderBindings.SetParameters(BatchedParameters, ComputeShader, &ShaderBindingState);
					RHICmdList.SetBatchedShaderParameters(CurrentShader, BatchedParameters);

					RHICmdList.DispatchComputeShader(FMath::DivideAndRoundUp<uint32>(Cmd.NumMaxVertices, 64), 1, 1);
				}

				// Make sure buffers are readable again and disable UAV overlap.
				RHICmdList.EndUAVOverlap(OverlapUAVs);
				RHICmdList.Transition(TransitionsAfter);

				if (&RHICmdList != &ParentCmdList)
				{
					RHICmdList.FinishRecording();
				}
			}

			// Need to kick parallel translate command lists?
			if (QueuedCommandLists.Num() > 0)
			{
				ParentCmdList.QueueAsyncCommandListSubmit(QueuedCommandLists, FRHICommandListImmediate::ETranslatePriority::Normal);
			}

			if (BuildParams.Num() > 0)
			{
				// Can't use parallel command list because we have to make sure we are not building BVH data
				// on the same RTGeometry on multiple threads at the same time. Ideally move the build
				// requests over to the RaytracingGeometry manager so they can be correctly scheduled
				// with other build requests in the engine (see UE-106982)
				SCOPED_DRAW_OR_COMPUTE_EVENT(ParentCmdList, Build);

				FRHIBufferRange ScratchBufferRange;
				ScratchBufferRange.Buffer = ScratchBuffer;
				ScratchBufferRange.Offset = 0;
				ParentCmdList.BuildAccelerationStructures(BuildParams, ScratchBufferRange);
			}

		}
	}
		
#undef SCOPED_DRAW_OR_COMPUTE_EVENT
}

void FRayTracingDynamicGeometryCollection::EndUpdate(FRHICommandListImmediate& RHICmdList)
{
	ReferencedUniformBuffers.Empty(ReferencedUniformBuffers.Max());

	Clear();
}

uint32 FRayTracingDynamicGeometryCollection::ComputeScratchBufferSize()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRayTracingDynamicGeometryCollection::ComputeScratchBufferSize);

	const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;

	uint32 BLASScratchSize = 0;

	for (FRayTracingGeometryBuildParams& Params : BuildParams)
	{
		const FRayTracingAccelerationStructureSize BLASSizeInfo = Params.Geometry->GetSizeInfo();
		const uint64 ScratchSize = Params.BuildMode == EAccelerationStructureBuildMode::Build ? BLASSizeInfo.BuildScratchSize : BLASSizeInfo.UpdateScratchSize;
		BLASScratchSize = Align(BLASScratchSize + ScratchSize, ScratchAlignment);
	}

	return BLASScratchSize;
}

#undef USE_RAY_TRACING_DYNAMIC_GEOMETRY_PARALLEL_COMMAND_LISTS

#endif // RHI_RAYTRACING
