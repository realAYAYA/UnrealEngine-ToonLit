// Copyright Epic Games, Inc. All Rights Reserved.

#include "StereoRendering.h"
#include "SceneView.h"

#include "Engine/Engine.h"

bool IStereoRendering::IsStereoEyeView(const FSceneView& View)
{
	return IsStereoEyePass(View.StereoPass);
}

bool IStereoRendering::IsAPrimaryView(const FSceneView& View)
{
	return IsAPrimaryPass(View.StereoPass);
}

bool IStereoRendering::IsASecondaryView(const FSceneView& View)
{
	return IsASecondaryPass(View.StereoPass);
}
