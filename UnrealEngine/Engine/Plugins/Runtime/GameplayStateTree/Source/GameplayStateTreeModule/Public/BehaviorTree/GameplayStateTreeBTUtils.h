// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BehaviorTree/BehaviorTreeTypes.h"

enum class EStateTreeRunStatus : uint8;

namespace GameplayStateTreeBTUtils
{
	GAMEPLAYSTATETREEMODULE_API EBTNodeResult::Type StateTreeRunStatusToBTNodeResult(EStateTreeRunStatus Result);
}