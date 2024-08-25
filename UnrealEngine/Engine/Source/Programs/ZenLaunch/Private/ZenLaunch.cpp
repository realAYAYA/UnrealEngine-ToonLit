// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenLaunch.h"

#include "Containers/UnrealString.h"
#include "Experimental/ZenServerInterface.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "String/LexFromString.h"

#include "RequiredProgramMainCPPInclude.h"

DEFINE_LOG_CATEGORY_STATIC(LogZenLaunch, Log, All);

IMPLEMENT_APPLICATION(ZenLaunch, "ZenLaunch");

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);
	ON_SCOPE_EXIT
	{ 
		LLM(FLowLevelMemTracker::Get().UpdateStatsPerFrame());
		RequestEngineExit(TEXT("Exiting"));
		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	};

	// Although standalone tools can set the project path via the cmdline this will not change the ProjectDir being
	// used as standalone tools have a beespoke path in FGenericPlatformMisc::ProjectDir. We can work around this
	// for now by doing our own parsing then using the project dir override feature.
	// This is a band aid while we consider adding better support for project files/directories with stand alone tools.
	if (ArgC >= 2)
	{
		FString Cmd(ArgV[1]);

		if (!Cmd.IsEmpty() && !Cmd.StartsWith(TEXT("-")) && Cmd.EndsWith(FProjectDescriptor::GetExtension()))
		{
			FString ProjectDir = FPaths::GetPath(Cmd);
			ProjectDir = FFileManagerGeneric::DefaultConvertToRelativePath(*ProjectDir);

			// The path should end with a trailing slash (see FGenericPlatformMisc::ProjectDir) so we use
			// NormalizeFilename not NormalizeDirectoryName as the latter will remove trailing slashes. We
			// also need to add one if it is missing.
			// We probably should move this path fixup code to 'FPlatformMisc::SetOverrideProjectDir'
			FPaths::NormalizeFilename(ProjectDir);
			if (!ProjectDir.EndsWith(TEXT("/")))
			{
				ProjectDir += TEXT("/");
			}

			FPlatformMisc::SetOverrideProjectDir(ProjectDir);
		}
	}

	if (int32 Ret = GEngineLoop.PreInit(ArgC, ArgV))
	{
		return Ret;
	}

	const TCHAR* CommandLine = FCommandLine::Get();
	TArray<uint32> SponsorProcessIDs;
	for (FString Token; FParse::Token(CommandLine, Token, /*UseEscape*/ false);)
	{
		TArray<FString> SponsorProcessIDStrings;
		Token.ReplaceInline(TEXT("\""), TEXT(""));
		const auto GetSwitchValues = [Token = FStringView(Token)](FStringView Match, TArray<FString>& OutValues)
		{
			if (Token.StartsWith(Match))
			{
				OutValues.Emplace(Token.RightChop(Match.Len()));
			}
		};

		GetSwitchValues(TEXT("-SponsorProcessID="), SponsorProcessIDStrings);

		for (const FString& SponsorProcessIDString : SponsorProcessIDStrings)
		{
			uint32 SponsorProcessID = 0;
			LexFromString(SponsorProcessID, SponsorProcessIDString);
			if (SponsorProcessID == 0)
			{
				UE_LOG(LogZenLaunch, Warning, TEXT("Skipping invalid sponsor process ID: %s"), *SponsorProcessIDString);
				continue;
			}

			FProcHandle SponsorProcess = FPlatformProcess::OpenProcess(SponsorProcessID);
			ON_SCOPE_EXIT
			{ 
				FPlatformProcess::CloseProc(SponsorProcess);
			};

			if (!SponsorProcess.IsValid() || !FPlatformProcess::IsProcRunning(SponsorProcess))
			{
				UE_LOG(LogZenLaunch, Warning, TEXT("Skipping sponsor process ID because the process is not accessible and running: %s"), *SponsorProcessIDString);
				continue;
			}

			SponsorProcessIDs.Add(SponsorProcessID);
		}
	}

	if (SponsorProcessIDs.IsEmpty())
	{
		UE_LOG(LogZenLaunch, Error, TEXT("No valid sponsor process IDs supplied on the commandline. "
			"Please supply process IDs that require ZenServer to launch and stay running via the -SponsorProcessID=X commandline argument."));
		return 1;
	}

	UE::Zen::FZenServiceInstance& ZenServiceInstance = UE::Zen::GetDefaultServiceInstance();
	const UE::Zen::FServiceSettings& ZenServiceSettings = ZenServiceInstance.GetServiceSettings();

	if (ZenServiceSettings.IsAutoLaunch() &&
		ZenServiceInstance.GetServiceSettings().SettingsVariant.Get<UE::Zen::FServiceAutoLaunchSettings>().bLimitProcessLifetime)
	{
		if (!ZenServiceInstance.AddSponsorProcessIDs(SponsorProcessIDs))
		{
			UE_LOG(LogZenLaunch, Error, TEXT("Failed to add sponsor process IDs to launched ZenServer."));
			return 1;
		}
	}

	return 0;
}
