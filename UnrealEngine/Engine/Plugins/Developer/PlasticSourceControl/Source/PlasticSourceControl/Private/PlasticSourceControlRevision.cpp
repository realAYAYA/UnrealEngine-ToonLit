// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlasticSourceControlRevision.h"
#include "PlasticSourceControlModule.h"
#include "PlasticSourceControlProvider.h"
#include "PlasticSourceControlState.h"
#include "PlasticSourceControlUtils.h"
#include "SPlasticSourceControlSettings.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ISourceControlModule.h"

#define LOCTEXT_NAMESPACE "PlasticSourceControl"

bool FPlasticSourceControlRevision::Get(FString& InOutFilename, EConcurrency::Type InConcurrency) const
{
	if (InConcurrency != EConcurrency::Synchronous)
	{
		UE_LOG(LogSourceControl, Warning, TEXT("Only EConcurrency::Synchronous is tested/supported for this operation."));
	}

	// if a filename for the temp file wasn't supplied generate a unique-ish one
	if (InOutFilename.Len() == 0)
	{
		// create the diff dir if we don't already have it (Plastic wont)
		IFileManager::Get().MakeDirectory(*FPaths::DiffDir(), true);
		// create a unique temp file name based on the unique revision Id
		const FString TempFileName = FString::Printf(TEXT("%stemp-%d-%s"), *FPaths::DiffDir(), ChangesetNumber, *FPaths::GetCleanFilename(Filename));
		InOutFilename = FPaths::ConvertRelativePathToFull(TempFileName);
	}

	bool bCommandSuccessful;
	if (FPaths::FileExists(InOutFilename))
	{
		bCommandSuccessful = true; // if the temp file already exists, reuse it directly
	}
	else if (State)
	{
		const FString& PathToPlasticBinary = FPlasticSourceControlModule::Get().GetProvider().AccessSettings().GetBinaryPath();

		// Format the revision specification of the file, like rev:Content/BP.uasset#cs:12@repo@server:8087
		const FString RevisionSpecification = FString::Printf(TEXT("rev:%s#cs:%d@%s"), *Filename, ChangesetNumber, *State->RepSpec);
		bCommandSuccessful = PlasticSourceControlUtils::RunDumpToFile(PathToPlasticBinary, RevisionSpecification, InOutFilename);
	}
	else
	{
		UE_LOG(LogSourceControl, Error, TEXT("Revision(%s %d): unknown state!"), *Filename, RevisionId);
		bCommandSuccessful = false;
	}
	return bCommandSuccessful;
}

bool FPlasticSourceControlRevision::GetAnnotated( TArray<FAnnotationLine>& OutLines ) const
{
	return false;
}

bool FPlasticSourceControlRevision::GetAnnotated( FString& InOutFilename ) const
{
	return false;
}

const FString& FPlasticSourceControlRevision::GetFilename() const
{
	return Filename;
}

int32 FPlasticSourceControlRevision::GetRevisionNumber() const
{
	return ChangesetNumber; // Using the Changelist as the Revision number to display in the Asset Diff Menu
}

const FString& FPlasticSourceControlRevision::GetRevision() const
{
	return Revision;
}

const FString& FPlasticSourceControlRevision::GetDescription() const
{
	return Description;
}

const FString& FPlasticSourceControlRevision::GetUserName() const
{
	return UserName;
}

const FString& FPlasticSourceControlRevision::GetClientSpec() const
{
	// Note: show Branch instead of the Workspace of the submitter since it's Perforce only
	return Branch;
}

const FString& FPlasticSourceControlRevision::GetAction() const
{
	return Action;
}

TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> FPlasticSourceControlRevision::GetBranchSource() const
{
	// if this revision was copied/moved from some other revision
	return BranchSource;
}

const FDateTime& FPlasticSourceControlRevision::GetDate() const
{
	return Date;
}

int32 FPlasticSourceControlRevision::GetCheckInIdentifier() const
{
	return ChangesetNumber;
}

int32 FPlasticSourceControlRevision::GetFileSize() const
{
	return FileSize;
}

#undef LOCTEXT_NAMESPACE
