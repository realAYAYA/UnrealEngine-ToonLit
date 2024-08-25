// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSceneGPUDrivenUpdate.h"
#include "ScenePrivate.h"
#include "Lumen.h"
#include "LumenSceneData.h"

static TAutoConsoleVariable<int> CVarLumenSceneCardMinResolution(
	TEXT("r.LumenScene.SurfaceCache.CardMinResolution"),
	4,
	TEXT("Minimum mesh card size resolution to be visible in Lumen Scene"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenSceneCardTexelDensityScale(
	TEXT("r.LumenScene.SurfaceCache.CardTexelDensityScale"),
	100.0f,
	TEXT("Lumen card texels per world space distance"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenSceneFarFieldTexelDensity(
	TEXT("r.LumenScene.SurfaceCache.FarField.CardTexelDensity"),
	0.001f,
	TEXT("Far Field Lumen card texels per world space unit"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenSceneFarFieldDistance(
	TEXT("r.LumenScene.SurfaceCache.FarField.CardDistance"),
	40000.00f,
	TEXT("Far Field Lumen card culling distance"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenSceneCardCaptureMargin(
	TEXT("r.LumenScene.SurfaceCache.CardCaptureMargin"),
	0.0f,
	TEXT("How far from Lumen scene range start to capture cards."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSceneStats(
	TEXT("r.LumenScene.Stats"),
	0,
	TEXT("Display various Lumen GPU Scene stats for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSceneVisualizePrimitiveGroups(
	TEXT("r.LumenScene.VisualizePrimitiveGroups"),
	0,
	TEXT("Visualize Lumen GPU Scene Primitive Groups."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarOrthoLumenSceneMinCardResolution(
	TEXT("r.Lumen.Ortho.LumenSceneMinCardResolution"),
	1,
	TEXT("If an orthographic view is present, forc the SurfaceCache MinCard to be set to OrthoMinCardResolution, otherwise use the standard MinCardResolution")
	TEXT("0 is disabled, higher values will force the resolution in Orthographic views"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float LumenScene::GetCardMaxDistance(const FViewInfo& View)
{
	// Limit to global distance field range
	const float LastClipmapExtent = Lumen::GetGlobalDFClipmapExtent(Lumen::GetNumGlobalDFClipmaps(View) - 1);
	float MaxCardDistanceFromCamera = LastClipmapExtent;

#if RHI_RAYTRACING
	// Limit to ray tracing culling radius if ray tracing is used
	if (Lumen::UseHardwareRayTracing(*View.Family) && RayTracing::GetCullingMode(View.Family->EngineShowFlags) != RayTracing::ECullingMode::Disabled)
	{
		MaxCardDistanceFromCamera = GetRayTracingCullingRadius();
	}
#endif

	return MaxCardDistanceFromCamera + CVarLumenSceneCardCaptureMargin.GetValueOnRenderThread();
}

float LumenScene::GetCardTexelDensity()
{
	return CVarLumenSceneCardTexelDensityScale.GetValueOnRenderThread() * (GLumenFastCameraMode ? .2f : 1.0f);
}

float LumenScene::GetFarFieldCardTexelDensity()
{
	return CVarLumenSceneFarFieldTexelDensity.GetValueOnRenderThread();
}

float LumenScene::GetFarFieldCardMaxDistance()
{
	return CVarLumenSceneFarFieldDistance.GetValueOnRenderThread();
}

int32 LumenScene::GetCardMinResolution(bool bOrthographicCamera)
{
	if (bOrthographicCamera)
	{
		int32 OrthoMinCardResolution = CVarOrthoLumenSceneMinCardResolution.GetValueOnRenderThread();
		if(OrthoMinCardResolution > 0)
		{
			return OrthoMinCardResolution;
		}
	}
	return FMath::Clamp(CVarLumenSceneCardMinResolution.GetValueOnRenderThread(), 1, 1024);
}

FLumenSceneReadback::FLumenSceneReadback()
{
	ReadbackBuffers.AddDefaulted(MaxReadbackBuffers);
}

FLumenSceneReadback::~FLumenSceneReadback()
{
	for (int32 BufferIndex = 0; BufferIndex < ReadbackBuffers.Num(); ++BufferIndex)
	{
		if (ReadbackBuffers[BufferIndex].AddOps)
		{
			delete ReadbackBuffers[BufferIndex].AddOps;
			ReadbackBuffers[BufferIndex].AddOps = nullptr;
		}

		if (ReadbackBuffers[BufferIndex].RemoveOps)
		{
			delete ReadbackBuffers[BufferIndex].RemoveOps;
			ReadbackBuffers[BufferIndex].RemoveOps = nullptr;
		}
	}
}

FLumenSceneReadback::FBuffersRDG FLumenSceneReadback::GetWriteBuffers(FRDGBuilder& GraphBuilder)
{
	FBuffersRDG BuffersRDG;

	// Only run when queue isn't full. It is NOT safe to EnqueueCopy on a buffer that already has a pending copy.
	if (ReadbackBuffersNumPending != MaxReadbackBuffers)
	{
		{
			FRDGBufferDesc BufferDesc(FRDGBufferDesc::CreateStructuredDesc(sizeof(FAddOp), MaxAddOps));
			BufferDesc.Usage |= BUF_SourceCopy;
			BuffersRDG.AddOps = GraphBuilder.CreateBuffer(BufferDesc, TEXT("Lumen.SceneAddOps"));
		}

		{
			FRDGBufferDesc BufferDesc(FRDGBufferDesc::CreateStructuredDesc(sizeof(FRemoveOp), MaxRemoveOps));
			BufferDesc.Usage |= BUF_SourceCopy;
			BuffersRDG.RemoveOps = GraphBuilder.CreateBuffer(BufferDesc, TEXT("Lumen.SceneRemoveOps"));
		}
	}

	return BuffersRDG;
}

void FLumenSceneReadback::SubmitWriteBuffers(FRDGBuilder& GraphBuilder, FBuffersRDG SrcBuffers)
{
	if (!ReadbackBuffers[ReadbackBuffersWriteIndex].AddOps)
	{
		ReadbackBuffers[ReadbackBuffersWriteIndex].AddOps = new FRHIGPUBufferReadback(TEXT("Lumen.SceneAddOpsReadback"));
	}

	if (!ReadbackBuffers[ReadbackBuffersWriteIndex].RemoveOps)
	{
		ReadbackBuffers[ReadbackBuffersWriteIndex].RemoveOps = new FRHIGPUBufferReadback(TEXT("Lumen.SceneRemoveOpsReadback"));
	}

	FBuffersRHI DstBuffers = ReadbackBuffers[ReadbackBuffersWriteIndex];

	AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("LumenSceneAddOpsReadback"), SrcBuffers.AddOps,
		[DstBuffers, SrcBuffers](FRHICommandList& RHICmdList)
		{
			DstBuffers.AddOps->EnqueueCopy(RHICmdList, SrcBuffers.AddOps->GetRHI(), 0u);
		});

	AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("LumenSceneRemoveOpsReadback"), SrcBuffers.RemoveOps,
		[DstBuffers, SrcBuffers](FRHICommandList& RHICmdList)
		{
			DstBuffers.RemoveOps->EnqueueCopy(RHICmdList, SrcBuffers.RemoveOps->GetRHI(), 0u);
		});

	ReadbackBuffersWriteIndex = (ReadbackBuffersWriteIndex + 1) % MaxReadbackBuffers;
	ReadbackBuffersNumPending = FMath::Min(ReadbackBuffersNumPending + 1, MaxReadbackBuffers);
}

FLumenSceneReadback::FBuffersRHI FLumenSceneReadback::GetLatestReadbackBuffers()
{
	FBuffersRHI LatestReadbackBuffers;

	// Find latest buffer that is ready
	while (ReadbackBuffersNumPending > 0)
	{
		uint32 Index = (ReadbackBuffersWriteIndex + MaxReadbackBuffers - ReadbackBuffersNumPending) % MaxReadbackBuffers;
		if (ReadbackBuffers[Index].AddOps->IsReady() && ReadbackBuffers[Index].RemoveOps->IsReady())
		{
			--ReadbackBuffersNumPending;
			LatestReadbackBuffers = ReadbackBuffers[Index];
		}
		else
		{
			break;
		}
	}

	return LatestReadbackBuffers;
}

class FLumenSceneUpdateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenSceneUpdateCS)
		SHADER_USE_PARAMETER_STRUCT(FLumenSceneUpdateCS, FGlobalShader)

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWSceneAddOps)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWSceneRemoveOps)
		SHADER_PARAMETER(uint32, MaxSceneAddOps)
		SHADER_PARAMETER(uint32, MaxSceneRemoveOps)
		SHADER_PARAMETER(float, CardMaxDistanceSq)
		SHADER_PARAMETER(float, CardTexelDensity)
		SHADER_PARAMETER(float, FarFieldCardMaxDistanceSq)
		SHADER_PARAMETER(float, FarFieldCardTexelDensity)
		SHADER_PARAMETER(float, MinCardResolution)
		SHADER_PARAMETER_ARRAY(FVector4f, WorldCameraOrigins, [LUMEN_MAX_VIEWS])
		SHADER_PARAMETER(uint32, NumCameraOrigins)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:
	static uint32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenSceneUpdateCS, "/Engine/Private/Lumen/LumenScene.usf", "LumenSceneUpdateCS", SF_Compute);

class FVisualizePrimitiveGroupsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizePrimitiveGroupsCS)
	SHADER_USE_PARAMETER_STRUCT(FVisualizePrimitiveGroupsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:
	static uint32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizePrimitiveGroupsCS, "/Engine/Private/Lumen/LumenScene.usf", "VisualizePrimitiveGroupsCS", SF_Compute);

class FLumenSceneStatsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenSceneStatsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenSceneStatsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, SceneAddOps)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SceneRemoveOps)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

public:
	static uint32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenSceneStatsCS, "/Engine/Private/Lumen/LumenScene.usf", "LumenSceneStatsCS", SF_Compute);

/**
 * Run Lumen GPU Scene Update to find which primitive groups are visible and require cards and which should be hidden.
 */
void LumenScene::GPUDrivenUpdate(FRDGBuilder& GraphBuilder, const FScene* Scene, TArray<FViewInfo>& Views, const FLumenSceneFrameTemporaries& FrameTemporaries)
{
	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);

	FLumenSceneReadback::FBuffersRDG ReadbackBuffers = LumenSceneData.SceneReadback.GetWriteBuffers(GraphBuilder);
	if (!ReadbackBuffers.AddOps || !ReadbackBuffers.RemoveOps)
	{
		return;
	}

	// Need to clear buffers, as first element will be used as an allocator
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ReadbackBuffers.AddOps), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ReadbackBuffers.RemoveOps), 0);

	{
		TArray<FVector, TInlineAllocator<LUMEN_MAX_VIEWS>> LumenSceneCameraOrigins;
		float CardMaxDistance = 0.0f;
		float LumenSceneDetail = 0.0f;
		bool bHasOrthographicView = false;

		for (const FViewInfo& View : Views)
		{
			LumenSceneCameraOrigins.Add(Lumen::GetLumenSceneViewOrigin(View, Lumen::GetNumGlobalDFClipmaps(View) - 1));
			CardMaxDistance = FMath::Max(CardMaxDistance, LumenScene::GetCardMaxDistance(View));
			LumenSceneDetail = FMath::Max(LumenSceneDetail, FMath::Clamp<float>(View.FinalPostProcessSettings.LumenSceneDetail, .125f, 8.0f));
			if (!bHasOrthographicView && !View.IsPerspectiveProjection())
			{
				bHasOrthographicView = true;
			}
		}

		FLumenSceneUpdateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenSceneUpdateCS::FParameters>();
		PassParameters->View = Views[0].ViewUniformBuffer;
		PassParameters->LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
		PassParameters->RWSceneAddOps = GraphBuilder.CreateUAV(ReadbackBuffers.AddOps);
		PassParameters->RWSceneRemoveOps = GraphBuilder.CreateUAV(ReadbackBuffers.RemoveOps);
		PassParameters->MaxSceneAddOps = LumenSceneData.SceneReadback.GetMaxAddOps();
		PassParameters->MaxSceneRemoveOps = LumenSceneData.SceneReadback.GetMaxRemoveOps();
		PassParameters->CardMaxDistanceSq = CardMaxDistance * CardMaxDistance;
		PassParameters->CardTexelDensity = LumenScene::GetCardTexelDensity();
		PassParameters->FarFieldCardMaxDistanceSq = LumenScene::GetFarFieldCardMaxDistance() * LumenScene::GetFarFieldCardMaxDistance();
		PassParameters->FarFieldCardTexelDensity = LumenScene::GetFarFieldCardTexelDensity();
		PassParameters->MinCardResolution = FMath::Clamp(FMath::RoundToInt(LumenScene::GetCardMinResolution(bHasOrthographicView) / LumenSceneDetail), 1, 1024);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			PassParameters->WorldCameraOrigins[ViewIndex] = FVector4f((FVector3f)Views[ViewIndex].ViewMatrices.GetViewOrigin(), 0.0f);
		}
		PassParameters->NumCameraOrigins = Views.Num();

		auto ComputeShader = Views[0].ShaderMap->GetShader<FLumenSceneUpdateCS>();

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(LumenSceneData.PrimitiveGroups.Num(), FLumenSceneUpdateCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("LumenSceneUpdate"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	if (CVarLumenSceneStats.GetValueOnRenderThread() != 0)
	{
		ShaderPrint::SetEnabled(true);

		FLumenSceneStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenSceneStatsCS::FParameters>();
		ShaderPrint::SetParameters(GraphBuilder, Views[0].ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
		PassParameters->View = Views[0].ViewUniformBuffer;
		PassParameters->LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
		PassParameters->SceneAddOps = GraphBuilder.CreateSRV(ReadbackBuffers.AddOps);
		PassParameters->SceneRemoveOps = GraphBuilder.CreateSRV(ReadbackBuffers.RemoveOps);

		auto ComputeShader = Views[0].ShaderMap->GetShader<FLumenSceneStatsCS>();

		const FIntVector GroupCount = FIntVector(1, 1, 1);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("LumenSceneStats"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	if (CVarLumenSceneVisualizePrimitiveGroups.GetValueOnRenderThread() != 0)
	{
		ShaderPrint::SetEnabled(true);
		ShaderPrint::RequestSpaceForLines(256 * 1024);

		FVisualizePrimitiveGroupsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizePrimitiveGroupsCS::FParameters>();
		ShaderPrint::SetParameters(GraphBuilder, Views[0].ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
		PassParameters->View = Views[0].ViewUniformBuffer;
		PassParameters->LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;

		auto ComputeShader = Views[0].ShaderMap->GetShader<FVisualizePrimitiveGroupsCS>();

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(LumenSceneData.PrimitiveGroups.Num(), FVisualizePrimitiveGroupsCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("VisualizePrimitiveGroups"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}


	LumenSceneData.SceneReadback.SubmitWriteBuffers(GraphBuilder, ReadbackBuffers);
}