// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

/**
 * Online subsystem delegates that are more external to the online subsystems themselves
 * This is NOT to replace the individual interfaces that have the Add/Clear/Trigger syntax
 */
class ONLINESUBSYSTEM_API FOnlineSubsystemDelegates
{

public:

	/**
	 * Notification that a new online subsystem instance has been created
	 * 
	 * @param NewSubsystem the new instance created
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnOnlineSubsystemCreated, class IOnlineSubsystem* /*NewSubsystem*/);
	static FOnOnlineSubsystemCreated OnOnlineSubsystemCreated;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
