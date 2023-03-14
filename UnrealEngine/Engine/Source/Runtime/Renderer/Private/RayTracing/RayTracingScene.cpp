// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingScene.h"

#if RHI_RAYTRACING

#include "RayTracingInstanceBufferUtil.h"
#include "RenderCore.h"
#include "RayTracingDefinitions.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RaytracingOptions.h"

BEGIN_SHADER_PARAMETER_STRUCT(FBuildInstanceBufferPassParams, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, InstanceBuffer)
END_SHADER_PARAMETER_STRUCT()

FRayTracingScene::FRayTracingScene()
{

}

FRayTracingScene::~FRayTracingScene()
{
	// Make sure that all async tasks complete before we tear down any memory they might reference.
	WaitForTasks();
}

FRayTracingSceneWithGeometryInstances FRayTracingScene::BuildInitializationData() const
{
	return CreateRayTracingSceneWithGeometryInstances(
		Instances,
		uint8(ERayTracingSceneLayer::NUM),
		RAY_TRACING_NUM_SHADER_SLOTS,
		NumMissShaderSlots,
		NumCallableShaderSlots);
}

void FRayTracingScene::Create(FRDGBuilder& GraphBuilder, const FGPUScene* GPUScene, const FViewMatrices& ViewMatrices)
{
	CreateWithInitializationData(GraphBuilder, GPUScene, ViewMatrices, BuildInitializationData());
}

void FRayTracingScene::InitPreViewTranslation(const FViewMatrices& ViewMatrices)
{
	const FLargeWorldRenderPosition AbsoluteViewOrigin(ViewMatrices.GetViewOrigin());
	const FVector ViewTileOffset = AbsoluteViewOrigin.GetTileOffset();

	RelativePreViewTranslation = ViewMatrices.GetPreViewTranslation() + ViewTileOffset;
	ViewTilePosition = AbsoluteViewOrigin.GetTile();
}

void FRayTracingScene::CreateWithInitializationData(FRDGBuilder& GraphBuilder, const FGPUScene* GPUScene, const FViewMatrices& ViewMatrices, FRayTracingSceneWithGeometryInstances SceneWithGeometryInstances)
{
	QUICK_SCOPE_CYCLE_COUNTER(FRayTracingScene_BeginCreate);

	// Round up buffer sizes to some multiple to avoid pathological growth reallocations.
	static constexpr uint32 AllocationGranularity = 8 * 1024;
	static constexpr uint64 BufferAllocationGranularity = 16 * 1024 * 1024;

	// Make sure that all async tasks complete before we run initialization code again and create new tasks.
	WaitForTasks();

	static const uint8 NumLayers = uint8(ERayTracingSceneLayer::NUM);

	checkf(SceneWithGeometryInstances.Scene.IsValid(), 
		TEXT("Ray tracing scene RHI object is expected to have been created by BuildInitializationData() or CreateRayTracingSceneWithGeometryInstances()"));

	RayTracingSceneRHI = SceneWithGeometryInstances.Scene;

	const FRayTracingSceneInitializer2& SceneInitializer = RayTracingSceneRHI->GetInitializer();

	const uint32 NumNativeInstances = SceneWithGeometryInstances.NumNativeGPUSceneInstances + SceneWithGeometryInstances.NumNativeGPUInstances + SceneWithGeometryInstances.NumNativeCPUInstances;
	const uint32 NumNativeInstancesAligned = FMath::DivideAndRoundUp(FMath::Max(NumNativeInstances, 1U), AllocationGranularity) * AllocationGranularity;
	const uint32 NumTransformsAligned = FMath::DivideAndRoundUp(FMath::Max(SceneWithGeometryInstances.NumNativeCPUInstances, 1U), AllocationGranularity) * AllocationGranularity;

	FRayTracingAccelerationStructureSize SizeInfo = RayTracingSceneRHI->GetSizeInfo();
	SizeInfo.ResultSize = FMath::DivideAndRoundUp(FMath::Max(SizeInfo.ResultSize, 1ull), BufferAllocationGranularity) * BufferAllocationGranularity;

	// Allocate GPU buffer if current one is too small or significantly larger than what we need.
	if (!RayTracingSceneBuffer.IsValid() 
		|| SizeInfo.ResultSize > RayTracingSceneBuffer->GetSize() 
		|| SizeInfo.ResultSize < RayTracingSceneBuffer->GetSize() / 2)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("FRayTracingScene::SceneBuffer"));
		RayTracingSceneBuffer = RHICreateBuffer(uint32(SizeInfo.ResultSize), EBufferUsageFlags::AccelerationStructure, 0, ERHIAccess::BVHWrite, CreateInfo);
		State = ERayTracingSceneState::Writable;
	}

	LayerSRVs.SetNum(NumLayers);

	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		FShaderResourceViewInitializer ViewInitializer(RayTracingSceneBuffer, RayTracingSceneRHI->GetLayerBufferOffset(LayerIndex), 0);
		LayerSRVs[LayerIndex] = RHICreateShaderResourceView(ViewInitializer);
	}

	{
		const uint64 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
		FRDGBufferDesc ScratchBufferDesc;
		ScratchBufferDesc.Usage = EBufferUsageFlags::RayTracingScratch | EBufferUsageFlags::StructuredBuffer;
		ScratchBufferDesc.BytesPerElement = uint32(ScratchAlignment);
		ScratchBufferDesc.NumElements = uint32(FMath::DivideAndRoundUp(SizeInfo.BuildScratchSize, ScratchAlignment));

		BuildScratchBuffer = GraphBuilder.CreateBuffer(ScratchBufferDesc, TEXT("FRayTracingScene::ScratchBuffer"));
	}

	{
		FRDGBufferDesc InstanceBufferDesc;
		InstanceBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
		InstanceBufferDesc.BytesPerElement = GRHIRayTracingInstanceDescriptorSize;
		InstanceBufferDesc.NumElements = NumNativeInstancesAligned;

		InstanceBuffer = GraphBuilder.CreateBuffer(InstanceBufferDesc, TEXT("FRayTracingScene::InstanceBuffer"));
	}

	{
		// Round to PoT to avoid resizing too often
		const uint32 NumGeometries = FMath::RoundUpToPowerOfTwo(SceneInitializer.ReferencedGeometries.Num());
		const uint32 AccelerationStructureAddressesBufferSize = NumGeometries * sizeof(FRayTracingAccelerationStructureAddress);

		if (AccelerationStructureAddressesBuffer.NumBytes < AccelerationStructureAddressesBufferSize)
		{
			// Need to pass "BUF_MultiGPUAllocate", as virtual addresses are different per GPU
			AccelerationStructureAddressesBuffer.Initialize(
				TEXT("FRayTracingScene::AccelerationStructureAddressesBuffer"), AccelerationStructureAddressesBufferSize, BUF_Volatile | BUF_MultiGPUAllocate);
		}
	}

	{
		// Create/resize instance upload buffer (if necessary)
		const uint32 UploadBufferSize = NumNativeInstancesAligned * sizeof(FRayTracingInstanceDescriptorInput);

		if (!InstanceUploadBuffer.IsValid()
			|| UploadBufferSize > InstanceUploadBuffer->GetSize()
			|| UploadBufferSize < InstanceUploadBuffer->GetSize() / 2)
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FRayTracingScene::InstanceUploadBuffer"));
			InstanceUploadBuffer = RHICreateStructuredBuffer(sizeof(FRayTracingInstanceDescriptorInput), UploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
			InstanceUploadSRV = RHICreateShaderResourceView(InstanceUploadBuffer);
		}
	}

	{
		const uint32 UploadBufferSize = NumTransformsAligned * sizeof(FVector4f) * 3;

		// Create/resize transform upload buffer (if necessary)
		if (!TransformUploadBuffer.IsValid()
			|| UploadBufferSize > TransformUploadBuffer->GetSize()
			|| UploadBufferSize < TransformUploadBuffer->GetSize() / 2)
		{
			FRHIResourceCreateInfo CreateInfo(TEXT("FRayTracingScene::TransformUploadBuffer"));
			TransformUploadBuffer = RHICreateStructuredBuffer(sizeof(FVector4f), UploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
			TransformUploadSRV = RHICreateShaderResourceView(TransformUploadBuffer);
		}
	}

	if (NumNativeInstances > 0)
	{
		const uint32 InstanceUploadBytes = NumNativeInstances * sizeof(FRayTracingInstanceDescriptorInput);
		const uint32 TransformUploadBytes = SceneWithGeometryInstances.NumNativeCPUInstances * 3 * sizeof(FVector4f);

		FRayTracingInstanceDescriptorInput* InstanceUploadData = (FRayTracingInstanceDescriptorInput*)RHILockBuffer(InstanceUploadBuffer, 0, InstanceUploadBytes, RLM_WriteOnly);
		FVector4f* TransformUploadData = (FVector4f*)RHILockBuffer(TransformUploadBuffer, 0, TransformUploadBytes, RLM_WriteOnly);

		// Fill instance upload buffer on separate thread since results are only needed in RHI thread
		FillInstanceUploadBufferTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
			[InstanceUploadData = MakeArrayView(InstanceUploadData, NumNativeInstances),
			TransformUploadData = MakeArrayView(TransformUploadData, SceneWithGeometryInstances.NumNativeCPUInstances * 3),
			NumNativeGPUSceneInstances = SceneWithGeometryInstances.NumNativeGPUSceneInstances,
			NumNativeCPUInstances = SceneWithGeometryInstances.NumNativeCPUInstances,
			Instances = MakeArrayView(Instances),
			InstanceGeometryIndices = MoveTemp(SceneWithGeometryInstances.InstanceGeometryIndices),
			BaseUploadBufferOffsets = MoveTemp(SceneWithGeometryInstances.BaseUploadBufferOffsets),
			RayTracingSceneRHI = RayTracingSceneRHI,
			PreViewTranslation = ViewMatrices.GetPreViewTranslation()]()
		{
			FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
			FillRayTracingInstanceUploadBuffer(
				RayTracingSceneRHI,
				PreViewTranslation,
				Instances,
				InstanceGeometryIndices,
				BaseUploadBufferOffsets,
				NumNativeGPUSceneInstances,
				NumNativeCPUInstances,
				InstanceUploadData,
				TransformUploadData);
		}, TStatId(), nullptr, ENamedThreads::AnyThread);
		
		if (InstancesDebugData.Num() > 0)
		{
			check(InstancesDebugData.Num() == Instances.Num());
			InstanceDebugBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("FRayTracingScene::InstanceDebugData"), InstancesDebugData);
		}

		FBuildInstanceBufferPassParams* PassParams = GraphBuilder.AllocParameters<FBuildInstanceBufferPassParams>();
		PassParams->InstanceBuffer = GraphBuilder.CreateUAV(InstanceBuffer);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("BuildTLASInstanceBuffer"),
			PassParams,
			ERDGPassFlags::Compute,
			[PassParams,
			this,
			GPUScene,
			ViewTilePosition = ViewTilePosition,
			RelativePreViewTranslation = RelativePreViewTranslation,
			&SceneInitializer,
			NumNativeGPUSceneInstances = SceneWithGeometryInstances.NumNativeGPUSceneInstances,
			NumNativeCPUInstances = SceneWithGeometryInstances.NumNativeCPUInstances,
			GPUInstances = MoveTemp(SceneWithGeometryInstances.GPUInstances)
			](FRHICommandListImmediate& RHICmdList)
			{
				WaitForTasks();
				RHICmdList.UnlockBuffer(InstanceUploadBuffer);
				RHICmdList.UnlockBuffer(TransformUploadBuffer);

				// Pull this out here, because command list playback (where the lambda is executed) doesn't update the GPU mask
				FRHIGPUMask IterateGPUMasks = RHICmdList.GetGPUMask();

				RHICmdList.EnqueueLambda([BufferRHIRef = AccelerationStructureAddressesBuffer.Buffer, &SceneInitializer, IterateGPUMasks](FRHICommandListImmediate& RHICmdList)
					{
						QUICK_SCOPE_CYCLE_COUNTER(GetAccelerationStructuresAddresses);

						for (uint32 GPUIndex : IterateGPUMasks)
						{
							FRayTracingAccelerationStructureAddress* AddressesPtr = (FRayTracingAccelerationStructureAddress*)RHICmdList.LockBufferMGPU(
								BufferRHIRef,
								GPUIndex,
								0,
								SceneInitializer.ReferencedGeometries.Num() * sizeof(FRayTracingAccelerationStructureAddress), RLM_WriteOnly);

							const uint32 NumGeometries = SceneInitializer.ReferencedGeometries.Num();
							for (uint32 GeometryIndex = 0; GeometryIndex < NumGeometries; ++GeometryIndex)
							{
								AddressesPtr[GeometryIndex] = SceneInitializer.ReferencedGeometries[GeometryIndex]->GetAccelerationStructureAddress(GPUIndex);
							}

							RHICmdList.UnlockBufferMGPU(BufferRHIRef, GPUIndex);
						}
					});

				BuildRayTracingInstanceBuffer(
					RHICmdList,
					GPUScene,
					ViewTilePosition,
					FVector3f(RelativePreViewTranslation),
					PassParams->InstanceBuffer->GetRHI(),
					InstanceUploadSRV,
					AccelerationStructureAddressesBuffer.SRV,
					TransformUploadSRV,
					NumNativeGPUSceneInstances,
					NumNativeCPUInstances,
					GPUInstances);
			});
	}
}

void FRayTracingScene::WaitForTasks() const
{
	if (FillInstanceUploadBufferTask.IsValid())
	{
		QUICK_SCOPE_CYCLE_COUNTER(WaitForRayTracingSceneFillInstanceUploadBuffer);
		TRACE_CPUPROFILER_EVENT_SCOPE(WaitForRayTracingSceneFillInstanceUploadBuffer);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(FillInstanceUploadBufferTask, ENamedThreads::GetRenderThread_Local());

		FillInstanceUploadBufferTask = {};
	}
}

bool FRayTracingScene::IsCreated() const
{
	return RayTracingSceneRHI.IsValid();
}

FRHIRayTracingScene* FRayTracingScene::GetRHIRayTracingScene() const
{
	return RayTracingSceneRHI.GetReference();
}

FRHIRayTracingScene* FRayTracingScene::GetRHIRayTracingSceneChecked() const
{
	FRHIRayTracingScene* Result = GetRHIRayTracingScene();
	checkf(Result, TEXT("Ray tracing scene was not created. Perhaps Create() was not called."));
	return Result;
}

FRHIBuffer* FRayTracingScene::GetBufferChecked() const
{
	checkf(RayTracingSceneBuffer.IsValid(), TEXT("Ray tracing scene buffer was not created. Perhaps Create() was not called."));
	return RayTracingSceneBuffer.GetReference();
}

FRHIShaderResourceView* FRayTracingScene::GetLayerSRVChecked(ERayTracingSceneLayer Layer) const
{
	checkf(LayerSRVs[uint8(Layer)].IsValid(), TEXT("Ray tracing scene SRV was not created. Perhaps Create() was not called."));
	return LayerSRVs[uint8(Layer)].GetReference();
}

void FRayTracingScene::AddInstanceDebugData(const FRHIRayTracingGeometry* GeometryRHI, const FPrimitiveSceneProxy* Proxy, bool bDynamic)
{
	FRayTracingInstanceDebugData& InstanceDebugData = InstancesDebugData.AddDefaulted_GetRef();
	InstanceDebugData.Flags = bDynamic ? 1 : 0;
	InstanceDebugData.GeometryAddress = uint64(GeometryRHI);

	if (Proxy)
	{
		InstanceDebugData.ProxyHash = Proxy->GetTypeHash();
	}
}

void FRayTracingScene::Reset()
{
	WaitForTasks();

	Instances.Reset();
	InstancesDebugData.Reset();
	NumMissShaderSlots = 1;
	NumCallableShaderSlots = 0;
	CallableCommands.Reset();
	UniformBuffers.Reset();
	GeometriesToBuild.Reset();
	UsedCoarseMeshStreamingHandles.Reset();

	Allocator.Flush();

	BuildScratchBuffer = nullptr;
	InstanceDebugBuffer = nullptr;
}

void FRayTracingScene::ResetAndReleaseResources()
{
	Reset();

	Instances.Empty();
	InstancesDebugData.Empty();
	CallableCommands.Empty();
	UniformBuffers.Empty();
	GeometriesToBuild.Empty();
	UsedCoarseMeshStreamingHandles.Empty();
	RayTracingSceneBuffer = nullptr;
	RayTracingSceneRHI = nullptr;
	State = ERayTracingSceneState::Writable;
}

void FRayTracingScene::Transition(FRDGBuilder& GraphBuilder, ERayTracingSceneState InState)
{
	if (State == InState)
	{
		return;
	}

	GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingTransition"), ERDGPassFlags::None, 
		[this, InState](FRHIComputeCommandList& RHICmdList)
	{
		if (InState == ERayTracingSceneState::Writable)
		{
			RHICmdList.Transition(FRHITransitionInfo(RayTracingSceneBuffer, ERHIAccess::BVHRead | ERHIAccess::SRVMask, ERHIAccess::BVHWrite));
		}
		else
		{
			RHICmdList.Transition(FRHITransitionInfo(RayTracingSceneBuffer, ERHIAccess::BVHWrite, ERHIAccess::BVHRead | ERHIAccess::SRVMask));
		}
	});

	State = InState;
}

#endif // RHI_RAYTRACING
