// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"

/**
 * Interface for the 'ElectraHTTPStream' module.
 */
class IElectraHTTPStreamModule : public IModuleInterface
{
public:
	virtual ~IElectraHTTPStreamModule() = default;
};
