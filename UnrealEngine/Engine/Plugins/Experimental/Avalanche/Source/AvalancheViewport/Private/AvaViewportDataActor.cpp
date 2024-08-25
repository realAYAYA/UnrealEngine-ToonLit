// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaViewportDataActor.h"

AAvaViewportDataActor::AAvaViewportDataActor()
{
	ViewportData.PostProcessInfo.Type = EAvaViewportPostProcessType::None;
	ViewportData.PostProcessInfo.Opacity = 1.f;
	ViewportData.VirtualSize = FIntPoint::ZeroValue;
	ViewportData.VirtualSizeAspectRatioState = EAvaViewportVirtualSizeAspectRatioState::LockedToCamera;
}
