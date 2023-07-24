// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUserInterfaceCommand
{
public:
	/** Executes the command. */
	static void Run();

protected:
	/**
	 * Initializes the Slate application.
	 */
	static void InitializeSlateApplication(bool bOpenTraceFile, const TCHAR* TraceFile);

	/**
	 * Shuts down the Slate application.
	 */
	static void ShutdownSlateApplication();

private:
	/**
	* Attempts to get a utrace file path from the command line.
	* Returns true if a path was found and false otherwise.
	*/
	static bool GetTraceFileFromCmdLine(TCHAR* OutTraceFile, uint32 MaxPath);
};
