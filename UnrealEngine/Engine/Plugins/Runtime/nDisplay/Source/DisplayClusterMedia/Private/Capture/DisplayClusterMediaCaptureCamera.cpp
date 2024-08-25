// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureCamera.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterRootActor.h"
#include "IDisplayCluster.h"
#include "Game/IDisplayClusterGameManager.h"


FDisplayClusterMediaCaptureCamera::FDisplayClusterMediaCaptureCamera(const FString& InMediaId, const FString& InClusterNodeId, const FString& InCameraId, const FString& InViewportId, UMediaOutput* InMediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy)
	: FDisplayClusterMediaCaptureViewport(InMediaId, InClusterNodeId, InViewportId, InMediaOutput, SyncPolicy)
	, CameraId(InCameraId)
{
}

bool FDisplayClusterMediaCaptureCamera::GetCaptureSizeFromConfig(FIntPoint& OutSize) const
{
	if (const ADisplayClusterRootActor* const ActiveRootActor = IDisplayCluster::Get().GetGameMgr()->GetRootActor())
	{
		TArray<UActorComponent*> ICVFXCameraComponents;
		IDisplayClusterViewportConfiguration* ViewportConfiguration = ActiveRootActor->GetViewportConfiguration();
		if (const ADisplayClusterRootActor* const ConfigurationRootActor = ViewportConfiguration ? ViewportConfiguration->GetRootActor(EDisplayClusterRootActorType::Configuration) : nullptr)
		{
			ConfigurationRootActor->GetComponents(UDisplayClusterICVFXCameraComponent::StaticClass(), ICVFXCameraComponents);
		}

		// Get capture size from camera config
		for (const UActorComponent* const Component : ICVFXCameraComponents)
		{
			if (const UDisplayClusterICVFXCameraComponent* const ICVFXCamera = Cast<UDisplayClusterICVFXCameraComponent>(Component))
			{
				if (ICVFXCamera->GetName() == CameraId)
				{
					const FDisplayClusterConfigurationICVFX_CameraSettings& CameraSettings = ICVFXCamera->GetCameraSettingsICVFX();

					if (CameraSettings.CustomFrustum.bEnable)
					{
						OutSize = CameraSettings.CustomFrustum.EstimatedOverscanResolution;
					}
					else
					{
						OutSize = CameraSettings.CustomFrustum.InnerFrustumResolution;
					}

					return true;
				}
			}
		}
	}

	return false;
}
