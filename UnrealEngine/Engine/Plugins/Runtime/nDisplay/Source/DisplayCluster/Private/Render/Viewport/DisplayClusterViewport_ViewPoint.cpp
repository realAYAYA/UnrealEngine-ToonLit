// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "CineCameraComponent.h"

#include "SceneView.h"

#include "Misc/DisplayClusterLog.h"

#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"

namespace UE::DisplayCluster::Viewport::ViewPoint
{
	enum EDisplayClusterEyeType : int32
	{
		StereoLeft = 0,
		Mono = 1,
		StereoRight = 2,
		COUNT
	};
};
using namespace UE::DisplayCluster::Viewport::ViewPoint;

///////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
bool IDisplayClusterViewport::GetCameraComponentView(UCameraComponent* InCameraComponent, const float InDeltaTime, const bool bUseCameraPostprocess, FMinimalViewInfo& InOutViewInfo, float* OutCustomNearClippingPlane)
{
	if (!InCameraComponent)
	{
		// Required camera component
		return false;
	}

	InCameraComponent->GetCameraView(InDeltaTime, InOutViewInfo);

	if(!bUseCameraPostprocess)
	{
		InOutViewInfo.PostProcessSettings = FPostProcessSettings();
		InOutViewInfo.PostProcessBlendWeight = 0.0f;
	}

	// Get custom NCP from CineCamera component:
	if (OutCustomNearClippingPlane)
	{
		*OutCustomNearClippingPlane = -1;

		// Get settings from this cinecamera component
		if (UCineCameraComponent* CineCameraComponent = Cast<UCineCameraComponent>(InCameraComponent))
		{
			// Supports ICVFX camera component as input
			if (UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent = Cast<UDisplayClusterICVFXCameraComponent>(CineCameraComponent))
			{
				// Getting settings from the actual CineCamera component
				CineCameraComponent = ICVFXCameraComponent->GetActualCineCameraComponent();
			}

			if (CineCameraComponent && CineCameraComponent->bOverride_CustomNearClippingPlane)
			{
				*OutCustomNearClippingPlane = CineCameraComponent->CustomNearClippingPlane;
			}
		}
	}

	return true;
}

bool IDisplayClusterViewport::GetPlayerCameraView(UWorld* InWorld, const bool bUseCameraPostprocess, FMinimalViewInfo& InOutViewInfo)
{
	if (InWorld)
	{
		if (APlayerController* const CurPlayerController = InWorld->GetFirstPlayerController())
		{
			if (APlayerCameraManager* const CurPlayerCameraManager = CurPlayerController->PlayerCameraManager)
			{
				InOutViewInfo = CurPlayerCameraManager->GetCameraCacheView(); // Get desired view with postprocess from player camera

				if (!bUseCameraPostprocess)
				{
					InOutViewInfo.PostProcessSettings = FPostProcessSettings();
					InOutViewInfo.PostProcessBlendWeight = 0.0f;
				}

				InOutViewInfo.FOV = CurPlayerCameraManager->GetFOVAngle();
				CurPlayerCameraManager->GetCameraViewPoint(/*out*/ InOutViewInfo.Location, /*out*/ InOutViewInfo.Rotation);

				if (!bUseCameraPostprocess)
				{
					InOutViewInfo.PostProcessBlendWeight = 0.0f;
				}

				return true;
			}
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
UDisplayClusterCameraComponent* FDisplayClusterViewport::GetViewPointCameraComponent(const EDisplayClusterRootActorType InRootActorType) const
{
	ADisplayClusterRootActor* RootActor = Configuration->GetRootActor(InRootActorType);
	if (!RootActor)
	{
		if (CanShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::GetViewPointCameraComponent_NoRootActorFound))
		{
			UE_LOG(LogDisplayClusterViewport, Warning, TEXT("Viewport '%s' has no root actor found"), *GetId());
		}

		return nullptr;
	}

	ResetShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::GetViewPointCameraComponent_NoRootActorFound);

	if (!ProjectionPolicy.IsValid())
	{
		// ignore viewports with uninitialized prj policy
		return nullptr;
	}

	if (!Configuration->IsSceneOpened())
	{
		return nullptr;
	}

	// Get camera ID assigned to the viewport
	const FString& CameraId = GetRenderSettings().CameraId;
	if (CameraId.Len() > 0)
	{
		if (CanShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::GetViewPointCameraComponent_HasAssignedViewPoint))
		{
			UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Viewport '%s' has assigned ViewPoint '%s'"), *GetId(), *CameraId);
		}
	}
	else
	{
		ResetShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::GetViewPointCameraComponent_HasAssignedViewPoint);
	}

	// Get camera component assigned to the viewport (or default camera if nothing assigned)
	if (UDisplayClusterCameraComponent* const ViewCamera = (CameraId.IsEmpty() ?
		RootActor->GetDefaultCamera() :
		RootActor->GetComponentByName<UDisplayClusterCameraComponent>(CameraId)))
	{
		ResetShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::GetViewPointCameraComponent);

		return ViewCamera;
	}

	if (CanShowLogMsgOnce(EDisplayClusterViewportShowLogMsgOnce::GetViewPointCameraComponent_NotFound))
	{
		UE_LOG(LogDisplayClusterViewport, Warning, TEXT("ViewPoint '%s' is not found for viewport '%s'. The default viewpoint will be used."), *CameraId, *GetId());
	}

	return CameraId.IsEmpty() ? nullptr : RootActor->GetDefaultCamera();
}

bool FDisplayClusterViewport::SetupViewPoint(FMinimalViewInfo& InOutViewInfo)
{
	if (UDisplayClusterCameraComponent* SceneCameraComponent = GetViewPointCameraComponent(EDisplayClusterRootActorType::Scene))
	{
		// Get ViewPoint from DCRA component
		SceneCameraComponent->GetDesiredView(*Configuration, InOutViewInfo, &CustomNearClippingPlane);

		// The projection policy can override these ViewPoint data.
		if (ProjectionPolicy.IsValid())
		{
			if (ADisplayClusterRootActor* SceneRootActor = Configuration->GetRootActor(EDisplayClusterRootActorType::Scene))
			{
				const float DeltaTime = SceneRootActor->GetWorldDeltaSeconds();
				ProjectionPolicy->SetupProjectionViewPoint(this, DeltaTime, InOutViewInfo, &CustomNearClippingPlane);
			}
		}

		return true;
	}

	return false;
}

float FDisplayClusterViewport::GetStereoEyeOffsetDistance(const uint32 InContextNum)
{
	float StereoEyeOffsetDistance = 0.f;

	if (UDisplayClusterCameraComponent* ConfigurationCameraComponent = GetViewPointCameraComponent(EDisplayClusterRootActorType::Configuration))
	{
		// Calculate eye offset considering the world scale
		const float CfgEyeDist = ConfigurationCameraComponent->GetInterpupillaryDistance();
		const float EyeOffset = CfgEyeDist / 2.f;
		const float EyeOffsetValues[] = { -EyeOffset, 0.f, EyeOffset };

		// Decode current eye type
		const EDisplayClusterEyeType EyeType = (Contexts.Num() == 1)
			? EDisplayClusterEyeType::Mono
			: (InContextNum == 0) ? EDisplayClusterEyeType::StereoLeft : EDisplayClusterEyeType::StereoRight;

		float PassOffset = 0.f;

		if (EyeType == EDisplayClusterEyeType::Mono)
		{
			// For monoscopic camera let's check if the "force offset" feature is used
			// * Force left (-1) ==> 0 left eye
			// * Force right (1) ==> 2 right eye
			// * Default (0) ==> 1 mono
			const EDisplayClusterEyeStereoOffset CfgEyeOffset = ConfigurationCameraComponent->GetStereoOffset();
			const int32 EyeOffsetIdx =
				(CfgEyeOffset == EDisplayClusterEyeStereoOffset::None ? 0 :
					(CfgEyeOffset == EDisplayClusterEyeStereoOffset::Left ? -1 : 1));

			PassOffset = EyeOffsetValues[EyeOffsetIdx + 1];
			// Eye swap is not available for monoscopic so just save the value
			StereoEyeOffsetDistance = PassOffset;
		}
		else
		{
			// For stereo camera we can only swap eyes if required (no "force offset" allowed)
			PassOffset = EyeOffsetValues[EyeType];

			// Apply eye swap
			const bool  CfgEyeSwap = ConfigurationCameraComponent->GetSwapEyes();
			StereoEyeOffsetDistance = (CfgEyeSwap ? -PassOffset : PassOffset);
		}
	}

	return StereoEyeOffsetDistance;
}
