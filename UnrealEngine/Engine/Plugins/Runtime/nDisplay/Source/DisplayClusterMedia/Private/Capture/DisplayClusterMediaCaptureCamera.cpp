// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureCamera.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterRootActor.h"
#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "RHICommandList.h"
#include "RHIResources.h"


FDisplayClusterMediaCaptureCamera::FDisplayClusterMediaCaptureCamera(const FString& InMediaId, const FString& InClusterNodeId, const FString& InCameraId, const FString& InViewportId, UMediaOutput* InMediaOutput)
	: FDisplayClusterMediaCaptureViewport(InMediaId, InClusterNodeId, InViewportId, InMediaOutput)
	, CameraId(InCameraId)
{
	if (const ADisplayClusterRootActor* const RootActor = IDisplayCluster::Get().GetGameMgr()->GetRootActor())
	{
		TArray<UActorComponent*> ICVFXCameraComponents;
		RootActor->GetComponents(UDisplayClusterICVFXCameraComponent::StaticClass(), ICVFXCameraComponents);

		for (const UActorComponent* const Component : ICVFXCameraComponents)
		{
			if (const UDisplayClusterICVFXCameraComponent* const ICVFXCamera = Cast<UDisplayClusterICVFXCameraComponent>(Component))
			{
				if (ICVFXCamera->GetName() == CameraId)
				{
					CameraResolution = ICVFXCamera->GetCameraSettingsICVFX().CustomFrustum.InnerFrustumResolution;
				}
			}
		}
	}
}

FIntPoint FDisplayClusterMediaCaptureCamera::GetCaptureSize() const
{
	return CameraResolution;
}
