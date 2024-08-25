// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ViewportToolBarContext.generated.h"

class SViewportToolBar;
class SLevelViewport;

UCLASS(MinimalAPI)
class UViewportToolBarContext : public UObject
{
	GENERATED_BODY()
public:
	TWeakPtr<SViewportToolBar> ViewportToolBar;
	TWeakPtr<SLevelViewport> Viewport;
};
