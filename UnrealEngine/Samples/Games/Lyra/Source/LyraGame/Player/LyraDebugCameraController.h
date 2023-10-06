// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DebugCameraController.h"

#include "LyraDebugCameraController.generated.h"

class UObject;


/**
 * ALyraDebugCameraController
 *
 *	Used for controlling the debug camera when it is enabled via the cheat manager.
 */
UCLASS()
class ALyraDebugCameraController : public ADebugCameraController
{
	GENERATED_BODY()

public:

	ALyraDebugCameraController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:

	virtual void AddCheats(bool bForce) override;
};
