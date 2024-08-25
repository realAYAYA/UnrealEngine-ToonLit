// Copyright Epic Games, Inc. All Rights Reserved.

#include "RayTracingScene.h"

#if RHI_RAYTRACING

#include "RayTracingInstanceBufferUtil.h"
#include "RenderCore.h"
#include "RayTracingDefinitions.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RaytracingOptions.h"
#include "PrimitiveSceneProxy.h"
#include "SceneUniformBuffer.h"
#include "SceneRendering.h"
#include "RayTracingInstanceCulling.h"

BEGIN_SHADER_PARAMETER_STRUCT(FBuildInstanceBufferPassParams, )
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, InstanceBuffer)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, DebugInstanceGPUSceneIndexBuffer)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
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

void FRayTracingScene::Create(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FGPUScene* GPUScene)
{
	CreateWithInitializationData(GraphBuilder, View, GPUScene, BuildInitializationData());
}

void FRayTracingScene::InitPreViewTranslation(const FViewMatrices& ViewMatrices)
{
	PreViewTranslation = FDFVector3(ViewMatrices.GetPreViewTranslation());
}

void FRayTracingScene::CreateWithInitializationData(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FGPUScene* GPUScene, FRayTracingSceneWithGeometryInstances SceneWithGeometryInstances)
{
	QUICK_SCOPE_CYCLE_COUNTER(FRayTracingScene_BeginCreate);

	// Round up buffer sizes to some multiple to avoid pathological growth reallocations.
	static constexpr uint32 AllocationGranularity = 8 * 1024;
	static constexpr uint64 BufferAllocationGranularity = 16 * 1024 * 1024;

	// Make sure that all async tasks complete before we run initialization code again and create new tasks.
	WaitForTasks();

	FRHICommandListBase& RHICmdList = GraphBuilder.RHICmdList;

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
		RayTracingSceneBuffer = GraphBuilder.RHICmdList.CreateBuffer(uint32(SizeInfo.ResultSize), EBufferUsageFlags::AccelerationStructure, 0, ERHIAccess::BVHWrite, CreateInfo);

		FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(1, uint32(SizeInfo.ResultSize));
		Desc.Usage = EBufferUsageFlags::AccelerationStructure;
		RayTracingScenePooledBuffer = new FRDGPooledBuffer(RHICmdList, RayTracingSceneBuffer, Desc, Desc.NumElements, TEXT("FRayTracingScene::SceneBuffer::RDG"));
	}
	RayTracingSceneBufferRDG = GraphBuilder.RegisterExternalBuffer(RayTracingScenePooledBuffer);

	LayerSRVs.SetNum(NumLayers);
	for (uint32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		LayerSRVs[LayerIndex] = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(RayTracingSceneBufferRDG, RayTracingSceneRHI->GetLayerBufferOffset(LayerIndex), 0));
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
				GraphBuilder.RHICmdList, TEXT("FRayTracingScene::AccelerationStructureAddressesBuffer"), AccelerationStructureAddressesBufferSize, BUF_Volatile | BUF_MultiGPUAllocate);
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
			InstanceUploadBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FRayTracingInstanceDescriptorInput), UploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
			InstanceUploadSRV = RHICmdList.CreateShaderResourceView(InstanceUploadBuffer);
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
			TransformUploadBuffer = RHICmdList.CreateStructuredBuffer(sizeof(FVector4f), UploadBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
			TransformUploadSRV = RHICmdList.CreateShaderResourceView(TransformUploadBuffer);
		}
	}

	FRDGBufferUAVRef DebugInstanceGPUSceneIndexBufferUAV = nullptr;
	if (bNeedsDebugInstanceGPUSceneIndexBuffer)
	{
		FRDGBufferDesc DebugInstanceGPUSceneIndexBufferDesc;
		DebugInstanceGPUSceneIndexBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess | EBufferUsageFlags::ShaderResource | EBufferUsageFlags::StructuredBuffer;
		DebugInstanceGPUSceneIndexBufferDesc.BytesPerElement = sizeof(uint32);
		DebugInstanceGPUSceneIndexBufferDesc.NumElements = FMath::Max(NumNativeInstances, 1u);

		DebugInstanceGPUSceneIndexBuffer = GraphBuilder.CreateBuffer(DebugInstanceGPUSceneIndexBufferDesc, TEXT("FRayTracingScene::DebugInstanceGPUSceneIndexBuffer"));
		DebugInstanceGPUSceneIndexBufferUAV = GraphBuilder.CreateUAV(DebugInstanceGPUSceneIndexBuffer);

		AddClearUAVPass(GraphBuilder, DebugInstanceGPUSceneIndexBufferUAV, 0xFFFFFFFF);
	}

	if (NumNativeInstances > 0)
	{
		const uint32 InstanceUploadBytes = NumNativeInstances * sizeof(FRayTracingInstanceDescriptorInput);
		const uint32 TransformUploadBytes = SceneWithGeometryInstances.NumNativeCPUInstances * 3 * sizeof(FVector4f);

		FRayTracingInstanceDescriptorInput* InstanceUploadData = (FRayTracingInstanceDescriptorInput*)RHICmdList.LockBuffer(InstanceUploadBuffer, 0, InstanceUploadBytes, RLM_WriteOnly);
		FVector4f* TransformUploadData = (TransformUploadBytes > 0) ? (FVector4f*)RHICmdList.LockBuffer(TransformUploadBuffer, 0, TransformUploadBytes, RLM_WriteOnly) : nullptr;

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
			PreViewTranslation = View.ViewMatrices.GetPreViewTranslation()]()
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
		PassParams->DebugInstanceGPUSceneIndexBuffer = DebugInstanceGPUSceneIndexBufferUAV;
		PassParams->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("BuildTLASInstanceBuffer"),
			PassParams,
			ERDGPassFlags::Compute,
			[PassParams,
			this,
			GPUScene,
			PreViewTranslation = PreViewTranslation,
			&SceneInitializer,
			NumNativeGPUSceneInstances = SceneWithGeometryInstances.NumNativeGPUSceneInstances,
			NumNativeCPUInstances = SceneWithGeometryInstances.NumNativeCPUInstances,
			GPUInstances = MoveTemp(SceneWithGeometryInstances.GPUInstances),
			CullingParameters = View.RayTracingCullingParameters
			](FRHICommandListImmediate& RHICmdList)
			{
				WaitForTasks();
				RHICmdList.UnlockBuffer(InstanceUploadBuffer);

				if (NumNativeCPUInstances > 0)
				{
					RHICmdList.UnlockBuffer(TransformUploadBuffer);
				}

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
					PreViewTranslation,
					PassParams->InstanceBuffer->GetRHI(),
					InstanceUploadSRV,
					AccelerationStructureAddressesBuffer.SRV,
					TransformUploadSRV,
					NumNativeGPUSceneInstances,
					NumNativeCPUInstances,
					GPUInstances,
					CullingParameters.bUseInstanceCulling ? &CullingParameters : nullptr,
					PassParams->DebugInstanceGPUSceneIndexBuffer ? PassParams->DebugInstanceGPUSceneIndexBuffer->GetRHI() : nullptr);
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

FRDGBufferRef FRayTracingScene::GetBufferChecked() const
{
	checkf(RayTracingSceneBufferRDG, TEXT("Ray tracing scene buffer was not created. Perhaps Create() was not called."));
	return RayTracingSceneBufferRDG;
}

FShaderResourceViewRHIRef FRayTracingScene::CreateLayerViewRHI(FRHICommandListBase& RHICmdList, ERayTracingSceneLayer Layer) const
{
	const uint8 LayerIndex = uint8(Layer);
	checkf(RayTracingSceneBuffer, TEXT("Ray tracing scene was not created.Perhaps Create() was not called."));
	return RHICmdList.CreateShaderResourceView(FShaderResourceViewInitializer(RayTracingSceneBuffer, RayTracingSceneRHI->GetLayerBufferOffset(LayerIndex), 0));
}

FRDGBufferSRVRef FRayTracingScene::GetLayerView(ERayTracingSceneLayer Layer) const
{
	checkf(LayerSRVs[uint8(Layer)], TEXT("Ray tracing scene SRV was not created. Perhaps Create() was not called."));
	return LayerSRVs[uint8(Layer)];
}

uint32 FRayTracingScene::AddInstance(FRayTracingGeometryInstance Instance, const FPrimitiveSceneProxy* Proxy, bool bDynamic)
{
	FRHIRayTracingGeometry* GeometryRHI = Instance.GeometryRHI;

	const uint32 InstanceIndex = Instances.Add(MoveTemp(Instance));

	if (bInstanceDebugDataEnabled)
	{
		FRayTracingInstanceDebugData& InstanceDebugData = InstancesDebugData.AddDefaulted_GetRef();
		InstanceDebugData.Flags = bDynamic ? 1 : 0;
		InstanceDebugData.GeometryAddress = uint64(GeometryRHI);

		if (Proxy)
		{
			InstanceDebugData.ProxyHash = Proxy->GetTypeHash();
		}

		check(Instances.Num() == InstancesDebugData.Num());
	}

	return InstanceIndex;
}

uint32 FRayTracingScene::AddInstancesUninitialized(uint32 NumInstances)
{
	const uint32 OldNum = Instances.AddUninitialized(NumInstances);

	if (bInstanceDebugDataEnabled)
	{
		InstancesDebugData.AddUninitialized(NumInstances);

		check(Instances.Num() == InstancesDebugData.Num());
	}

	return OldNum;
}

void FRayTracingScene::SetInstance(uint32 InstanceIndex, FRayTracingGeometryInstance InInstance, const FPrimitiveSceneProxy* Proxy, bool bDynamic)
{
	FRHIRayTracingGeometry* GeometryRHI = InInstance.GeometryRHI;

	FRayTracingGeometryInstance* Instance = &Instances[InstanceIndex];
	new (Instance) FRayTracingGeometryInstance(MoveTemp(InInstance));

	if (bInstanceDebugDataEnabled)
	{
		FRayTracingInstanceDebugData InstanceDebugData;
		InstanceDebugData.Flags = bDynamic ? 1 : 0;
		InstanceDebugData.GeometryAddress = uint64(GeometryRHI);

		if (Proxy)
		{
			InstanceDebugData.ProxyHash = Proxy->GetTypeHash();
		}

		InstancesDebugData[InstanceIndex] = InstanceDebugData;
	}
}

void FRayTracingScene::Reset(bool bInInstanceDebugDataEnabled)
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
	DebugInstanceGPUSceneIndexBuffer = nullptr;

	bInstanceDebugDataEnabled = bInInstanceDebugDataEnabled;
}

void FRayTracingScene::ResetAndReleaseResources()
{
	Reset(false);

	Instances.Empty();
	InstancesDebugData.Empty();
	CallableCommands.Empty();
	UniformBuffers.Empty();
	GeometriesToBuild.Empty();
	UsedCoarseMeshStreamingHandles.Empty();
	RayTracingSceneBuffer = nullptr;
	RayTracingScenePooledBuffer = nullptr;
	RayTracingSceneRHI = nullptr;
}

#endif // RHI_RAYTRACING
