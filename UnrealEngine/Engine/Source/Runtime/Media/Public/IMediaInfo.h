// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class IMediaModule;

class IMediaInfo : public IModuleInterface
{
public:
	virtual void Initialize(IMediaModule* MediaModule) = 0;
};
