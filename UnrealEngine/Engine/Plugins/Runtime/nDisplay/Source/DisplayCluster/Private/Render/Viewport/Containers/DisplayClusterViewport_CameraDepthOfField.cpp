// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Containers/DisplayClusterViewport_CameraDepthOfField.h"

#include "Engine/Texture2D.h"
#include "SceneView.h"
#include "TextureResource.h"

void FDisplayClusterViewport_CameraDepthOfField::SetupSceneView(FSceneView& InOutView) const
{
	InOutView.bEnableDynamicCocOffset = bEnableDepthOfFieldCompensation;
	InOutView.InFocusDistance = DistanceToWall + DistanceToWallOffset;

	if (CompensationLUT.IsValid())
	{
		InOutView.DynamicCocOffsetLUT = CompensationLUT->GetResource() ? CompensationLUT->GetResource()->GetTextureRHI() : nullptr;
	}
}