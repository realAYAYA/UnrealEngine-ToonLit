// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"
#include "MediaCapture.h"
#include "MediaIOCoreModule.h"
#include "ImagePixelData.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "SceneView.h"
#include "ScreenPass.h"


class FRDGTexture;

/**
 * View extension that calls into MediaCapture to capture frames as soon as the frame is rendered.
 */
class FMediaCaptureSceneViewExtension : public FSceneViewExtensionBase
{
public:
	
	FMediaCaptureSceneViewExtension(const FAutoRegister& InAutoRegister, UMediaCapture* InMediaCapture, EMediaCapturePhase InCapturePhase, int32 InPriority)
		: FSceneViewExtensionBase(InAutoRegister)
		, WeakCapture(InMediaCapture)
		, CapturePhase(InCapturePhase)
		, Priority(InPriority)
	{
		
	}

	//~ Begin FSceneViewExtensionBase Interface
	virtual int32 GetPriority() const override
	{
		return Priority;
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {};
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override 
	{
		if (bValidPhase)
		{
			// Copied from PostProcessing.h
			if (InView.GetFeatureLevel() >= ERHIFeatureLevel::SM5)
			{
				bPostProcessingEnabled =
					InView.Family->EngineShowFlags.PostProcessing &&
					!InView.Family->EngineShowFlags.VisualizeDistanceFieldAO &&
					!InView.Family->EngineShowFlags.VisualizeShadingModels &&
					!InView.Family->EngineShowFlags.VisualizeVolumetricCloudConservativeDensity &&
					!InView.Family->EngineShowFlags.VisualizeVolumetricCloudEmptySpaceSkipping &&
					!InView.Family->EngineShowFlags.ShaderComplexity;
			}
			else
			{
				bPostProcessingEnabled = InView.Family->EngineShowFlags.PostProcessing && !InView.Family->EngineShowFlags.ShaderComplexity && IsMobileHDR();
			}

			if (CapturePhase != EMediaCapturePhase::BeforePostProcessing && CapturePhase != EMediaCapturePhase::EndFrame)
			{
				if (!bPostProcessingEnabled)
				{
					LastErrorMessage = TEXT("Media Capture will not work since it is scheduled in a post processing phase and post processing is not enabled.");
					UE_LOG(LogMediaIOCore, Warning, TEXT("%s"), *LastErrorMessage);
					bValidPhase = false;
				}
			}
		}
	};
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}; 
	
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override
	{
		if ((CapturePhase == EMediaCapturePhase::AfterMotionBlur && PassId == EPostProcessingPass::MotionBlur)
			|| (CapturePhase == EMediaCapturePhase::AfterToneMap && PassId == EPostProcessingPass::Tonemap)
			|| (CapturePhase == EMediaCapturePhase::AfterFXAA && PassId == EPostProcessingPass::FXAA)
			|| (CapturePhase == EMediaCapturePhase::BeforePostProcessing && PassId == EPostProcessingPass::SSRInput))
		{
			InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateSP(this, &FMediaCaptureSceneViewExtension::PostProcessCallback_RenderThread));
		}
	}

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		return true;
	}
	//~ End FSceneViewExtensionBase Interface

	FScreenPassTexture PostProcessCallback_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MediaCaptureExtensionCallback);

		FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor));
		check(SceneColor.IsValid());

		if (FRDGTextureRef TextureRef = SceneColor.Texture)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "MediaCaptureSceneExtension");
			if (WeakCapture.IsValid())
			{
				WeakCapture->CaptureImmediate_RenderThread(GraphBuilder, TextureRef, SceneColor.ViewRect);
			}
		}

		return InOutInputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}

	bool IsValid() const
	{
		return bValidPhase;
	}

private:
	TWeakObjectPtr<UMediaCapture> WeakCapture;
	EMediaCapturePhase CapturePhase = EMediaCapturePhase::AfterMotionBlur;
	bool bPostProcessingEnabled = true;
	bool bValidPhase = true;
	FString LastErrorMessage;
	int32 Priority = 0;
};

