// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "AvaNullComponent.generated.h"

UCLASS(MinimalAPI, DisplayName = "Null Component")
class UAvaNullComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UAvaNullComponent();
};
