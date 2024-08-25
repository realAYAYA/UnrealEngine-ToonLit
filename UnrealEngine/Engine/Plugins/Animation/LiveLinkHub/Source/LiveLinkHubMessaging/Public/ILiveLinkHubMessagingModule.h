// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Modules/ModuleInterface.h"

#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointer.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHubConnectionEstablished, FGuid SourceId);

class LIVELINKHUBMESSAGING_API ILiveLinkHubMessagingModule : public IModuleInterface
{
public:
	/** Delegate called when a connection is established to a livelink hub. */
	virtual FOnHubConnectionEstablished& OnConnectionEstablished() = 0;
};
