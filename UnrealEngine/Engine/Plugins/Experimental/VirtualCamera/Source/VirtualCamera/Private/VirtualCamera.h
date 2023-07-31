// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVirtualCamera, Log, Log);


class FConcertVirtualCameraManager;

/**
 *
 */
class IVirtualCameraModule : public IModuleInterface
{
public:
	static const IVirtualCameraModule& Get();

	virtual FConcertVirtualCameraManager* GetConcertVirtualCameraManager() const = 0;
};
