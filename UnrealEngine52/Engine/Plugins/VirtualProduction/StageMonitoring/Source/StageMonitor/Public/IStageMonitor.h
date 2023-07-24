// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "StageMessages.h"
#include "UObject/StructOnScope.h"


class IStageMonitorSession;

/**
 * Interface for the stage monitor. Users can get the instance from the stage monitor module
 */
class STAGEMONITOR_API IStageMonitor
{
public:

	virtual ~IStageMonitor() {}
	
	/**
	 * Returns true if monitor is actively listening for activities
	 */
	virtual bool IsActive() const = 0;
};
