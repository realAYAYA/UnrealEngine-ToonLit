// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/DisplayClusterWarpPolicyBase.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Components/DisplayClusterCameraComponent.h"

#include "Containers/DisplayClusterWarpEye.h"

#include "DisplayClusterWarpLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterWarpPolicyBase
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterWarpPolicyBase::FDisplayClusterWarpPolicyBase(const FString& InType, const FString& InWarpPolicyName)
	: PolicyInstanceId(FString::Printf(TEXT("WarpPolicy_%s_%s"), *InType, *InWarpPolicyName))
{ }
