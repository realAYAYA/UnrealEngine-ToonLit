// Copyright Epic Games, Inc. All Rights Reserved.

#include "Game/ViewExtension/TextureShareSceneViewExtension.h"

#include "Object/TextureShareObject.h"
#include "Object/TextureShareObjectProxy.h"

#include "Module/TextureShareLog.h"
#include "Misc/TextureShareStrings.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "SceneView.h"

#include "PostProcess/SceneRenderTargets.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareSceneViewExtensionHelpers
{
	static void GetTextureShareCoreSceneViewMatrices(const FViewMatrices& InViewMatrices, FTextureShareCoreSceneViewMatrices& OutViewMatrices)
	{
		OutViewMatrices.ProjectionMatrix = InViewMatrices.GetProjectionMatrix();
		OutViewMatrices.ProjectionNoAAMatrix = InViewMatrices.GetProjectionNoAAMatrix();
		OutViewMatrices.ViewMatrix = InViewMatrices.GetViewMatrix();
		OutViewMatrices.ViewProjectionMatrix = InViewMatrices.GetViewProjectionMatrix();
		OutViewMatrices.TranslatedViewProjectionMatrix = InViewMatrices.GetTranslatedViewProjectionMatrix();
		OutViewMatrices.PreViewTranslation = InViewMatrices.GetPreViewTranslation();
		OutViewMatrices.ViewOrigin = InViewMatrices.GetViewOrigin();
		OutViewMatrices.ProjectionScale = InViewMatrices.GetProjectionScale();
		OutViewMatrices.TemporalAAProjectionJitter = InViewMatrices.GetTemporalAAJitter();
		OutViewMatrices.ScreenScale = InViewMatrices.GetScreenScale();
	}

	static void GetTextureShareCoreSceneView(const FSceneView& InSceneView, FTextureShareCoreSceneView& OutSceneView)
	{
		GetTextureShareCoreSceneViewMatrices(InSceneView.ViewMatrices, OutSceneView.ViewMatrices);

		OutSceneView.UnscaledViewRect = InSceneView.UnscaledViewRect;
		OutSceneView.UnconstrainedViewRect = InSceneView.UnconstrainedViewRect;
		OutSceneView.ViewLocation = InSceneView.ViewLocation;
		OutSceneView.ViewRotation = InSceneView.ViewRotation;
		OutSceneView.BaseHmdOrientation = InSceneView.BaseHmdOrientation;
		OutSceneView.BaseHmdLocation = InSceneView.BaseHmdLocation;
		OutSceneView.WorldToMetersScale = InSceneView.WorldToMetersScale;

		OutSceneView.StereoViewIndex = InSceneView.StereoViewIndex;
		OutSceneView.PrimaryViewIndex = InSceneView.PrimaryViewIndex;

		OutSceneView.FOV = InSceneView.FOV;
		OutSceneView.DesiredFOV = InSceneView.DesiredFOV;
	}

	static void GetTextureShareCoreSceneGameTime(const FGameTime& InGameTime, FTextureShareCoreSceneGameTime& OutGameTime)
	{
		OutGameTime.RealTimeSeconds = InGameTime.GetRealTimeSeconds();
		OutGameTime.WorldTimeSeconds = InGameTime.GetWorldTimeSeconds();
		OutGameTime.DeltaRealTimeSeconds = InGameTime.GetDeltaRealTimeSeconds();
		OutGameTime.DeltaWorldTimeSeconds = InGameTime.GetDeltaWorldTimeSeconds();
	}

	static void GetTextureShareCoreSceneViewFamily(const FSceneViewFamily& InViewFamily, FTextureShareCoreSceneViewFamily& OutViewFamily)
	{
		GetTextureShareCoreSceneGameTime(InViewFamily.Time, OutViewFamily.GameTime);

		OutViewFamily.FrameNumber = InViewFamily.FrameNumber;
		OutViewFamily.bIsHDR = InViewFamily.bIsHDR;
		OutViewFamily.GammaCorrection = InViewFamily.GammaCorrection;
		OutViewFamily.SecondaryViewFraction = InViewFamily.SecondaryViewFraction;
	}
};
using namespace TextureShareSceneViewExtensionHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
struct FTextureShareSceneView
{
	FTextureShareSceneView(const FSceneViewFamily& InViewFamily, const FSceneView& InSceneView, const FTextureShareSceneViewInfo& InViewInfo)
		: ViewInfo(InViewInfo)
		, SceneView(InSceneView)
	{
#if WITH_MGPU
		if (InViewFamily.bMultiGPUForkAndJoin)
		{
			GPUIndex = InSceneView.GPUMask.GetFirstIndex();
		}
#endif

		UnconstrainedViewRect = SceneView.UnconstrainedViewRect;
		UnscaledViewRect = SceneView.UnscaledViewRect;
	}

public:
	int32 GPUIndex = -1;

	FIntRect UnconstrainedViewRect;
	FIntRect UnscaledViewRect;

	FTextureShareSceneViewInfo ViewInfo;

	const FSceneView& SceneView;
};

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareSceneViewExtension
//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareSceneViewExtension::GetSceneViewData_RenderThread(const FTextureShareSceneView& InView)
{
	const FSceneView& InSceneView = InView.SceneView;
	if (InSceneView.Family)
	{
		const FSceneViewFamily& InViewFamily = *InSceneView.Family;

		// Create new data container for viewport eye
		FTextureShareCoreSceneViewData SceneViewData(InView.ViewInfo.ViewDesc);

		// Get view eye data
		GetTextureShareCoreSceneView(InSceneView, SceneViewData.View);
		GetTextureShareCoreSceneViewFamily(InViewFamily, SceneViewData.ViewFamily);

		// Save scene viewport eye data
		TArraySerializable<FTextureShareCoreSceneViewData>& DstSceneData = ObjectProxy->GetCoreProxyData_RenderThread().SceneData;
		if (FTextureShareCoreSceneViewData* ExistValue = DstSceneData.FindByEqualsFunc(SceneViewData.ViewDesc))
		{
			*ExistValue = SceneViewData;
		}
		else
		{
			DstSceneData.Add(SceneViewData);
		}
	}
}

void FTextureShareSceneViewExtension::ShareSceneViewColors_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, const FTextureShareSceneView& InView)
{
	const auto AddShareTexturePass = [&](const TCHAR* InTextureName, const FRDGTextureRef& InTextureRef)
	{
		// Send resource
		ObjectProxy->ShareResource_RenderThread(GraphBuilder, FTextureShareCoreResourceDesc(InTextureName, InView.ViewInfo.ViewDesc, ETextureShareTextureOp::Read),
			InTextureRef, InView.GPUIndex, &InView.UnconstrainedViewRect);
	};

	AddShareTexturePass(TextureShareStrings::SceneTextures::SceneColor, SceneTextures.Color.Resolve);

	AddShareTexturePass(TextureShareStrings::SceneTextures::SceneDepth, SceneTextures.Depth.Resolve);
	AddShareTexturePass(TextureShareStrings::SceneTextures::SmallDepthZ, SceneTextures.SmallDepth);

	AddShareTexturePass(TextureShareStrings::SceneTextures::GBufferA, SceneTextures.GBufferA);
	AddShareTexturePass(TextureShareStrings::SceneTextures::GBufferB, SceneTextures.GBufferB);
	AddShareTexturePass(TextureShareStrings::SceneTextures::GBufferC, SceneTextures.GBufferC);
	AddShareTexturePass(TextureShareStrings::SceneTextures::GBufferD, SceneTextures.GBufferD);
	AddShareTexturePass(TextureShareStrings::SceneTextures::GBufferE, SceneTextures.GBufferE);
	AddShareTexturePass(TextureShareStrings::SceneTextures::GBufferF, SceneTextures.GBufferF);
}

//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareSceneViewExtension::FTextureShareSceneViewExtension(const FAutoRegister& AutoRegister, const TSharedRef<ITextureShareObjectProxy, ESPMode::ThreadSafe>& InObjectProxy, FViewport* InLinkedViewport)
	: FSceneViewExtensionBase(AutoRegister)
	, LinkedViewport(InLinkedViewport)
	, ObjectProxy(InObjectProxy)
{ }

FTextureShareSceneViewExtension::~FTextureShareSceneViewExtension()
{ }

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareSceneViewExtension::Initialize(const TSharedPtr<FTextureShareSceneViewExtension, ESPMode::ThreadSafe> InViewExtension)
{
	FScopeLock Lock(&DataCS);

	// finally restore the current value
	if (InViewExtension.IsValid())
	{
		PreRenderViewFamilyFunction = InViewExtension->PreRenderViewFamilyFunction;
		PostRenderViewFamilyFunction = InViewExtension->PostRenderViewFamilyFunction;

		OnBackBufferReadyToPresentFunction = InViewExtension->OnBackBufferReadyToPresentFunction;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
FViewport* FTextureShareSceneViewExtension::GetLinkedViewport() const
{
	FScopeLock Lock(&DataCS);
	return LinkedViewport;
}

void FTextureShareSceneViewExtension::SetLinkedViewport(FViewport* InLinkedViewport)
{
	FScopeLock Lock(&DataCS);
	LinkedViewport = InLinkedViewport;
}

bool FTextureShareSceneViewExtension::IsStereoRenderingAllowed() const
{
	return LinkedViewport && LinkedViewport->IsStereoRenderingAllowed();
}

void FTextureShareSceneViewExtension::SetPreRenderViewFamilyFunction(TFunctionTextureShareViewExtension* In)
{
	FScopeLock Lock(&DataCS);

	PreRenderViewFamilyFunction.Reset();
	if (In)
	{
		PreRenderViewFamilyFunction = *In;
	}
}

void FTextureShareSceneViewExtension::SetPostRenderViewFamilyFunction(TFunctionTextureShareViewExtension* In)
{
	FScopeLock Lock(&DataCS);

	PostRenderViewFamilyFunction.Reset();
	if (In)
	{
		PostRenderViewFamilyFunction = *In;
	}
}

void FTextureShareSceneViewExtension::SetOnBackBufferReadyToPresentFunction(TFunctionTextureShareOnBackBufferReadyToPresent* In)
{
	FScopeLock Lock(&DataCS);

	OnBackBufferReadyToPresentFunction.Reset();
	if (In)
	{
		OnBackBufferReadyToPresentFunction = *In;
	}
}

void FTextureShareSceneViewExtension::SetEnableObjectProxySync(bool bInEnabled)
{
	FScopeLock Lock(&DataCS);

	bEnableObjectProxySync = bInEnabled;
}

bool FTextureShareSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	FScopeLock Lock(&DataCS);

	if (!bEnabled)
	{
		return false;
	}

	return (LinkedViewport == Context.Viewport);
}

void FTextureShareSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	FScopeLock Lock(&DataCS);

	// Reset values
	Views.Empty();

	// Initialize views
	for (const FSceneView* SceneViewIt : InViewFamily.Views)
	{
		if (SceneViewIt)
		{
			if (const FTextureShareSceneViewInfo* ViewInfo = ObjectProxy->GetData_RenderThread().Views.Find(SceneViewIt->StereoViewIndex, SceneViewIt->StereoPass))
			{
				Views.Add(FTextureShareSceneView(InViewFamily, *SceneViewIt, *ViewInfo));
			}
		}
	}

	// Add RDG pass
	AddPass(GraphBuilder, RDG_EVENT_NAME("PreRenderViewFamily_RenderThread"), [this, &InViewFamily](FRHICommandListImmediate& RHICmdList)
		{
			PreRenderViewFamily_RenderThread(RHICmdList, InViewFamily);
		});
}

void FTextureShareSceneViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	AddPass(GraphBuilder, RDG_EVENT_NAME("PostRenderViewFamily_RenderThread"), [this, &InViewFamily](FRHICommandListImmediate& RHICmdList)
		{
			PostRenderViewFamily_RenderThread(RHICmdList, InViewFamily);
		});
}

void FTextureShareSceneViewExtension::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	FScopeLock Lock(&DataCS);

	// Handle view extension functor callbacks
	if (PreRenderViewFamilyFunction)
	{
		PreRenderViewFamilyFunction(RHICmdList, *this);
	}
}

void FTextureShareSceneViewExtension::PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily)
{
	FScopeLock Lock(&DataCS);

	for (const FTextureShareSceneView& ViewIt : Views)
	{
		// Always get scene view data
		GetSceneViewData_RenderThread(ViewIt);

		// Share only if the resource is requested from a remote process
		if (const FTextureShareCoreResourceRequest* ExistResourceRequest = ObjectProxy->GetData_RenderThread().FindResourceRequest(FTextureShareCoreResourceDesc(TextureShareStrings::SceneTextures::FinalColor, ViewIt.ViewInfo.ViewDesc, ETextureShareTextureOp::Undefined)))
		{
			FTexture2DRHIRef RenderTargetTexture = InViewFamily.RenderTarget->GetRenderTargetTexture();
			if (RenderTargetTexture.IsValid())
			{
				// Send
				const FTextureShareCoreResourceDesc SendResourceDesc(TextureShareStrings::SceneTextures::FinalColor, ViewIt.ViewInfo.ViewDesc, ETextureShareTextureOp::Read);
				ObjectProxy->ShareResource_RenderThread(RHICmdList, SendResourceDesc, RenderTargetTexture, ViewIt.GPUIndex, &ViewIt.UnscaledViewRect);

				if (bEnableObjectProxySync)
				{
					// Receive
					const FTextureShareCoreResourceDesc ReceiveResourceDesc(TextureShareStrings::SceneTextures::FinalColor, ViewIt.ViewInfo.ViewDesc, ETextureShareTextureOp::Write, ETextureShareSyncStep::FrameSceneFinalColorEnd);
					if (ObjectProxy->ShareResource_RenderThread(RHICmdList, ReceiveResourceDesc, RenderTargetTexture, ViewIt.GPUIndex, &ViewIt.UnscaledViewRect))
					{
						ObjectProxy->FrameSync_RenderThread(RHICmdList, ETextureShareSyncStep::FrameSceneFinalColorEnd);
					}
				}
			}
		}
	}

	// Handle view extension functor callbacks
	if (PostRenderViewFamilyFunction)
	{
		PostRenderViewFamilyFunction(RHICmdList, *this);
	}
}

void FTextureShareSceneViewExtension::OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	FScopeLock Lock(&DataCS);

	// Send scene textures on request
	for (const FTextureShareSceneView& ViewIt : Views)
	{
		ShareSceneViewColors_RenderThread(GraphBuilder, SceneTextures, ViewIt);
	}
}

void FTextureShareSceneViewExtension::OnBackBufferReadyToPresent_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& InBackbuffer)
{
	FScopeLock Lock(&DataCS);

	// Handle view extension functor callbacks
	if (OnBackBufferReadyToPresentFunction)
	{
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
		OnBackBufferReadyToPresentFunction(RHICmdList, *this, InBackbuffer);
	}
}
