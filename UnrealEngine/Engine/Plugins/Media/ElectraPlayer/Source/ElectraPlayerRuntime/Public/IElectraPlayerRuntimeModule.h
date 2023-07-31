// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Modules/ModuleInterface.h"

/**
 * Interface for the ElectraPlayerRuntime module.
 */
class IElectraPlayerRuntimeModule
	: public IModuleInterface
{
public:

	/**
	 * Is the ElectraPlayerRuntime module initialized?
	 * @return True if the module is initialized.
	 */
	virtual bool IsInitialized() const = 0;

	virtual ~IElectraPlayerRuntimeModule() { }
};
