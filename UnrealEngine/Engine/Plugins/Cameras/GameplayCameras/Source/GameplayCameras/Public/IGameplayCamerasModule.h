// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Stats/Stats.h"

#if WITH_EDITOR
class IGameplayCamerasLiveEditManager;
#endif

DECLARE_STATS_GROUP(TEXT("Camera System Evaluation"), STATGROUP_CameraSystem, STATCAT_Advanced)
DECLARE_STATS_GROUP(TEXT("Camera Animation Evaluation"), STATGROUP_CameraAnimation, STATCAT_Advanced)

class IGameplayCamerasModule : public IModuleInterface
{
public:
	/**
	 * Singleton-like access to ICameraModule
	 *
	 * @return The ICameraModule instance, loading the module on demand if needed
	 */
	static IGameplayCamerasModule& Get();

#if WITH_EDITOR
	/**
	 * Gets the live edit manager.
	 */
	virtual TSharedPtr<IGameplayCamerasLiveEditManager> GetLiveEditManager() const = 0;

	/**
	 * Sets the live edit manager.
	 */
	virtual void SetLiveEditManager(TSharedPtr<IGameplayCamerasLiveEditManager> InLiveEditManager) = 0;
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
