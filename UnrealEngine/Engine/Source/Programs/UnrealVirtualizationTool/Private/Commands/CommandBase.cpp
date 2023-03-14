// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommandBase.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
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

bool FCommand::TryParseChangelist(FStringView ClientSpecName, FStringView ChangelistNumber, TArray<FString>& OutPackages)
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

			// TODO: Updating only the CL we want does not currently work and even if it did we still end up with a pointless
			// p4 changes command before updating the files. Given we know the changelist number via FSourceControlChangelistRef
			// we should be able to just request the file states be updated.
			// This is also a lot of code to write for a simple "give me all files in a changelist" operation, if we don't add
			// support directly in the API we should move this to a utility namespace in the source control module.

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

			return true;
		}
	}

	UE_LOG(LogVirtualizationTool, Error, TEXT("Failed to find the changelist '%.*s'"), ChangelistNumber.Len(), ChangelistNumber.GetData());

	return false;
}



} //namespace UE::Virtualization 