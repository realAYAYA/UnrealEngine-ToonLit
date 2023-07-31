// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterProjectionBlueprintAPIImpl.h"

#include "IDisplayClusterProjection.h"
#include "DisplayClusterProjectionLog.h"
#include "DisplayClusterProjectionStrings.h"

#include "Policy/Camera/DisplayClusterProjectionCameraPolicy.h"
#include "Policy/Camera/DisplayClusterProjectionCameraPolicyFactory.h"

#include "Camera/CameraComponent.h"

#include "IDisplayCluster.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// Policy: CAMERA
//////////////////////////////////////////////////////////////////////////////////////////////
void UDisplayClusterProjectionBlueprintAPIImpl::CameraPolicySetCamera(const FString& ViewportId, UCameraComponent* NewCamera, float FOVMultiplier)
{
	IDisplayClusterRenderManager* RenderManager = IDisplayCluster::Get().GetRenderMgr();
	if (RenderManager && RenderManager->GetViewportManager())
	{
		IDisplayClusterViewport* Viewport = RenderManager->GetViewportManager()->FindViewport(ViewportId);
		if (Viewport != nullptr)
		{
			// @todo: Add extra settings latter to BP call if required
			FDisplayClusterProjectionCameraPolicySettings Settings;
			Settings.FOVMultiplier = FOVMultiplier;
			Settings.bCameraOverrideDefaults = true;

			IDisplayClusterProjection::Get().CameraPolicySetCamera(Viewport->GetProjectionPolicy(), NewCamera, Settings);
		}
	}
}

