// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenTracingUtils.h"
#include "LumenSceneRendering.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "SystemTextures.h"

float GLumenSkylightLeakingRoughness = 0.3f;
FAutoConsoleVariableRef CVarLumenSkylightLeakingRoughness(
	TEXT("r.Lumen.SkylightLeaking.Roughness"),
	GLumenSkylightLeakingRoughness,
	TEXT("Roughness used to sample the skylight leaking cubemap.  A value of 0 gives no prefiltering of the skylight leaking, while larger values can be useful to hide sky features in the leaking."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSampleFog(
	TEXT("r.Lumen.SampleFog"),
	0,
	TEXT("Sample the fog contribution in Lumen tracing. Disabled by default."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

void GetLumenCardTracingParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FLumenSceneData& LumenSceneData,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	bool bSurfaceCacheFeedback,
	FLumenCardTracingParameters& TracingParameters)
{
	LLM_SCOPE_BYTAG(Lumen);

	TracingParameters.View = View.ViewUniformBuffer;
	TracingParameters.LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
	TracingParameters.ReflectionStruct = CreateReflectionUniformBuffer(GraphBuilder, View);
	
	TracingParameters.DiffuseColorBoost = 1.0f / FMath::Max(View.FinalPostProcessSettings.LumenDiffuseColorBoost, 1.0f);
	TracingParameters.SkylightLeaking = View.FinalPostProcessSettings.LumenSkylightLeaking;
	TracingParameters.SkylightLeakingRoughness = GLumenSkylightLeakingRoughness;
	TracingParameters.InvFullSkylightLeakingDistance = 1.0f / FMath::Clamp<float>(View.FinalPostProcessSettings.LumenFullSkylightLeakingDistance, .1f, Lumen::GetMaxTraceDistance(View));

	TracingParameters.SampleHeightFog = CVarLumenSampleFog.GetValueOnRenderThread() > 0 ? 1u : 0u;
	TracingParameters.FogUniformParameters = CreateFogUniformBuffer(GraphBuilder, View);

	// GPUScene
	const FScene* Scene = ((const FScene*)View.Family->Scene);
	const FGPUSceneResourceParameters GPUSceneParameters = Scene->GPUScene.GetShaderParameters();

	TracingParameters.GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
	TracingParameters.GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
	TracingParameters.GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;

	if (FrameTemporaries.CardPageLastUsedBufferUAV && FrameTemporaries.CardPageHighResLastUsedBufferUAV)
	{
		TracingParameters.RWCardPageLastUsedBuffer = FrameTemporaries.CardPageLastUsedBufferUAV;
		TracingParameters.RWCardPageHighResLastUsedBuffer = FrameTemporaries.CardPageHighResLastUsedBufferUAV;
	}
	else
	{
		TracingParameters.RWCardPageLastUsedBuffer = GraphBuilder.CreateUAV(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DummyCardPageLastUsedBuffer")));
		TracingParameters.RWCardPageHighResLastUsedBuffer = GraphBuilder.CreateUAV(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DummyCardPageHighResLastUsedBuffer")));
	}

	// Lumen surface cache feedback
	if (FrameTemporaries.SurfaceCacheFeedbackResources.BufferUAV && bSurfaceCacheFeedback)
	{
		TracingParameters.RWSurfaceCacheFeedbackBufferAllocator = FrameTemporaries.SurfaceCacheFeedbackResources.BufferAllocatorUAV;
		TracingParameters.RWSurfaceCacheFeedbackBuffer = FrameTemporaries.SurfaceCacheFeedbackResources.BufferUAV;
		TracingParameters.SurfaceCacheFeedbackBufferSize = FrameTemporaries.SurfaceCacheFeedbackResources.BufferSize;
		TracingParameters.SurfaceCacheFeedbackBufferTileJitter = LumenSceneData.SurfaceCacheFeedback.GetFeedbackBufferTileJitter();
		TracingParameters.SurfaceCacheFeedbackBufferTileWrapMask = Lumen::GetFeedbackBufferTileWrapMask();
	}
	else
	{
		TracingParameters.RWSurfaceCacheFeedbackBufferAllocator = LumenSceneData.SurfaceCacheFeedback.GetDummyFeedbackAllocatorUAV(GraphBuilder);
		TracingParameters.RWSurfaceCacheFeedbackBuffer = LumenSceneData.SurfaceCacheFeedback.GetDummyFeedbackUAV(GraphBuilder);
		TracingParameters.SurfaceCacheFeedbackBufferSize = 0;
		TracingParameters.SurfaceCacheFeedbackBufferTileJitter = FIntPoint(0, 0);
		TracingParameters.SurfaceCacheFeedbackBufferTileWrapMask = 0;
	}

	extern float GLumenSurfaceCacheFeedbackResLevelBias;
	TracingParameters.SurfaceCacheFeedbackResLevelBias = GLumenSurfaceCacheFeedbackResLevelBias + 0.5f; // +0.5f required for uint to float rounding in shader
	TracingParameters.SurfaceCacheUpdateFrameIndex = Scene->GetLumenSceneData(View)->GetSurfaceCacheUpdateFrameIndex();

	// Lumen surface cache atlas
	TracingParameters.DirectLightingAtlas = FrameTemporaries.DirectLightingAtlas;
	TracingParameters.IndirectLightingAtlas = FrameTemporaries.IndirectLightingAtlas;
	TracingParameters.FinalLightingAtlas = FrameTemporaries.FinalLightingAtlas;
	TracingParameters.AlbedoAtlas = FrameTemporaries.AlbedoAtlas;
	TracingParameters.OpacityAtlas = FrameTemporaries.OpacityAtlas;
	TracingParameters.NormalAtlas = FrameTemporaries.NormalAtlas;
	TracingParameters.EmissiveAtlas = FrameTemporaries.EmissiveAtlas;
	TracingParameters.DepthAtlas = FrameTemporaries.DepthAtlas;

	if (View.GlobalDistanceFieldInfo.PageObjectGridBuffer)
	{
		TracingParameters.GlobalDistanceFieldPageObjectGridBuffer = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(View.GlobalDistanceFieldInfo.PageObjectGridBuffer));
	}
	else
	{
		TracingParameters.GlobalDistanceFieldPageObjectGridBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f)));
	}

	TracingParameters.NumGlobalSDFClipmaps = View.GlobalDistanceFieldInfo.Clipmaps.Num();
}
