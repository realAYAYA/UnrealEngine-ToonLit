// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportManagerViewExtension.h"
#include "DisplayClusterSceneViewExtensions.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "SceneRendering.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "ScreenPass.h"
#include "CommonRenderResources.h"
#include "PostProcess/PostProcessing.h"
// for FPostProcessMaterialInputs
#include "PostProcess/PostProcessMaterial.h"

namespace DisplayClusterViewportManagerViewExtensionHelpers
{
	static const FName RendererModuleName(TEXT("Renderer"));
};
using namespace DisplayClusterViewportManagerViewExtensionHelpers;

///////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportManagerViewExtension
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportManagerViewExtension::FDisplayClusterViewportManagerViewExtension(const FAutoRegister& AutoRegister, const FDisplayClusterViewportManager* InViewportManager)
	: FSceneViewExtensionBase(AutoRegister)
	, ViewportManager(InViewportManager)
	, ViewportManagerProxy(InViewportManager->GetViewportManagerProxy())
{
	RegisterCallbacks();
}

FDisplayClusterViewportManagerViewExtension::~FDisplayClusterViewportManagerViewExtension()
{
	ViewportManagerProxy.Reset();
	UnregisterCallbacks();
}

bool FDisplayClusterViewportManagerViewExtension::IsActive() const
{
	return ViewportManagerProxy.IsValid();
}

void FDisplayClusterViewportManagerViewExtension::Release()
{
	check(IsInGameThread());

	ViewportManagerProxy.Reset();
}

void FDisplayClusterViewportManagerViewExtension::Release_RenderThread()
{
	check(IsInRenderingThread());

	ViewportManagerProxy.Reset();
	ViewportProxies.Empty();

	UnregisterCallbacks();
}


void FDisplayClusterViewportManagerViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	if (InView.bIsViewInfo)
	{
		FViewInfo& ViewInfo = static_cast<FViewInfo&>(InView);

		// UE-145088: VR HMD device post-processing cannot be applied to nDisplay rendering
		ViewInfo.bHMDHiddenAreaMaskActive = false;
	}
}

void FDisplayClusterViewportManagerViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (!IsActive())
	{
		return;
	}

	switch (PassId)
	{
	case EPostProcessingPass::SSRInput:
		for (const FViewportProxy& ViewportIt : ViewportProxies)
		{
			if (ViewportIt.ViewportProxy.IsValid() && ViewportIt.ViewportProxy.Pin()->ShouldUsePostProcessPassAfterSSRInput())
			{
				InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FDisplayClusterViewportManagerViewExtension::PostProcessPassAfterSSRInput_RenderThread));
				break;
			}
		}
		break;

	case EPostProcessingPass::FXAA:
		for (const FViewportProxy& ViewportIt : ViewportProxies)
		{
			if (ViewportIt.ViewportProxy.IsValid() && ViewportIt.ViewportProxy.Pin()->ShouldUsePostProcessPassAfterFXAA())
			{
				InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FDisplayClusterViewportManagerViewExtension::PostProcessPassAfterFXAA_RenderThread));
				break;
			}
		}
		break;

	case EPostProcessingPass::Tonemap:
		for (const FViewportProxy& ViewportIt : ViewportProxies)
		{
			if (ViewportIt.ViewportProxy.IsValid() && ViewportIt.ViewportProxy.Pin()->ShouldUsePostProcessPassTonemap())
			{
				InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FDisplayClusterViewportManagerViewExtension::PostProcessPassAfterTonemap_RenderThread));
				break;
			}
		}
		break;

	default:
		break;
	}
}

FScreenPassTexture FDisplayClusterViewportManagerViewExtension::PostProcessPassAfterFXAA_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	if (!IsActive())
	{
		return ReturnUntouchedSceneColorForPostProcessing(Inputs);
	}

	uint32 ContextNum = 0;
	if (FDisplayClusterViewportProxy* ViewportProxyPtr = ViewportManagerProxy.Pin()->ImplFindViewport_RenderThread(View.StereoViewIndex, &ContextNum))
	{
		if (ViewportProxyPtr->ShouldUsePostProcessPassAfterFXAA())
		{
			return ViewportProxyPtr->OnPostProcessPassAfterFXAA_RenderThread(GraphBuilder, View, Inputs, ContextNum);
		}
	}

	return ReturnUntouchedSceneColorForPostProcessing(Inputs);
}

FScreenPassTexture FDisplayClusterViewportManagerViewExtension::PostProcessPassAfterSSRInput_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	if (!IsActive())
	{
		return ReturnUntouchedSceneColorForPostProcessing(Inputs);
	}

	uint32 ContextNum = 0;
	if (FDisplayClusterViewportProxy* ViewportProxyPtr = ViewportManagerProxy.Pin()->ImplFindViewport_RenderThread(View.StereoViewIndex, &ContextNum))
	{
		if (ViewportProxyPtr->ShouldUsePostProcessPassAfterSSRInput())
		{
			return ViewportProxyPtr->OnPostProcessPassAfterSSRInput_RenderThread(GraphBuilder, View, Inputs, ContextNum);
		}
	}

	return ReturnUntouchedSceneColorForPostProcessing(Inputs);
}

FScreenPassTexture FDisplayClusterViewportManagerViewExtension::PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	if (!IsActive())
	{
		return ReturnUntouchedSceneColorForPostProcessing(Inputs);
	}

	uint32 ContextNum = 0;
	if (FDisplayClusterViewportProxy* ViewportProxyPtr = ViewportManagerProxy.Pin()->ImplFindViewport_RenderThread(View.StereoViewIndex, &ContextNum))
	{
		if (ViewportProxyPtr->ShouldUsePostProcessPassTonemap())
		{
			return ViewportProxyPtr->OnPostProcessPassAfterTonemap_RenderThread(GraphBuilder, View, Inputs, ContextNum);
		}
	}

	return ReturnUntouchedSceneColorForPostProcessing(Inputs);
}

/**
* A helper function that extracts the right scene color texture, untouched, to be used further in post processing.
*/
FScreenPassTexture FDisplayClusterViewportManagerViewExtension::ReturnUntouchedSceneColorForPostProcessing(const FPostProcessMaterialInputs& InOutInputs)
{
	if (InOutInputs.OverrideOutput.IsValid())
	{
		return InOutInputs.OverrideOutput;
	}
	else
	{
		/** We don't want to modify scene texture in any way. We just want it to be passed back onto the next stage. */
		FScreenPassTexture SceneTexture = const_cast<FScreenPassTexture&>(InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor]);
		return SceneTexture;
	}
}

bool FDisplayClusterViewportManagerViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	if (!IsActive())
	{
		return false;
	}

	static const FDisplayClusterSceneViewExtensionContext DCViewExtensionContext;
	if (Context.IsA(MoveTempIfPossible(DCViewExtensionContext)))
	{
		const FDisplayClusterSceneViewExtensionContext& DisplayContext = static_cast<const FDisplayClusterSceneViewExtensionContext&>(Context);
		if (DisplayContext.ViewportManager == ViewportManager)
		{
			// Apply only for DC viewports
			return true;
		}
	}

	return false;
}

void FDisplayClusterViewportManagerViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (!IsActive())
	{
		return;
	}

	ViewportProxies.Empty();

	// Get all viewport proxies that are uses in this view family
	for (int32 ViewIndex = 0; ViewIndex < InViewFamily.Views.Num(); ViewIndex++)
	{
		if (const FSceneView* SceneView = InViewFamily.Views[ViewIndex])
		{
			FViewportProxy NewViewportProxy;

			if (IDisplayClusterViewportProxy* ViewportProxyPtr = ViewportManagerProxy.Pin()->FindViewport_RenderThread(SceneView->StereoViewIndex, &NewViewportProxy.ViewportProxyContext.ContextNum))
			{
				NewViewportProxy.ViewportProxyContext.ViewFamilyProfileDescription = InViewFamily.ProfileDescription;

				NewViewportProxy.ViewportProxy = (static_cast<FDisplayClusterViewportProxy*>(ViewportProxyPtr))->AsShared();
				NewViewportProxy.ViewIndex = ViewIndex;

				ViewportProxies.Add(NewViewportProxy);
			}
		}
	}
}

void FDisplayClusterViewportManagerViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
	if (!IsActive())
	{
		return;
	}

	for (FViewportProxy& ViewportIt : ViewportProxies)
	{
		if (const FSceneView* SceneView = InViewFamily.Views[ViewportIt.ViewIndex])
		{
			if (ViewportIt.ViewportProxy.IsValid())
			{
				ViewportIt.ViewportProxy.Pin()->OnPostRenderViewFamily_RenderThread(GraphBuilder, InViewFamily, *SceneView, ViewportIt.ViewportProxyContext);
			}
		}
	}

	ViewportProxies.Empty();
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewportManagerViewExtension::OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	if (!IsActive())
	{
		return;
	}

	for (FViewportProxy& ViewportIt : ViewportProxies)
	{
		if (ViewportIt.ViewportProxy.IsValid())
		{
			ViewportIt.ViewportProxy.Pin()->OnResolvedSceneColor_RenderThread(GraphBuilder, SceneTextures, ViewportIt.ViewportProxyContext);
		}
	}
}

void FDisplayClusterViewportManagerViewExtension::RegisterCallbacks()
{
	if (!ResolvedSceneColorCallbackHandle.IsValid())
	{
		if (IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName))
		{
			ResolvedSceneColorCallbackHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FDisplayClusterViewportManagerViewExtension::OnResolvedSceneColor_RenderThread);
		}
	}
}

void FDisplayClusterViewportManagerViewExtension::UnregisterCallbacks()
{
	if (ResolvedSceneColorCallbackHandle.IsValid())
	{
		if (IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName))
		{
			RendererModule->GetResolvedSceneColorCallbacks().Remove(ResolvedSceneColorCallbackHandle);
		}

		ResolvedSceneColorCallbackHandle.Reset();
	}
}
