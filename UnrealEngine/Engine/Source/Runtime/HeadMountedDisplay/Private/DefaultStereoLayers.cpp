// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultStereoLayers.h"
#include "HeadMountedDisplayBase.h"

#include "EngineModule.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "RendererInterface.h"
#include "StereoLayerRendering.h"
#include "RHIStaticStates.h"
#include "StaticBoundShaderState.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "SceneView.h"
#include "CommonRenderResources.h"
#include "IXRLoadingScreen.h"
#include "RenderGraphUtils.h"

namespace 
{

	/*=============================================================================
	*
	* Helper functions
	*
	*/

	//=============================================================================
	static FMatrix ConvertTransform(const FTransform& In)
	{

		const FQuat InQuat = In.GetRotation();
		FQuat OutQuat(-InQuat.Y, -InQuat.Z, -InQuat.X, -InQuat.W);

		const FVector InPos = In.GetTranslation();
		FVector OutPos(InPos.Y, InPos.Z, InPos.X);

		const FVector InScale = In.GetScale3D();
		FVector OutScale(InScale.Y, InScale.Z, InScale.X);

		return FTransform(OutQuat, OutPos, OutScale).ToMatrixWithScale() * FMatrix(
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 0, 0, 1));
	}

}

FDefaultStereoLayers::FDefaultStereoLayers(const FAutoRegister& AutoRegister, FHeadMountedDisplayBase* InHMDDevice) 
	: FHMDSceneViewExtension(AutoRegister)
	, HMDDevice(InHMDDevice)
{

}

//=============================================================================

// static
void FDefaultStereoLayers::StereoLayerRender(FRHICommandListImmediate& RHICmdList, const TArray<FLayerDesc>& LayersToRender, const FLayerRenderParams& RenderParams)
{
	check(IsInRenderingThread());
	if (!LayersToRender.Num())
	{
		return;
	}

	IRendererModule& RendererModule = GetRendererModule();
	using TOpaqueBlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>;
	using TAlphaBlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>;

	// Set render state
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None, true, false>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	RHICmdList.SetViewport((float)RenderParams.Viewport.Min.X, (float)RenderParams.Viewport.Min.Y, 0, (float)RenderParams.Viewport.Max.X, (float)RenderParams.Viewport.Max.Y, 1.0f);

	// Set initial shader state
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FStereoLayerVS> VertexShader(ShaderMap);
	TShaderMapRef<FStereoLayerPS> PixelShader(ShaderMap);
	TShaderMapRef<FStereoLayerPS_External> PixelShader_External(ShaderMap);

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	// Force initialization of pipeline state on first iteration:
	bool bLastWasOpaque = (LayersToRender[0].Flags & LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) == 0;
	bool bLastWasExternal = (LayersToRender[0].Flags & LAYER_FLAG_TEX_EXTERNAL) == 0;

	// For each layer
	for (const FLayerDesc& Layer : LayersToRender)
	{
		check(Layer.IsVisible());
		const bool bIsOpaque = (Layer.Flags & LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) != 0;
		const bool bIsExternal = (Layer.Flags & LAYER_FLAG_TEX_EXTERNAL) != 0;
		bool bPipelineStateNeedsUpdate = false;

		if (bIsOpaque != bLastWasOpaque)
		{
			bLastWasOpaque = bIsOpaque;
			GraphicsPSOInit.BlendState = bIsOpaque ? TOpaqueBlendState::GetRHI() : TAlphaBlendState::GetRHI();
			bPipelineStateNeedsUpdate = true;
		}

		if (bIsExternal != bLastWasExternal)
		{
			bLastWasExternal = bIsExternal;
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = bIsExternal ? PixelShader_External.GetPixelShader() : PixelShader.GetPixelShader();
			bPipelineStateNeedsUpdate = true;
		}

		if (bPipelineStateNeedsUpdate)
		{
			// Updater render state
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
		}

		FMatrix LayerMatrix = ConvertTransform(Layer.Transform);

		FVector2D QuadSize = Layer.QuadSize * 0.5f;
		if (Layer.Flags & LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO)
		{
			const FRHITexture2D* Tex2D = Layer.Texture->GetTexture2D();
			if (Tex2D)
			{
				const float SizeX = Tex2D->GetSizeX();
				const float SizeY = Tex2D->GetSizeY();
				if (SizeX != 0)
				{
					const float AspectRatio = SizeY / SizeX;
					QuadSize.Y = QuadSize.X * AspectRatio;
				}
			}
		}

		// Set shader uniforms
		VertexShader->SetParameters(
			RHICmdList,
			QuadSize,
			Layer.UVRect,
			RenderParams.RenderMatrices[static_cast<int>(Layer.PositionType)],
			LayerMatrix);

		PixelShader->SetParameters(
			RHICmdList,
			TStaticSamplerState<SF_Trilinear>::GetRHI(),
			Layer.Texture);

		const FIntPoint TargetSize = RenderParams.Viewport.Size();
		// Draw primitive
		RendererModule.DrawRectangle(
			RHICmdList,
			0.0f, 0.0f,
			TargetSize.X, TargetSize.Y,
			0.0f, 0.0f,
			1.0f, 1.0f,
			TargetSize,
			FIntPoint(1, 1),
			VertexShader
		);
	}
}

void FDefaultStereoLayers::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	check(IsInRenderingThread());

	if (!GetStereoLayersDirty())
	{
		return;
	}
	
	// Sort layers
	SortedSceneLayers.Reset();
	SortedOverlayLayers.Reset();

	ForEachLayer([&](uint32 /* unused */, const FLayerDesc& Layer)
	{
		if (!Layer.IsVisible())
		{
			return;
		}
		if (Layer.PositionType == ELayerType::FaceLocked)
		{
			SortedOverlayLayers.Add(Layer);
		}
		else
		{
			SortedSceneLayers.Add(Layer);
		}
	});

	auto SortLayersPredicate = [&](const FLayerDesc& A, const FLayerDesc& B)
	{
		return A.Priority < B.Priority;
	};
	SortedSceneLayers.Sort(SortLayersPredicate);
	SortedOverlayLayers.Sort(SortLayersPredicate);
}


void FDefaultStereoLayers::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	if (!IStereoRendering::IsStereoEyeView(InView))
	{
		return;
	}

	AddPass(GraphBuilder, RDG_EVENT_NAME("StereoLayerRender"), [this, &InView](FRHICommandListImmediate& RHICmdList)
	{
		FViewMatrices ModifiedViewMatrices = InView.ViewMatrices;
		ModifiedViewMatrices.HackRemoveTemporalAAProjectionJitter();
		const FMatrix& ProjectionMatrix = ModifiedViewMatrices.GetProjectionMatrix();
		const FMatrix& ViewProjectionMatrix = ModifiedViewMatrices.GetViewProjectionMatrix();

		// Calculate a view matrix that only adjusts for eye position, ignoring head position, orientation and world position.
		FVector EyeShift;
		FQuat EyeOrientation;
		HMDDevice->GetRelativeEyePose(IXRTrackingSystem::HMDDeviceId, InView.StereoViewIndex, EyeOrientation, EyeShift);

		FMatrix EyeMatrix = FTranslationMatrix(-EyeShift) * FInverseRotationMatrix(EyeOrientation.Rotator()) * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

		FQuat HmdOrientation = HmdTransform.GetRotation();
		FVector HmdLocation = HmdTransform.GetTranslation();
		FMatrix TrackerMatrix = FTranslationMatrix(-HmdLocation) * FInverseRotationMatrix(HmdOrientation.Rotator()) * EyeMatrix;

		FLayerRenderParams RenderParams{
			InView.UnscaledViewRect, // Viewport
			{
				ViewProjectionMatrix,				// WorldLocked,
				TrackerMatrix * ProjectionMatrix,	// TrackerLocked,
				EyeMatrix * ProjectionMatrix		// FaceLocked
			}
		};

		TArray<FRHITransitionInfo, TInlineAllocator<16>> Infos;
		for (const FLayerDesc& SceneLayer : SortedSceneLayers)
		{
			Infos.Add(FRHITransitionInfo(SceneLayer.Texture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
		}
		for (const FLayerDesc& OverlayLayer : SortedOverlayLayers)
		{
			Infos.Add(FRHITransitionInfo(OverlayLayer.Texture, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
		}
		if (Infos.Num())
		{
			RHICmdList.Transition(Infos);
		}

		FTexture2DRHIRef RenderTarget = HMDDevice->GetSceneLayerTarget_RenderThread(InView.StereoViewIndex, RenderParams.Viewport);
		if (!RenderTarget.IsValid())
		{
			RenderTarget = InView.Family->RenderTarget->GetRenderTargetTexture();
		}

		FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Load_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("StereoLayerRender"));
		RHICmdList.SetViewport((float)RenderParams.Viewport.Min.X, (float)RenderParams.Viewport.Min.Y, 0.0f, (float)RenderParams.Viewport.Max.X, (float)RenderParams.Viewport.Max.Y, 1.0f);

		if (bSplashIsShown || !IsBackgroundLayerVisible())
		{
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}

		StereoLayerRender(RHICmdList, SortedSceneLayers, RenderParams);

		// Optionally render face-locked layers into a non-reprojected target if supported by the HMD platform
		FTexture2DRHIRef OverlayRenderTarget = HMDDevice->GetOverlayLayerTarget_RenderThread(InView.StereoViewIndex, RenderParams.Viewport);
		if (OverlayRenderTarget.IsValid())
		{
			RHICmdList.EndRenderPass();

			FRHIRenderPassInfo RPInfoOverlayRenderTarget(OverlayRenderTarget, ERenderTargetActions::Load_Store);
			RHICmdList.BeginRenderPass(RPInfoOverlayRenderTarget, TEXT("StereoLayerRenderIntoOverlay"));

			DrawClearQuad(RHICmdList, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			RHICmdList.SetViewport((float)RenderParams.Viewport.Min.X, (float)RenderParams.Viewport.Min.Y, 0.0f, (float)RenderParams.Viewport.Max.X, (float)RenderParams.Viewport.Max.Y, 1.0f);
		}

		StereoLayerRender(RHICmdList, SortedOverlayLayers, RenderParams);

		RHICmdList.EndRenderPass();
	});
}


void FDefaultStereoLayers::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	// Initialize HMD position.
	FQuat HmdOrientation = FQuat::Identity;
	FVector HmdPosition = FVector::ZeroVector;
	HMDDevice->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HmdOrientation, HmdPosition);
	HmdTransform = FTransform(HmdOrientation, HmdPosition);
}
