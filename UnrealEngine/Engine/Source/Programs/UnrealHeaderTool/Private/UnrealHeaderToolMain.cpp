// Copyright Epic Games, Inc. All Rights Reserved.


#include "UnrealHeaderTool.h"
#include "CoreMinimal.h"
#include "Containers/UnrealString.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "Misc/CompilationResult.h"
#include "UnrealHeaderToolGlobals.h"
#include "RequiredProgramMainCPPInclude.h"

IMPLEMENT_APPLICATION(UnrealHeaderTool, "UnrealHeaderTool");

DEFINE_LOG_CATEGORY(LogCompile);

/**
 * Application entry point
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	FString CmdLine;

	for (int32 Arg = 0; Arg < ArgC; Arg++)
	{
		FString LocalArg = ArgV[Arg];
		if (LocalArg.Contains(TEXT(" "), ESearchCase::CaseSensitive))
		{
			CmdLine += TEXT("\"");
			CmdLine += LocalArg;
			CmdLine += TEXT("\"");
		}
		else
		{
			CmdLine += LocalArg;
		}

		if (Arg + 1 < ArgC)
		{
			CmdLine += TEXT(" ");
		}
	}

	FString ShortCmdLine = FCommandLine::RemoveExeName(*CmdLine);
	ShortCmdLine.TrimStartInline();	

	// Get game name from the commandline. It will later be used to load the correct ini files.
	FString ModuleInfoFilename;
	if (ShortCmdLine.Len() && **ShortCmdLine != TEXT('-'))
	{
		const TCHAR* CmdLinePtr = *ShortCmdLine;

		// Parse the game name or project filename.  UHT reads the list of plugins from there in case one of the plugins is UHT plugin.
		FString GameName = FParse::Token(CmdLinePtr, false);

		// This parameter is the absolute path to the file which contains information about the modules
		// that UHT needs to generate code for.
		ModuleInfoFilename = FParse::Token(CmdLinePtr, false );
	}

#if !NO_LOGGING
	const static bool bVerbose = FParse::Param(*CmdLine,TEXT("VERBOSE"));
	if (bVerbose)
	{
		UE_SET_LOG_VERBOSITY(LogCompile, Verbose);
	}
#endif

	// Make sure the engine is properly cleaned up whenever we exit this function
	ON_SCOPE_EXIT
	{
		FEngineLoop::AppPreExit();
		FEngineLoop::AppExit();
	};

	GIsUCCMakeStandaloneHeaderGenerator = true;
	if (GEngineLoop.PreInit(*ShortCmdLine) != 0)
	{
		UE_LOG(LogCompile, Error, TEXT("Failed to initialize the engine (PreInit failed)."));
		return ECompilationResult::CrashOrAssert;
	}

	// Log full command line for UHT as UBT overrides LogInit verbosity settings
	UE_LOG(LogCompile, Log, TEXT("UHT Command Line: %s"), *CmdLine);

	if (ModuleInfoFilename.IsEmpty())
	{
		if (!FPlatformMisc::IsDebuggerPresent())
		{
			UE_LOG(LogCompile, Error, TEXT( "Missing module info filename on command line" ));
			return ECompilationResult::OtherCompilationError;
		}

		// If we have a debugger, let's use a pre-existing manifest file to streamline debugging
		// without the user having to faff around trying to get a UBT-generated manifest
		ModuleInfoFilename = FPaths::ConvertRelativePathToFull(FPlatformProcess::BaseDir(), TEXT("../../Source/Programs/UnrealHeaderTool/Resources/UHTDebugging.manifest"));
	}

	extern ECompilationResult::Type UnrealHeaderTool_Main(const FString& ModuleInfoFilename);
	ECompilationResult::Type Result = UnrealHeaderTool_Main(ModuleInfoFilename);
	return Result;
}

