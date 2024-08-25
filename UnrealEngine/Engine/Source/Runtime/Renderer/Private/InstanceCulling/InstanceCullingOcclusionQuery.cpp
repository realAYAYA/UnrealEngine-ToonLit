// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceCullingOcclusionQuery.h"

#include "Containers/ArrayView.h"
#include "Containers/ResourceArray.h"
#include "GPUScene.h"
#include "GlobalShader.h"
#include "RHIAccess.h"
#include "RHIFeatureLevel.h"
#include "RHIGlobals.h"
#include "RHIShaderPlatform.h"
#include "RHIStaticStates.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "UnifiedBuffer.h"
#include "HAL/IConsoleManager.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

static TAutoConsoleVariable<int32> CVarInstanceCullingOcclusionQueries(
	TEXT("r.InstanceCulling.OcclusionQueries"),
	0,
	TEXT("EXPERIMENTAL: Use per-instance software occlusion queries to perform less conservative visibility test than what's possible with HZB alone"),
	ECVF_RenderThreadSafe | ECVF_Preview);

struct FInstanceCullingOcclusionQueryDeferredContext;

namespace
{

// YURIY_TODO: Put the helpers somewhere in the common RHI code
struct FResourceArrayView : public FResourceArrayInterface
{
	const void* Data = nullptr;
	uint32 SizeInBytes = 0;

	template <typename T>
	FResourceArrayView(TArrayView<T> View)
		: Data(View.GetData())
		, SizeInBytes(View.Num() * View.GetTypeSize())
	{}

	// FResourceArrayInterface
	virtual const void* GetResourceData() const override final { return Data; }
	virtual uint32 GetResourceDataSize() const override final { return SizeInBytes; }
	virtual void Discard() override final {};
	virtual bool IsStatic() const override final { return true; }
	virtual bool GetAllowCPUAccess() const override final { return false; };
	virtual void SetAllowCPUAccess(bool /*bInNeedsCPUAccess*/) override final {};
};

template <typename T>
static FBufferRHIRef CreateBufferWithData(FRHICommandListBase& RHICmdList, EBufferUsageFlags UsageFlags, ERHIAccess ResourceState, const TCHAR* Name, TConstArrayView<T> Data)
{
	FResourceArrayView DataView(Data);
	FRHIResourceCreateInfo CreateInfo(Name);
	CreateInfo.ResourceArray = &DataView;
	return RHICmdList.CreateBuffer(DataView.SizeInBytes, UsageFlags, Data.GetTypeSize(), ResourceState, CreateInfo);
}


static EPixelFormat GetPreferredVisibilityMaskFormat()
{
	EPixelFormat PossibleFormats[] =
	{
		PF_R8_UINT,  // may be available if typed UAV load/store is supported on current hardware
		PF_R32_UINT, // guaranteed to be supported
	};

	for (EPixelFormat Format : PossibleFormats)
	{
		EPixelFormatCapabilities Capabilities = GPixelFormats[Format].Capabilities;
		if (EnumHasAllFlags(Capabilities, EPixelFormatCapabilities::TypedUAVLoad | EPixelFormatCapabilities::TypedUAVStore))
		{
			return Format;
		}
	}

	return PF_Unknown;
}

}

/*
* Prepares indirect draw parameters for per-instance per-pixel occlusion query rendering pass.
*/
class FInstanceCullingOcclusionQueryCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryCS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryCS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static constexpr int32 NumThreadsPerGroup = 64;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, OutIndirectArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, OutInstanceIdBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutVisibilityMask) // One uint32 per instance (0 if instance is culled, non-0 otherwise)
		SHADER_PARAMETER(uint32, InstanceSceneDataSOAStride)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, InstanceIdBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER(FVector2f, HZBSize)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(float, OcclusionSlop)
		SHADER_PARAMETER(int32, NumInstances)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryCS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "MainCS", SF_Compute);

class FInstanceCullingOcclusionQueryVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryVS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryVS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectDrawArgsBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(uint32, InstanceSceneDataSOAStride)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, InstanceIdBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER(FVector2f, HZBSize)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(float, OcclusionSlop)
	END_SHADER_PARAMETER_STRUCT()
};

class FInstanceCullingOcclusionQueryPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryPS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryPS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutVisibilityMask) // One uint32 per instance (0 if instance is culled, non-0 otherwise)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryVS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryPS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "MainPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FOcclusionInstanceCullingParameters,)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingOcclusionQueryVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingOcclusionQueryPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FInstanceCullingOcclusionQueryBox : public FRenderResource
{
public:
	FBufferRHIRef IndexBuffer;
	FBufferRHIRef VertexBuffer;
	FVertexDeclarationRHIRef VertexDeclaration;

	// Destructor
	virtual ~FInstanceCullingOcclusionQueryBox() {}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		static const uint16 BoxIndexBufferData[] =
		{
			// Tri list
			0, 1, 2, 0, 2, 3,
			4, 5, 6, 4, 6, 7,
			1, 4, 7, 1, 7, 2,
			5, 0, 3, 5, 3, 6,
			5, 4, 1, 5, 1, 0,
			3, 2, 7, 3, 7, 6,
			// Line list
			0, 1, 0, 3, 0, 5,
			7, 2, 7, 6, 7, 4,
			3, 2, 1, 2, 3, 6,
			5, 6, 5, 4, 1, 4
		};

		static const FVector3f BoxVertexBufferData[] =
		{
			FVector3f(-1.0f, +1.0f, +1.0f),
			FVector3f(+1.0f, +1.0f, +1.0f),
			FVector3f(+1.0f, -1.0f, +1.0f),
			FVector3f(-1.0f, -1.0f, +1.0f),
			FVector3f(+1.0f, +1.0f, -1.0f),
			FVector3f(-1.0f, +1.0f, -1.0f),
			FVector3f(-1.0f, -1.0f, -1.0f),
			FVector3f(+1.0f, -1.0f, -1.0f),
		};

		IndexBuffer = CreateBufferWithData(RHICmdList, EBufferUsageFlags::IndexBuffer, ERHIAccess::VertexOrIndexBuffer,
			TEXT("FInstanceCullingOcclusionQueryBox_IndexBuffer"),
			MakeArrayView(BoxIndexBufferData));

		VertexBuffer = CreateBufferWithData(RHICmdList, EBufferUsageFlags::VertexBuffer, ERHIAccess::VertexOrIndexBuffer,
			TEXT("FInstanceCullingOcclusionQueryBox_VertexBuffer"),
			MakeArrayView(BoxVertexBufferData));

		FVertexDeclarationElementList VertexDeclarationElements;
		VertexDeclarationElements.Add(FVertexElement(0, 0, VET_Float3, 0, 12));
		VertexDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(VertexDeclarationElements);
	}

	virtual void ReleaseRHI() override
	{
		IndexBuffer.SafeRelease();
		VertexBuffer.SafeRelease();
		VertexDeclaration.SafeRelease();
	}
};

TGlobalResource<FInstanceCullingOcclusionQueryBox> GInstanceCullingOcclusionQueryBox;

static void RenderInstanceOcclusionCulling(
	FRHICommandList& RHICmdList,
	FViewInfo& View,
	FOcclusionInstanceCullingParameters* PassParameters)
{
	TShaderMapRef<FInstanceCullingOcclusionQueryVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FInstanceCullingOcclusionQueryPS> PixelShader(View.ShaderMap);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	FIntVector4 ViewRect = PassParameters->VS.ViewRect;
	RHICmdList.SetViewport(ViewRect.X, ViewRect.Y, 0.0f, ViewRect.Z, ViewRect.W, 1.0f);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GInstanceCullingOcclusionQueryBox.VertexDeclaration;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI(); // Depth test, no write
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI(); // Blend state does not matter, as we are not writing to render targets
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	ClearUnusedGraphResources(VertexShader, &PassParameters->VS);
	ClearUnusedGraphResources(PixelShader, &PassParameters->PS);

	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

	RHICmdList.SetStreamSource(0, GInstanceCullingOcclusionQueryBox.VertexBuffer, 0);

	FRDGBufferRef IndirectArgsBuffer = PassParameters->VS.IndirectDrawArgsBuffer;
	IndirectArgsBuffer->MarkResourceAsUsed();

	RHICmdList.DrawIndexedPrimitiveIndirect(GInstanceCullingOcclusionQueryBox.IndexBuffer, IndirectArgsBuffer->GetRHI(), 0);
}


/*
* Structure to compute data that's not available on the rendering thread during RDG setup.
* In particular, we want to wait for visible mesh draw commands as late as possible.
*/
struct FInstanceCullingOcclusionQueryDeferredContext
{
	FInstanceCullingOcclusionQueryDeferredContext(const FViewInfo* InView, int32 InNumGPUSceneInstances, EMeshPass::Type InMeshPass)
		: View(InView)
		, NumGPUSceneInstances(InNumGPUSceneInstances)
		, MeshPass(InMeshPass)
	{
	}

	static FORCEINLINE bool IsRelevantCommand(const FVisibleMeshDrawCommand& VisibleCommand)
	{
		// There may be multiple visible mesh draw commands that refer to the same instance when GPU-based LOD selection is used.
		// This filter is designed to remove the duplicates, keeping only the "authoritative" instance.
		// TODO: a less implicit mechanism would be welcome here, such as a dedicated flag.
		const EMeshDrawCommandCullingPayloadFlags Flags = VisibleCommand.CullingPayloadFlags;
		const bool bCompatibleFlags = Flags == EMeshDrawCommandCullingPayloadFlags::Default
			|| Flags == EMeshDrawCommandCullingPayloadFlags::MinScreenSizeCull;

		// Only commands with HasPrimitiveIdStreamIndex are compatible with GPU Instance Culling
		const bool bSupportsGPUSceneInstancing = EnumHasAnyFlags(VisibleCommand.Flags, EFVisibleMeshDrawCommandFlags::HasPrimitiveIdStreamIndex);

		// NumPrimitives is 0 if mesh draw command uses IndirectArgs
		// This path is currently not implemented/supported by oclcusion query culling.
		// Commands that use instance runs are currently not supported.
		return bCompatibleFlags
			&& bSupportsGPUSceneInstancing
			&& VisibleCommand.PrimitiveIdInfo.InstanceSceneDataOffset != INDEX_NONE
			&& VisibleCommand.NumRuns == 0;
	};

	static FORCEINLINE uint32 GetCommandNumInstances(const FVisibleMeshDrawCommand& VisibleMeshDrawCommand, const FScene *Scene)
	{
		const bool bFetchInstanceCountFromScene = EnumHasAnyFlags(VisibleMeshDrawCommand.Flags, EFVisibleMeshDrawCommandFlags::FetchInstanceCountFromScene);
		if (bFetchInstanceCountFromScene)
		{
			check(Scene != nullptr);
			check(!VisibleMeshDrawCommand.PrimitiveIdInfo.bIsDynamicPrimitive);
			return uint32(Scene->Primitives[VisibleMeshDrawCommand.PrimitiveIdInfo.ScenePrimitiveId]->GetNumInstanceSceneDataEntries());
		}
		return VisibleMeshDrawCommand.MeshDrawCommand->NumInstances;
	}

	void Execute()
	{
		if (bExecuted)
		{
			return;
		}

		TRACE_CPUPROFILER_EVENT_SCOPE(FInstanceCullingOcclusionQueryDeferredContext::Execute);

		bExecuted = true;

		const FParallelMeshDrawCommandPass& MeshDrawCommandPass = View->ParallelMeshDrawCommandPasses[MeshPass];

		// Execute() is expected to run late enough to not stall here.
		// If it does happen, then we may have to move the render pass to later point in the frame.
		MeshDrawCommandPass.WaitForSetupTask(); 

		const FMeshCommandOneFrameArray& VisibleMeshDrawCommands = MeshDrawCommandPass.GetMeshDrawCommands();

		const FScene *Scene = View->Family->Scene->GetRenderScene();

		NumInstances = CountVisibleInstances(VisibleMeshDrawCommands, Scene);

		NumThreadGroups = FComputeShaderUtils::GetGroupCount(NumInstances, FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup);

		const int32 MaxSupportedInstances = GRHIGlobals.MaxDispatchThreadGroupsPerDimension.X * FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup;
		if (!ensureMsgf(NumThreadGroups.X * FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup <= MaxSupportedInstances,
			TEXT("Number of instances (%d) is greater than currently supported by FInstanceCullingOcclusionQueryRenderer (%d). ")
			TEXT("Per-instance occlusion queries will be disabled. ")
			TEXT("Increase FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup or implement wrapped group count support."),
			NumInstances, MaxSupportedInstances))
		{
			return;
		}

		// Align buffer sizes to ensure each thread in the thread group has a valid slot to write without introducing bounds checks
		AlignedNumInstances = NumThreadGroups.X * FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup;

		if (AlignedNumInstances == 0)
		{
			return;
		}

		const uint32 DynamicPrimitiveInstanceOffset = View->DynamicPrimitiveCollector.GetInstanceSceneDataOffset();

		FillVisibleInstanceIds(VisibleMeshDrawCommands, DynamicPrimitiveInstanceOffset, Scene);

		bValid = true;
	}

	uint32 CountVisibleInstances(const FMeshCommandOneFrameArray& VisibleMeshDrawCommands, const FScene *Scene) const 
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FInstanceCullingOcclusionQueryDeferredContext::CountVisibleInstances);

		uint32 Result = 0;

		for (const FVisibleMeshDrawCommand& VisibleCommand : VisibleMeshDrawCommands)
		{
			if (!IsRelevantCommand(VisibleCommand))
			{
				continue;
			}
			Result += GetCommandNumInstances(VisibleCommand, Scene);
		}

		return Result;
	}

	void FillVisibleInstanceIds(const FMeshCommandOneFrameArray& VisibleMeshDrawCommands, const uint32 DynamicPrimitiveInstanceOffset, const FScene *Scene)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FInstanceCullingOcclusionQueryDeferredContext::FillVisibleInstanceIds);

		check(AlignedNumInstances != 0);

		// Write output data directly, bypassing TArray::Add overhead (resize branch, etc.)
		VisibleInstanceIds.SetNumUninitialized(AlignedNumInstances);
		uint32* ResultData = VisibleInstanceIds.GetData();
		uint32* ResultCursor = ResultData;
		
		for (const FVisibleMeshDrawCommand& VisibleCommand : VisibleMeshDrawCommands)
		{
			if (!IsRelevantCommand(VisibleCommand))
			{
				continue;
			}
			uint32 CommandNumInstances = GetCommandNumInstances(VisibleCommand, Scene);
			if (CommandNumInstances == 0u)
			{
				continue;
			}

			uint32 InstanceBaseIndex = VisibleCommand.PrimitiveIdInfo.InstanceSceneDataOffset;
			if (VisibleCommand.PrimitiveIdInfo.bIsDynamicPrimitive)
			{
				InstanceBaseIndex += DynamicPrimitiveInstanceOffset;
			}

			check(InstanceBaseIndex + CommandNumInstances <= uint32(NumGPUSceneInstances));

			for (uint32 i = 0; i < CommandNumInstances; ++i)
			{
				*ResultCursor = InstanceBaseIndex + i;
				++ResultCursor;
			}
		}

		for (int32 i = NumInstances; i < AlignedNumInstances; ++i)
		{
			*ResultCursor = 0;
			++ResultCursor;
		}

		check(ResultCursor == ResultData + AlignedNumInstances);
	}

	FRDGBufferNumElementsCallback DeferredAlignedNumInstances()
	{
		return [Context = this]() -> uint32
			{
				Context->Execute();
				return Context->AlignedNumInstances;
			};
	}

	FRDGBufferInitialDataCallback DeferredInstanceIdData()
	{
		return [Context = this]() -> const void*
			{
				Context->Execute();
				return Context->VisibleInstanceIds.GetData();
			};
	}

	FRDGBufferInitialDataSizeCallback DeferredInstanceIdDataSize()
	{
		return [Context = this]() -> uint64
			{
				Context->Execute();
				return Context->VisibleInstanceIds.Num() * Context->VisibleInstanceIds.GetTypeSize();
			};
	}

	// Execute function may be called multiple times, but we only want to run computations once
	bool bExecuted = false;

	// If this is false, then some late validation have failed and rendering should be skipped
	bool bValid = false;

	const FViewInfo* View = nullptr;
	int32 NumGPUSceneInstances = 0;
	EMeshPass::Type MeshPass = EMeshPass::Num;
	int32 NumInstances = 0;
	int32 AlignedNumInstances = 0;
	FIntVector NumThreadGroups = FIntVector::ZeroValue;

	TArray<uint32> VisibleInstanceIds;
};

uint32 FInstanceCullingOcclusionQueryRenderer::Render(
	FRDGBuilder& GraphBuilder,
	FGPUScene& GPUScene,
	FViewInfo& View)
{
	if (!IsCompatibleWithView(View))
	{
		return 0;
	}

	const uint32 ViewMask = RegisterView(View);

	if (ViewMask == 0)
	{
		// Silently fall back to no culling when we hit the limit of maximum supported views
		return 0;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FInstanceCullingOcclusionQueryRenderer::Render);

	const int32 NumGPUSceneInstances = GPUScene.GetNumInstances();

	FInstanceCullingOcclusionQueryDeferredContext* DeferredContext = GraphBuilder.AllocObject<FInstanceCullingOcclusionQueryDeferredContext>(&View, NumGPUSceneInstances, EMeshPass::BasePass);

	FRDGTextureRef DepthTexture = View.GetSceneTextures().Depth.Target;
	FRDGTextureRef HZBTexture = View.HZB;

	checkf(DepthTexture && HZBTexture,
		TEXT("Occlusion query instance culling pass requires scene depth texture and HZB. See FInstanceCullingOcclusionQueryRenderer::IsCompatibleWithView()"));

	const FIntVector HZBSize = HZBTexture->Desc.GetSize();

	const FGPUSceneResourceParameters GPUSceneParameters = GPUScene.GetShaderParameters(GraphBuilder);

	const FIntPoint ViewRectSize = View.ViewRect.Size();

	EPixelFormat VisibilityMaskFormat = GetPreferredVisibilityMaskFormat();
	int32 VisibilityMaskStride = GPixelFormats[VisibilityMaskFormat].BlockBytes;

	// Create the result buffer on demand
	if (!CurrentInstanceOcclusionQueryBuffer)
	{
		const int32 AlignedNumGPUSceneInstances =
			FMath::DivideAndRoundUp(NumGPUSceneInstances, FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup)
			* FInstanceCullingOcclusionQueryCS::NumThreadsPerGroup;

		CurrentInstanceOcclusionQueryBuffer = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateBufferDesc(VisibilityMaskStride, AlignedNumGPUSceneInstances),
			TEXT("FInstanceCullingOcclusionQueryRenderer_VisibleInstanceMask"));

		InstanceOcclusionQueryBufferFormat = VisibilityMaskFormat;

		AllocatedNumInstances = NumGPUSceneInstances;

		// Create a wide-format alias for the underlying resource for a more efficient clear
		FRDGBufferUAVRef UAV = GraphBuilder.CreateUAV(CurrentInstanceOcclusionQueryBuffer, PF_R32G32B32A32_UINT);
		AddClearUAVPass(GraphBuilder, UAV, 0xFFFFFFFF);
	}

	checkf(uint32(NumGPUSceneInstances) == AllocatedNumInstances, TEXT("Number of instances in GPUScene is not expected change to during the frame"));

	FRDGBufferRef VisibleInstanceMaskBuffer = CurrentInstanceOcclusionQueryBuffer;
	FRDGBufferUAVRef VisibilityMaskUAV = GraphBuilder.CreateUAV(VisibleInstanceMaskBuffer, VisibilityMaskFormat);

	FRDGBufferRef IndirectArgsBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndexedIndirectParameters>(1),
		TEXT("FInstanceCullingOcclusionQueryRenderer_IndirectArgsBuffer"));
	FRDGBufferUAVRef IndirectArgsUAV = GraphBuilder.CreateUAV(IndirectArgsBuffer, PF_R32_UINT);

	// Buffer of GPUScene instance indices to run occlusion queries for (input for setup CS)
	FRDGBufferRef SetupInstanceIdBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1 /*real size is provided via callback later*/),
		TEXT("FInstanceCullingOcclusionQueryRenderer_SetupInstanceIdBuffer"), 
		DeferredContext->DeferredAlignedNumInstances());

	GraphBuilder.QueueBufferUpload(SetupInstanceIdBuffer,
		DeferredContext->DeferredInstanceIdData(),
		DeferredContext->DeferredInstanceIdDataSize());

	FRDGBufferSRVRef SetupInstanceIdBufferSRV = GraphBuilder.CreateSRV(SetupInstanceIdBuffer, PF_R32_UINT);

	// Buffer of GPUScene instance indices that passed the filtering in the setup CS pass and should be rendered in the subsequent graphics pass
	FRDGBufferRef InstanceIdBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1 /*real size is provided via callback later*/),
		TEXT("FInstanceCullingOcclusionQueryRenderer_InstanceIdBuffer"),
		DeferredContext->DeferredAlignedNumInstances());

	FRDGBufferUAVRef InstanceIdUAV = GraphBuilder.CreateUAV(InstanceIdBuffer, PF_R32_UINT);
	FRDGBufferSRVRef InstanceIdSRV = GraphBuilder.CreateSRV(InstanceIdBuffer, PF_R32_UINT);

	AddClearUAVPass(GraphBuilder, IndirectArgsUAV, 0);

	// Compute pass to perform initial per-instance filtering and prepare instance list for per-pixel occlusion tests
	{
		FInstanceCullingOcclusionQueryCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInstanceCullingOcclusionQueryCS::FParameters>();

		PassParameters->OutIndirectArgsBuffer = IndirectArgsUAV;
		PassParameters->OutInstanceIdBuffer = InstanceIdUAV;
		PassParameters->OutVisibilityMask = VisibilityMaskUAV;
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->HZBTexture = HZBTexture;
		PassParameters->HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->HZBSize = FVector2f(HZBSize.X, HZBSize.Y);
		PassParameters->ViewRect = FIntVector4(View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y);
		PassParameters->OcclusionSlop = OCCLUSION_SLOP;
		PassParameters->InstanceSceneDataSOAStride = GPUSceneParameters.InstanceDataSOAStride;
		PassParameters->GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
		PassParameters->GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
		PassParameters->GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;
		PassParameters->NumInstances = 0; // filled from DeferredContext later
		PassParameters->InstanceIdBuffer = SetupInstanceIdBufferSRV;

		TShaderMapRef<FInstanceCullingOcclusionQueryCS> ComputeShader(View.ShaderMap);

		ClearUnusedGraphResources(ComputeShader, PassParameters);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("InstanceCullingOcclusionQueryRenderer_Setup"),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, DeferredContext, ComputeShader](FRHIRayTracingCommandList& RHICmdList)
		{
			if (!DeferredContext->bValid)
			{
				return;
			}

			PassParameters->NumInstances = DeferredContext->NumInstances;

			FComputeShaderUtils::Dispatch(
				RHICmdList,
				ComputeShader,
				*PassParameters,
				DeferredContext->NumThreadGroups);
		});
	}

	// Perform per-instance per-pixel occlusion tests by drawing bounding boxes that write into VisibleInstanceMaskBuffer slots for visible instances
	{
		FOcclusionInstanceCullingParameters* PassParameters = GraphBuilder.AllocParameters<FOcclusionInstanceCullingParameters>();

		PassParameters->VS.IndirectDrawArgsBuffer = IndirectArgsBuffer;
		PassParameters->VS.View = View.ViewUniformBuffer;
		PassParameters->VS.HZBTexture = HZBTexture;
		PassParameters->VS.HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->VS.HZBSize = FVector2f(HZBSize.X, HZBSize.Y);
		PassParameters->VS.ViewRect = FIntVector4(View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y);
		PassParameters->VS.OcclusionSlop = OCCLUSION_SLOP;
		PassParameters->VS.InstanceSceneDataSOAStride = GPUSceneParameters.InstanceDataSOAStride;
		PassParameters->VS.GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
		PassParameters->VS.GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
		PassParameters->VS.GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;
		PassParameters->VS.InstanceIdBuffer = InstanceIdSRV;
		PassParameters->PS.OutVisibilityMask = VisibilityMaskUAV;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(DepthTexture,
			ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ENoAction,
			FExclusiveDepthStencil::DepthRead_StencilNop);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("InstanceCullingOcclusionQueryRenderer_Draw"),
			PassParameters, ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
			[PassParameters, DeferredContext, &View](FRHICommandList& RHICmdList)
			{
				if (!DeferredContext->bValid)
				{
					return;
				}

				RenderInstanceOcclusionCulling(RHICmdList, View, PassParameters);
			});
	}

	return ViewMask;
}

void FInstanceCullingOcclusionQueryRenderer::MarkInstancesVisible(FRDGBuilder& GraphBuilder, TConstArrayView<FGPUSceneInstanceRange> Ranges)
{
	if (!InstanceOcclusionQueryBuffer)
	{
		// Previous frame buffer does not exist, nothing to clear
		return;
	}

	EPixelFormat VisibilityMaskFormat = GetPreferredVisibilityMaskFormat();

	FRDGBufferRef Buffer = GraphBuilder.RegisterExternalBuffer(InstanceOcclusionQueryBuffer);

	// Consecutive uses of the UAV will run in parallel.
	// Allocating a unique RDG UAV here will still ensure that a barrier is inserted before the first dispatch.
	FRDGBufferUAVRef UAV = GraphBuilder.CreateUAV(Buffer, VisibilityMaskFormat, ERDGUnorderedAccessViewFlags::SkipBarrier);

	// NOTE: It is possible to make this more efficient using a specialized GPU scatter shader, if we see many small batches here in practice
	for (FGPUSceneInstanceRange Range : Ranges)
	{
		FMemsetResourceParams MemsetParams;
		MemsetParams.Value = 0xFFFFFFFF; // Mark instance visible in all views
		MemsetParams.Count = Range.NumInstanceSceneDataEntries;
		MemsetParams.DstOffset = Range.InstanceSceneDataOffset;
		MemsetResource(GraphBuilder, UAV, MemsetParams);
	}
}

void FInstanceCullingOcclusionQueryRenderer::EndFrame(FRDGBuilder& GraphBuilder)
{
	if (CurrentInstanceOcclusionQueryBuffer)
	{
		GraphBuilder.QueueBufferExtraction(CurrentInstanceOcclusionQueryBuffer, &InstanceOcclusionQueryBuffer, ERHIAccess::SRVMask);
		CurrentInstanceOcclusionQueryBuffer = {};
		AllocatedNumInstances = 0;
	}
	CurrentRenderedViewIDs.Empty();
}

uint32 FInstanceCullingOcclusionQueryRenderer::RegisterView(const FViewInfo& View)
{
	if (CurrentRenderedViewIDs.Num() < MaxViews)
	{
		int32 Index = CurrentRenderedViewIDs.AddUnique(View.GetViewKey());
		check(Index >= 0 && Index < MaxViews);
		return 1u << Index;
	}
	else
	{
		return 0;
	}
}

bool FInstanceCullingOcclusionQueryRenderer::IsCompatibleWithView(const FViewInfo& View)
{
	EPixelFormat VisibilityMaskFormat = GetPreferredVisibilityMaskFormat();
	return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(View.GetShaderPlatform())
		&& View.GetSceneTextures().Depth.Target
		&& View.HZB
		&& VisibilityMaskFormat != PF_Unknown
		&& CVarInstanceCullingOcclusionQueries.GetValueOnRenderThread() != 0;
}

// Debugging utilities

class FInstanceCullingOcclusionQueryDebugVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryDebugVS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryDebugVS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, InstanceSceneDataSOAStride)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstanceSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUSceneInstancePayloadData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, InstanceOcclusionQueryBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER(FVector2f, HZBSize)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(float, OcclusionSlop)
	END_SHADER_PARAMETER_STRUCT()
};

class FInstanceCullingOcclusionQueryDebugPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInstanceCullingOcclusionQueryDebugPS);
	SHADER_USE_PARAMETER_STRUCT(FInstanceCullingOcclusionQueryDebugPS, FGlobalShader);

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FDataDrivenShaderPlatformInfo::GetSupportsVertexShaderSRVs(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryDebugVS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "DebugMainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FInstanceCullingOcclusionQueryDebugPS, "/Engine/Private/InstanceCulling/InstanceCullingOcclusionQuery.usf", "DebugMainPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FOcclusionInstanceCullingDebugParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingOcclusionQueryDebugVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingOcclusionQueryDebugPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static void RenderInstanceOcclusionCullingDebug(
	FRHICommandList& RHICmdList,
	const FViewInfo& View,
	FOcclusionInstanceCullingDebugParameters* PassParameters,
	int32 NumInstances)
{
	TShaderMapRef<FInstanceCullingOcclusionQueryDebugVS> VertexShader(View.ShaderMap);
	TShaderMapRef<FInstanceCullingOcclusionQueryDebugPS> PixelShader(View.ShaderMap);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	FIntVector4 ViewRect = PassParameters->VS.ViewRect;
	RHICmdList.SetViewport(ViewRect.X, ViewRect.Y, 0.0f, ViewRect.Z, ViewRect.W, 1.0f);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GInstanceCullingOcclusionQueryBox.VertexDeclaration;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI(); // No depth test or write
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI(); // Premultiplied
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_LineList;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	ClearUnusedGraphResources(VertexShader, &PassParameters->VS);
	ClearUnusedGraphResources(PixelShader, &PassParameters->PS);

	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

	RHICmdList.SetStreamSource(0, GInstanceCullingOcclusionQueryBox.VertexBuffer, 0);

	RHICmdList.DrawIndexedPrimitive(GInstanceCullingOcclusionQueryBox.IndexBuffer, 0, 0, 24, 36, 12, NumInstances);
}

void FInstanceCullingOcclusionQueryRenderer::RenderDebug(FRDGBuilder& GraphBuilder, FGPUScene& GPUScene, const FViewInfo& View, FSceneTextures& SceneTextures)
{
	if (!IsCompatibleWithView(View) || !InstanceOcclusionQueryBuffer)
	{
		return;
	}

	FRDGTextureRef SceneColor = View.GetSceneTextures().Color.Target;
	FRDGTextureRef SceneDepth = View.GetSceneTextures().Depth.Target;
	FRDGBufferRef InstanceOcclusionQueryBufferRDG = GraphBuilder.RegisterExternalBuffer(InstanceOcclusionQueryBuffer);

	FRDGTextureRef DepthTexture = View.GetSceneTextures().Depth.Target;
	FRDGTextureRef HZBTexture = View.HZB;

	checkf(DepthTexture && HZBTexture,
		TEXT("Occlusion query instance culling requires scene depth texture and HZB. See FInstanceCullingOcclusionQueryRenderer::IsCompatibleWithView()"));

	const FIntVector HZBSize = HZBTexture->Desc.GetSize();

	const int32 NumInstances = GPUScene.GetNumInstances();
	const FGPUSceneResourceParameters GPUSceneParameters = GPUScene.GetShaderParameters(GraphBuilder);

	FOcclusionInstanceCullingDebugParameters* PassParameters = GraphBuilder.AllocParameters<FOcclusionInstanceCullingDebugParameters>();

	PassParameters->VS.OcclusionSlop = OCCLUSION_SLOP;
	PassParameters->VS.ViewRect = FIntVector4(View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y);
	PassParameters->VS.View = View.ViewUniformBuffer;
	PassParameters->VS.InstanceSceneDataSOAStride = GPUSceneParameters.InstanceDataSOAStride;
	PassParameters->VS.GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
	PassParameters->VS.GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
	PassParameters->VS.GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;
	PassParameters->VS.InstanceOcclusionQueryBuffer = GraphBuilder.CreateSRV(InstanceOcclusionQueryBufferRDG, InstanceOcclusionQueryBufferFormat);
	PassParameters->VS.HZBTexture = HZBTexture;
	PassParameters->VS.HZBSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->VS.HZBSize = FVector2f(HZBSize.X, HZBSize.Y);
	PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColor, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneDepth,
		ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ENoAction,
		FExclusiveDepthStencil::DepthRead_StencilNop);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("InstanceCullingOcclusionQueryRenderer_Draw"),
		PassParameters, ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
		[PassParameters, NumInstances, &View](FRHICommandList& RHICmdList)
		{
			RenderInstanceOcclusionCullingDebug(RHICmdList, View, PassParameters, NumInstances);
		});

}

