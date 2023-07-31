// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PostProcess/PostProcessSelectionOutline.h"
#include "PostProcess/PostProcessCompositeEditorPrimitives.h"
#include "EditorPrimitivesRendering.h"
#include "SceneTextureParameters.h"
#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "ClearQuad.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneRenderTargets.h"

namespace
{
class FSelectionOutlinePS : public FEditorPrimitiveShader
{
public:
	DECLARE_GLOBAL_SHADER(FSelectionOutlinePS);
	SHADER_USE_PARAMETER_STRUCT(FSelectionOutlinePS, FEditorPrimitiveShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Color)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Depth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ColorTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, ColorSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EditorPrimitivesDepth)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, EditorPrimitivesStencil)
		SHADER_PARAMETER(FScreenTransform, ColorToDepth)
		SHADER_PARAMETER(FVector3f, OutlineColor)
		SHADER_PARAMETER(float, SelectionHighlightIntensity)
		SHADER_PARAMETER(FVector3f, SubduedOutlineColor)
		SHADER_PARAMETER(float, BSPSelectionIntensity)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FSelectionOutlinePS, "/Engine/Private/PostProcessSelectionOutline.usf", "MainPS", SF_Pixel);
} //! namespace

BEGIN_SHADER_PARAMETER_STRUCT(FSelectionOutlinePassParameters, )
	//SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	SHADER_PARAMETER_STRUCT_INCLUDE(FNaniteSelectionOutlineParameters, NaniteSelectionOutlineParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

FScreenPassTexture AddSelectionOutlinePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FSelectionOutlineInputs& Inputs, const Nanite::FRasterResults* NaniteRasterResults)
{
	check(Inputs.SceneColor.IsValid());
	check(Inputs.SceneDepth.IsValid());

	const bool bNaniteEnabled = NaniteRasterResults != nullptr;

	RDG_EVENT_SCOPE(GraphBuilder, "EditorSelectionOutlines");
	RDG_GPU_STAT_SCOPE(GraphBuilder, EditorPrimitives);

	const uint32 NumSamples = View.GetSceneTexturesConfig().NumSamples;

	// Patch uniform buffers with updated state for rendering the outline mesh draw commands.
	const FViewInfo* EditorView = CreateEditorPrimitiveView(View, Inputs.SceneColor.ViewRect, NumSamples);

	FRDGTextureRef DepthStencilTexture = nullptr;

	// Generate custom depth / stencil for outline shapes.
	{
		{
			FRDGTextureDesc DepthStencilDesc = Inputs.SceneColor.Texture->Desc;
			DepthStencilDesc.Reset();
			DepthStencilDesc.Format = PF_DepthStencil;
			// This is a reversed Z depth surface, so 0.0f is the far plane.
			DepthStencilDesc.ClearValue = FClearValueBinding((float)ERHIZBuffer::FarPlane, 0);
			DepthStencilDesc.Flags = TexCreate_DepthStencilTargetable | TexCreate_ShaderResource;
			DepthStencilDesc.NumSamples = NumSamples;

			DepthStencilTexture = GraphBuilder.CreateTexture(DepthStencilDesc, TEXT("Editor.SelectionOutline"));
		}

		auto* PassParameters = GraphBuilder.AllocParameters<FSelectionOutlinePassParameters>();

		FScene* Scene = View.Family->Scene->GetRenderScene();

		const FScreenPassTextureViewport SceneColorViewport(Inputs.SceneColor);

		if (bNaniteEnabled)
		{
			Nanite::GetEditorSelectionPassParameters(GraphBuilder, *Scene, View, SceneColorViewport.Rect, NaniteRasterResults, &PassParameters->NaniteSelectionOutlineParameters);
		}

		PassParameters->NaniteSelectionOutlineParameters.View = EditorView->ViewUniformBuffer;
		PassParameters->SceneTextures = Inputs.SceneTextures;
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthStencilTexture,
			ERenderTargetLoadAction::EClear,
			ERenderTargetLoadAction::EClear,
			FExclusiveDepthStencil::DepthWrite_StencilWrite);

		const_cast<FViewInfo&>(View).ParallelMeshDrawCommandPasses[EMeshPass::EditorSelection].BuildRenderingCommands(GraphBuilder, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("OutlineDepth %dx%d", SceneColorViewport.Rect.Width(), SceneColorViewport.Rect.Height()),
			PassParameters,
			ERDGPassFlags::Raster,
			[&View, SceneColorViewport, DepthStencilTexture, NaniteRasterResults, PassParameters, bNaniteEnabled](FRHICommandListImmediate& RHICmdList)
		{
			RHICmdList.SetViewport(SceneColorViewport.Rect.Min.X, SceneColorViewport.Rect.Min.Y, 0.0f, SceneColorViewport.Rect.Max.X, SceneColorViewport.Rect.Max.Y, 1.0f);

			{
				SCOPED_DRAW_EVENT(RHICmdList, EditorSelection);

				// Run selection pass on static elements
				View.ParallelMeshDrawCommandPasses[EMeshPass::EditorSelection].DispatchDraw(nullptr, RHICmdList, &PassParameters->InstanceCullingDrawParams);
			}

			// Render Nanite mesh outlines after regular mesh outline, but before borders
			if (bNaniteEnabled)
			{
				Nanite::DrawEditorSelection(RHICmdList, View, SceneColorViewport.Rect, PassParameters->NaniteSelectionOutlineParameters);
			}

			// to get an outline around the objects if it's partly outside of the screen
			{
				SCOPED_DRAW_EVENT(RHICmdList, DrawOutlineBorder);

				FIntRect InnerRect = SceneColorViewport.Rect;

				// 1 as we have an outline that is that thick
				InnerRect.InflateRect(-1);

				// top
				RHICmdList.SetScissorRect(true, SceneColorViewport.Rect.Min.X, SceneColorViewport.Rect.Min.Y, SceneColorViewport.Rect.Max.X, InnerRect.Min.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorViewport.Extent, FIntRect());
				// bottom
				RHICmdList.SetScissorRect(true, SceneColorViewport.Rect.Min.X, InnerRect.Max.Y, SceneColorViewport.Rect.Max.X, SceneColorViewport.Rect.Max.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorViewport.Extent, FIntRect());
				// left
				RHICmdList.SetScissorRect(true, SceneColorViewport.Rect.Min.X, SceneColorViewport.Rect.Min.Y, InnerRect.Min.X, SceneColorViewport.Rect.Max.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorViewport.Extent, FIntRect());
				// right
				RHICmdList.SetScissorRect(true, InnerRect.Max.X, SceneColorViewport.Rect.Min.Y, SceneColorViewport.Rect.Max.X, SceneColorViewport.Rect.Max.Y);
				DrawClearQuad(RHICmdList, false, FLinearColor(), true, (float)ERHIZBuffer::FarPlane, true, 0, SceneColorViewport.Extent, FIntRect());

				RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
			}
		});

		// Render HairStrands outlines
		if (HairStrands::HasViewHairStrandsData(View))
		{
			HairStrands::DrawEditorSelection(GraphBuilder, View, SceneColorViewport.Rect, DepthStencilTexture);
		}
	}

	FScreenPassRenderTarget Output = Inputs.OverrideOutput;

	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, Inputs.SceneColor, View.GetOverwriteLoadAction(), TEXT("SelectionOutlineColor"));
	}

	// Render selection outlines.
	{
		const FScreenPassTextureViewport OutputViewport(Output);
		const FScreenPassTextureViewport ColorViewport(Inputs.SceneColor);
		const FScreenPassTextureViewport DepthViewport(Inputs.SceneDepth);

		FRHISamplerState* PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		FSelectionOutlinePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSelectionOutlinePS::FParameters>();
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->Color = GetScreenPassTextureViewportParameters(ColorViewport);
		PassParameters->Depth = GetScreenPassTextureViewportParameters(DepthViewport);
		PassParameters->ColorToDepth = FScreenTransform::ChangeTextureUVCoordinateFromTo(ColorViewport, DepthViewport);
		PassParameters->ColorTexture = Inputs.SceneColor.Texture;
		PassParameters->ColorSampler = PointClampSampler;
		PassParameters->DepthTexture = Inputs.SceneDepth.Texture;
		PassParameters->DepthSampler = PointClampSampler;
		PassParameters->EditorPrimitivesDepth = DepthStencilTexture;
		PassParameters->EditorPrimitivesStencil = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(DepthStencilTexture, PF_X24_G8));
		PassParameters->OutlineColor = FVector3f(View.SelectionOutlineColor);
		PassParameters->SelectionHighlightIntensity = GEngine->SelectionHighlightIntensity;
		PassParameters->SubduedOutlineColor = FVector3f(View.SubduedSelectionOutlineColor);
		PassParameters->BSPSelectionIntensity = GEngine->BSPSelectionHighlightIntensity;

		FSelectionOutlinePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FSelectionOutlinePS::FSampleCountDimension>(NumSamples);

		TShaderMapRef<FSelectionOutlinePS> PixelShader(View.ShaderMap, PermutationVector);

		AddDrawScreenPass(
			GraphBuilder,
			RDG_EVENT_NAME("OutlineColor %dx%d", OutputViewport.Rect.Width(), OutputViewport.Rect.Height()),
			View,
			OutputViewport,
			ColorViewport,
			PixelShader,
			PassParameters);
	}

	return MoveTemp(Output);
}

#endif
