// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Runtime/Renderer/Private/ScreenPass.h"
#include "Modules/ModuleManager.h"
#include "ScreenRendering.h"
#include "Utils.h"

namespace UE::PixelStreaming
{
	inline FTextureRHIRef CreateRHITexture(uint32 Width, uint32 Height)
	{
		// Create empty texture
		FRHITextureCreateDesc TextureDesc =
			FRHITextureCreateDesc::Create2D(TEXT("PixelStreamingBlankTexture"), Width, Height, EPixelFormat::PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::RenderTargetable)
			.SetInitialState(ERHIAccess::Present)
			.DetermineInititialState();

		if (RHIGetInterfaceType() == ERHIInterfaceType::Vulkan)
		{
			TextureDesc.AddFlags(ETextureCreateFlags::External);
		}
		else
		{
			TextureDesc.AddFlags(ETextureCreateFlags::Shared);
		}

		return GDynamicRHI->RHICreateTexture(TextureDesc);
	}

	/*
	 * Copy from one texture to another.
	 * Assumes SourceTexture is in ERHIAccess::CopySrc and DestTexture is in ERHIAccess::CopyDest
	 * Fence can be nullptr if no fence is to be used.
	 */
	inline void CopyTexture(FRHICommandList& RHICmdList, FTextureRHIRef SourceTexture, FTextureRHIRef DestTexture, FRHIGPUFence* Fence)
	{
		if (SourceTexture->GetDesc().Format == DestTexture->GetDesc().Format
			&& SourceTexture->GetDesc().Extent.X == DestTexture->GetDesc().Extent.X
			&& SourceTexture->GetDesc().Extent.Y == DestTexture->GetDesc().Extent.Y)
		{

			RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
			RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest));

			// source and dest are the same. simple copy
			RHICmdList.CopyTexture(SourceTexture, DestTexture, {});
		}
		else
		{
			IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");

			RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

			// source and destination are different. rendered copy
			FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
			RHICmdList.BeginRenderPass(RPInfo, TEXT("PixelStreaming::CopyTexture"));
			{
				FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
				TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
				TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

				RHICmdList.SetViewport(0, 0, 0.0f, DestTexture->GetDesc().Extent.X, DestTexture->GetDesc().Extent.Y, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTexture);

				FIntPoint TargetBufferSize(DestTexture->GetDesc().Extent.X, DestTexture->GetDesc().Extent.Y);
				RendererModule->DrawRectangle(RHICmdList, 0, 0, // Dest X, Y
					DestTexture->GetDesc().Extent.X,			// Dest Width
					DestTexture->GetDesc().Extent.Y,			// Dest Height
					0, 0,										// Source U, V
					1, 1,										// Source USize, VSize
					TargetBufferSize,							// Target buffer size
					FIntPoint(1, 1),							// Source texture size
					VertexShader, EDRF_Default);
			}

			RHICmdList.EndRenderPass();

			RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::SRVMask, ERHIAccess::CopySrc));
			RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::RTV, ERHIAccess::CopyDest));
		}

		if (Fence != nullptr)
		{
			RHICmdList.WriteGPUFence(Fence);
		}
	}

	inline void CopyTextureToReadbackTexture(FTextureRHIRef SourceTexture, TSharedPtr<FRHIGPUTextureReadback> GPUTextureReadback, void* OutBuffer)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		FRDGBuilder GraphBuilder(RHICmdList);

		FRDGTextureRef RDGSourceTexture = RegisterExternalTexture(GraphBuilder, SourceTexture, TEXT("SourceCopyTextureToReadbackTexture"));
		FRDGTextureRef RDGStagingTexture = nullptr;

		if (RDGSourceTexture->Desc.Format != EPixelFormat::PF_B8G8R8A8)
		{
			// We need the pixel format to be BGRA8 so we first draw it to a staging texture
			{
				const FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(RDGSourceTexture->Desc.Extent, PF_B8G8R8A8, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_RenderTargetable));
				RDGStagingTexture = GraphBuilder.CreateTexture(Desc, TEXT("StagingCopyTextureToReadbackTexture"));
			}

			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

			TShaderMapRef<FScreenPassVS> VertexShader(ShaderMap);

			// Setup the pixel shader
			TShaderMapRef<FCopyRectPS> PixelShader(ShaderMap);

			FCopyRectPS::FParameters* PixelShaderParameters = GraphBuilder.AllocParameters<FCopyRectPS::FParameters>();
			PixelShaderParameters->InputTexture = RDGSourceTexture;
			PixelShaderParameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PixelShaderParameters->RenderTargets[0] = FRenderTargetBinding(RDGStagingTexture, ERenderTargetLoadAction::ELoad);

			ClearUnusedGraphResources(PixelShader, PixelShaderParameters);

			// We are not doing any clever blending stuff so we just use defaults here
			FRHIBlendState* BlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
			FRHIDepthStencilState* DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();

			// Create the pipline state that will execute
			const FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState, DepthStencilState);

			// Add the pass the the graph builder
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("PixelStreamingChangePixelFormat"),
				PixelShaderParameters,
				ERDGPassFlags::Raster,
				[PipelineState, Extent = RDGSourceTexture->Desc.Extent, PixelShader, PixelShaderParameters](FRHICommandList& RHICmdList) {
					PipelineState.Validate();

					RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, Extent.X, Extent.Y, 1.0f);

					SetScreenPassPipelineState(RHICmdList, PipelineState);

					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PixelShaderParameters);

					DrawRectangle(
						RHICmdList,
						0, 0, Extent.X, Extent.Y,
						0, 0, Extent.X, Extent.Y,
						Extent,
						Extent,
						PipelineState.VertexShader,
						EDRF_UseTriangleOptimization);
				});
		}
		else
		{
			// Otherwise if the PixelFormat is already BGRA8 we can just use the SourceTexture as the staging texture
			RDGStagingTexture = RDGSourceTexture;
		}

		// Do the copy from staging RBGA8 texture into the readback
		AddEnqueueCopyPass(GraphBuilder, GPUTextureReadback.Get(), RDGStagingTexture);

		GraphBuilder.Execute();

		// Lock and copy out the content of the TextureReadback to the CPU
		int32 OutRowPitchInPixels;
		int32 BlockSize = GPixelFormats[RDGStagingTexture->Desc.Format].BlockBytes;
		void* ResultsBuffer = GPUTextureReadback->Lock(OutRowPitchInPixels);
		if (RDGSourceTexture->Desc.Extent.X == OutRowPitchInPixels)
		{
			// Source pixel width is the same as the stride of the result buffer (ie no padding), we can do a plain memcpy
			FPlatformMemory::Memcpy(OutBuffer, ResultsBuffer, (RDGSourceTexture->Desc.Extent.X * RDGSourceTexture->Desc.Extent.Y * BlockSize));
		}
		else
		{
			// Source pixel width differs from the stride of the result buffer (ie padding), do a memcpy that accounts for this
			MemCpyStride(OutBuffer, ResultsBuffer, RDGSourceTexture->Desc.Extent.X * BlockSize, OutRowPitchInPixels * BlockSize, RDGSourceTexture->Desc.Extent.Y);
		}
		GPUTextureReadback->Unlock();
	}
} // namespace UE::PixelStreaming
