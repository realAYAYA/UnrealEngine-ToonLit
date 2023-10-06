// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualizationFileBackend.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "VirtualizationUtilities.h"

namespace UE::Virtualization
{

FFileSystemBackend::FFileSystemBackend(FStringView ProjectName, FStringView ConfigName, FStringView DebugName)
	: IVirtualizationBackend(ConfigName, DebugName, EOperations::Push | EOperations::Pull)
{
}

bool FFileSystemBackend::Initialize(const FString& ConfigEntry)
{
	if (!FParse::Value(*ConfigEntry, TEXT("Path="), RootDirectory))
	{
		RootDirectory = *WriteToString<512>(FPaths::ProjectSavedDir(), TEXT("VAPayloads"));
	}

	FString EnvOverrideName;
	if (FParse::Value(*ConfigEntry, TEXT("EnvPathOverride="), EnvOverrideName))
	{
		FString OverridePath = FPlatformMisc::GetEnvironmentVariable(*EnvOverrideName);
		if (!OverridePath.IsEmpty())
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Overriding path with envvar '%s'"), *GetDebugName(), *EnvOverrideName);
			RootDirectory = OverridePath;
		}
	}

	FPaths::NormalizeDirectoryName(RootDirectory);

	if (RootDirectory.IsEmpty())
	{
		UE_LOG(LogVirtualization, Error, TEXT("[%s] Config file entry 'Path=' was empty"), *GetDebugName());
		return false;
	}

	// TODO: Validate that the given path is usable?

	int32 RetryCountIniFile = INDEX_NONE;
	if (FParse::Value(*ConfigEntry, TEXT("RetryCount="), RetryCountIniFile))
	{
		RetryCount = RetryCountIniFile;
	}

	int32 RetryWaitTimeMSIniFile = INDEX_NONE;
	if (FParse::Value(*ConfigEntry, TEXT("RetryWaitTime="), RetryWaitTimeMSIniFile))
	{
		RetryWaitTimeMS = RetryWaitTimeMSIniFile;
	}

	// Now log a summary of the backend settings to make issues easier to diagnose
	UE_LOG(LogVirtualization, Log, TEXT("[%s] Using path: '%s'"), *GetDebugName(), *FPaths::ConvertRelativePathToFull(RootDirectory));
	UE_LOG(LogVirtualization, Log, TEXT("[%s] Will retry failed read attempts %d times with a gap of %dms betwen them"), *GetDebugName(), RetryCount, RetryWaitTimeMS);

	return true;
}

IVirtualizationBackend::EConnectionStatus FFileSystemBackend::OnConnect()
{
	return IVirtualizationBackend::EConnectionStatus::Connected;
}

bool FFileSystemBackend::PushData(TArrayView<FPushRequest> Requests, EPushFlags Flags)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFileSystemBackend::PushData);

	int32 ErrorCount = 0;

	const bool bEnableExistenceCheck = !EnumHasAllFlags(Flags, EPushFlags::Force);

	for (FPushRequest& Request : Requests)
	{
		const FIoHash& PayloadId = Request.GetIdentifier();
	
		if (bEnableExistenceCheck && DoesPayloadExist(PayloadId))
		{
			UE_LOG(LogVirtualization, Verbose, TEXT("[%s] Already has a copy of the payload '%s'."), *GetDebugName(), *LexToString(PayloadId));
			Request.SetResult(FPushResult::GetAsAlreadyExists());

			continue;
		}

		// Make sure to log any disk write failures to the user, even if this backend will often be optional as they are
		// not expected and could indicate bigger problems.
		// 
		// First we will write out the payload to a temp file, after which we will move it to the correct storage location
		// this helps reduce the chance of leaving corrupted data on disk in the case of a power failure etc.
		const FString TempFilePath = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("vapayload"));

		TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileWriter(*TempFilePath));

		if (FileAr == nullptr)
		{
			TStringBuilder<MAX_SPRINTF> SystemErrorMsg;
			Utils::GetFormattedSystemError(SystemErrorMsg);

			UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to write payload '%s' to '%s' due to system error: %s"),
				*GetDebugName(),
				*LexToString(PayloadId),
				*TempFilePath,
				SystemErrorMsg.ToString());

			ErrorCount++;
			Request.SetResult(FPushResult::GetAsError());

			continue;
		}

		{
			FCompressedBuffer Payload = Request.GetPayload();
			*FileAr << Payload;
		}

		if (!FileAr->Close())
		{
			TStringBuilder<MAX_SPRINTF> SystemErrorMsg;
			Utils::GetFormattedSystemError(SystemErrorMsg);

			UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to write payload '%s' contents to '%s' due to system error: %s"),
				*GetDebugName(),
				*LexToString(PayloadId),
				*TempFilePath,
				SystemErrorMsg.ToString());

			IFileManager::Get().Delete(*TempFilePath, true, false, true);  // Clean up the temp file if it is still around but do not failure cases to the user

			ErrorCount++;
			Request.SetResult(FPushResult::GetAsError());

			continue;
		}

		TStringBuilder<512> FilePath;
		CreateFilePath(PayloadId, FilePath);

		// If the file already exists we don't need to replace it, we will also do our own error logging.
		if (IFileManager::Get().Move(FilePath.ToString(), *TempFilePath, /*Replace*/ false, /*EvenIfReadOnly*/ false, /*Attributes*/ false, /*bDoNotRetryOrError*/ true))
		{
			Request.SetResult(FPushResult::GetAsPushed());
		}
		else
		{
			// Store the error message in case we need to display it
			TStringBuilder<MAX_SPRINTF> SystemErrorMsg;
			Utils::GetFormattedSystemError(SystemErrorMsg);

			IFileManager::Get().Delete(*TempFilePath, true, false, true); // Clean up the temp file if it is still around but do not failure cases to the user

			// Check if another thread or process was writing out the payload at the same time, if so we 
			// don't need to give an error message.
			if (DoesPayloadExist(PayloadId))
			{
				UE_LOG(LogVirtualization, Verbose, TEXT("[%s] Already has a copy of the payload '%s'."), *GetDebugName(), *LexToString(PayloadId));
				Request.SetResult(FPushResult::GetAsAlreadyExists());
				continue;
			}
			else
			{
				UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to move payload '%s' to it's final location '%s' due to system error: %s"),
					*GetDebugName(),
					*LexToString(PayloadId),
					*FilePath,
					SystemErrorMsg.ToString());

				ErrorCount++;
				Request.SetResult(FPushResult::GetAsError());
			}
		}
	}

	return ErrorCount == 0;
}

bool FFileSystemBackend::PullData(TArrayView<FPullRequest> Requests, EPullFlags Flags, FText& OutErrors)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFileSystemBackend::PullData);

	for (FPullRequest& Request : Requests)
	{
		TStringBuilder<512> FilePath;
		CreateFilePath(Request.GetIdentifier(), FilePath);

		// TODO: Should we allow the error severity to be configured via ini or just not report this case at all?
		if (!IFileManager::Get().FileExists(FilePath.ToString()))
		{
			UE_LOG(LogVirtualization, Verbose, TEXT("[%s] Does not contain the payload '%s'"), 
				*GetDebugName(), 
				*LexToString(Request.GetIdentifier()));

			continue;
		}

		TUniquePtr<FArchive> FileAr = OpenFileForReading(FilePath.ToString());

		if (FileAr == nullptr)
		{
			TStringBuilder<MAX_SPRINTF> SystemErrorMsg;
			Utils::GetFormattedSystemError(SystemErrorMsg);

			UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to load payload '%s' from file '%s' due to system error: %s"),
				*GetDebugName(),
				*LexToString(Request.GetIdentifier()),
				FilePath.ToString(),
				SystemErrorMsg.ToString());

			Request.SetError();

			continue;
		}

		Request.SetPayload(FCompressedBuffer::Load(*FileAr));

		if (FileAr->IsError())
		{
			UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to serialize payload '%s' from file '%s'"),
				*GetDebugName(),
				*LexToString(Request.GetIdentifier()),
				FilePath.ToString());

			Request.SetError();

			continue;
		}
	}

	return true;
}

bool FFileSystemBackend::DoesPayloadExist(const FIoHash& Id)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFileSystemBackend::DoesPayloadExist);

	TStringBuilder<512> FilePath;
	CreateFilePath(Id, FilePath);

	return IFileManager::Get().FileExists(FilePath.ToString());
}

void FFileSystemBackend::CreateFilePath(const FIoHash& PayloadId, FStringBuilderBase& OutPath)
{
	TStringBuilder<52> PayloadPath;
	Utils::PayloadIdToPath(PayloadId, PayloadPath);

	OutPath << RootDirectory << TEXT("/") << PayloadPath;
}

TUniquePtr<FArchive> FFileSystemBackend::OpenFileForReading(const TCHAR* FilePath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FFileSystemBackend::OpenFileForReading);

	int32 Retries = 0;

	while (Retries < RetryCount)
	{
		TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileReader(FilePath));
		if (FileAr)
		{
			return FileAr;
		}
		else
		{
			UE_LOG(LogVirtualization, Warning, TEXT("[%s] Failed to open '%s' for reading attempt retrying (%d/%d) in %dms..."), *GetDebugName(), FilePath, Retries, RetryCount, RetryWaitTimeMS);
			FPlatformProcess::SleepNoStats(static_cast<float>(RetryWaitTimeMS) * 0.001f);

			Retries++;
		}
	}

	return nullptr;
}

UE_REGISTER_VIRTUALIZATION_BACKEND_FACTORY(FFileSystemBackend, FileSystem);

} // namespace UE::Virtualization
