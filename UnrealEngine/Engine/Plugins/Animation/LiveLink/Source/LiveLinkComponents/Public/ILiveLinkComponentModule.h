// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "LiveLinkComponentController.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FLiveLinkComponentRegistered, ULiveLinkComponentController*);

/**
 * Interface for LiveLinkComponent module
 */
class ILiveLinkComponentsModule : public IModuleInterface
{
public:
	virtual FLiveLinkComponentRegistered& OnLiveLinkComponentRegistered() = 0;

public:
	/** Virtual destructor. */
	virtual ~ILiveLinkComponentsModule() { }
};

