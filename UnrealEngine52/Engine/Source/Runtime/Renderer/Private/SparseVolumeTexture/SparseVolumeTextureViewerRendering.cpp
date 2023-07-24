// Copyright Epic Games, Inc. All Rights Reserved.

#include "SparseVolumeTexture/SparseVolumeTextureViewerRendering.h"
#include "SparseVolumeTexture/SparseVolumeTextureViewerSceneProxy.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "SceneView.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "RendererInterface.h"
#include "UniformBuffer.h"
#include "SceneTextureParameters.h"
#include "ShaderCompiler.h"
#include "RendererUtils.h"
#include "EngineAnalytics.h"


////////////////////////////////////////////////////////////////////////////////////////////////////

class FVisualizeSparseVolumeTextureVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeSparseVolumeTextureVS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeSparseVolumeTextureVS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, DepthAsDeviceZ)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return !IsMobilePlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VERTEX_SHADER"), 1);
	}
};

class FVisualizeSparseVolumeTexturePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVisualizeSparseVolumeTexturePS);
	SHADER_USE_PARAMETER_STRUCT(FVisualizeSparseVolumeTexturePS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		RENDER_TARGET_BINDING_SLOTS()
		SHADER_PARAMETER(FVector4f, WorldToLocal0)
		SHADER_PARAMETER(FVector4f, WorldToLocal1)
		SHADER_PARAMETER(FVector4f, WorldToLocal2)
		SHADER_PARAMETER(FVector3f, WorldToLocalNoScale0)
		SHADER_PARAMETER(FVector3f, WorldToLocalNoScale1)
		SHADER_PARAMETER(FVector3f, WorldToLocalNoScale2)
		SHADER_PARAMETER(FVector3f, SparseVolumeTextureResolution)
		SHADER_PARAMETER(FVector3f, SparseVolumeTexturePageTableResolution)
		SHADER_PARAMETER(uint32, ComponentToVisualize)
		SHADER_PARAMETER_TEXTURE(Texture3D, SparseVolumeTextureA)
		SHADER_PARAMETER_TEXTURE(Texture3D, SparseVolumeTextureB)
		SHADER_PARAMETER_TEXTURE(Texture3D<uint>, SparseVolumeTexturePageTable)
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return !IsMobilePlatform(Parameters.Platform) && EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PIXEL_SHADER"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVisualizeSparseVolumeTextureVS, "/Engine/Private/SparseVolumeTexture/VisualizeSparseVolumeTexture.usf", "VisualizeSparseVolumeTextureVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVisualizeSparseVolumeTexturePS, "/Engine/Private/SparseVolumeTexture/VisualizeSparseVolumeTexture.usf", "VisualizeSparseVolumeTexturePS", SF_Pixel);


////////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_GPU_STAT(SparseVolumeTextureViewer);

void AddSparseVolumeTextureViewerRenderPass(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer, FSceneTextures& SceneTextures)
{
	FScene* Scene = SceneRenderer.Scene;

	if (Scene->SparseVolumeTextureViewers.Num() == 0)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "SparseVolumeTextureViewer");
	RDG_GPU_STAT_SCOPE(GraphBuilder, SparseVolumeTextureViewer);
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, SparseVolumeTextureViewer);

	for (FViewInfo& View : SceneRenderer.Views)
	{
		const FIntRect& ViewportRect = View.ViewRect;

		for (auto& SVTProxy : Scene->SparseVolumeTextureViewers)
		{
			const FMatrix44f& WorldToLocal = SVTProxy->WorldToLocal;
			const FMatrix44f& WorldToLocalNoScale = SVTProxy->WorldToLocalNoScale;
			
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			FVisualizeSparseVolumeTextureVS::FPermutationDomain VsPermutationVector;
			TShaderMapRef<FVisualizeSparseVolumeTextureVS> VertexShader(GlobalShaderMap, VsPermutationVector);
			FVisualizeSparseVolumeTexturePS::FPermutationDomain PsPermutationVector;
			TShaderMapRef<FVisualizeSparseVolumeTexturePS> PixelShader(GlobalShaderMap, PsPermutationVector);

			FVisualizeSparseVolumeTexturePS::FParameters* PsPassParameters = GraphBuilder.AllocParameters<FVisualizeSparseVolumeTexturePS::FParameters>();
			PsPassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
			PsPassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);
			PsPassParameters->WorldToLocal0 = FVector4f(WorldToLocal.M[0][0], WorldToLocal.M[1][0], WorldToLocal.M[2][0], WorldToLocal.M[3][0]);
			PsPassParameters->WorldToLocal1 = FVector4f(WorldToLocal.M[0][1], WorldToLocal.M[1][1], WorldToLocal.M[2][1], WorldToLocal.M[3][1]);
			PsPassParameters->WorldToLocal2 = FVector4f(WorldToLocal.M[0][2], WorldToLocal.M[1][2], WorldToLocal.M[2][2], WorldToLocal.M[3][2]);
			PsPassParameters->WorldToLocalNoScale0 = FVector3f(WorldToLocalNoScale.M[0][0], WorldToLocalNoScale.M[1][0], WorldToLocalNoScale.M[2][0]);
			PsPassParameters->WorldToLocalNoScale1 = FVector3f(WorldToLocalNoScale.M[0][1], WorldToLocalNoScale.M[1][1], WorldToLocalNoScale.M[2][1]);
			PsPassParameters->WorldToLocalNoScale2 = FVector3f(WorldToLocalNoScale.M[0][2], WorldToLocalNoScale.M[1][2], WorldToLocalNoScale.M[2][2]);
			PsPassParameters->ComponentToVisualize = SVTProxy->ComponentToVisualize;
			PsPassParameters->SparseVolumeTextureResolution = FVector3f::OneVector;
			PsPassParameters->SparseVolumeTexturePageTableResolution = FVector3f::OneVector;
			PsPassParameters->SparseVolumeTextureA = GBlackVolumeTexture->TextureRHI;
			PsPassParameters->SparseVolumeTextureB = GBlackVolumeTexture->TextureRHI;
			PsPassParameters->SparseVolumeTexturePageTable = GBlackUintVolumeTexture->TextureRHI;


			if (SVTProxy->SparseVolumeTextureSceneProxy)
			{
				const FSparseVolumeAssetHeader& Header = SVTProxy->SparseVolumeTextureSceneProxy->GetHeader();

				PsPassParameters->SparseVolumeTextureResolution = FVector3f(Header.SourceVolumeResolution);
				PsPassParameters->SparseVolumeTexturePageTableResolution = FVector3f(Header.PageTableVolumeResolution);

				FRHITexture* TextureA = SVTProxy->SparseVolumeTextureSceneProxy->GetPhysicalTileDataATextureRHI();
				if (TextureA)
				{
					PsPassParameters->SparseVolumeTextureA = TextureA;
				}
				FRHITexture* TextureB = SVTProxy->SparseVolumeTextureSceneProxy->GetPhysicalTileDataBTextureRHI();
				if (TextureB)
				{
					PsPassParameters->SparseVolumeTextureB = TextureB;
				}
				FRHITexture* PageTableTexture = SVTProxy->SparseVolumeTextureSceneProxy->GetPageTableTextureRHI();
				if (PageTableTexture)
				{
					PsPassParameters->SparseVolumeTexturePageTable = PageTableTexture;
				}
			}

			ClearUnusedGraphResources(PixelShader, PsPassParameters);

			float VsDepthAsDeviceZ = 0.1f;

			GraphBuilder.AddPass(
				{},
				PsPassParameters,
				ERDGPassFlags::Raster,
				[PsPassParameters, VertexShader, PixelShader, ViewportRect, VsDepthAsDeviceZ](FRHICommandList& RHICmdListLambda)
				{
					RHICmdListLambda.SetViewport(ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f, ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdListLambda.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					SetGraphicsPipelineState(RHICmdListLambda, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdListLambda, PixelShader, PixelShader.GetPixelShader(), *PsPassParameters);

					FVisualizeSparseVolumeTextureVS::FParameters VsPassParameters;
					VsPassParameters.DepthAsDeviceZ = VsDepthAsDeviceZ;
					SetShaderParameters(RHICmdListLambda, VertexShader, VertexShader.GetVertexShader(), VsPassParameters);

					RHICmdListLambda.DrawPrimitive(0, 1, 1);
				});
		}
	}

}
