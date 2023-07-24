// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/RehydrateCommand.h"

#include "ISourceControlProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "ProjectFiles.h"
#include "UnrealVirtualizationTool.h"
#include "Virtualization/VirtualizationSystem.h"

namespace UE::Virtualization
{

FRehydrateCommand::FRehydrateCommand(FStringView CommandName)
	: FCommand(CommandName)
{

}

void FRehydrateCommand::PrintCmdLineHelp()
{
	UE_LOG(LogVirtualizationTool, Display, TEXT(""));
	UE_LOG(LogVirtualizationTool, Display, TEXT("-Mode=Rehydrate -Package=<string>"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("-Mode=Rehydrate -PackageDir=<string>"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("-Mode=Rehydrate -Changelist=<number>"));
}

bool FRehydrateCommand::Initialize(const TCHAR* CmdLine)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRehydrateCommand::Initialize);

	// Note that we haven't loaded any projects config files and so don't really have
	// any valid project mount points so we cannot use FPackagePath or FPackageName
	// and expect to find anything!

	TArray<FString> Tokens;
	TArray<FString> Switches;

	ParseCommandLine(CmdLine, Tokens, Switches);
	
	FString SwitchValue;
	for (const FString& Switch :  Switches)
	{
		EPathResult Result = ParseSwitchForPaths(Switch, Packages);
		if (Result == EPathResult::Error)
		{
			return false;
		}
		else if (Result == EPathResult::Success)
		{
			continue; // If we already matched the switch we don't need to check against any others
		}

		if (FParse::Value(*Switch, TEXT("ClientSpecName="), SwitchValue))
		{
			ClientSpecName = SwitchValue;
			UE_LOG(LogVirtualizationTool, Display, TEXT("\tWorkspace name provided '%s'"), *ClientSpecName);
		}
		else if (FParse::Value(*Switch, TEXT("Changelist="), SwitchValue))
		{
			return TryParseChangelist(ClientSpecName, SwitchValue, Packages, nullptr);
		}
		else if (Switch == TEXT("Checkout"))
		{
			bShouldCheckout = true;
		}
	}

	return true;
}

bool FRehydrateCommand::Run(const TArray<FProject>& Projects)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRehydrateCommand::Run);

	for (const FProject& Project : Projects)
	{
		TStringBuilder<128> ProjectName;
		ProjectName << Project.GetProjectName();

		UE_LOG(LogVirtualizationTool, Display, TEXT("\tProcessing package(s) for the project '%s'..."), ProjectName.ToString());

		FConfigFile EngineConfigWithProject;
		if (!Project.TryLoadConfig(EngineConfigWithProject))
		{
			return false;
		}

		Project.RegisterMountPoints();

		UE::Virtualization::FInitParams InitParams(ProjectName, EngineConfigWithProject);
		UE::Virtualization::Initialize(InitParams, UE::Virtualization::EInitializationFlags::ForceInitialize);

		ON_SCOPE_EXIT
		{
			UE::Virtualization::Shutdown();
		};

		const TArray<FString> ProjectPackages = Project.GetAllPackages();

		if (!TryCheckOutFilesForProject(ClientSpecName, Project.GetProjectRoot(), ProjectPackages))
		{
			return false;
		}

		UE_LOG(LogVirtualizationTool, Display, TEXT("\t\tAttempting to rehydrate packages..."), ProjectName.ToString());


		ERehydrationOptions Options = ERehydrationOptions::None;
		if (bShouldCheckout)
		{
			Options |= ERehydrationOptions::Checkout;
		}

		FRehydrationResult Result = UE::Virtualization::IVirtualizationSystem::Get().TryRehydratePackages(ProjectPackages, Options);
		if (!Result.WasSuccessful())
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("The rehydration process failed with the following errors:"));
			for (const FText& Error : Result.Errors)
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("\t%s"), *Error.ToString());
			}
			return false;
		}

		if (bShouldCheckout)
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("\t\t%d packages were checked out of revision control"), Result.CheckedOutPackages.Num());
		}

		Project.UnRegisterMountPoints();

		UE_LOG(LogVirtualizationTool, Display, TEXT("\t\tRehyration of project packages complete!"), ProjectName.ToString());
	}

	return true;
}

const TArray<FString>& FRehydrateCommand::GetPackages() const
{
	return Packages;
}

} // namespace UE::Virtualization
