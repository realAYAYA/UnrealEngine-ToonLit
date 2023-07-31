// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertSandboxPlatformFile.h"
#include "ConcertSyncClientUtil.h"
#include "ConcertLogGlobal.h"

#include "Algo/Transform.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFileManager.h"
#include "Modules/ModuleManager.h"

#include "ISourceControlState.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"

#if WITH_EDITOR
	#include "IDirectoryWatcher.h"
	#include "DirectoryWatcherModule.h"
#endif

namespace ConcertSandboxPlatformFileUtil
{

FString GetContentFolderName(const FString& InContentPath)
{
	check(InContentPath.Len() > 0);

	// Clean any trailing slash from the content path
	FString ContentFolderName = InContentPath;
	if (ContentFolderName[ContentFolderName.Len() - 1] == TEXT('/') || ContentFolderName[ContentFolderName.Len() - 1] == TEXT('\\'))
	{
		ContentFolderName.RemoveAt(ContentFolderName.Len() - 1, 1, /*bAllowShrinking*/false);
	}

	// Content paths are always in the form /Bla/Content, so we need to use GetBaseFilename after calling GetPath to get the 'Bla' part for the sandbox path
	ContentFolderName = FPaths::GetPath(MoveTemp(ContentFolderName));
	ContentFolderName = FPaths::GetCleanFilename(MoveTemp(ContentFolderName));

	return ContentFolderName;
}

bool FlushPackageFile(const FString& InFilename, FName* OutPackageName = nullptr, bool bForceLoad = true)
{
	FString PackageName;
	if (FPackageName::TryConvertFilenameToLongPackageName(InFilename, PackageName))
	{
		ConcertSyncClientUtil::FlushPackageLoading(PackageName, bForceLoad);
		if (OutPackageName)
		{
			*OutPackageName = *PackageName;
		}
		return true;
	}
	return false;
}

}

FConcertSandboxPlatformFile::FConcertSandboxPlatformFile(const FString& InSandboxRootPath)
	: SandboxRootPath(InSandboxRootPath)
	, LowerLevel(nullptr)
	, bSandboxEnabled(false)
{
}

FConcertSandboxPlatformFile::~FConcertSandboxPlatformFile()
{
	if (LowerLevel && this == &FPlatformFileManager::Get().GetPlatformFile())
	{
		FPlatformFileManager::Get().SetPlatformFile(*LowerLevel);
	}

	FPackageName::OnContentPathMounted().RemoveAll(this);
	FPackageName::OnContentPathDismounted().RemoveAll(this);

#if WITH_EDITOR
	if (IDirectoryWatcher* DirectoryWatcher = ConcertSyncClientUtil::GetDirectoryWatcherIfLoaded())
	{
		for (FSandboxMountPoint& SandboxMountPoint : SandboxMountPoints)
		{
			if (SandboxMountPoint.OnDirectoryChangedHandle.IsValid())
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(SandboxMountPoint.Path.GetSandboxPath(), SandboxMountPoint.OnDirectoryChangedHandle);
				SandboxMountPoint.OnDirectoryChangedHandle.Reset();
			}
		}
	}
#endif

	if (LowerLevel)
	{
		// Wipe the sandbox directory
		LowerLevel->DeleteDirectoryRecursively(*SandboxRootPath);	
	}
}

void FConcertSandboxPlatformFile::SetSandboxEnabled(bool bInEnabled)
{
	bSandboxEnabled = bInEnabled;
}

bool FConcertSandboxPlatformFile::IsSandboxEnabled() const
{
	return bSandboxEnabled;
}

bool FConcertSandboxPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	SetLowerLevel(Inner);

	// Wipe the sandbox directory
	LowerLevel->DeleteDirectoryRecursively(*SandboxRootPath);
	LowerLevel->CreateDirectoryTree(*SandboxRootPath);

	// Set-up the initial set of content mount paths
	TArray<FString> RootPaths;
	FPackageName::QueryRootContentPaths(RootPaths);
	for (const FString& RootPath : RootPaths)
	{
		RegisterContentMountPath(FPackageName::LongPackageNameToFilename(RootPath));
	}

	// Watch for new content mount paths
	FPackageName::OnContentPathMounted().AddRaw(this, &FConcertSandboxPlatformFile::OnContentPathMounted);
	FPackageName::OnContentPathDismounted().AddRaw(this, &FConcertSandboxPlatformFile::OnContentPathDismounted);

	bSandboxEnabled = true;
	FPlatformFileManager::Get().SetPlatformFile(*this);

	return true;
}

void FConcertSandboxPlatformFile::Tick()
{
}

IPlatformFile* FConcertSandboxPlatformFile::GetLowerLevel()
{
	return LowerLevel;
}

void FConcertSandboxPlatformFile::SetLowerLevel(IPlatformFile* NewLowerLevel)
{
	check(NewLowerLevel && NewLowerLevel != this);
	LowerLevel = NewLowerLevel;
}

const TCHAR* FConcertSandboxPlatformFile::GetName() const
{
	return GetTypeName();
}

bool FConcertSandboxPlatformFile::DeletedPackageExistsInNonSandbox(FString InFilename) const
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(MoveTemp(InFilename));
	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return LowerLevel->FileExists(*ResolvedPath.GetNonSandboxPath());
		}
	}
	return false;
}

bool FConcertSandboxPlatformFile::FileExists(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return false;
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return true;
		}
	}

	return LowerLevel->FileExists(*ResolvedPath.GetNonSandboxPath());
}

int64 FConcertSandboxPlatformFile::FileSize(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return -1;
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->FileSize(*ResolvedPath.GetSandboxPath());
		}
	}

	return LowerLevel->FileSize(*ResolvedPath.GetNonSandboxPath());
}

bool FConcertSandboxPlatformFile::DeleteFile(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return false;
		}

		if (!LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()) || LowerLevel->DeleteFile(*ResolvedPath.GetSandboxPath()))
		{
			SetPathDeleted(ResolvedPath, true);
			NotifyFileDeleted(ResolvedPath);
			return true;
		}

		return false; // Do not attempt to delete the non-sandbox file
	}

	return LowerLevel->DeleteFile(*ResolvedPath.GetNonSandboxPath());
}

bool FConcertSandboxPlatformFile::IsReadOnly(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return false;
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->IsReadOnly(*ResolvedPath.GetSandboxPath());
		}

		return false; // Can always overwrite missing sandbox files
	}

	return LowerLevel->IsReadOnly(*ResolvedPath.GetNonSandboxPath());
}

bool FConcertSandboxPlatformFile::SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return false;
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			if (bNewReadOnlyValue)
			{
				return true; // Do not allow sandbox files to be made read-only, but don't report failure
			}

			return LowerLevel->SetReadOnly(*ResolvedPath.GetSandboxPath(), bNewReadOnlyValue);
		}

		return false; // Do not attempt to modify the non-sandbox file
	}

	return LowerLevel->SetReadOnly(*ResolvedPath.GetNonSandboxPath(), bNewReadOnlyValue);
}

bool FConcertSandboxPlatformFile::MoveFile(const TCHAR* To, const TCHAR* From)
{
	const FConcertSandboxPlatformFilePath ResolvedToPath = ToSandboxPath(To);
	const FConcertSandboxPlatformFilePath ResolvedFromPath = ToSandboxPath(From);

	if (ResolvedToPath.HasSandboxPath())
	{
		// Migrate any existing target file from outside the sandbox so the lower-level will fail to overwrite the existing file
		MigrateFileToSandbox(ResolvedToPath);

		if (ResolvedFromPath.HasSandboxPath())
		{
			// Sandbox -> Sandbox
			if (IsPathDeleted(ResolvedFromPath))
			{
				// Cannot move a deleted file
				return false;
			}

			bool bSuccess = false;
			if (LowerLevel->FileExists(*ResolvedFromPath.GetSandboxPath()))
			{
				// Moving an internal sandbox file - can move
				bSuccess = LowerLevel->MoveFile(*ResolvedToPath.GetSandboxPath(), *ResolvedFromPath.GetSandboxPath());
			}
			else
			{
				// Moving an external sandbox file - must copy
				bSuccess = LowerLevel->CopyFile(*ResolvedToPath.GetSandboxPath(), *ResolvedFromPath.GetNonSandboxPath());
			}

			if (bSuccess)
			{
				SetPathDeleted(ResolvedToPath, false);
				SetPathDeleted(ResolvedFromPath, true);
				NotifyFileDeleted(ResolvedFromPath);
			}

			return bSuccess;
		}
		else
		{
			// Non-sandbox -> Sandbox
			if (LowerLevel->MoveFile(*ResolvedToPath.GetSandboxPath(), *ResolvedFromPath.GetNonSandboxPath()))
			{
				SetPathDeleted(ResolvedToPath, false);
				return true;
			}
			return false;
		}
	}
	else if (ResolvedFromPath.HasSandboxPath())
	{
		// Sandbox -> Non-sandbox
		if (IsPathDeleted(ResolvedFromPath))
		{
			// Cannot move a deleted file
			return false;
		}

		bool bSuccess = false;
		if (LowerLevel->FileExists(*ResolvedFromPath.GetSandboxPath()))
		{
			// Moving an internal sandbox file - can move
			bSuccess = LowerLevel->MoveFile(*ResolvedToPath.GetNonSandboxPath(), *ResolvedFromPath.GetSandboxPath());
		}
		else
		{
			// Moving an external sandbox file - must copy
			bSuccess = LowerLevel->CopyFile(*ResolvedToPath.GetNonSandboxPath(), *ResolvedFromPath.GetNonSandboxPath());
		}

		if (bSuccess)
		{
			SetPathDeleted(ResolvedFromPath, true);
			NotifyFileDeleted(ResolvedFromPath);
		}

		return bSuccess;
	}

	// Non-sandbox -> Non-sandbox
	return LowerLevel->MoveFile(*ResolvedToPath.GetNonSandboxPath(), *ResolvedFromPath.GetNonSandboxPath());
}

FDateTime FConcertSandboxPlatformFile::GetTimeStamp(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return FDateTime::MinValue();
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->GetTimeStamp(*ResolvedPath.GetSandboxPath());
		}
	}

	return LowerLevel->GetTimeStamp(*ResolvedPath.GetNonSandboxPath());
}

void FConcertSandboxPlatformFile::SetTimeStamp(const TCHAR* Filename, FDateTime DateTime)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return;
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->SetTimeStamp(*ResolvedPath.GetSandboxPath(), DateTime);
		}

		return; // Do not attempt to modify the non-sandbox file
	}

	return LowerLevel->SetTimeStamp(*ResolvedPath.GetNonSandboxPath(), DateTime);
}

FDateTime FConcertSandboxPlatformFile::GetAccessTimeStamp(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return FDateTime::MinValue();
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->GetAccessTimeStamp(*ResolvedPath.GetSandboxPath());
		}
	}

	return LowerLevel->GetAccessTimeStamp(*ResolvedPath.GetNonSandboxPath());
}

FString FConcertSandboxPlatformFile::GetFilenameOnDisk(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath) || LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->GetFilenameOnDisk(*ResolvedPath.GetSandboxPath());
		}
	}

	return LowerLevel->GetFilenameOnDisk(*ResolvedPath.GetNonSandboxPath());
}

IFileHandle* FConcertSandboxPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return nullptr;
		}

		if (bAllowWrite)
		{
			MigrateFileToSandbox(ResolvedPath);
		}

		if (bAllowWrite || LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->OpenRead(*ResolvedPath.GetSandboxPath(), bAllowWrite);
		}
	}

	return LowerLevel->OpenRead(*ResolvedPath.GetNonSandboxPath(), bAllowWrite);
}

IFileHandle* FConcertSandboxPlatformFile::OpenReadNoBuffering(const TCHAR* Filename, bool bAllowWrite)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return nullptr;
		}

		if (bAllowWrite)
		{
			MigrateFileToSandbox(ResolvedPath);
		}

		if (bAllowWrite || LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->OpenReadNoBuffering(*ResolvedPath.GetSandboxPath(), bAllowWrite);
		}
	}

	return LowerLevel->OpenReadNoBuffering(*ResolvedPath.GetNonSandboxPath(), bAllowWrite);
}

IFileHandle* FConcertSandboxPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		MigrateFileToSandbox(ResolvedPath);

		IFileHandle* Handle = LowerLevel->OpenWrite(*ResolvedPath.GetSandboxPath(), bAppend, bAllowRead);
		if (Handle)
		{
			SetPathDeleted(ResolvedPath, false);
		}
		return Handle;
	}

	return LowerLevel->OpenWrite(*ResolvedPath.GetNonSandboxPath(), bAppend, bAllowRead);
}

bool FConcertSandboxPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Directory);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return false;
		}

		if (LowerLevel->DirectoryExists(*ResolvedPath.GetSandboxPath()))
		{
			return true;
		}
	}

	return LowerLevel->DirectoryExists(*ResolvedPath.GetNonSandboxPath());
}

bool FConcertSandboxPlatformFile::CreateDirectory(const TCHAR* Directory)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Directory);

	if (ResolvedPath.HasSandboxPath())
	{
		if (LowerLevel->CreateDirectory(*ResolvedPath.GetSandboxPath()))
		{
			SetPathDeleted(ResolvedPath, false);
			return true;
		}

		return false; // Do not attempt to create the non-sandbox directory
	}

	return LowerLevel->CreateDirectory(*ResolvedPath.GetNonSandboxPath());
}

bool FConcertSandboxPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Directory);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return true;
		}

		// Iterate the directory to validate that it is really empty before deleting it
		const TArray<FDirectoryItem> DirectoryItems = GetDirectoryContents(ResolvedPath, Directory);
		if (DirectoryItems.Num() == 0 && (!LowerLevel->DirectoryExists(*ResolvedPath.GetSandboxPath()) || LowerLevel->DeleteDirectory(*ResolvedPath.GetSandboxPath())))
		{
			SetPathDeleted(ResolvedPath, true);
			return true;
		}

		return false; // Do not attempt to create the non-sandbox directory
	}

	return LowerLevel->DeleteDirectory(*ResolvedPath.GetNonSandboxPath());
}

FFileStatData FConcertSandboxPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(FilenameOrDirectory);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return FFileStatData();
		}

		if (LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->GetStatData(*ResolvedPath.GetSandboxPath());
		}

		FFileStatData StatData = LowerLevel->GetStatData(*ResolvedPath.GetNonSandboxPath());
		// Sandbox files are always writeable.
		StatData.bIsReadOnly = false;
		return StatData;
	}

	return LowerLevel->GetStatData(*ResolvedPath.GetNonSandboxPath());
}

bool FConcertSandboxPlatformFile::IterateDirectory(const TCHAR* Directory, FDirectoryVisitor& Visitor)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Directory);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return false;
		}

		const TArray<FDirectoryItem> DirectoryItems = GetDirectoryContents(ResolvedPath, Directory);
		for (const FDirectoryItem& DirectoryItem : DirectoryItems)
		{
			if (!Visitor.Visit(*DirectoryItem.Path, DirectoryItem.StatData.bIsDirectory))
			{
				return false;
			}
		}

		return true;
	}

	return LowerLevel->IterateDirectory(Directory, Visitor); // Note: Using the path we were given here to ensure the calling code gets the expected path
}

bool FConcertSandboxPlatformFile::IterateDirectoryStat(const TCHAR* Directory, FDirectoryStatVisitor& Visitor)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Directory);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath))
		{
			return false;
		}

		const TArray<FDirectoryItem> DirectoryItems = GetDirectoryContents(ResolvedPath, Directory);
		for (const FDirectoryItem& DirectoryItem : DirectoryItems)
		{
			if (!Visitor.Visit(*DirectoryItem.Path, DirectoryItem.StatData))
			{
				return false;
			}
		}

		return true;
	}

	return LowerLevel->IterateDirectoryStat(Directory, Visitor); // Note: Using the path we were given here to ensure the calling code gets the expected path
}

IAsyncReadFileHandle* FConcertSandboxPlatformFile::OpenAsyncRead(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath) || LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->OpenAsyncRead(*ResolvedPath.GetSandboxPath());
		}
	}

	return LowerLevel->OpenAsyncRead(*ResolvedPath.GetNonSandboxPath());
}

void FConcertSandboxPlatformFile::SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags MinPriority)
{
	LowerLevel->SetAsyncMinimumPriority(MinPriority);
}

FString FConcertSandboxPlatformFile::ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		if (IsPathDeleted(ResolvedPath) || LowerLevel->FileExists(*ResolvedPath.GetSandboxPath()))
		{
			return LowerLevel->ConvertToAbsolutePathForExternalAppForRead(*ResolvedPath.GetSandboxPath());
		}
	}

	return LowerLevel->ConvertToAbsolutePathForExternalAppForRead(*ResolvedPath.GetNonSandboxPath());
}

FString FConcertSandboxPlatformFile::ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename)
{
	const FConcertSandboxPlatformFilePath ResolvedPath = ToSandboxPath(Filename);

	if (ResolvedPath.HasSandboxPath())
	{
		return LowerLevel->ConvertToAbsolutePathForExternalAppForWrite(*ResolvedPath.GetSandboxPath());
	}

	return LowerLevel->ConvertToAbsolutePathForExternalAppForWrite(*ResolvedPath.GetNonSandboxPath());
}


bool FConcertSandboxPlatformFile::CopyFileWithSCC(
	const TCHAR* InToFilename,
	const TCHAR* InFromFilename,
	const FPersistParameters& InParam,
	FPersistResult& OutResult)
{
	// If this file maps to a package then we need to flush its linker so that we can overwrite the file on disk
	ConcertSandboxPlatformFileUtil::FlushPackageFile(InToFilename);

	ISourceControlProvider* SourceControlProvider = InParam.SourceControlProvider;
	// Get the source control state of the destination file
	FSourceControlStatePtr ToFileSCCState;
	if (SourceControlProvider && SourceControlProvider->IsEnabled())
	{
		ToFileSCCState = SourceControlProvider->GetState(InToFilename, EStateCacheUsage::ForceUpdate);
	}

	// We don't need to do anything with source control if the file is already checked-out or added
	const bool bRequiresSCCAction = ToFileSCCState && !ToFileSCCState->IsCheckedOut() && !ToFileSCCState->IsAdded();

	// If the file can be checked-out, do so now
	if (bRequiresSCCAction && ToFileSCCState->IsSourceControlled())
	{
		// the static analysis tool is not able to see that `SourceControlProvider` won't be null if `bRequiresSCCAction` is true
		CA_ASSUME(SourceControlProvider != nullptr);
		if (ToFileSCCState->CanCheckout() && SourceControlProvider->UsesCheckout())
		{
			TArray<FString> FilesToBeCheckedOut;
			FilesToBeCheckedOut.Add(InToFilename);

			if (SourceControlProvider->Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) != ECommandResult::Succeeded)
			{
				FText Failure = FText::Format(
					NSLOCTEXT("Concert", "CheckoutFileFailure", "Failed to check-out file '{0}' from source control when persiting sandbox state!"),
					FText::FromString(InToFilename));
				UE_LOG(LogConcert, Warning, TEXT("%s"),  *Failure.ToString());
				OutResult.FailureReasons.Add(MoveTemp(Failure));
				return false;
			}
		}
		else
		{
			FText Failure = FText::Format(
				NSLOCTEXT("Concert", "CanCheckoutFileFailure", "Can't check-out file '{0}' from source control when persiting sandbox state!"),
				FText::FromString(InToFilename));
			UE_LOG(LogConcert, Warning, TEXT("%s"), *Failure.ToString());
			OutResult.FailureReasons.Add(MoveTemp(Failure));
			return false;
		}
	}
	else if (InParam.bShouldMakeWritableIfNoSourceControl)
	{
		if (LowerLevel->FileExists(InToFilename) && !LowerLevel->SetReadOnly(InToFilename, false))
		{
			FText Failure = FText::Format(
				NSLOCTEXT("Concert", "CanMakeWritable", "Can't make file '{0}' writable when persiting sandbox state!"),
				FText::FromString(InToFilename));
			UE_LOG(LogConcert, Warning, TEXT("%s"), *Failure.ToString());
			OutResult.FailureReasons.Add(MoveTemp(Failure));
			return false;
		}
	}

	// Copy the on-disk sandbox file
	FString ToFileDir = FPaths::GetPath(InToFilename);
	if (!CreateDirectoryTree(*ToFileDir) || !LowerLevel->CopyFile(InToFilename, InFromFilename))
	{
		FText Failure = FText::Format(
			NSLOCTEXT("Concert", "CopyFileFailure", "Failed to copy file '{0}' (from '{1}') when persiting sandbox state!"),
			FText::FromString(InToFilename), FText::FromString(InFromFilename));
		UE_LOG(LogConcert, Warning, TEXT("%s"), *Failure.ToString());
		OutResult.FailureReasons.Add(MoveTemp(Failure));
		return false;
	}

		// If the file is new, add it to source control now
	if (bRequiresSCCAction && !ToFileSCCState->IsSourceControlled())
	{
		TArray<FString> FilesToBeAdded;
		FilesToBeAdded.Add(InToFilename);

		if (SourceControlProvider->Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeAdded) != ECommandResult::Succeeded)
		{
			FText Failure = FText::Format(
				NSLOCTEXT("Concert", "AddFileFailure", "Failed to add file '{0}' to source control when persiting sandbox state!"),
				FText::FromString(InToFilename));
			UE_LOG(LogConcert, Warning, TEXT("%s"), *Failure.ToString());
			OutResult.FailureReasons.Add(MoveTemp(Failure));
			return false;
		}
	}
	return true;
}

/** Delete file using source control */
bool FConcertSandboxPlatformFile::DeleteFileWithSCC(const TCHAR* InFilename, const FPersistParameters& InParam, FPersistResult& OutResult)
{
	// If this file maps to a package then we need to flush its linker so that we can remove the file from disk
	ConcertSandboxPlatformFileUtil::FlushPackageFile(InFilename);

	ISourceControlProvider* SourceControlProvider = InParam.SourceControlProvider;
	// Get the source control state of the file
	FSourceControlStatePtr FileSCCState;
	if (SourceControlProvider && SourceControlProvider->IsEnabled())
	{
		FileSCCState = SourceControlProvider->GetState(InFilename, EStateCacheUsage::ForceUpdate);
	}

	// Try and let source control remove the file first
	if (FileSCCState && FileSCCState->IsSourceControlled())
	{
		const bool bAdded = FileSCCState->IsAdded();

		if (bAdded || FileSCCState->IsCheckedOut())
		{
			if (SourceControlProvider->Execute(ISourceControlOperation::Create<FRevert>(), InFilename) != ECommandResult::Succeeded)
			{
				FText Failure = FText::Format(NSLOCTEXT("Concert", "RevertFileFailure", "Failed to revert file '{0}' to source control when persisting sandbox state!"), FText::FromString(InFilename));
				UE_LOG(LogConcert, Warning, TEXT("%s"), *Failure.ToString());
				OutResult.FailureReasons.Add(MoveTemp(Failure));
				return false;
			}

			if (!bAdded)
			{
				if (SourceControlProvider->Execute(ISourceControlOperation::Create<FDelete>(), InFilename) != ECommandResult::Succeeded)
				{
					FText Failure = FText::Format(NSLOCTEXT("Concert", "DeleteSCCFileFailure", "Failed to delete file '{0}' from source control when persiting sandbox state!"), FText::FromString(InFilename));
					UE_LOG(LogConcert, Warning, TEXT("%s"), *Failure.ToString());
					OutResult.FailureReasons.Add(MoveTemp(Failure));
					return false;
				}
			}
		}
	}

	// Delete file if it still exists
	if (LowerLevel->FileExists(InFilename) && !LowerLevel->DeleteFile(InFilename))
	{
		FText Failure = FText::Format(NSLOCTEXT("Concert", "DeleteFileFailure", "Failed to delete file '{0}' when persiting sandbox state!"), FText::FromString(InFilename));
		UE_LOG(LogConcert, Warning, TEXT("%s"), *Failure.ToString());
		OutResult.FailureReasons.Add(MoveTemp(Failure));
		return false;
	}
	return true;
}

FPersistResult FConcertSandboxPlatformFile::PersistSandbox(TArrayView<const FString> InFiles, FPersistParameters InParams)
{
	FPersistResult OutResult;
	// We need to disable the sandbox while we do this
	bool bFullOperationSuccess = true;
	TGuardValue<TAtomic<bool>, bool> DisableSandboxGuard(bSandboxEnabled, false);
	for (const FString& File : InFiles)
	{
		FConcertSandboxPlatformFilePath FilePath = ToSandboxPath(File, true);
		bool bSuccess = IsPathDeleted(FilePath) ?
			DeleteFileWithSCC(*FilePath.GetNonSandboxPath(), InParams, OutResult) :
			CopyFileWithSCC(*FilePath.GetNonSandboxPath(), *FilePath.GetSandboxPath(), InParams, OutResult);

		// if the file operation was successful, mark the file as persisted
		if (bSuccess)
		{
			PersistedFiles.Add(File, LowerLevel->GetTimeStamp(*FilePath.GetSandboxPath()));
		}

		bFullOperationSuccess &= bSuccess;
	}
	OutResult.PersistStatus = bFullOperationSuccess ? EPersistStatus::Success : EPersistStatus::Failure;
	return OutResult;
}

void FConcertSandboxPlatformFile::DiscardSandbox(TArray<FName>& OutPackagesPendingHotReload, TArray<FName>& OutPackagesPendingPurge)
{
	// We need to disable the sandbox while we do this
	TGuardValue<TAtomic<bool>, bool> DisableSandboxGuard(bSandboxEnabled, false);

#if WITH_EDITOR
	TArray<FFileChangeData> FileChanges;
#else
	TArray<FString> FileChanges; // Dummy array for the lambda capture below
#endif

	// Add any files that were deleted by the sandbox but exist in the non-sandbox directory
	{
		FScopeLock DeletedSandboxPathsLock(&DeletedSandboxPathsCS);
		for (auto It = DeletedSandboxPaths.CreateIterator(); It; ++It)
		{
			// If this file maps to a package then we need to flush its linker so that we can remove the file from the sandbox
			FName PackageName;
			ConcertSandboxPlatformFileUtil::FlushPackageFile(It->GetNonSandboxPath(), &PackageName);

			if (LowerLevel->FileExists(*It->GetNonSandboxPath()))
			{
				if (!PackageName.IsNone())
				{
					OutPackagesPendingHotReload.Add(PackageName);
				}
#if WITH_EDITOR
				FileChanges.Emplace(It->GetNonSandboxPath(), FFileChangeData::FCA_Added);
#endif
			}
			It.RemoveCurrent();
			continue;
		}

		// Clear the deleted path information
		DeletedSandboxPaths.Reset();
	}

	{
		FScopeLock SandboxMountPointsLock(&SandboxMountPointsCS);
		for (const FSandboxMountPoint& SandboxMountPoint : SandboxMountPoints)
		{
			// Modify any files that exist in both the non-sandbox and sandbox directories
			// Delete any files that exist in the sandbox but don't exist in the non-sandbox directory
			LowerLevel->IterateDirectoryRecursively(*SandboxMountPoint.Path.GetSandboxPath(), [this, &SandboxMountPoint, &FileChanges, &OutPackagesPendingHotReload, &OutPackagesPendingPurge](const TCHAR* InFilenameOrDirectory, const bool InIsDirectory) -> bool
			{
				if (!InIsDirectory)
				{
					const FConcertSandboxPlatformFilePath RemappedFilePath = FConcertSandboxPlatformFilePath::CreateNonSandboxPath(FPaths::ConvertRelativePathToFull(InFilenameOrDirectory), SandboxMountPoint.Path);

					// If this file maps to a package then we need to flush its linker so that we can remove the file from the sandbox
					FName PackageName;
					ConcertSandboxPlatformFileUtil::FlushPackageFile(RemappedFilePath.GetNonSandboxPath(), &PackageName, false);

					if (LowerLevel->FileExists(*RemappedFilePath.GetNonSandboxPath()))
					{
						if (!PackageName.IsNone())
						{
							OutPackagesPendingHotReload.Add(PackageName);
							OutPackagesPendingPurge.Remove(PackageName);
						}
#if WITH_EDITOR
						FileChanges.Emplace(RemappedFilePath.GetNonSandboxPath(), FFileChangeData::FCA_Modified);
#endif
					}
					else
					{
						if (!PackageName.IsNone())
						{
							OutPackagesPendingPurge.Add(PackageName);
							OutPackagesPendingHotReload.Remove(PackageName);
						}
#if WITH_EDITOR
						FileChanges.Emplace(RemappedFilePath.GetNonSandboxPath(), FFileChangeData::FCA_Removed);
#endif
					}
				}
				return true; // Continue iteration
			});

			// Delete everything under the mount point
			LowerLevel->IterateDirectory(*SandboxMountPoint.Path.GetSandboxPath(), [this](const TCHAR* InFilenameOrDirectory, const bool InIsDirectory) -> bool
			{
				if (InIsDirectory)
				{
					LowerLevel->DeleteDirectoryRecursively(InFilenameOrDirectory);
				}
				else
				{
					LowerLevel->DeleteFile(InFilenameOrDirectory);
				}
				return true; // Continue iteration
			});
		}
	}

#if WITH_EDITOR
	// Notify that the sandboxed directories have been restored to their original state
	if (FDirectoryWatcherModule* DirectoryWatcherModule = ConcertSyncClientUtil::GetDirectoryWatcherModuleIfLoaded())
	{
		if (FileChanges.Num() > 0)
		{
			DirectoryWatcherModule->RegisterExternalChanges(FileChanges);

			// Force a sync of the Asset Registry immediately to process the
			// file changes. This ensures that the Asset Registry is brought
			// up to date especially after file/package deletes, since
			// otherwise it may not get updated before packages are hot
			// reloaded and stale entries could still be present in the
			// registry.
			ConcertSyncClientUtil::SynchronizeAssetRegistry();
		}
	}
#endif
}

void FConcertSandboxPlatformFile::AddFilesAsPersisted(TArrayView<const FString> InFiles)
{
	for (const FString& File : InFiles)
	{
		PersistedFiles.Add(File, GetTimeStamp(*File));
	}
}

void FConcertSandboxPlatformFile::RemoveFilesAsPersisted(TArrayView<const FString> InFiles)
{
	for (const FString& File : InFiles)
	{
		PersistedFiles.Remove(File);
	}
}

const TMap<FString, FDateTime>& FConcertSandboxPlatformFile::GetPersistedFiles() const
{
	return PersistedFiles;
}

TArray<FString> FConcertSandboxPlatformFile::GatherSandboxChangedFilenames() const
{
	TArray<FString> ChangedFiles;
	
	// Gather deleted paths
	{
		FScopeLock DeletedSandboxPathsLock(&DeletedSandboxPathsCS);
		ChangedFiles.Reserve(DeletedSandboxPaths.Num());
		Algo::TransformIf(DeletedSandboxPaths, ChangedFiles,
			[this](const FConcertSandboxPlatformFilePath& DeletedPath)
			{
				return LowerLevel->FileExists(*DeletedPath.GetNonSandboxPath());
			},
			[this](const FConcertSandboxPlatformFilePath& DeletedPath)
			{
				return DeletedPath.GetNonSandboxPath();
			});
	}

	// Gather mounts files
	{
		FScopeLock SandboxMountPointsLock(&SandboxMountPointsCS);
		for (const FSandboxMountPoint& MountPoint : SandboxMountPoints)
		{
			LowerLevel->IterateDirectoryRecursively(*MountPoint.Path.GetSandboxPath(), [this, &MountPoint, &ChangedFiles](const TCHAR* InFilenameOrDirectory, const bool InIsDirectory) -> bool
			{
				if (!InIsDirectory)
				{
					ChangedFiles.Add(FConcertSandboxPlatformFilePath::CreateNonSandboxPath(FPaths::ConvertRelativePathToFull(InFilenameOrDirectory), MountPoint.Path).GetNonSandboxPath());
				}
				return true; // continue iteration
			});
		}
	}

	// Remove files marked as persisted already
	ChangedFiles.RemoveAllSwap([this](const FString& InFile)
	{
		if (const FDateTime* ModifiedTime = PersistedFiles.Find(InFile))
		{
			return const_cast<FConcertSandboxPlatformFile*>(this)->GetTimeStamp(*InFile) <= *ModifiedTime;
		}
		return false;
	});

	return ChangedFiles;
}

void FConcertSandboxPlatformFile::OnContentPathMounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	RegisterContentMountPath(InFilesystemPath);
}

void FConcertSandboxPlatformFile::OnContentPathDismounted(const FString& InAssetPath, const FString& InFilesystemPath)
{
	UnregisterContentMountPath(InFilesystemPath);
}

void FConcertSandboxPlatformFile::RegisterContentMountPath(const FString& InContentPath)
{
	FString AbsoluteSandboxPath = FPaths::ConvertRelativePathToFull(SandboxRootPath / ConcertSandboxPlatformFileUtil::GetContentFolderName(InContentPath)) / TEXT("");
	FString AbsoluteNonSandboxPath = FPaths::ConvertRelativePathToFull(InContentPath) / TEXT("");
	LowerLevel->CreateDirectory(*AbsoluteSandboxPath);
	{
		FScopeLock SandboxMountPointsLock(&SandboxMountPointsCS);

		FSandboxMountPoint& SandboxMountPoint = SandboxMountPoints.Add_GetRef(FSandboxMountPoint{ FConcertSandboxPlatformFilePath(MoveTemp(AbsoluteNonSandboxPath), MoveTemp(AbsoluteSandboxPath)) });
#if WITH_EDITOR
		if (IDirectoryWatcher* DirectoryWatcher = ConcertSyncClientUtil::GetDirectoryWatcher())
		{
			DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(SandboxMountPoint.Path.GetSandboxPath(), IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FConcertSandboxPlatformFile::OnDirectoryChanged, SandboxMountPoint.Path), SandboxMountPoint.OnDirectoryChangedHandle, IDirectoryWatcher::IncludeDirectoryChanges);
		}
#endif
	}
}

void FConcertSandboxPlatformFile::UnregisterContentMountPath(const FString& InContentPath)
{
	const FString AbsoluteSandboxPath = FPaths::ConvertRelativePathToFull(SandboxRootPath / ConcertSandboxPlatformFileUtil::GetContentFolderName(InContentPath)) / TEXT("");
	{
		FScopeLock SandboxMountPointsLock(&SandboxMountPointsCS);
		SandboxMountPoints.RemoveAll([&AbsoluteSandboxPath](FSandboxMountPoint& InSandboxMountPoint) -> bool
		{
			const bool bShouldRemove = InSandboxMountPoint.Path.GetSandboxPath() == AbsoluteSandboxPath;
#if WITH_EDITOR
			if (bShouldRemove && InSandboxMountPoint.OnDirectoryChangedHandle.IsValid())
			{
				if (IDirectoryWatcher* DirectoryWatcher = ConcertSyncClientUtil::GetDirectoryWatcherIfLoaded())
				{
					DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(InSandboxMountPoint.Path.GetSandboxPath(), InSandboxMountPoint.OnDirectoryChangedHandle);
					InSandboxMountPoint.OnDirectoryChangedHandle.Reset();
				}
			}
#endif
			return bShouldRemove;
		});
	}
	{
		FScopeLock DeletedSandboxPathsLock(&DeletedSandboxPathsCS);
		for (auto It = DeletedSandboxPaths.CreateIterator(); It; ++It)
		{
			if (It->GetSandboxPath().StartsWith(AbsoluteSandboxPath))
			{
				It.RemoveCurrent();
				continue;
			}
		}
	}
	LowerLevel->DeleteDirectoryRecursively(*AbsoluteSandboxPath);
}

FConcertSandboxPlatformFilePath FConcertSandboxPlatformFile::ToSandboxPath(FString InFilename, const bool bEvenIfDisabled) const
{
	return ToSandboxPath_Absolute(FPaths::ConvertRelativePathToFull(MoveTemp(InFilename)), bEvenIfDisabled);
}

FConcertSandboxPlatformFilePath FConcertSandboxPlatformFile::ToSandboxPath_Absolute(FString InFilename, const bool bEvenIfDisabled) const
{
	if (bEvenIfDisabled || IsSandboxEnabled())
	{
		FScopeLock SandboxMountPointsLock(&SandboxMountPointsCS);
		for (const FSandboxMountPoint& SandboxMountPoint : SandboxMountPoints)
		{
			// Mount point are stored with a trailing slash to prevent matching mount point with similar names -> (/Bla/Content, /Bla/ContentSupreme)
			// So we test without the slash to make sure with can match mount point directly -> (/Bla/Content match /Bla/Content/)
			const FString& PathStr = SandboxMountPoint.Path.GetNonSandboxPath();
			int32 PathStrNoSlashLength = PathStr.Len() - 1;
			if (FCString::Strnicmp(*InFilename, *PathStr, PathStrNoSlashLength) == 0 && (InFilename.Len() == PathStrNoSlashLength || InFilename[PathStrNoSlashLength] == TEXT('/')))
			{
				return FConcertSandboxPlatformFilePath::CreateSandboxPath(MoveTemp(InFilename), SandboxMountPoint.Path);
			}
		}
	}

	return FConcertSandboxPlatformFilePath(MoveTemp(InFilename));
}

FConcertSandboxPlatformFilePath FConcertSandboxPlatformFile::FromSandboxPath(FString InFilename) const
{
	return FromSandboxPath_Absolute(FPaths::ConvertRelativePathToFull(MoveTemp(InFilename)));
}

FConcertSandboxPlatformFilePath FConcertSandboxPlatformFile::FromSandboxPath_Absolute(FString InFilename) const
{
	FScopeLock SandboxMountPointsLock(&SandboxMountPointsCS);

	for (const FSandboxMountPoint& SandboxMountPoint : SandboxMountPoints)
	{
		// Mount point are stored with a trailing slash to prevent matching mount point with similar names -> (/Bla/Content, /Bla/ContentSupreme)
		// So we test without the slash to make sure with can match mount point directly -> (/Bla/Content match /Bla/Content/)
		const FString& PathStr = SandboxMountPoint.Path.GetSandboxPath();
		int32 PathStrNoSlashLength = PathStr.Len() - 1;
		if (FCString::Strnicmp(*InFilename, *PathStr, PathStrNoSlashLength) == 0 && (InFilename.Len() == PathStrNoSlashLength || InFilename[PathStrNoSlashLength] == TEXT('/')))
		{
			return FConcertSandboxPlatformFilePath::CreateNonSandboxPath(MoveTemp(InFilename), SandboxMountPoint.Path);
		}
	}

	return FConcertSandboxPlatformFilePath(MoveTemp(InFilename));
}

bool FConcertSandboxPlatformFile::IsPathDeleted(const FConcertSandboxPlatformFilePath& InPath) const
{
	FScopeLock DeletedSandboxPathsLock(&DeletedSandboxPathsCS);
	return DeletedSandboxPaths.Contains(InPath);
}

void FConcertSandboxPlatformFile::SetPathDeleted(const FConcertSandboxPlatformFilePath& InPath, const bool bIsDeleted)
{
	FScopeLock DeletedSandboxPathsLock(&DeletedSandboxPathsCS);
	if (bIsDeleted)
	{
		DeletedSandboxPaths.Add(InPath);
	}
	else
	{
		DeletedSandboxPaths.Remove(InPath);
	}
}

void FConcertSandboxPlatformFile::NotifyFileDeleted(const FConcertSandboxPlatformFilePath& InPath)
{
	if (!IsSandboxEnabled())
	{
		return;
	}

#if WITH_EDITOR
	if (FDirectoryWatcherModule* DirectoryWatcherModule = ConcertSyncClientUtil::GetDirectoryWatcherModuleIfLoaded())
	{
		FFileChangeData FileChange(InPath.GetNonSandboxPath(), FFileChangeData::FCA_Removed);
		DirectoryWatcherModule->RegisterExternalChanges(TArrayView<const FFileChangeData>(&FileChange, 1));
	}
#endif
}

void FConcertSandboxPlatformFile::MigrateFileToSandbox(const FConcertSandboxPlatformFilePath& InPath) const
{
	checkf(InPath.HasSandboxPath(), TEXT("MigrateFileToSandbox requires a sandbox path to be set!"));

	// Migrate the non-sandbox directory structure to the sandbox
	{
		const FString SandboxDirectoryPath = FPaths::GetPath(InPath.GetSandboxPath());

		// We create the directory if no part of it has been explicitly deleted in this sandbox
		bool bCreateDirectory = true;
		{
			// Walk the paths backwards for as long as they match (which is the sandbox relative part of the paths)
			FConcertSandboxPlatformFilePath TmpSandboxFilePath(FPaths::GetPath(InPath.GetNonSandboxPath()), CopyTemp(SandboxDirectoryPath));
			while (FPaths::GetBaseFilename(TmpSandboxFilePath.GetNonSandboxPath()) == FPaths::GetBaseFilename(TmpSandboxFilePath.GetSandboxPath()))
			{
				if (IsPathDeleted(TmpSandboxFilePath))
				{
					bCreateDirectory = false;
					break;
				}
				TmpSandboxFilePath = FConcertSandboxPlatformFilePath(FPaths::GetPath(TmpSandboxFilePath.GetNonSandboxPath()), FPaths::GetPath(TmpSandboxFilePath.GetSandboxPath()));
			}
		}
		if (bCreateDirectory)
		{
			LowerLevel->CreateDirectoryTree(*SandboxDirectoryPath);
		}
	}

	if (IsPathDeleted(InPath))
	{
		// Sandbox has explicitly deleted this file - don't resurrect it from the non-sandbox file
		return;
	}

	if (LowerLevel->FileExists(*InPath.GetSandboxPath()))
	{
		// Sandbox already has a file at this location - nothing to do
		return;
	}

	if (!LowerLevel->FileExists(*InPath.GetNonSandboxPath()))
	{
		// Non-sandbox has no file at this location - nothing to do
		return;
	}

	// Copy the file into the sandbox
	LowerLevel->CopyFile(*InPath.GetSandboxPath(), *InPath.GetNonSandboxPath());

	// Ensure the migrated file is writable
	LowerLevel->SetReadOnly(*InPath.GetSandboxPath(), false);
}

TArray<FConcertSandboxPlatformFile::FDirectoryItem> FConcertSandboxPlatformFile::GetDirectoryContents(const FConcertSandboxPlatformFilePath& InPath, const TCHAR* InDirBase) const
{
	checkf(InPath.HasSandboxPath(), TEXT("GetDirectoryContents requires a sandbox path to be set!"));
	TMap<FString, FFileStatData> FoundItems;

	// Gather the items, the sandbox iteration is straightforward
	LowerLevel->IterateDirectoryStat(*InPath.GetSandboxPath(), [&FoundItems](const TCHAR* FilenameOrDirectory, const FFileStatData& StatData)
	{
		FoundItems.Add(FPaths::GetCleanFilename(FilenameOrDirectory), StatData);
		return true;
	});
	// Gather the non-sandbox, validating we haven't already gathered the sandbox equivalent or the file/dir is marked as deleted
	LowerLevel->IterateDirectoryStat(*InPath.GetNonSandboxPath(), [this, &InPath, &FoundItems](const TCHAR* FilenameOrDirectory, const FFileStatData& StatData)
	{
		FString SandboxFilenameOrDirectory = FilenameOrDirectory;
		FString CleanFilenameOrDir = FPaths::GetCleanFilename(SandboxFilenameOrDirectory);

		if (!FoundItems.Contains(CleanFilenameOrDir) &&
			!IsPathDeleted(FConcertSandboxPlatformFilePath::CreateSandboxPath(MoveTemp(SandboxFilenameOrDirectory), InPath)))
		{
			FoundItems.Add(MoveTemp(CleanFilenameOrDir), StatData);
		}
		return true;
	});

	// Turn the found items in an array and re-base on InDirBase
	FString DirBase(InDirBase);
	TArray<FDirectoryItem> DirectoryContents;
	DirectoryContents.Reserve(FoundItems.Num());
	for (const auto& FoundItemPair : FoundItems)
	{
		DirectoryContents.Add(FDirectoryItem{ DirBase / FoundItemPair.Key, FoundItemPair.Value });
	}

	// Sort the result
	DirectoryContents.Sort([](const FDirectoryItem& InOne, const FDirectoryItem& InTwo) -> bool
	{
		return InOne.Path < InTwo.Path;
	});

	return DirectoryContents;
}

#if WITH_EDITOR

void FConcertSandboxPlatformFile::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges, FConcertSandboxPlatformFilePath MountPath)
{
	if (!IsSandboxEnabled())
	{
		return;
	}

	if (FDirectoryWatcherModule* DirectoryWatcherModule = ConcertSyncClientUtil::GetDirectoryWatcherModuleIfLoaded())
	{
		TArray<FFileChangeData> RemappedFileChanges;
		RemappedFileChanges.Reserve(FileChanges.Num());

		// Map the sandbox paths back to their original roots and notify the directory watcher
		for (const FFileChangeData& FileChange : FileChanges)
		{
			const FConcertSandboxPlatformFilePath RemappedFilePath = FConcertSandboxPlatformFilePath::CreateNonSandboxPath(FPaths::ConvertRelativePathToFull(FileChange.Filename), MountPath);
			RemappedFileChanges.Emplace(RemappedFilePath.GetNonSandboxPath(), FileChange.Action);

			// Make sure the deleted state of this item is synchronized correctly
			if (FileChange.Action == FFileChangeData::FCA_Added)
			{
				SetPathDeleted(RemappedFilePath, false);
			}
			else if (FileChange.Action == FFileChangeData::FCA_Removed)
			{
				SetPathDeleted(RemappedFilePath, true);
			}
		}

		DirectoryWatcherModule->RegisterExternalChanges(RemappedFileChanges);
	}
}

#endif
