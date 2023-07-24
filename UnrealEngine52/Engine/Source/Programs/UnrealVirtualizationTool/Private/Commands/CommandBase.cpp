// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommandBase.h"

#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SourceControlInitSettings.h"
#include "SourceControlOperations.h"
#include "UnrealVirtualizationTool.h"

namespace UE::Virtualization
{

/** Utility for testing if a file path resolves to a valid package file or not */
bool IsPackageFile(const FString FilePath)
{
	// ::IsPackageExtension requires a TCHAR so we cannot use FPathViews here
	const FString Extension = FPaths::GetExtension(FilePath);

	// Currently we don't virtualize text based assets so no call to FPackageName::IsTextPackageExtension
	return FPackageName::IsPackageExtension(*Extension);
}

FCommand::FCommand(FStringView InCommandName)
	: CommandName(InCommandName)
{

}

FCommand::~FCommand()
{

}

void FCommand::ParseCommandLine(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches)
{
	// TODO: Taken from UCommandlet, maybe consider moving this code to a general purpose utility. We 
	// could also make a version that returns TArray<FStringView>

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

FCommand::EPathResult FCommand::ParseSwitchForPaths(const FString& Switch, TArray<FString>& OutPackages)
{
	FString Path;
	if (FParse::Value(*Switch, TEXT("Package="), Path))
	{
		FPaths::NormalizeFilename(Path);

		if (!FPackageName::IsPackageFilename(Path))
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Requested package file '%s' is not a valid package filename"), *Path);
			return EPathResult::Error;
		}

		if (!IFileManager::Get().FileExists(*Path))
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Could not find the requested package file '%s'"), *Path);
			return EPathResult::Error;
		}

		OutPackages.Add(Path);
		return EPathResult::Success;
	}
	else if (FParse::Value(*Switch, TEXT("PackageDir="), Path) || FParse::Value(*Switch, TEXT("PackageFolder="), Path))
	{
		// Note that 'PackageFolder' is the switch used by the resave commandlet, so allowing it here for compatibility purposes
		FPaths::NormalizeFilename(Path);
		if (IFileManager::Get().DirectoryExists(*Path))
		{
			IFileManager::Get().IterateDirectoryRecursively(*Path, [&OutPackages](const TCHAR* Path, bool bIsDirectory)
				{
					if (!bIsDirectory && FPackageName::IsPackageFilename(Path))
					{
						FString FilePath(Path);
						FPaths::NormalizeFilename(FilePath);

						OutPackages.Add(MoveTemp(FilePath));
					}

					return true; // Continue
				});

			return EPathResult::Success;
		}
		else
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Could not find the requested directory '%s'"), *Path);
			return EPathResult::Error;
		}
	}

	return EPathResult::NotFound;
}

bool FCommand::TryConnectToSourceControl(FStringView ClientSpecName)
{
	if (SCCProvider.IsValid())
	{
		// Already connected so just return
		return true;
	}

	UE_LOG(LogVirtualizationTool, Log, TEXT("Trying to connect to source control..."));

	FSourceControlInitSettings SCCSettings(FSourceControlInitSettings::EBehavior::OverrideAll);
	SCCSettings.AddSetting(TEXT("P4Client"), ClientSpecName);

	SCCProvider = ISourceControlModule::Get().CreateProvider(FName("Perforce"), TEXT("UnrealVirtualizationTool"), SCCSettings);
	if (SCCProvider.IsValid())
	{
		SCCProvider->Init(true);
		return true;
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to create a perforce connection"));
		return false;
	}
}

bool FCommand::TryCheckOutFilesForProject(FStringView ClientSpecName, FStringView ProjectRoot, const TArray<FString>& ProjectPackages)
{
	// Override the root directory for source control to use the project for which we are trying to hydrate packages for
	ISourceControlModule::Get().RegisterSourceControlProjectDirDelegate(FSourceControlProjectDirDelegate::CreateLambda([&ProjectRoot]()
		{
			return *WriteToString<260>(ProjectRoot, TEXT("/"));
		}));

	ON_SCOPE_EXIT
	{
		ISourceControlModule::Get().UnregisterSourceControlProjectDirDelegate();
	};

	if (!TryConnectToSourceControl(ClientSpecName))
	{
		return false;
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("\t\tChecking status of package files..."));

	TArray<TSharedRef<ISourceControlState>> FileStates;
	if (SCCProvider->GetState(ProjectPackages, FileStates, EStateCacheUsage::ForceUpdate) != ECommandResult::Succeeded)
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find file states for packages from source control"));
		return false;
	}

	TArray<FString> FilesToCheckout;
	FilesToCheckout.Reserve(FileStates.Num());

	for (const TSharedRef<ISourceControlState>& State : FileStates)
	{
		if (State->CanCheckout())
		{
			FilesToCheckout.Add(State->GetFilename());
		}
	}

	if (!FilesToCheckout.IsEmpty())
	{
		UE_LOG(LogVirtualizationTool, Display, TEXT("\t\tChecking out %d file(s) from source control..."), FilesToCheckout.Num());

		if (SCCProvider->Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToCheckout, EConcurrency::Synchronous) != ECommandResult::Succeeded)
		{
			UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to checkout packages from source control"));
			return false;
		}
	}

	UE_LOG(LogVirtualizationTool, Display, TEXT("\t\tAll files checked out and writable"));

	return true;
}

bool FCommand::TryParseChangelist(FStringView ClientSpecName, FStringView ChangelistNumber, TArray<FString>& OutPackages, FSourceControlChangelistPtr* OutChangelist)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TryParseChangelist);

	UE_LOG(LogVirtualizationTool, Display, TEXT("\tAttempting to parse changelist '%.*s' in workspace '%.*s'"), 
		ChangelistNumber.Len(), ChangelistNumber.GetData(), 
		ClientSpecName.Len(), ClientSpecName.GetData());

	if (!TryConnectToSourceControl(ClientSpecName))
	{
		return false;
	}

	if (!SCCProvider.IsValid())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("No valid source control connection found!"));
		return false;
	}

	if (!SCCProvider->UsesChangelists())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("The source control provider does not support the use of changelists"));
		return false;
	}

	TArray<FSourceControlChangelistRef> Changelists = SCCProvider->GetChangelists(EStateCacheUsage::ForceUpdate);
	if (Changelists.IsEmpty())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find any changelists"));
		return false;
	}

	// TODO: At the moment we have to poll for all changelists under the current workspace and then iterate over
	// them to find the one we want.
	// We need to add better support in the API to go from a changelist number to a FSourceControlChangelistRef
	// directly.
	TArray<FSourceControlChangelistStateRef> ChangelistsStates;
	if (SCCProvider->GetState(Changelists, ChangelistsStates, EStateCacheUsage::Use) != ECommandResult::Succeeded)
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find changelist data"));
		return false;
	}

	for (FSourceControlChangelistStateRef& ChangelistState : ChangelistsStates)
	{
		const FText DisplayText = ChangelistState->GetDisplayText();

		if (ChangelistNumber == DisplayText.ToString())
		{
			TSharedRef<FUpdatePendingChangelistsStatus, ESPMode::ThreadSafe> Operation = ISourceControlOperation::Create<FUpdatePendingChangelistsStatus>();

			FSourceControlChangelistRef Changelist = ChangelistState->GetChangelist();
			Operation->SetChangelistsToUpdate(MakeArrayView(&Changelist, 1));
			Operation->SetUpdateFilesStates(true);

			if (SCCProvider->Execute(Operation, EConcurrency::Synchronous) != ECommandResult::Succeeded)
			{
				UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find the files in changelist '%.*s'"), ChangelistNumber.Len(), ChangelistNumber.GetData());
				return false;
			}

			const TArray<FSourceControlStateRef>& FilesinChangelist = ChangelistState->GetFilesStates();
			for (const FSourceControlStateRef& FileState : FilesinChangelist)
			{
				if (IsPackageFile(FileState->GetFilename()))
				{
					OutPackages.Add(FileState->GetFilename());
				}
				else
				{
					UE_LOG(LogVirtualizationTool, Log, TEXT("Ignoring non-package file '%s'"), *FileState->GetFilename());
				}
			}

			if (OutChangelist != nullptr)
			{
				*OutChangelist = Changelist;
			}

			return true;
		}
	}

	UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find the changelist '%.*s'"), ChangelistNumber.Len(), ChangelistNumber.GetData());

	return false;
}

FString FCommand::FindClientSpecForChangelist(FStringView ChangelistNumber)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FindClientSpecForChangelist);

	UE_LOG(LogVirtualizationTool, Display, TEXT("\tAttempting to find the workspace for '%.*s'"),
		ChangelistNumber.Len(), ChangelistNumber.GetData());

	if (!TryConnectToSourceControl(FStringView()))
	{
		return FString();
	}

	if (!SCCProvider.IsValid())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("No valid source control connection found!"));
		return FString();
	}

	if (!SCCProvider->UsesChangelists())
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("The source control provider does not support the use of changelists"));
		return FString();
	}

	TSharedRef<FGetChangelistDetails> Operation = ISourceControlOperation::Create<FGetChangelistDetails>(ChangelistNumber);

	if (SCCProvider->Execute(Operation, EConcurrency::Synchronous) == ECommandResult::Succeeded && !Operation->GetChangelistDetails().IsEmpty())
	{
		const FString* ClientSpec = Operation->GetChangelistDetails()[0].Find(TEXT("Client"));
		if (ClientSpec != nullptr)
		{
			return *ClientSpec;
		}
		else
		{ 
			UE_LOG(LogVirtualizationTool, Error, TEXT("Unable to find the 'client' field for the changelist '%.*s'"), ChangelistNumber.Len(), ChangelistNumber.GetData());
			return FString();
		}
	}
	else
	{
		UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find details for the changelist '%.*s'"), ChangelistNumber.Len(), ChangelistNumber.GetData());
		return FString();
	}
}

} //namespace UE::Virtualization 
