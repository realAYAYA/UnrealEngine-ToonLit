// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Stats/Stats2.h"

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

