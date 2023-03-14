// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorViewportClient.h"
#include "Templates/SharedPointer.h"

class FAdvancedPreviewScene;

/**
 * Super simple viewport client designed to work SMutableMeshViewport
 */
class FMutableMeshViewportClient : public FEditorViewportClient
{
public:
	FMutableMeshViewportClient(const TSharedRef<FAdvancedPreviewScene>& InPreviewScene);
};

