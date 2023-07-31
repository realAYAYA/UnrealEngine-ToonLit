// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualizationSourceControlBackend.h"

#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "IO/IoHash.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Logging/MessageLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "SourceControlInitSettings.h"
#include "SourceControlOperations.h"
#include "VirtualizationSourceControlUtilities.h"
#include "VirtualizationUtilities.h"
#include "Interfaces/IPluginManager.h"

// When the SourceControl module (or at least the perforce source control module) is thread safe we
// can enable this and stop using the hacky work around 'TryToDownloadFileFromBackgroundThread'
#define IS_SOURCE_CONTROL_THREAD_SAFE 0

#define LOCTEXT_NAMESPACE "Virtualization"

namespace UE::Virtualization
{

/** 
 * A quick and dirty implementation of a std::counting_semaphore that we can use to 
 * limit the number of threads that can create a new perforce connection when pulling or 
 * pushing payloads.
 * 
 * In the worst case scenario where a user needs to pull all of their payloads from the source
 * control backend rather than a faster backend we need to make sure that they will not overwhelm
 * their server with requests.
 * In the future we can use this sort of limit to help gather requests from many threads into a 
 * single batch request from the server which will be much more efficient than the current 'one
 * payload, one request' system. Although we might want to consider gathering multiple requests
 * at a higher level so that all backends can work on the same batching principle.
 */
class FSemaphore 
{
public:
	UE_NONCOPYABLE(FSemaphore);

	enum class EAcquireResult
	{
		/** The acquire was a success and the thread can continue */
		Success,
		/** The wait event failed, the semaphore object is no longer safe */
		EventFailed
	};

	FSemaphore() = delete;
	explicit FSemaphore(int32 InitialCount)
		: WaitEvent(EEventMode::ManualReset)
		, Counter(InitialCount)
		, DebugCount(0)
	{

	}

	~FSemaphore()
	{
		checkf(DebugCount == 0, TEXT("'%d' threads are still waiting on the UE::Virtualization::FSemaphore being destroyed"));
	}

	/** Will block until the calling thread can pass through the semaphore. Note that it might return an error if the WaitEvent fails */
	EAcquireResult Acquire()
	{
		CriticalSection.Lock();
		DebugCount++;

		while (Counter-- <= 0)
		{
			Counter++;
			WaitEvent->Reset();

			CriticalSection.Unlock();

			if (!WaitEvent->Wait())
			{
				--DebugCount;
				return EAcquireResult::EventFailed;
			}
			CriticalSection.Lock();
		}

		CriticalSection.Unlock();

		return EAcquireResult::Success;
	}

	void Release()
	{
		FScopeLock _(&CriticalSection);

		Counter++;
		WaitEvent->Trigger();

		--DebugCount;
	}

private:
	FEventRef WaitEvent;
	FCriticalSection CriticalSection;

	std::atomic<int32> Counter;
	std::atomic<int32> DebugCount;
};

/** Structure to make it easy to acquire/release a FSemaphore for a given scope */
struct FSemaphoreScopeLock
{
	FSemaphoreScopeLock(FSemaphore* InSemaphore)
		: Semaphore(InSemaphore)
	{
		if (Semaphore != nullptr)
		{
			Semaphore->Acquire();
		}
	}

	~FSemaphoreScopeLock()
	{
		if (Semaphore != nullptr)
		{
			Semaphore->Release();
		}
	}

private:
	FSemaphore* Semaphore;
};

/** Utility function to create a directory to submit payloads from. */
[[nodiscard]] static bool TryCreateSubmissionSessionDirectory(FStringView SessionDirectoryPath)
{
	// Write out an ignore file to the submission directory (will create the directory if needed)
	{
		TStringBuilder<260> IgnoreFilePath;

		// TODO: We should find if P4IGNORE is actually set and if so extract the filename to use.
		// This will require extending the source control module
		FPathViews::Append(IgnoreFilePath, SessionDirectoryPath, TEXT(".p4ignore.txt"));

		// A very basic .p4ignore file that should make sure that we are only submitting valid .upayload files.
		// 
		// Since the file should only exist while we are pushing payloads, it is not expected that anyone will need
		// to read the file. Due to this we only include the bare essentials in terms of documentation.

		TStringBuilder<512> FileContents;

		FileContents << TEXT("# Ignore all files\n*\n\n");
		FileContents << TEXT("# Allow.payload files as long as they are the expected 3 directories deep\n!*/*/*/*.upayload\n\n");

		if (!FFileHelper::SaveStringToFile(FileContents, IgnoreFilePath.ToString()))
		{
			return false;
		}
	}

	return true;
}

/** Builds a changelist description to be used when submitting a payload to source control */
static void CreateDescription(const FString& ProjectName, TArrayView<const FPushRequest*>& FileRequests, TStringBuilder<512>& OutDescription)
{
	// TODO: Maybe make writing out the project name an option or allow for a codename to be set via ini file?
	OutDescription << TEXT("Submitted for project: ");
	OutDescription << ProjectName;

	bool bInitialNewline = false;

	for (const FPushRequest* Request : FileRequests)
	{
		if (!Request->GetContext().IsEmpty())
		{
			if (!bInitialNewline)
			{
				OutDescription << TEXT("\n");
				bInitialNewline = true;
			}

			OutDescription << TEXT("\n") << Request->GetIdentifier() << "\t: " << Request->GetContext();
		}
	}
}

[[nodiscard]] static ECommandResult::Type GetDepotPathStates(ISourceControlProvider& SCCProvider, const TArray<FString>& DepotPaths, TArray<FSourceControlStateRef>& OutStates)
{
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateOperation->SetRequireDirPathEndWithSeparator(true);

	ECommandResult::Type Result = SCCProvider.Execute(UpdateOperation, DepotPaths);
	if (Result != ECommandResult::Succeeded)
	{
		return Result;
	}

	return SCCProvider.GetState(DepotPaths, OutStates, EStateCacheUsage::Use);
}

/** Parse all error messages in a FSourceControlResultInfo and return true if the file not found error message is found */
[[nodiscard]] static bool IsDepotFileMissing(const FSourceControlResultInfo& ResultInfo)
{
	// Ideally we'd parse for this sort of thing in the source control module itself and return an error enum
	for (const FText& ErrorTest : ResultInfo.ErrorMessages)
	{
		if (ErrorTest.ToString().Find(" - no such file(s).") != INDEX_NONE)
		{
			return true;
		}
	}

	return false;
}

FSourceControlBackend::FSourceControlBackend(FStringView ProjectName, FStringView ConfigName, FStringView InDebugName)
	: IVirtualizationBackend(ConfigName, InDebugName, EOperations::Push | EOperations::Pull)
	, ProjectName(ProjectName)
{
}

bool FSourceControlBackend::Initialize(const FString& ConfigEntry)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::Initialize);

	if (!TryApplySettingsFromConfigFiles(ConfigEntry))
	{
		return false;
	}

	return true;
}

IVirtualizationBackend::EConnectionStatus FSourceControlBackend::OnConnect()
{
	// First check that the 'PerforceSourceControl' plugin exists and is enabled so that we can give specific
	// error messages if not.
	// We could try to force enable the plugin at this point, in the same way that the stand alone tool does
	// but it is probably better to inform the user and have them add/enable the plugin for their target explicitly
	// rather than have us do it behind the scenes.
	// This is only expected to be a problem when first enabling VA for a project/target and not something a user
	// will experience day to day.
	TSharedPtr<IPlugin> P4Plugin = IPluginManager::Get().FindPlugin("PerforceSourceControl");
	if (!P4Plugin)
	{
		UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to find the 'PerforceSourceControl' plugin"), *GetDebugName());
		return IVirtualizationBackend::EConnectionStatus::Error;
	}

	if (!P4Plugin->IsEnabled())
	{
		UE_LOG(LogVirtualization, Error, TEXT("[%s] The 'PerforceSourceControl' plugin is disabled for this target"), *GetDebugName());
		return IVirtualizationBackend::EConnectionStatus::Error;
	}

	// We do not want the connection to have a client workspace so explicitly set it to empty
	FSourceControlInitSettings SCCSettings(FSourceControlInitSettings::EBehavior::OverrideExisting);
	SCCSettings.SetConfigBehavior(FSourceControlInitSettings::EConfigBehavior::ReadOnly);

	if (!ServerAddress.IsEmpty())
	{
		SCCSettings.AddSetting(TEXT("P4Port"), ServerAddress);
	}
	SCCSettings.AddSetting(TEXT("P4Client"), TEXT(""));

	SCCProvider = ISourceControlModule::Get().CreateProvider(FName("Perforce"), TEXT("Virtualization"), SCCSettings);
	if (!SCCProvider.IsValid())
	{
		UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to create a perforce source control provider"), *GetDebugName());
		return IVirtualizationBackend::EConnectionStatus::Error;
	}

	SCCProvider->Init(true);

	if (IsInGameThread())
	{
		// Note that if the connect is failing then we expect it to fail here rather than in the subsequent attempts to get the meta info file
		TSharedRef<FConnect, ESPMode::ThreadSafe> ConnectCommand = ISourceControlOperation::Create<FConnect>();
		if (SCCProvider->Execute(ConnectCommand, FString(), EConcurrency::Synchronous) != ECommandResult::Succeeded)
		{
			FTextBuilder Errors;
			for (const FText& Msg : ConnectCommand->GetResultInfo().ErrorMessages)
			{
				Errors.AppendLine(Msg);
			}

			FMessageLog Log("LogVirtualization");
			Log.Warning(FText::Format(LOCTEXT("FailedSourceControlConnection", "Failed to connect to source control backend with the following errors:\n{0}\nThe source control backend had trouble connecting!\nTrying logging in with the 'p4 login' command or by using p4vs/UnrealGameSync."),
				Errors.ToText()));

			OnConnectionError();
			return IVirtualizationBackend::EConnectionStatus::Error;
		}
	}

	// When a source control depot is set up a file named 'payload_metainfo.txt' should be submitted to it's root.
	// This allows us to check for the existence of the file to confirm that the depot root is indeed valid.
	const FString PayloadMetaInfoPath = WriteToString<512>(DepotRoot, TEXT("payload_metainfo.txt")).ToString();

#if IS_SOURCE_CONTROL_THREAD_SAFE
	TSharedRef<FDownloadFile, ESPMode::ThreadSafe> DownloadCommand = ISourceControlOperation::Create<FDownloadFile>();
	if (SCCProvider->Execute(DownloadCommand, PayloadMetaInfoPath, EConcurrency::Synchronous) != ECommandResult::Succeeded)
	{
		FMessageLog Log("LogVirtualization");

		Log.Warning(FText::Format(LOCTEXT("FailedMetaInfo", "Failed to find 'payload_metainfo.txt' in the depot '{0}'\nThe source control backend will be unable to pull payloads, is your source control  config set up correctly?"),
			FText::FromString(DepotRoot)));

		OnConnectionError();
		return IVirtualizationBackend::EConnectionStatus::Error;
}
#else
	TSharedRef<FDownloadFile, ESPMode::ThreadSafe> DownloadCommand = ISourceControlOperation::Create<FDownloadFile>();
	if (!SCCProvider->TryToDownloadFileFromBackgroundThread(DownloadCommand, PayloadMetaInfoPath))
	{
		FMessageLog Log("LogVirtualization");

		Log.Warning(FText::Format(LOCTEXT("FailedMetaInfo", "Failed to find 'payload_metainfo.txt' in the depot '{0}'\nThe source control backend will be unable to pull payloads, is your source control  config set up correctly?"),
			FText::FromString(DepotRoot)));

		OnConnectionError();
		return IVirtualizationBackend::EConnectionStatus::Error;
	}
#endif //IS_SOURCE_CONTROL_THREAD_SAFE

	FSharedBuffer MetaInfoBuffer = DownloadCommand->GetFileData(PayloadMetaInfoPath);
	if (MetaInfoBuffer.IsNull())
	{
		FMessageLog Log("LogVirtualization");

		Log.Warning(FText::Format(LOCTEXT("FailedMetaInfo", "Failed to find 'payload_metainfo.txt' in the depot '{0}'\nThe source control backend will be unable to pull payloads, is your source control  config set up correctly?"),
			FText::FromString(DepotRoot)));

		OnConnectionError();
		return IVirtualizationBackend::EConnectionStatus::Error;
	}

	// Currently we do not do anything with the payload meta info, in the future we could structure
	// it's format to include more information that might be worth logging or something. 
	// But for now being able to pull the payload meta info path at least shows that we can use the
	// depot.
	return IVirtualizationBackend::EConnectionStatus::Connected;
}

bool FSourceControlBackend::PullData(TArrayView<FPullRequest> Requests)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PullData);

#if 0
	TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PullData);

	for (FPullRequest& Request : Requests)
	{
		TStringBuilder<512> DepotPath;
		CreateDepotPath(Request.Identifier, DepotPath);

		// TODO: When multiple threads are blocked waiting on this we could gather X payloads together and make a single
		// batch request on the same connection, which should be a lot faster with less overhead.
		// Although ideally this backend will not get hit very often.
		FSemaphoreScopeLock _(ConcurrentConnectionLimit.Get());

		UE_LOG(LogVirtualization, Verbose, TEXT("[%s] Attempting to pull '%s' from source control"), *GetDebugName(), *DepotPath);

		int32 Retries = 0;

		while (Retries < RetryCount)
		{
			// Only warn if the backend is configured to retry
			if (Retries != 0)
			{
				UE_LOG(LogVirtualization, Warning, TEXT("[%s] Failed to download '%s' retrying (%d/%d) in %dms..."), *GetDebugName(), DepotPath.ToString(), Retries, RetryCount, RetryWaitTimeMS);
				FPlatformProcess::SleepNoStats(RetryWaitTimeMS * 0.001f);
			}

#if IS_SOURCE_CONTROL_THREAD_SAFE
			TSharedRef<FDownloadFile, ESPMode::ThreadSafe> DownloadCommand = ISourceControlOperation::Create<FDownloadFile>(FDownloadFile::EVerbosity::None);
			if (SCCProvider->Execute(DownloadCommand, DepotPath.ToString(), EConcurrency::Synchronous) == ECommandResult::Succeeded)
			{
				// The payload was created by FCompressedBuffer::Compress so we can return it as a FCompressedBuffer.
				FSharedBuffer Buffer = DownloadCommand->GetFileData(DepotPath);
				return FCompressedBuffer::FromCompressed(Buffer);
			}
#else
			TSharedRef<FDownloadFile> DownloadCommand = ISourceControlOperation::Create<FDownloadFile>(FDownloadFile::EVerbosity::None);
			if (SCCProvider->TryToDownloadFileFromBackgroundThread(DownloadCommand, DepotPath.ToString()))
			{
				// The payload was created by FCompressedBuffer::Compress so we can return it as a FCompressedBuffer.
				Request.Status = FPullRequest::EStatus::Success;
				Request.Payload = FCompressedBuffer::FromCompressed(DownloadCommand->GetFileData(DepotPath));

				break;
			}
#endif

			// If this was the first try then check to see if the error being returns is that the file does not exist
			// in the depot. If it does not exist then there is no point in us retrying and we can error out at this point.
			if (Retries == 0 && IsDepotFileMissing(DownloadCommand->GetResultInfo()))
			{
				break;
			}

			Retries++;
		}
	}

	return true;

#else
	TArray<FString> DepotPaths;
	DepotPaths.Reserve(Requests.Num());

	for (FPullRequest& Request : Requests)
	{
		TStringBuilder<512> DepotPath;
		CreateDepotPath(Request.GetIdentifier(), DepotPath);

		DepotPaths.Add(DepotPath.ToString());
	}

	// TODO: When multiple threads are blocked waiting on this we could gather X payloads together and make a single
	// batch request on the same connection, which should be a lot faster with less overhead.
	// Although ideally this backend will not get hit very often.
	FSemaphoreScopeLock _(ConcurrentConnectionLimit.Get());

//	UE_LOG(LogVirtualization, Verbose, TEXT("[%s] Attempting to pull '%s' from source control"), *GetDebugName(), *DepotPath);

	int32 Retries = 0;

	while (Retries < RetryCount)
	{
		// Only warn if the backend is configured to retry
		if (Retries != 0)
		{
		//	UE_LOG(LogVirtualization, Warning, TEXT("[%s] Failed to download '%s' retrying (%d/%d) in %dms..."), *GetDebugName(), DepotPath.ToString(), Retries, RetryCount, RetryWaitTimeMS);
			FPlatformProcess::SleepNoStats(RetryWaitTimeMS * 0.001f);
		}

#if IS_SOURCE_CONTROL_THREAD_SAFE
		TSharedRef<FDownloadFile, ESPMode::ThreadSafe> DownloadCommand = ISourceControlOperation::Create<FDownloadFile>(FDownloadFile::EVerbosity::None);
		if (SCCProvider->Execute(DownloadCommand, DepotPath.ToString(), EConcurrency::Synchronous) == ECommandResult::Succeeded)
		{
			// The payload was created by FCompressedBuffer::Compress so we can return it as a FCompressedBuffer.
			FSharedBuffer Buffer = DownloadCommand->GetFileData(DepotPath);

			Request.Payload = FCompressedBuffer::FromCompressed(Buffer);
			Request.Status = FPullRequest::EStatus::Success;
		}
#else
		TSharedRef<FDownloadFile> DownloadCommand = ISourceControlOperation::Create<FDownloadFile>(FDownloadFile::EVerbosity::None);
		if (SCCProvider->TryToDownloadFileFromBackgroundThread(DownloadCommand, DepotPaths))
		{
			for(int32 Index = 0; Index < Requests.Num(); ++Index)
			{
				// The payload was created by FCompressedBuffer::Compress so we can return it as a FCompressedBuffer.
				FSharedBuffer Buffer = DownloadCommand->GetFileData(DepotPaths[Index]);

				Requests[Index].SetPayload(FCompressedBuffer::FromCompressed(Buffer));
			}

			break;
		}
#endif

		// If this was the first try then check to see if the error being returns is that the file does not exist
		// in the depot. If it does not exist then there is no point in us retrying and we can error out at this point.
		if (Retries == 0 && IsDepotFileMissing(DownloadCommand->GetResultInfo()))
		{
			break;
		}

		Retries++;
	}

	// TODO -  Should we error here if the payload failed? 

	return true;
#endif
}

bool FSourceControlBackend::DoesPayloadExist(const FIoHash& Id)
{
	TArray<bool> Result;

	if (FSourceControlBackend::DoPayloadsExist(MakeArrayView<const FIoHash>(&Id, 1), Result))
	{
		check(Result.Num() == 1);
		return Result[0];
	}
	else
	{
		return false;
	}
}

bool FSourceControlBackend::PushData(TArrayView<FPushRequest> Requests)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData);

	// First check to see which payloads actually need pushing, if we are lucky we might
	// not need to even bother with workspace manipulation if all payloads already exist.

	FSemaphoreScopeLock _(ConcurrentConnectionLimit.Get());

	// TODO: At some point FVirtualizationManager will do this so we know there cannot be duplicate
	// payload requests, at which point we can rework this code.
	// We create a new array of requests that we know are unique along with a map of the request 
	// results that we can use to set the original requests statuses at the end.

	TMap<FIoHash, FPushResult> PayloadStatusMap;
	PayloadStatusMap.Reserve(Requests.Num());

	TArray<const FPushRequest*> UniqueRequests;
	UniqueRequests.Reserve(Requests.Num());

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData::FindUniquePayloads);

		for (const FPushRequest& Request : Requests)
		{
			if (!PayloadStatusMap.Contains(Request.GetIdentifier()))
			{
				PayloadStatusMap.Add(Request.GetIdentifier(), FPushResult::GetAsError());
				UniqueRequests.Add(&Request);
			}
		}

		UE_LOG(LogVirtualization, Log, TEXT("[%s] Found %d unique payload(s)"), *GetDebugName(), UniqueRequests.Num());
	}

	TArray<const FPushRequest*> RequestsToPush;
	RequestsToPush.Reserve(UniqueRequests.Num());

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData::CheckIfPayloadsAlreadyExist);

		const int32 NumBatches = FMath::DivideAndRoundUp<int32>(UniqueRequests.Num(), MaxBatchCount);

		TArray<FIoHash> PayloadIdentifiers;
		PayloadIdentifiers.Reserve(MaxBatchCount);

		TArray<bool> PayloadResults;
		PayloadResults.Reserve(MaxBatchCount);

		for (int32 BatchIndex = 0; BatchIndex < NumBatches; ++BatchIndex)
		{
			PayloadIdentifiers.Reset(0);
			PayloadResults.Reset(0);

			const int32 BatchStart = BatchIndex * MaxBatchCount;
			const int32 BatchEnd = FMath::Min((BatchIndex + 1) * MaxBatchCount, UniqueRequests.Num());

			for (int32 ReqIndex = BatchStart; ReqIndex < BatchEnd; ++ReqIndex)
			{
				PayloadIdentifiers.Add(UniqueRequests[ReqIndex]->GetIdentifier());
			}

			if (!DoPayloadsExist(PayloadIdentifiers, PayloadResults))
			{
				UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to find the current file state for payloads"), *GetDebugName());
				return false;
			}

			check(PayloadIdentifiers.Num() == PayloadResults.Num());

			for (int32 Index = 0; Index < PayloadResults.Num(); ++Index)
			{
				const FPushRequest* Request = UniqueRequests[BatchStart + Index];

				if (PayloadResults[Index])
				{
					PayloadStatusMap[Request->GetIdentifier()] = FPushResult::GetAsAlreadyExists();
				}
				else
				{
					RequestsToPush.Add(Request);
				}
			}
		}
	}

	UE_LOG(LogVirtualization, Log, TEXT("[%s] Determined that %d payload(s) require submission to source control"), *GetDebugName(), RequestsToPush.Num());

	if (RequestsToPush.IsEmpty())
	{
		for (FPushRequest& Request : Requests)
		{
			Request.SetResult(PayloadStatusMap[Request.GetIdentifier()]);
		}

		return true;
	}

	// TODO: Consider creating one workspace and one temp dir per session rather than per push.
	// Although this would require more checking on start up to check for lingering workspaces
	// and directories in case of editor crashes.
	// We'd also need to remove each submitted file from the workspace after submission so that
	// we can delete the local file

	// We cannot easily submit files from within the project root due to p4 ignore rules
	// so we will use the user temp directory instead. We append a guid to the root directory
	// to avoid potentially conflicting with other editor processes that might be running.

	const FGuid SessionGuid = FGuid::NewGuid();
	
	UE_LOG(LogVirtualization, Log, TEXT("[%s] Started payload submission session '%s' for '%d' payload(s)"), *GetDebugName(), *LexToString(SessionGuid), RequestsToPush.Num());

	TStringBuilder<260> SessionDirectory;
	FPathViews::Append(SessionDirectory, SubmissionRootDir, SessionGuid);

	if (!TryCreateSubmissionSessionDirectory(SessionDirectory))
	{
		UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to created directory '%s' to submit payloads from"), *GetDebugName(), SessionDirectory.ToString());
		return false;
	}

	UE_LOG(LogVirtualization, Log, TEXT("[%s] Created directory '%s' to submit payloads from"), *GetDebugName(), SessionDirectory.ToString());

	ON_SCOPE_EXIT
	{
		// Clean up the payload file from disk and the temp directories, but we do not need to give errors if any of these operations fail.
		IFileManager::Get().DeleteDirectory(SessionDirectory.ToString(), false, true);
	};

	TStringBuilder<64> WorkspaceName;
	WorkspaceName << TEXT("VASubmission-") << SessionGuid;

	// Create a temp workspace so that we can submit the payload from
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData::CreateWorkspace);
		TSharedRef<FCreateWorkspace> CreateWorkspaceCommand = ISourceControlOperation::Create<FCreateWorkspace>(WorkspaceName, SessionDirectory);

		if (ClientStream.IsEmpty())
		{
			TStringBuilder<512> DepotMapping;
			DepotMapping << DepotRoot << TEXT("...");

			TStringBuilder<128> ClientMapping;
			ClientMapping << TEXT("//") << WorkspaceName << TEXT("/...");

			CreateWorkspaceCommand->AddNativeClientViewMapping(DepotMapping, ClientMapping);
		}
		else
		{
			CreateWorkspaceCommand->SetStream(ClientStream);
		}

		if (bUsePartitionedClient)
		{
			CreateWorkspaceCommand->SetType(FCreateWorkspace::EType::Partitioned);
		}

		CreateWorkspaceCommand->SetDescription(TEXT("This workspace was autogenerated when submitting virtualized payloads to source control"));

		if (SCCProvider->Execute(CreateWorkspaceCommand) != ECommandResult::Succeeded)
		{
			UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to create temp workspace '%s' to use for payload submission"),
				*GetDebugName(),
				WorkspaceName.ToString());

			return false;
		}
	}

	ON_SCOPE_EXIT
	{
		// Remove the temp workspace mapping
		if (SCCProvider->Execute(ISourceControlOperation::Create<FDeleteWorkspace>(WorkspaceName)) != ECommandResult::Succeeded)
		{
			UE_LOG(LogVirtualization, Warning, TEXT("[%s] Failed to remove temp workspace '%s' please delete manually"), *GetDebugName(), WorkspaceName.ToString());
		}
	};

	FString OriginalWorkspace;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData::SwitchWorkspace);

		FSourceControlResultInfo SwitchToNewWorkspaceInfo;
		if (SCCProvider->SwitchWorkspace(WorkspaceName, SwitchToNewWorkspaceInfo, &OriginalWorkspace) != ECommandResult::Succeeded)
		{
			UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to switch to temp workspace '%s' when trying to submit payloads"),
				*GetDebugName(),
				WorkspaceName.ToString());

			return false;
		}
	}

	ON_SCOPE_EXIT
	{
		FSourceControlResultInfo SwitchToOldWorkspaceInfo;
		if (SCCProvider->SwitchWorkspace(OriginalWorkspace, SwitchToOldWorkspaceInfo, nullptr) != ECommandResult::Succeeded)
		{
			UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to restore the original workspace '%s'"), *GetDebugName(), *OriginalWorkspace);
		}
	};

	const int32 NumBatches = FMath::DivideAndRoundUp<int32>(RequestsToPush.Num(), MaxBatchCount);

	UE_LOG(LogVirtualization, Log, TEXT("[%s] Splitting the submission into '%d' batches"), *GetDebugName(), NumBatches);

	for (int32 BatchIndex = 0; BatchIndex < NumBatches; ++BatchIndex)
	{
		UE_LOG(LogVirtualization, Log, TEXT("[%s] Processing batch %d/%d..."), *GetDebugName(), BatchIndex + 1, NumBatches);

		const int32 BatchStart = BatchIndex * MaxBatchCount;
		const int32 BatchEnd = FMath::Min((BatchIndex + 1) * MaxBatchCount, RequestsToPush.Num());

		TArrayView<const FPushRequest*> RequestBatch(&RequestsToPush[BatchStart], BatchEnd - BatchStart);

		TArray<FString> FilesToSubmit;
		FilesToSubmit.Reserve(RequestBatch.Num());

		// Write the payloads to disk so that they can be submitted (source control module currently requires the files to
		// be on disk)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData::CreateFiles);

			for (const FPushRequest* Request : RequestBatch)
			{
				check(Request != nullptr);

				TStringBuilder<52> LocalPayloadPath;
				Utils::PayloadIdToPath(Request->GetIdentifier(), LocalPayloadPath);

				TStringBuilder<260> PayloadFilePath;
				FPathViews::Append(PayloadFilePath, SessionDirectory, LocalPayloadPath);

				UE_LOG(LogVirtualization, Verbose, TEXT("[%s] Writing payload to '%s' for submission"), *GetDebugName(), PayloadFilePath.ToString());

				FCompressedBuffer Payload = Request->GetPayload();
				if (Payload.IsNull())
				{
					UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to acquire payload '%s' contents to '%s' for writing"),
						*GetDebugName(),
						*LexToString(Request->GetIdentifier()),
						PayloadFilePath.ToString());

					return false;
				}

				TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileWriter(PayloadFilePath.ToString()));
				if (!FileAr)
				{
					TStringBuilder<MAX_SPRINTF> SystemErrorMsg;
					Utils::GetFormattedSystemError(SystemErrorMsg);

					UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to write payload '%s' contents to '%s' due to system error: %s"),
						*GetDebugName(),
						*LexToString(Request->GetIdentifier()),
						PayloadFilePath.ToString(),
						SystemErrorMsg.ToString());

					return false;
				}

				Payload.Save(*FileAr);

				if (!FileAr->Close())
				{
					TStringBuilder<MAX_SPRINTF> SystemErrorMsg;
					Utils::GetFormattedSystemError(SystemErrorMsg);

					UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to write payload '%s' contents to '%s' due to system error: %s"),
						*GetDebugName(),
						*LexToString(Request->GetIdentifier()),
						*PayloadFilePath,
						SystemErrorMsg.
						ToString());

					return false;
				}

				FilesToSubmit.Emplace(MoveTemp(PayloadFilePath));
			}
		}

		check(RequestBatch.Num() == FilesToSubmit.Num());

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData::AddFiles);

			if (SCCProvider->Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToSubmit) != ECommandResult::Succeeded)
			{
				UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to mark the payload file for Add in source control"), *GetDebugName());
				return false;
			}
		}

		// Now submit the payload
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FSourceControlBackend::PushData::SubmitFiles);

			TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = ISourceControlOperation::Create<FCheckIn>();

			TStringBuilder<512> Description;
			CreateDescription(ProjectName, RequestBatch, Description);

			CheckInOperation->SetDescription(FText::FromString(Description.ToString()));

			if (SCCProvider->Execute(CheckInOperation, FilesToSubmit) != ECommandResult::Succeeded)
			{
				UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to submit the payload file(s) to source control"), *GetDebugName());
				return false;
			}
		}

		for (const FPushRequest* Request : RequestBatch)
		{
			PayloadStatusMap[Request->GetIdentifier()] = FPushResult::GetAsPushed();
		}

		// Try to clean up the files from this batch
		for (const FString& FilePath : FilesToSubmit)
		{
			const bool bRequireExists = false;
			const bool bEvenReadOnly = true;
			const bool bQuiet = false;

			IFileManager::Get().Delete(*FilePath, bRequireExists, bEvenReadOnly, bQuiet);
		}
	}

	// Finally set all of the request statuses
	for (FPushRequest& Request : Requests)
	{
		Request.SetResult(PayloadStatusMap[Request.GetIdentifier()]);
	}

	return true;
}

bool FSourceControlBackend::DoPayloadsExist(TArrayView<const FIoHash> PayloadIds, TArray<bool>& OutResults)
{
	TArray<FString> DepotPaths;
	DepotPaths.Reserve(PayloadIds.Num());

	TArray<FSourceControlStateRef> PathStates;

	for (const FIoHash& PayloadId : PayloadIds)
	{
		if (!PayloadId.IsZero())
		{
			TStringBuilder<52> LocalPayloadPath;
			Utils::PayloadIdToPath(PayloadId, LocalPayloadPath);

			DepotPaths.Emplace(WriteToString<512>(DepotRoot, LocalPayloadPath));
		}
	}

	ECommandResult::Type Result = GetDepotPathStates(*SCCProvider, DepotPaths, PathStates);
	if (Result != ECommandResult::Type::Succeeded)
	{
		UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to query the state of files in the source control depot"), *GetDebugName());
		return false;
	}

	check(DepotPaths.Num() == PathStates.Num()); // We expect that all paths return a state

	OutResults.SetNum(PayloadIds.Num());

	int32 StatusIndex = 0;
	for (int32 Index = 0; Index < PayloadIds.Num(); ++Index)
	{
		if (!PayloadIds[Index].IsZero())
		{
			OutResults[Index] = PathStates[StatusIndex++]->IsSourceControlled();
		}
	}

	return true;
}

bool FSourceControlBackend::TryApplySettingsFromConfigFiles(const FString& ConfigEntry)
{
	// We require that a valid depot root has been provided
	{
		if (!FParse::Value(*ConfigEntry, TEXT("DepotRoot="), DepotRoot))
		{
			UE_LOG(LogVirtualization, Error, TEXT("'DepotRoot=' not found in the config file"));
			return false;
		}

		if (!DepotRoot.EndsWith(TEXT("/")))
		{
			DepotRoot.AppendChar(TEXT('/'));
		}
	}

	// Now parse the optional config values

	{
		FParse::Value(*ConfigEntry, TEXT("Server="), ServerAddress);

		if (!ClientStream.IsEmpty())
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Using server address: '%s'"), *GetDebugName(), *ServerAddress);
		}
		else
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Will connect to the default server address"), *GetDebugName());
		}
	}

	// If the depot root is within a perforce stream then we must specify which stream. This may be a virtual stream with a custom view.
	{
		FParse::Value(*ConfigEntry, TEXT("ClientStream="), ClientStream);
		if (!ClientStream.IsEmpty())
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Using client stream: '%s'"), *GetDebugName(), *ClientStream);
		}
		else
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Not using client stream"), *GetDebugName());
		}
	}

	// Check to see if we should use partitioned clients or not. This is a perforce specific optimization to make the workspace churn cheaper on the server
	{
		FParse::Bool(*ConfigEntry, TEXT("UsePartitionedClient="), bUsePartitionedClient);
		UE_LOG(LogVirtualization, Log, TEXT("[%s] Using partitioned clients: '%s'"), *GetDebugName(), bUsePartitionedClient ? TEXT("true") : TEXT("false"));
	}

	// Allow the source control backend to retry failed pulls
	{
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

		UE_LOG(LogVirtualization, Log, TEXT("[%s] Will retry failed downloads attempts %d time(s) with a gap of %dms betwen them"), *GetDebugName(), RetryCount, RetryWaitTimeMS);
	}

	// Allow the number of concurrent connections to be limited
	{
		int32 MaxLimit = 8; // We use the UGS max of 8 as the default
		FParse::Value(*ConfigEntry, TEXT("MaxConnections="), MaxLimit);

		if (MaxLimit != INDEX_NONE)
		{
			ConcurrentConnectionLimit = MakeUnique<FSemaphore>(MaxLimit);
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Limted to %d concurrent source control connections"), *GetDebugName(), MaxLimit);
		}
		else
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Has no limit to it's concurrent source control connections"), *GetDebugName());
		}
	}

	// Check for the optional BatchCount parameter
	{
		int32 MaxBatchCountIniFile = 0;
		if (FParse::Value(*ConfigEntry, TEXT("MaxBatchCount="), MaxBatchCountIniFile))
		{
			MaxBatchCount = MaxBatchCountIniFile;
		}

		UE_LOG(LogVirtualization, Log, TEXT("[%s] Will push payloads in batches of up to %d payloads(s) at a time"), *GetDebugName(), MaxBatchCount);
	}

	// Check to see if connection error notification pop ups should be shown or not
	{
		FParse::Bool(*ConfigEntry, TEXT("SuppressNotifications="), bSuppressNotifications);

		if (bSuppressNotifications)
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Connection pop up warnings are supressed"), *GetDebugName());
		}
		else
		{
			UE_LOG(LogVirtualization, Log, TEXT("[%s] Connection pop up warnings will be shown"), *GetDebugName());
		}
	}

	if (!FindSubmissionWorkingDir(ConfigEntry))
	{
		return false;
	}

	return true;
}

void FSourceControlBackend::CreateDepotPath(const FIoHash& PayloadId, FStringBuilderBase& OutPath)
{
	TStringBuilder<52> PayloadPath;
	Utils::PayloadIdToPath(PayloadId, PayloadPath);

	OutPath << DepotRoot << PayloadPath;
}

bool FSourceControlBackend::FindSubmissionWorkingDir(const FString& ConfigEntry)
{
	// Note regarding path lengths.
	// During submission each payload path will be 90 characters in length which will then be appended to
	// the SubmissionWorkingDir

	SubmissionRootDir = FPlatformMisc::GetEnvironmentVariable(TEXT("UE-VirtualizationWorkingDir"));

	if (!SubmissionRootDir.IsEmpty())
	{
		FPaths::NormalizeDirectoryName(SubmissionRootDir);
		UE_LOG(LogVirtualization, Log, TEXT("[%s] Found Environment Variable: UE-VirtualizationWorkingDir"), *GetDebugName());	
	}
	else
	{

		bool bSubmitFromTempDir = false;
		FParse::Bool(*ConfigEntry, TEXT("SubmitFromTempDir="), bSubmitFromTempDir);

		TStringBuilder<260> PathBuilder;
		if (bSubmitFromTempDir)
		{
			FPathViews::Append(PathBuilder, FPlatformProcess::UserTempDir(), TEXT("UnrealEngine/VASubmission"));	
		}
		else
		{
			FPathViews::Append(PathBuilder, FPaths::ProjectSavedDir(), TEXT("VASubmission"));
		}

		SubmissionRootDir = PathBuilder;
	}

	if (IFileManager::Get().DirectoryExists(*SubmissionRootDir) || IFileManager::Get().MakeDirectory(*SubmissionRootDir))
	{
		UE_LOG(LogVirtualization, Log, TEXT("[%s] Setting '%s' as the working directory"), *GetDebugName(), *SubmissionRootDir);
		return true;
	}
	else
	{
		TStringBuilder<MAX_SPRINTF> SystemErrorMsg;
		Utils::GetFormattedSystemError(SystemErrorMsg);

		UE_LOG(LogVirtualization, Error, TEXT("[%s] Failed to set the  working directory to '%s' due to %s"), *GetDebugName(), *SubmissionRootDir, SystemErrorMsg.ToString());
		SubmissionRootDir.Empty();

		return false;
	}
}

void FSourceControlBackend::OnConnectionError()
{
	auto Callback = [](float Delta)->bool
	{
		FMessageLog Log("LogVirtualization");
		Log.Notify(LOCTEXT("ConnectionError", "Asset virtualization connect errors were encountered, see the message log for more info"));

		// This tick callback is one shot, so return false to prevent it being invoked again
		return false;
	};

	if (!bSuppressNotifications)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(Callback));
	}
}

UE_REGISTER_VIRTUALIZATION_BACKEND_FACTORY(FSourceControlBackend, P4SourceControl);
UE_REGISTER_VIRTUALIZATION_BACKEND_FACTORY_LEGACY(FSourceControlBackend, SourceControl, P4SourceControl);

} // namespace UE::Virtualization

#undef LOCTEXT_NAMESPACE
