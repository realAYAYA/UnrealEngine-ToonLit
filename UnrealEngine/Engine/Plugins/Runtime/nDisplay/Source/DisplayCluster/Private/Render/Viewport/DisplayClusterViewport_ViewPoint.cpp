// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterCameraComponent.h"

#include "SceneView.h"

#include "Misc/DisplayClusterLog.h"

namespace UE::DisplayClusterViewport
{
	enum EDisplayClusterEyeType : int32
	{
		StereoLeft = 0,
		Mono = 1,
		StereoRight = 2,
		COUNT
	};
};
using namespace UE::DisplayClusterViewport;

///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewport
///////////////////////////////////////////////////////////////////////////////////////
UDisplayClusterCameraComponent* FDisplayClusterViewport::GetViewPointCameraComponent() const
{
	ADisplayClusterRootActor* RootActor = GetRootActor();
	if (!RootActor)
	{
		UE_LOG(LogDisplayClusterViewport, Warning, TEXT("Viewport '%s' has no root actor found in game manager"), *GetId());

		return nullptr;
	}

	if (!ProjectionPolicy.IsValid())
	{
		// ignore viewports with uninitialized prj policy
		return nullptr;
	}

	FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl();
	if (!ViewportManager || !ViewportManager->IsSceneOpened())
	{
		return nullptr;
	}

	// Get camera ID assigned to the viewport
	const FString& CameraId = GetRenderSettings().CameraId;
	if (CameraId.Len() > 0)
	{
		UE_LOG(LogDisplayClusterViewport, Verbose, TEXT("Viewport '%s' has assigned ViewPoint '%s'"), *GetId(), *CameraId);
	}

	// Get camera component assigned to the viewport (or default camera if nothing assigned)
	if (UDisplayClusterCameraComponent* const ViewCamera = (CameraId.IsEmpty() ?
		RootActor->GetDefaultCamera() :
		RootActor->GetComponentByName<UDisplayClusterCameraComponent>(CameraId)))
	{
		return ViewCamera;
	}

	UE_LOG(LogDisplayClusterViewport, Warning, TEXT("ViewPoint '%s' is not found for viewport '%s'"), *CameraId, * GetId());

	return nullptr;
}

bool FDisplayClusterViewport::SetupViewPoint(FMinimalViewInfo& InOutViewInfo)
{
	if (UDisplayClusterCameraComponent* ViewCamera = GetViewPointCameraComponent())
	{
		// First try to get the viewpoint from the projection policy
		if (!ProjectionPolicy->GetViewPoint(this, InOutViewInfo.Rotation, InOutViewInfo.Location))
		{
			// if unsuccessful, get the viewport from the viewport camera
			InOutViewInfo.Location = ViewCamera->GetComponentLocation();
			InOutViewInfo.Rotation = ViewCamera->GetComponentRotation();
		}

		return true;
	}

	return false;
}

float FDisplayClusterViewport::GetStereoEyeOffsetDistance(const uint32 InContextNum)
{
	float StereoEyeOffsetDistance = 0.f;

	if (UDisplayClusterCameraComponent* ViewCamera = GetViewPointCameraComponent())
	{
		// First try to get the eye offset distance from the projection policy
		if (!ProjectionPolicy->GetStereoEyeOffsetDistance(this, InContextNum, StereoEyeOffsetDistance))
		{
			// if failed, get from the viewport camera

			// Calculate eye offset considering the world scale
			const float CfgEyeDist = ViewCamera->GetInterpupillaryDistance();
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
				const EDisplayClusterEyeStereoOffset CfgEyeOffset = ViewCamera->GetStereoOffset();
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
				const bool  CfgEyeSwap = ViewCamera->GetSwapEyes();
				StereoEyeOffsetDistance = (CfgEyeSwap ? -PassOffset : PassOffset);
			}
		}
	}

	return StereoEyeOffsetDistance;
}
