// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/Viewport/IDisplayClusterViewportProxy.h"
#include "Render/Viewport/IDisplayClusterViewportLightCardManager.h"

class DISPLAYCLUSTER_API IDisplayClusterViewportManagerProxy
{
public:
	virtual ~IDisplayClusterViewportManagerProxy() = default;

public:
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/**
	* Find viewport render thread proxy object by name
	* [Rendering thread func]
	*
	* @param ViewportId - Viewport name
	*
	* @return - viewport proxy object ref
	*/
	virtual IDisplayClusterViewportProxy* FindViewport_RenderThread(const FString& InViewportId) const = 0;

	/**
	* Find viewport render thread proxy object and context number by stereoscopic pass index
	* [Rendering thread func]
	*
	* @param StereoViewIndex - stereoscopic view index
	* @param OutContextNum - context number
	*
	* @return - viewport render thread proxy object ref
	*/
	virtual IDisplayClusterViewportProxy* FindViewport_RenderThread(const int32 StereoViewIndex, uint32* OutContextNum = nullptr) const = 0;

	/**
	* Return all exist viewports render thread proxy objects
	* [Rendering thread func]
	*
	* @return - arrays with viewport render thread proxy objects refs
	*/
	virtual const TArrayView<TSharedPtr<IDisplayClusterViewportProxy, ESPMode::ThreadSafe>> GetViewports_RenderThread() const = 0;

	/**
	* Return render frame targets for current frame
	* [Rendering thread func]
	*
	* @param OutRenderFrameTargets - frame RTTs (left, right)
	* @param OutTargetOffsets - frames offset on backbuffer
	* @param OutAdditionalFrameResources - (optional) array with additional render targetable resources (requested externally FDisplayClusterRenderFrameSettings::bShouldUseAdditionalTargetableFrameResource)
	*
	* @return - true if success
	*/
	virtual bool GetFrameTargets_RenderThread(TArray<FRHITexture2D*>& OutFrameResources, TArray<FIntPoint>& OutTargetOffsets, TArray<FRHITexture2D*>* OutAdditionalFrameResources=nullptr) const = 0;

	/**
	* Resolve to backbuffer
	* [Rendering thread func]
	*
	* @param InContextNum - renderframe source context num
	* @param DestArrayIndex - dest array index on backbuffer
	* @param WindowSize - dest backbuffer window size
	*
	* @return - true if success
	*/
	virtual bool ResolveFrameTargetToBackBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, const uint32 InContextNum, const int32 DestArrayIndex, FRHITexture2D* DstBackBuffer, FVector2D WindowSize) const = 0;

	/**
	* Return the light card manager, used to manage and render UV light cards
	* [Rendering thread func]
	*/
	virtual TSharedPtr<IDisplayClusterViewportLightCardManager, ESPMode::ThreadSafe> GetLightCardManager_RenderThread() const = 0;
};

