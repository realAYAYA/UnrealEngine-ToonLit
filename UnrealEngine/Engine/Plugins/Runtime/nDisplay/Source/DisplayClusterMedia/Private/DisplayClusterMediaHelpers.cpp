// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterMediaHelpers.h"

#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"


namespace DisplayClusterMediaHelpers
{
	//@todo This needs to be exposed from the DisplayCluster core module after its refactoring
	FString GenerateICVFXViewportName(const FString& ClusterNodeId, const FString& ICVFXCameraName)
	{
		return FString::Printf(TEXT("%s_icvfx_%s_incamera"), *ClusterNodeId, *ICVFXCameraName);
	}

	template<class TScreenPixelShader>
	void ResampleTextureImpl_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect& SrcRect, const FIntRect& DstRect)
	{
		FRHIRenderPassInfo RPInfo(DstTexture, ERenderTargetActions::Load_Store);
		RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

		RHICmdList.BeginRenderPass(RPInfo, TEXT("DisaplyClusterMedia_ResampleTexture"));
		{
			FIntVector SrcSizeXYZ = SrcTexture->GetSizeXYZ();
			FIntVector DstSizeXYZ = DstTexture->GetSizeXYZ();

			FIntPoint SrcSize(SrcSizeXYZ.X, SrcSizeXYZ.Y);
			FIntPoint DstSize(DstSizeXYZ.X, DstSizeXYZ.Y);

			RHICmdList.SetViewport(0.f, 0.f, 0.0f, DstSize.X, DstSize.Y, 1.0f);

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();


			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
			TShaderMapRef<TScreenPixelShader> PixelShader(ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			if (SrcRect.Size() != DstRect.Size())
			{
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);
			}
			else
			{
				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SrcTexture);
			}

			// Set up vertex uniform parameters for scaling and biasing the rectangle.
			// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.
			FDrawRectangleParameters Parameters;
			Parameters.PosScaleBias = FVector4f(DstRect.Size().X, DstRect.Size().Y, DstRect.Min.X, DstRect.Min.Y);
			Parameters.UVScaleBias = FVector4f(SrcRect.Size().X, SrcRect.Size().Y, SrcRect.Min.X, SrcRect.Min.Y);
			Parameters.InvTargetSizeAndTextureSize = FVector4f(1.0f / DstSize.X, 1.0f / DstSize.Y, 1.0f / SrcSize.X, 1.0f / SrcSize.Y);

			SetUniformBufferParameterImmediate(RHICmdList, VertexShader.GetVertexShader(), VertexShader->GetUniformBufferParameter<FDrawRectangleParameters>(), Parameters);

			FPixelShaderUtils::DrawFullscreenQuad(RHICmdList, 1);
		}

		RHICmdList.EndRenderPass();
		RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}

	void ResampleTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect& SrcRect, const FIntRect& DstRect)
	{
		ResampleTextureImpl_RenderThread<FScreenPS>(RHICmdList, SrcTexture, DstTexture, SrcRect, DstRect);
	}
}
