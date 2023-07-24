// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Transform.h"

#define VARIANT_MANAGER_THUMBNAIL_SIZE 256

class UTexture2D;

namespace ThumbnailGenerator
{
    UTexture2D* GenerateThumbnailFromTexture(UTexture2D* InImage);
    UTexture2D* GenerateThumbnailFromFile(FString FilePath);
    UTexture2D* GenerateThumbnailFromCamera(UObject* WorldContextObject, const FTransform& CameraTransform, float FOVDegrees = 50.0f, float MinZ = 50.0f, float Gamma = 2.2f);
    UTexture2D* GenerateThumbnailFromEditorViewport();
	UTexture2D* GenerateThumbnailFromObjectThumbnail(UObject* Object);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
