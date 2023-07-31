// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetManager.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResourcesPool.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Misc/DisplayClusterLog.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace DisplayClusterRenderTargetManager
{
	static EPixelFormat ImplGetCustomFormat(EDisplayClusterViewportCaptureMode CaptureMode)
	{
		// Pre-defined formats for targets:
		switch (CaptureMode)
		{
		case EDisplayClusterViewportCaptureMode::Chromakey:
			return EPixelFormat::PF_B8G8R8A8;

		case EDisplayClusterViewportCaptureMode::Lightcard:
			return EPixelFormat::PF_FloatRGBA;

		case EDisplayClusterViewportCaptureMode::MoviePipeline:
			// Movie pipeline always use PF_FloatRGBA
			return EPixelFormat::PF_FloatRGBA;

		default:
			break;
		}

		return EPixelFormat::PF_Unknown;
	}

	static void ImplViewportTextureResourceLogging(const FDisplayClusterViewport* InViewport, const uint32 ContextNum, const FString& ResourceId, const FDisplayClusterViewportTextureResource* InTextureResource)
	{
		if (InTextureResource != nullptr && InTextureResource->GetViewportResourceState(EDisplayClusterViewportResourceState::Initialized) == false)
		{
			// Log: New resource created
			UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new %s resource (%dx%d) for viewport '%s':%d"), *ResourceId, InTextureResource->GetSizeX(), InTextureResource->GetSizeY(), *InViewport->GetId(), ContextNum);
		}
	}
};

using namespace DisplayClusterRenderTargetManager;

////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// FDisplayClusterRenderTargetManager
////////////////////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterRenderTargetManager::FDisplayClusterRenderTargetManager(FDisplayClusterViewportManagerProxy* InViewportManagerProxy)
{
	ResourcesPool = MakeUnique<FDisplayClusterRenderTargetResourcesPool>(InViewportManagerProxy);
}

FDisplayClusterRenderTargetManager::~FDisplayClusterRenderTargetManager()
{
	Release();
}

void FDisplayClusterRenderTargetManager::Release()
{
	ResourcesPool->Release();
}

bool FDisplayClusterRenderTargetManager::AllocateRenderFrameResources(FViewport* InViewport, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const TArray<FDisplayClusterViewport*>& InViewports, FDisplayClusterRenderFrame& InOutRenderFrame)
{
	bool bResult = true;

	if(ResourcesPool->BeginReallocateResources(InRenderFrameSettings, InViewport))
	{
		// ReAllocate Render targets for all viewports
		for (FDisplayClusterRenderFrame::FFrameRenderTarget& FrameRenderTargetIt : InOutRenderFrame.RenderTargets)
		{
			if (FrameRenderTargetIt.bShouldUseRenderTarget)
			{
				// reallocate
				FDisplayClusterViewportRenderTargetResource* NewResource = ResourcesPool->AllocateRenderTargetResource(FrameRenderTargetIt.RenderTargetSize, ImplGetCustomFormat(FrameRenderTargetIt.CaptureMode));
				if (NewResource != nullptr)
				{
					// Set RenderFrame resource
					FrameRenderTargetIt.RenderTargetPtr = NewResource;

					// Assign for all views in render target families
					for (FDisplayClusterRenderFrame::FFrameViewFamily& ViewFamily : FrameRenderTargetIt.ViewFamilies)
					{
						for (FDisplayClusterRenderFrame::FFrameView& ViewIt : ViewFamily.Views)
						{
							if (ViewIt.Viewport != nullptr)
							{
								FDisplayClusterViewport* ViewportPtr = static_cast<FDisplayClusterViewport*>(ViewIt.Viewport);
								if (ViewportPtr)
								{
									// Array already resized in function FDisplayClusterViewport::UpdateFrameContexts() with RenderTargets.AddZeroed(ViewportContextAmount);
									check(ViewIt.ContextNum < (uint32)ViewportPtr->RenderTargets.Num());
									check(ViewportPtr->Contexts[ViewIt.ContextNum].bDisableRender == false);

									ViewportPtr->RenderTargets[ViewIt.ContextNum] = NewResource;

									if (NewResource->GetViewportResourceState(EDisplayClusterViewportResourceState::Initialized) == false)
									{
										// Log: New resource created
										UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new ViewportRenderTarget resource %08X (%dx%d) for viewport '%s'"), NewResource, NewResource->GetSizeX(), NewResource->GetSizeY(), *ViewportPtr->GetId());
									}
								}
							}
						}
					}
				}
			}
		}

		// Allocate viewport internal resources
		for (FDisplayClusterViewport* ViewportIt : InViewports)
		{
			if (ViewportIt && ViewportIt->RenderSettings.bFreezeRendering == false)
			{
				EPixelFormat CustomFormat = ImplGetCustomFormat(ViewportIt->RenderSettings.CaptureMode);

				// Allocate all context resources:
				for (const FDisplayClusterViewport_Context& ContextIt : ViewportIt->GetContexts())
				{
					const FIntPoint& ContextSize = ContextIt.ContextSize;
					const uint32& ContextNum = ContextIt.ContextNum;

					if (ViewportIt->InputShaderResources.Num() > (int32)ContextNum)
					{
						ViewportIt->InputShaderResources[ContextNum] = ResourcesPool->AllocateTextureResource(ContextSize, false, CustomFormat);
						ImplViewportTextureResourceLogging(ViewportIt, ContextNum, TEXT("InputShader"), ViewportIt->InputShaderResources[ContextNum]);
					}

					// Allocate custom resources:
					if (ViewportIt->AdditionalTargetableResources.Num() > (int32)ContextNum)
					{
						ViewportIt->AdditionalTargetableResources[ContextNum] = ResourcesPool->AllocateTextureResource(ContextSize, true, CustomFormat);
						ImplViewportTextureResourceLogging(ViewportIt, ContextNum, TEXT("AdditionalRTT"), ViewportIt->AdditionalTargetableResources[ContextNum]);
					}

					if (ViewportIt->MipsShaderResources.Num() > (int32)ContextNum)
					{
						ViewportIt->MipsShaderResources[ContextNum] = ResourcesPool->AllocateTextureResource(ContextSize, false, CustomFormat, ContextIt.NumMips);
						ImplViewportTextureResourceLogging(ViewportIt, ContextNum, TEXT("Mips"), ViewportIt->MipsShaderResources[ContextNum]);
					}
				}
			}
		}

		// Allocate frame targets for all visible  on backbuffer viewports
		FIntPoint ViewportSize = InViewport ? InViewport->GetSizeXY() : FIntPoint(0, 0);

		if (!AllocateFrameTargets(InRenderFrameSettings, ViewportSize, InOutRenderFrame))
		{
			UE_LOG(LogDisplayClusterViewport, Error, TEXT("DisplayClusterRenderTargetManager: Can't allocate frame targets."));
			bResult = false;
		}

		ResourcesPool->FinishReallocateResources();
	}

	return bResult;
}

bool FDisplayClusterRenderTargetManager::AllocateFrameTargets(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings, const FIntPoint& InViewportSize, FDisplayClusterRenderFrame& InOutRenderFrame)
{
	uint32 FrameTargetsAmount = 0;

	// Support side_by_side and top_bottom eye offset, aligned to Viewport size:
	FIntPoint TargetLocation(ForceInitToZero);
	FIntPoint TargetOffset(ForceInitToZero);

	//Re-allocate frame targets
	switch (InRenderFrameSettings.RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Preview model: render to external RTT resource
		return true;

	case EDisplayClusterRenderFrameMode::Mono:
		FrameTargetsAmount = 1;
		break;

	case EDisplayClusterRenderFrameMode::Stereo:
		FrameTargetsAmount = 2;
		break;

	case EDisplayClusterRenderFrameMode::SideBySide:
		FrameTargetsAmount = 2;
		TargetOffset.X = InViewportSize.X / 2;
		break;

	case EDisplayClusterRenderFrameMode::TopBottom:
		FrameTargetsAmount = 2;
		TargetOffset.Y = InViewportSize.Y / 2;
		break;

	default:
		// skip not implemented cases
		return false;
	}

	// Reallocate frame target resources
	TArray<FDisplayClusterViewportTextureResource*> NewFrameTargetResources;
	TArray<FDisplayClusterViewportTextureResource*> NewAdditionalFrameTargetableResources;

	for (uint32 FrameTargetsIt = 0; FrameTargetsIt < FrameTargetsAmount; FrameTargetsIt++)
	{
		FDisplayClusterViewportTextureResource* NewResource = ResourcesPool->AllocateTextureResource(InOutRenderFrame.FrameRect.Size(), true, PF_Unknown);
		if (NewResource != nullptr)
		{
			if (NewResource->GetViewportResourceState(EDisplayClusterViewportResourceState::Initialized) == false)
			{
				// Log: New resource created
				UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new RenderFrame resource (%dx%d)"), NewResource->GetSizeX(), NewResource->GetSizeY());
			}

			// calc and assign backbuffer offset (side_by_side, top_bottom)
			NewResource->BackbufferFrameOffset = InOutRenderFrame.FrameRect.Min + TargetLocation;
			TargetLocation += TargetOffset;
			NewFrameTargetResources.Add(NewResource);
		}

		if (InRenderFrameSettings.bShouldUseAdditionalFrameTargetableResource)
		{
			FDisplayClusterViewportTextureResource* NewAdditionalResource = ResourcesPool->AllocateTextureResource(InOutRenderFrame.FrameRect.Size(), true, PF_Unknown);
			if (NewAdditionalResource != nullptr)
			{
				if (NewAdditionalResource->GetViewportResourceState(EDisplayClusterViewportResourceState::Initialized) == false)
				{
					// Log: New resource created
					UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Created new RenderFrame2 resource (%dx%d)"), NewAdditionalResource->GetSizeX(), NewAdditionalResource->GetSizeY());
				}

				NewAdditionalFrameTargetableResources.Add(NewAdditionalResource);
			}
		}
	}

	// Assign frame resources for all visible viewports
	for (FDisplayClusterRenderFrame::FFrameRenderTarget& RenderTargetIt : InOutRenderFrame.RenderTargets)
	{
		for (FDisplayClusterRenderFrame::FFrameViewFamily& ViewFamilieIt : RenderTargetIt.ViewFamilies)
		{
			for (FDisplayClusterRenderFrame::FFrameView& ViewIt : ViewFamilieIt.Views)
			{
				if (ViewIt.Viewport != nullptr)
				{
					FDisplayClusterViewport* ViewportPtr = static_cast<FDisplayClusterViewport*>(ViewIt.Viewport);
					if (ViewportPtr && ViewportPtr->RenderSettings.bVisible)
					{
						ViewportPtr->OutputFrameTargetableResources = NewFrameTargetResources;
						ViewportPtr->AdditionalFrameTargetableResources = NewAdditionalFrameTargetableResources;

						// Adjust viewports frame rects. This offset saved in 'BackbufferFrameOffset'
						if (ViewIt.ContextNum < (uint32)ViewportPtr->Contexts.Num())
						{
							FDisplayClusterViewport_Context& Context = ViewportPtr->Contexts[ViewIt.ContextNum];
							Context.FrameTargetRect.Min -= InOutRenderFrame.FrameRect.Min;
							Context.FrameTargetRect.Max -= InOutRenderFrame.FrameRect.Min;
						}
					}
				}
			}
		}
	}

	return true;
}
