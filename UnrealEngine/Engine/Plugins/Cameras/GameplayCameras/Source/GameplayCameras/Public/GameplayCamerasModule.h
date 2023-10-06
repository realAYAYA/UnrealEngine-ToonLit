// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Stats/Stats.h"

DECLARE_STATS_GROUP(TEXT("Camera Animation Evaluation"), STATGROUP_CameraAnimation, STATCAT_Advanced)

class IGameplayCamerasModule : public IModuleInterface
{
	/**
	 * Singleton-like access to ICameraModule
	 *
	 * @return The ICameraModule instance, loading the module on demand if needed
	 */
	static IGameplayCamerasModule& Get();
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
