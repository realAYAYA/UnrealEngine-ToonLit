// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Debugging/ConsoleSlateDebugger.h"
#include "Debugging/ConsoleSlateDebuggerInvalidate.h"
#include "Debugging/ConsoleSlateDebuggerInvalidationRoot.h"
#include "Debugging/ConsoleSlateDebuggerPaint.h"
#include "Debugging/ConsoleSlateDebuggerUpdate.h"
#include "Debugging/ConsoleSlateDebuggerBreak.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "SlateGlobals.h"
#include "Types/SlateStructs.h"

DEFINE_LOG_CATEGORY(LogSlate);
DEFINE_LOG_CATEGORY(LogSlateStyles);

const float FOptionalSize::Unspecified = -1.0f;


/**
 * Implements the SlateCore module.
 */
class FSlateCoreModule
	: public IModuleInterface
{
public:
	FSlateCoreModule()
	{
#if WITH_SLATE_DEBUGGING
		SlateDebuggerEvent = MakeUnique<FConsoleSlateDebugger>();
		SlateDebuggerInvalidate = MakeUnique<FConsoleSlateDebuggerInvalidate>();
		SlateDebuggerInvalidationRoot = MakeUnique<FConsoleSlateDebuggerInvalidationRoot>();
		SlateDebuggerPaint = MakeUnique<FConsoleSlateDebuggerPaint>();
		SlateDebuggerUpdate = MakeUnique<FConsoleSlateDebuggerUpdate>();
		SlateDebuggerBreak = MakeUnique<FConsoleSlateDebuggerBreak>();
#endif
	}

#if WITH_SLATE_DEBUGGING
private:
	TUniquePtr<FConsoleSlateDebugger> SlateDebuggerEvent;
	TUniquePtr<FConsoleSlateDebuggerInvalidate> SlateDebuggerInvalidate;
	TUniquePtr<FConsoleSlateDebuggerInvalidationRoot> SlateDebuggerInvalidationRoot;
	TUniquePtr<FConsoleSlateDebuggerPaint> SlateDebuggerPaint;
	TUniquePtr<FConsoleSlateDebuggerUpdate> SlateDebuggerUpdate;
	TUniquePtr<FConsoleSlateDebuggerBreak> SlateDebuggerBreak;
#endif
};


IMPLEMENT_MODULE(FSlateCoreModule, SlateCore);
