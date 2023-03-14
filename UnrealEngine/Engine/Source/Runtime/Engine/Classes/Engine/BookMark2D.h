// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BookmarkBase.h"
#include "BookMark2D.generated.h"

USTRUCT()
struct FBookmark2DJumpToSettings
{
	GENERATED_BODY()
};

/**
 * Simple class to store 2D camera information.
 */
UCLASS(hidecategories=Object)
class ENGINE_API UBookMark2D : public UBookmarkBase
{
	GENERATED_UCLASS_BODY()

	/** Zoom of the camera */
	UPROPERTY(EditAnywhere, Category=BookMark2D)
	float Zoom2D;

	/** Location of the camera */
	UPROPERTY(EditAnywhere, Category=BookMark2D)
	FIntPoint Location;
};
