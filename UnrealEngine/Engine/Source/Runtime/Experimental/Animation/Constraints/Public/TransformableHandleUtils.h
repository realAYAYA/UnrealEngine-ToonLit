// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class USceneComponent;
class USkeletalMeshComponent;

namespace TransformableHandleUtils
{
	CONSTRAINTS_API void TickDependantComponents(const USceneComponent* InComponent);
	CONSTRAINTS_API void TickSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent);
}
