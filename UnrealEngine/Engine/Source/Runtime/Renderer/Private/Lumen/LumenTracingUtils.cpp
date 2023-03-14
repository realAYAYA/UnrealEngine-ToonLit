// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenTracingUtils.h"
#include "LumenSceneRendering.h"

float GLumenSkylightLeakingRoughness = 0.3f;
FAutoConsoleVariableRef CVarLumenSkylightLeakingRoughness(
	TEXT("r.Lumen.SkylightLeaking.Roughness"),
	GLumenSkylightLeakingRoughness,
	TEXT("Roughness used to sample the skylight leaking cubemap.  A value of 0 gives no prefiltering of the skylight leaking, while larger values can be useful to hide sky features in the leaking."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

FLumenCardTracingInputs::FLumenCardTracingInputs(FRDGBuilder& GraphBuilder, FLumenSceneData& LumenSceneData, const FLumenSceneFrameTemporaries& FrameTemporaries, bool bSurfaceCacheFeedback)
{
	LLM_SCOPE_BYTAG(Lumen);

	LumenCardSceneUniformBuffer = FrameTemporaries.LumenCardSceneUniformBuffer;

	check(FrameTemporaries.FinalLightingAtlas);

	AlbedoAtlas = FrameTemporaries.AlbedoAtlas;
	OpacityAtlas = FrameTemporaries.OpacityAtlas;
	NormalAtlas = FrameTemporaries.NormalAtlas;
	EmissiveAtlas = FrameTemporaries.EmissiveAtlas;
	DepthAtlas = FrameTemporaries.DepthAtlas;

	DirectLightingAtlas = FrameTemporaries.DirectLightingAtlas;
	IndirectLightingAtlas = FrameTemporaries.IndirectLightingAtlas;
	RadiosityNumFramesAccumulatedAtlas = FrameTemporaries.RadiosityNumFramesAccumulatedAtlas;
	FinalLightingAtlas = FrameTemporaries.FinalLightingAtlas;

	if (FrameTemporaries.CardPageLastUsedBufferUAV && FrameTemporaries.CardPageHighResLastUsedBufferUAV)
	{
		CardPageLastUsedBufferUAV = FrameTemporaries.CardPageLastUsedBufferUAV;
		CardPageHighResLastUsedBufferUAV = FrameTemporaries.CardPageHighResLastUsedBufferUAV;
	}
	else
	{
		CardPageLastUsedBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DummyCardPageLastUsedBuffer")));
		CardPageHighResLastUsedBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DummyCardPageHighResLastUsedBuffer")));
	}

	if (FrameTemporaries.SurfaceCacheFeedbackResources.BufferUAV && bSurfaceCacheFeedback)
	{
		SurfaceCacheFeedbackBufferAllocatorUAV = FrameTemporaries.SurfaceCacheFeedbackResources.BufferAllocatorUAV;
		SurfaceCacheFeedbackBufferUAV = FrameTemporaries.SurfaceCacheFeedbackResources.BufferUAV;
		SurfaceCacheFeedbackBufferSize = FrameTemporaries.SurfaceCacheFeedbackResources.BufferSize;
		SurfaceCacheFeedbackBufferTileJitter = LumenSceneData.SurfaceCacheFeedback.GetFeedbackBufferTileJitter();
		SurfaceCacheFeedbackBufferTileWrapMask = Lumen::GetFeedbackBufferTileWrapMask();
	}
	else
	{
		SurfaceCacheFeedbackBufferAllocatorUAV = LumenSceneData.SurfaceCacheFeedback.GetDummyFeedbackAllocatorUAV(GraphBuilder);
		SurfaceCacheFeedbackBufferUAV = LumenSceneData.SurfaceCacheFeedback.GetDummyFeedbackUAV(GraphBuilder);
		SurfaceCacheFeedbackBufferSize = 0;
		SurfaceCacheFeedbackBufferTileJitter = FIntPoint(0, 0);
		SurfaceCacheFeedbackBufferTileWrapMask = 0;
	}
}

void GetLumenCardTracingParameters(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View, 
	const FLumenCardTracingInputs& TracingInputs,
	FLumenCardTracingParameters& TracingParameters, 
	bool bShaderWillTraceCardsOnly)
{
	LLM_SCOPE_BYTAG(Lumen);

	TracingParameters.View = View.ViewUniformBuffer;
	TracingParameters.LumenCardScene = TracingInputs.LumenCardSceneUniformBuffer;
	TracingParameters.ReflectionStruct = CreateReflectionUniformBuffer(View, UniformBuffer_MultiFrame);
	
	TracingParameters.DiffuseColorBoost = 1.0f / FMath::Max(View.FinalPostProcessSettings.LumenDiffuseColorBoost, 1.0f);
	TracingParameters.SkylightLeaking = View.FinalPostProcessSettings.LumenSkylightLeaking;
	TracingParameters.SkylightLeakingRoughness = GLumenSkylightLeakingRoughness;
	TracingParameters.InvFullSkylightLeakingDistance = 1.0f / FMath::Clamp<float>(View.FinalPostProcessSettings.LumenFullSkylightLeakingDistance, .1f, Lumen::GetMaxTraceDistance(View));

	// GPUScene
	const FScene* Scene = ((const FScene*)View.Family->Scene);
	const FGPUSceneResourceParameters GPUSceneParameters = Scene->GPUScene.GetShaderParameters();

	TracingParameters.GPUSceneInstanceSceneData = GPUSceneParameters.GPUSceneInstanceSceneData;
	TracingParameters.GPUSceneInstancePayloadData = GPUSceneParameters.GPUSceneInstancePayloadData;
	TracingParameters.GPUScenePrimitiveSceneData = GPUSceneParameters.GPUScenePrimitiveSceneData;

	// Feedback
	extern float GLumenSurfaceCacheFeedbackResLevelBias;
	TracingParameters.RWCardPageLastUsedBuffer = TracingInputs.CardPageLastUsedBufferUAV;
	TracingParameters.RWCardPageHighResLastUsedBuffer = TracingInputs.CardPageHighResLastUsedBufferUAV;
	TracingParameters.RWSurfaceCacheFeedbackBufferAllocator = TracingInputs.SurfaceCacheFeedbackBufferAllocatorUAV;
	TracingParameters.RWSurfaceCacheFeedbackBuffer = TracingInputs.SurfaceCacheFeedbackBufferUAV;
	TracingParameters.SurfaceCacheFeedbackBufferSize = TracingInputs.SurfaceCacheFeedbackBufferSize;
	TracingParameters.SurfaceCacheFeedbackBufferTileJitter = TracingInputs.SurfaceCacheFeedbackBufferTileJitter;
	TracingParameters.SurfaceCacheFeedbackBufferTileWrapMask = TracingInputs.SurfaceCacheFeedbackBufferTileWrapMask;
	TracingParameters.SurfaceCacheFeedbackResLevelBias = GLumenSurfaceCacheFeedbackResLevelBias + 0.5f; // +0.5f required for uint to float rounding in shader
	TracingParameters.SurfaceCacheUpdateFrameIndex = Scene->GetLumenSceneData(View)->GetSurfaceCacheUpdateFrameIndex();

	// Lumen surface cache atlas
	TracingParameters.DirectLightingAtlas = TracingInputs.DirectLightingAtlas;
	TracingParameters.IndirectLightingAtlas = TracingInputs.IndirectLightingAtlas;
	TracingParameters.FinalLightingAtlas = TracingInputs.FinalLightingAtlas;
	TracingParameters.AlbedoAtlas = TracingInputs.AlbedoAtlas;
	TracingParameters.OpacityAtlas = TracingInputs.OpacityAtlas;
	TracingParameters.NormalAtlas = TracingInputs.NormalAtlas;
	TracingParameters.EmissiveAtlas = TracingInputs.EmissiveAtlas;
	TracingParameters.DepthAtlas = TracingInputs.DepthAtlas;

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