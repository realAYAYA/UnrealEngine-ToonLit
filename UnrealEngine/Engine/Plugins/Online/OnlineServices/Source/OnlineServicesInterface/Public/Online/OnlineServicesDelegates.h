// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::Online {

/**
 * Online services delegates that are more external to the online services themselves
 */

/**
 * Notification that a new online subsystem instance has been created
 *
 * @param NewSubsystem the new instance created
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOnlineServicesCreated, TSharedRef<class IOnlineServices> /*NewServices*/);
extern ONLINESERVICESINTERFACE_API FOnOnlineServicesCreated OnOnlineServicesCreated;

/* UE::Online */ }
