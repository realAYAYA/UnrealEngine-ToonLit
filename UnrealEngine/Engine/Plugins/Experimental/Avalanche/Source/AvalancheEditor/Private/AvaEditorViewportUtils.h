// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/SharedPointerFwd.h"

class IAvaViewportClient;

struct FAvaEditorViewportUtils
{
	static bool MeshSizeToPixelSize(const TSharedRef<IAvaViewportClient>& InViewportClient, double InMeshSize, double& OutPixelSize);

	static bool PixelSizeToMeshSize(const TSharedRef<IAvaViewportClient>& InViewportClient, double InPixelSize, double& OutMeshSize);
};
