// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "AI/NavigationSystemBase.h"
#include "PathFollowingManager.generated.h"

class AController;


UCLASS()
class AIMODULE_API UPathFollowingManager : public UObject, public IPathFollowingManagerInterface
{
	GENERATED_BODY()
public:
	UPathFollowingManager(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static void StopMovement(const AController& Controller);
	static bool IsFollowingAPath(const AController& Controller);
};
