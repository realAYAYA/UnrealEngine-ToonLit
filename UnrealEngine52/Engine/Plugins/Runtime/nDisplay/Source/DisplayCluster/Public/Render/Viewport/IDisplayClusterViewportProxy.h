// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettingsICVFX.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxy_Context.h"

class DISPLAYCLUSTER_API IDisplayClusterViewportProxy
{
public:
	virtual ~IDisplayClusterViewportProxy() = default;

public:
	virtual FString GetId() const = 0;
	virtual FString GetClusterNodeId() const = 0;

	virtual const TSharedPtr<class IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& GetProjectionPolicy_RenderThread() const = 0;

	virtual const FDisplayClusterViewport_RenderSettings& GetRenderSettings_RenderThread() const = 0;
	virtual const FDisplayClusterViewport_RenderSettingsICVFX& GetRenderSettingsICVFX_RenderThread() const = 0;
	virtual const FDisplayClusterViewport_PostRenderSettings& GetPostRenderSettings_RenderThread() const = 0;

	/** Return viewport proxy contexts data. */
	virtual const TArray<FDisplayClusterViewport_Context>& GetContexts_RenderThread() const = 0;

	/** Get viewport resources by type
	 *
	 * @param InResourceType - resource type (RTT, Shader, MIPS, etc)
	 * @param OutResources   - [Out] RHI resources array for all contexts
	 *
	 * @return - true if success
	 */
	virtual bool GetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources) const = 0;

	/** Get viewport resources with rects by type
	 *
	 * @param InResourceType - resource type (RTT, Shader, MIPS, etc)
	 * @param OutResources   - [Out] RHI resources array for all contexts
	 * @param OutRects       - [Out] RHI resources rects array for all contexts
	 *
	 * @return - true if success
	 */
	virtual bool GetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutRects) const = 0;

	/** Copy resource contexts by type
	 *
	 * @param RHICmdList         - RHIinterface
	 * @param InputResourceType  - Input resource type (RTT, Shader, MIPS, etc)
	 * @param OutputResourceType - Output resource type (RTT, Shader, MIPS, etc)
	 * @param InContextNum       - [optional] the type of context to copy (by default, all contexts are copied).
	 *
	 * @return - true if success
	 */
	virtual bool ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum = INDEX_NONE) const = 0;

	/** Return output resource type (support preview, remap, etc). */
	virtual EDisplayClusterViewportResourceType   GetOutputResourceType_RenderThread() const = 0;

	virtual const class IDisplayClusterViewportManagerProxy& GetOwner_RenderThread() const = 0;

	virtual void SetRenderSettings_RenderThread(const FDisplayClusterViewport_RenderSettings& InRenderSettings) const = 0;
	virtual void SetContexts_RenderThread(const TArray<FDisplayClusterViewport_Context>& InContexts) const = 0;
};
