// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestRunner.h"
#include "TestRunnerPrivate.h"

#include "HAL/PlatformOutputDevices.h"
#include "HAL/PlatformTLS.h"
#include "Logging/LogSuppressionInterface.h"
#include "LowLevelTestModule.h"
#include "Misc/ScopeExit.h"
#include "Misc/StringBuilder.h"
#include "Modules/ModuleManager.h"
#include "String/Find.h"
#include "String/LexFromString.h"
#include "TestCommon/CoreUtilities.h"

#if WITH_APPLICATION_CORE
#include "HAL/PlatformApplicationMisc.h"
#endif

#include <catch2/catch_session.hpp>

#include <iostream>

namespace UE::LowLevelTests
{

static ITestRunner* GTestRunner;

ITestRunner* ITestRunner::Get()
{
	return GTestRunner;
}

ITestRunner::ITestRunner()
{
	check(!GTestRunner);
	GTestRunner = this;
}

ITestRunner::~ITestRunner()
{
	check(GTestRunner == this);
	GTestRunner = nullptr;
}

class FTestRunner final : public ITestRunner
{
public:
	FTestRunner();

	void ParseCommandLine(TConstArrayView<const ANSICHAR*> Args);

	void SleepOnInit() const;

	void GlobalSetup() const;
	void GlobalTeardown() const;
	void Terminate() const;

	int32 RunCatchSession() const;

	bool HasGlobalSetup() const final { return bGlobalSetup; }
	bool HasLogOutput() const final { return bLogOutput || bDebugMode; }
	bool IsDebugMode() const final { return bDebugMode; }

private:
	static TArray<FName> GetGlobalModuleNames()
	{
		TArray<FName> ModuleNames;
		FModuleManager::Get().FindModules(TEXT("*GlobalLowLevelTests"), ModuleNames);
		return ModuleNames;
	}

	TArray<const ANSICHAR*> CatchArgs;
	FStringBuilderBase ExtraArgs;
	bool bGlobalSetup = true;
	bool bLogOutput = false;
	bool bDebugMode = false;
	bool bMultiThreaded = false;
	bool bWaitForInputToTerminate = false;
	int32 SleepOnInitSeconds = 0;
};

FTestRunner::FTestRunner()
{
	// Start setting up the Game Thread.
	GGameThreadId = FPlatformTLS::GetCurrentThreadId();
	GIsGameThreadIdInitialized = true;
}

void FTestRunner::ParseCommandLine(TConstArrayView<const ANSICHAR*> Args)
{
	bool bExtraArg = false;
	for (FAnsiStringView Arg : Args)
	{
		if (bExtraArg)
		{
			if (const int32 SpaceIndex = String::FindFirstChar(Arg, ' '); SpaceIndex != INDEX_NONE)
			{
				if (const int32 EqualIndex = String::FindFirstChar(Arg, '='); EqualIndex != INDEX_NONE && EqualIndex < SpaceIndex)
				{
					ExtraArgs.Append(Arg.Left(EqualIndex + 1));
					Arg.RightChopInline(EqualIndex + 1);
				}
				ExtraArgs.AppendChar('"').Append(Arg).AppendChar('"').AppendChar(' ');
			}
			else
			{
				ExtraArgs.Append(Arg).AppendChar(' ');
			}
		}
		else if (Arg == ANSITEXTVIEW("--"))
		{
			bExtraArg = true;
		}
		else if (Arg.StartsWith(ANSITEXTVIEW("--sleep=")))
		{
			LexFromString(SleepOnInitSeconds, WriteToString<16>(Arg.RightChop(8)).ToView());
		}
		else if (Arg == ANSITEXTVIEW("--global-setup"))
		{
			bGlobalSetup = true;
		}
		else if (Arg == ANSITEXTVIEW("--no-global-setup"))
		{
			bGlobalSetup = false;
		}
		else if (Arg == ANSITEXTVIEW("--log"))
		{
			bLogOutput = true;
		}
		else if (Arg == ANSITEXTVIEW("--no-log"))
		{
			bLogOutput = false;
		}
		else if (Arg == ANSITEXTVIEW("--debug"))
		{
			bDebugMode = true;
		}
		else if (Arg == ANSITEXTVIEW("--mt"))
		{
			bMultiThreaded = true;
		}
		else if (Arg == ANSITEXTVIEW("--no-mt"))
		{
			bMultiThreaded = false;
		}
		else if (Arg == ANSITEXTVIEW("--wait"))
		{
			bWaitForInputToTerminate = true;
		}
		else if (Arg == ANSITEXTVIEW("--no-wait"))
		{
			bWaitForInputToTerminate = true;
		}
		else
		{
			CatchArgs.Add(Arg.GetData());
		}
	}

	// Break in the debugger on failed assertions when attached.
	CatchArgs.Add("--break");
}

void FTestRunner::SleepOnInit() const
{
	if (SleepOnInitSeconds)
	{
		// Sleep to allow sync with Gauntlet.
		FPlatformProcess::Sleep(SleepOnInitSeconds);
	}
}

void FTestRunner::GlobalSetup() const
{
	if (!bGlobalSetup)
	{
		return;
	}

	FCommandLine::Set(*ExtraArgs);

	// Finish setting up the Game Thread, which requires the command line.
	FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetMainGameMask());
	FPlatformProcess::SetupGameThread();

	// Always set up GError to handle FatalError, failed assertions, and crashes and other fatal errors.
#if WITH_APPLICATION_CORE
	GError = FPlatformApplicationMisc::GetErrorOutputDevice();
#else
	GError = FPlatformOutputDevices::GetError();
#endif

	if (bLogOutput || bDebugMode)
	{
		// Set up GWarn to handle Error, Warning, Display; but only when log output is enabled.
#if WITH_APPLICATION_CORE
		GWarn = FPlatformApplicationMisc::GetFeedbackContext();
#else
		GWarn = FPlatformOutputDevices::GetFeedbackContext();
#endif

		// Set up default output devices to handle Log, Verbose, VeryVerbose.
		FPlatformOutputDevices::SetupOutputDevices();

		FLogSuppressionInterface::Get().ProcessConfigAndCommandLine();
	}

	for (FName ModuleName : GetGlobalModuleNames())
	{
		if (ILowLevelTestsModule* Module = FModuleManager::LoadModulePtr<ILowLevelTestsModule>(ModuleName))
		{
			Module->GlobalSetup();
		}
	}
}

void FTestRunner::GlobalTeardown() const
{
	if (!bGlobalSetup)
	{
		return;
	}

	for (FName ModuleName : GetGlobalModuleNames())
	{
		if (ILowLevelTestsModule* Module = FModuleManager::GetModulePtr<ILowLevelTestsModule>(ModuleName))
		{
			Module->GlobalTeardown();
		}
	}

	CleanupPlatform();

	FCommandLine::Reset();
}

void FTestRunner::Terminate() const
{
#if PLATFORM_DESKTOP
	if (bWaitForInputToTerminate)
	{
		std::cout << "Press enter to exit..." << std::endl;
		std::cin.ignore();
	}
#endif
}

int32 FTestRunner::RunCatchSession() const
{
	return Catch::Session().run(CatchArgs.Num(), CatchArgs.GetData());
}

} // UE::LowLevelTests

int RunTests(int32 ArgC, const ANSICHAR* ArgV[])
{
	UE::LowLevelTests::FTestRunner TestRunner;

	// Read command-line from file (if any). Some platforms do this earlier.
#ifndef PLATFORM_SKIP_ADDITIONAL_ARGS
	{
		int32 OverrideArgC = 0;
		const ANSICHAR** OverrideArgV = ReadAndAppendAdditionalArgs(GetProcessExecutablePath(), &OverrideArgC, ArgV, ArgC);
		if (OverrideArgV && OverrideArgC > 1)
		{
			ArgC = OverrideArgC;
			ArgV = OverrideArgV;
		}
	}
#endif

	TestRunner.ParseCommandLine(MakeArrayView(ArgV, ArgC));

	TestRunner.SleepOnInit();
	
	TestRunner.GlobalSetup();

	ON_SCOPE_EXIT
	{
		TestRunner.GlobalTeardown();
		TestRunner.Terminate();
		RequestEngineExit(TEXT("Exiting"));
		FModuleManager::Get().UnloadModulesAtShutdown();
	};

	return TestRunner.RunCatchSession();
}
