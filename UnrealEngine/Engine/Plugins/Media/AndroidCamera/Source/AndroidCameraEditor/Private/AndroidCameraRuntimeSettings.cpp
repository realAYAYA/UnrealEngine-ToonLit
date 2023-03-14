// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidCameraRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UAndroidCameraRuntimeSettings

UAndroidCameraRuntimeSettings::UAndroidCameraRuntimeSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bEnablePermission(true)
	, bRequiresAnyCamera(false)
	, bRequiresBackFacingCamera(false)
	, bRequiresFrontFacingCamera(false)
{
}
