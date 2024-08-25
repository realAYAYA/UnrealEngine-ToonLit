// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/PostProcessCompositeDebugPrimitives.h"
#if UE_ENABLE_DEBUG_DRAWING
#include "ScenePrivate.h"

BEGIN_SHADER_PARAMETER_STRUCT(FDebugPrimitivesPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

FScreenPassTexture AddDebugPrimitivePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FCompositePrimitiveInputs& Inputs)
{
	check(Inputs.SceneColor.IsValid());
	check(Inputs.SceneDepth.IsValid());

	RDG_EVENT_SCOPE(GraphBuilder, "CompositeDebugPrimitives");

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, Inputs.SceneColor, View.GetOverwriteLoadAction(), TEXT("Debug.DrawPrimitivesColor"));
	}
	const uint32 NumMSAASamples = Output.Texture->Desc.NumSamples;
	//Set sizing values to match the SceneColor size
	const FIntRect ViewRect = Output.ViewRect;
	FIntPoint Extent = Output.Texture->Desc.Extent;
	const FViewInfo* DebugView = CreateCompositePrimitiveView(View, ViewRect, NumMSAASamples);	

	//Prepare output textures for composite draw
	const FScreenPassTextureViewport OutputViewport(Output);
	FRDGTextureRef DebugPrimitiveDepth = CreateCompositeDepthTexture(GraphBuilder, Extent, NumMSAASamples);
	//Inputs is const so create a over-ridable texture reference
	FScreenPassTexture SceneDepth = Inputs.SceneDepth;
	FVector2f SceneDepthJitter = FVector2f(View.TemporalJitterPixels);

	if (IsTemporalAccumulationBasedMethod(View.AntiAliasingMethod))
	{
		TemporalUpscaleDepthPass(GraphBuilder,
			*DebugView,
			Inputs.SceneColor,
			SceneDepth,
			SceneDepthJitter);
	}

	//Simple element pixel shaders do not output background color for composite, 
	//so this allows the background to be drawn to the RT at the same time as depth without adding extra draw calls
	PopulateDepthPass(GraphBuilder,
		*DebugView,
		Inputs.SceneColor,
		SceneDepth,
		Output.Texture,
		DebugPrimitiveDepth,
		SceneDepthJitter,
		NumMSAASamples,
		true,
		Inputs.bUseMetalMSAAHDRDecode);

	//Composite the debug draw elements into the scene
	FDebugPrimitivesPassParameters* PassParameters = GraphBuilder.AllocParameters<FDebugPrimitivesPassParameters>();
	PassParameters->View = DebugView->GetShaderParameters();
	PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
	PassParameters->RenderTargets[0].SetLoadAction(ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(DebugPrimitiveDepth, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("DrawDebugPrimitives"),
		PassParameters,
		ERDGPassFlags::Raster,
		[&View, PassParameters, DebugView, OutputViewport](FRHICommandList& RHICmdList)
	{
		RHICmdList.SetViewport(OutputViewport.Rect.Min.X, OutputViewport.Rect.Min.Y, 0.0f, OutputViewport.Rect.Max.X, OutputViewport.Rect.Max.Y, 1.0f);

		FMeshPassProcessorRenderState DrawRenderState;
		DrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilWrite);
		DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());

		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());		
		DebugView->DebugSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, *DebugView, EBlendModeFilter::OpaqueAndMasked, SDPG_World);

		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_Always>::GetRHI());
		DebugView->DebugSimpleElementCollector.DrawBatchedElements(RHICmdList, DrawRenderState, *DebugView, EBlendModeFilter::OpaqueAndMasked, SDPG_Foreground);		
	});

	return MoveTemp(Output);
}

#endif