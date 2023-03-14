// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commands/RehydrateCommand.h"

#include "HAL/FileManager.h"
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

// Taken from UCommandlet
static void ParseCommandLine(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	FString NextToken;
	while (FParse::Token(CmdLine, NextToken, false))
	{
		if (**NextToken == TCHAR('-'))
		{
			new(Switches) FString(NextToken.Mid(1));
		}
		else
		{
			new(Tokens) FString(NextToken);
		}
	}
}

FRehydrateCommand::FRehydrateCommand(FStringView CommandName)
	: FCommand(CommandName)
{

}

FRehydrateCommand::~FRehydrateCommand()
{

}

void FRehydrateCommand::PrintCmdLineHelp()
{
	UE_LOG(LogVirtualizationTool, Display, TEXT("UnrealVirtualizationTool -ClientSpecName=<name> [optional] -Mode=Rehydrate -Package=<string> [global options]"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("\tRehyrates the given package"));

	UE_LOG(LogVirtualizationTool, Display, TEXT("UnrealVirtualizationTool -ClientSpecName=<name> [optional] -Mode=Rehydrate -PackageDir=<string> [global options]"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("\tRehyrates all packages in the given directory and its subdirectories"));

	UE_LOG(LogVirtualizationTool, Display, TEXT("UnrealVirtualizationTool -ClientSpecName=<name> [optional] -Mode=Rehydrate -Changelist=<number> [global options]"));
	UE_LOG(LogVirtualizationTool, Display, TEXT("\tRehyrates all packages in the given changelist"));
}

bool FRehydrateCommand::Initialize(const TCHAR* CmdLine)
{
	// Note that we haven't loaded any projects config files and so don't really have
	// any valid project mount points so we cannot use FPackagePath or FPackageName
	// and expect to find anything!

	TArray<FString> Tokens;
	TArray<FString> Switches;

	ParseCommandLine(CmdLine, Tokens, Switches);

	Packages.Reserve(32);

	for (const FString& Switch :  Switches)
	{
		FString Path;
		FString ChangelistNumber;

		if (FParse::Value(*Switch, TEXT("ClientSpecName="), ClientSpecName))
		{
			UE_LOG(LogVirtualizationTool, Display, TEXT("\tWorkspace name provided '%s'"), *ClientSpecName);
		}
		else if (FParse::Value(*Switch, TEXT("Package="), Path))
		{
			FPaths::NormalizeFilename(Path);

			if (IFileManager::Get().FileExists(*Path))
			{
				Packages.Add(Path);
			}
			else
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("Could not find the requested package file '%s'"), *Path);
				return false;
			}
		}
		else if (FParse::Value(*Switch, TEXT("PackageDir="), Path) || FParse::Value(*Switch, TEXT("PackageFolder="), Path))
		{
			// Note that 'PackageFolder' is the switch used by the resave commandlet, so allowing it here for compatibility purposes
			FPaths::NormalizeFilename(Path);
			if (IFileManager::Get().DirectoryExists(*Path))
			{
				IFileManager::Get().IterateDirectoryRecursively(*Path, [this](const TCHAR* Path, bool bIsDirectory)
					{
						if (!bIsDirectory && FPackageName::IsPackageFilename(Path))
						{
							FString FilePath(Path);
							FPaths::NormalizeFilename(FilePath);

							this->Packages.Add(MoveTemp(FilePath));
						}

						return true; // Continue
					});

				return true;
			}
			else
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("Could not find the requested directory '%s'"), *Path);
				return false;
			}
		}
		else if (FParse::Value(*Switch, TEXT("Changelist="), ChangelistNumber))
		{
			return TryParseChangelist(ClientSpecName, ChangelistNumber, Packages);
		}
	}

	return true;
}

bool FRehydrateCommand::Run(const TArray<FProject>& Projects)
{
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

		TArray<FText> Errors;
		UE::Virtualization::IVirtualizationSystem::Get().TryRehydratePackages(ProjectPackages, Errors);

		if (!Errors.IsEmpty())
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("The rehydration process failed with the following errors:"));
			for (const FText& Error : Errors)
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("\t%s"), *Error.ToString());
			}
			return false;
		}

		Project.UnRegisterMountPoints();

		UE_LOG(LogVirtualizationTool, Display, TEXT("\tRehyration of project packages complete"), ProjectName.ToString());
	}

	return true;
}

} // namespace UE::Virtualization
