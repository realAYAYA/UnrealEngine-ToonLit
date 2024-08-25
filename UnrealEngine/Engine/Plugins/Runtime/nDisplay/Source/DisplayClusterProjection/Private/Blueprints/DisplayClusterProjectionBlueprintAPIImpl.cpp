// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterProjectionBlueprintAPIImpl.h"
#include "Blueprints/DisplayClusterProjectionBlueprintLib.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// Policy: CAMERA
//////////////////////////////////////////////////////////////////////////////////////////////
void UDisplayClusterProjectionBlueprintAPIImpl::CameraPolicySetCamera(const FString& ViewportId, UCameraComponent* NewCamera, float FOVMultiplier)
{
	UDisplayClusterProjectionBlueprintLib::CameraPolicySetCamera(ViewportId, NewCamera, FOVMultiplier);
}
