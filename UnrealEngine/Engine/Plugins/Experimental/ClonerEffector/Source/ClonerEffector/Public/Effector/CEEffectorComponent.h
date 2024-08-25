// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "CEEffectorComponent.generated.h"

/** Class used to define a custom visualizer for this component and the effector actor */
UCLASS(MinimalAPI, Within=CEEffectorActor)
class UCEEffectorComponent : public USceneComponent
{
	GENERATED_BODY()
};