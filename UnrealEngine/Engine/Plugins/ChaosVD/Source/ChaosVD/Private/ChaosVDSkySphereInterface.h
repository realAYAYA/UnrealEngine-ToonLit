// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "ChaosVDSkySphereInterface.generated.h"

class ADirectionalLight;

UINTERFACE()
class UChaosVDSkySphereInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Simple interface to allow update the light source in the BP SkySphere from C++
 */
class CHAOSVD_API IChaosVDSkySphereInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, CallInEditor)
	void SetDirectionalLightSource(ADirectionalLight* NewLightSource);

	UFUNCTION(BlueprintNativeEvent, CallInEditor)
	void Refresh();
};
