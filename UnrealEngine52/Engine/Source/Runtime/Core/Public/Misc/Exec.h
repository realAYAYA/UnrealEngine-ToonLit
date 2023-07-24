// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"

class FOutputDevice;
class UWorld;

#ifndef UE_ALLOW_EXEC_COMMANDS
	#if UE_BUILD_SHIPPING && !WITH_EDITOR
		#define UE_ALLOW_EXEC_COMMANDS UE_ALLOW_EXEC_COMMANDS_IN_SHIPPING
	#else
		#define UE_ALLOW_EXEC_COMMANDS 1
	#endif
#endif

#ifndef UE_ALLOW_EXEC_DEV
	#define UE_ALLOW_EXEC_DEV !UE_BUILD_SHIPPING && UE_ALLOW_EXEC_COMMANDS
#endif

#ifndef UE_ALLOW_EXEC_EDITOR
	#define UE_ALLOW_EXEC_EDITOR WITH_EDITOR && UE_ALLOW_EXEC_COMMANDS
#endif

// Any object that is capable of taking commands.
class CORE_API FExec
{
public:
	virtual ~FExec();

#if UE_ALLOW_EXEC_COMMANDS

	/**
	 * Exec handler
	 *
	 * @param	InWorld	World context
	 * @param	Cmd		Command to parse
	 * @param	Ar		Output device to log to
	 *
	 * @return	true if command was handled, false otherwise
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar );
#else // UE_ALLOW_EXEC_COMMANDS

public:
	/**
	 * final override of Exec that asserts if called
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) final { check(false); return false; }
	
	/**
	 * final override of Exec to replace overrides where a default value for Ar is provided
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd ) final { check(false); return false; }

#endif // !UE_ALLOW_EXEC_COMMANDS

protected:
	/** Implementation of Exec that is only called in non-shipping targets */
	virtual bool Exec_Dev( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) { return false; }

	/** Implementation of Exec that is only called in editor */
	virtual bool Exec_Editor( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) { return false; }

};


