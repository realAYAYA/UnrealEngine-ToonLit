// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "PythonScriptTypes.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

class IPythonScriptPlugin : public IModuleInterface
{
public:
	/** Get this module */
	static IPythonScriptPlugin* Get()
	{
		static const FName ModuleName = "PythonScriptPlugin";
		return FModuleManager::GetModulePtr<IPythonScriptPlugin>(ModuleName);
	}

	/**
	 * Check to see whether the plugin was built with Python support enabled.
	 */
	virtual bool IsPythonAvailable() const = 0;

	/**
	 * Execute the given Python command.
	 * This may be literal Python code, or a file (with optional arguments) that you want to run.
	 * @return true if the command ran successfully, false if there were errors (the output log will show the errors).
	 */
	virtual bool ExecPythonCommand(const TCHAR* InPythonCommand) = 0;

	/**
	 * Execute the given Python command.
	 * @return true if the command ran successfully, false if there were errors.
	 */
	virtual bool ExecPythonCommandEx(FPythonCommandEx& InOutPythonCommand) = 0;
	
	/**
	 * Delegate called after Python has been initialized.
	 */
	virtual FSimpleMulticastDelegate& OnPythonInitialized() = 0;

	/**
	 * Delegate called before Python is shutdown.
	 */
	virtual FSimpleMulticastDelegate& OnPythonShutdown() = 0;
};
