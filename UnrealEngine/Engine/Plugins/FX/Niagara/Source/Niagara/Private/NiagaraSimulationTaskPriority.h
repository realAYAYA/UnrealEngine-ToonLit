// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Engine/EngineBaseTypes.h"

namespace NiagaraSimulationTaskPriority
{
	extern void Initialize();
	extern ENamedThreads::Type GetPostActorTickPriority();
	extern ENamedThreads::Type GetTickGroupPriority(ETickingGroup TickGroup);
}
