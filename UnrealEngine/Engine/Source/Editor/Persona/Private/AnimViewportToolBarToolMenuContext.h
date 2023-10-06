// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimViewportToolBarToolMenuContext.generated.h"

class SAnimViewportToolBar;

UCLASS()
class UAnimViewportToolBarToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<const SAnimViewportToolBar> AnimViewportToolBar;
};
