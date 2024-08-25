// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlAssetDataCache.h"

#include "Async/Async.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ISourceControlModule.h"
#include "ISourceControlState.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

#if WITH_EDITOR
#include "GameFramework/Actor.h"
#endif

static TAutoConsoleVariable<int32> CVarSourceControlAssetDataCacheMaxAsyncTask(
	TEXT("SourceControlAssetDataCache.MaxAsyncTask"),
	8,
	TEXT("Maximum number of task running in parallel to fetch AssetData information.")
);

void FSourceControlAssetDataCache::Startup()
{
	ISourceControlModule& SCCModule = ISourceControlModule::Get();

	ProviderChangedDelegateHandle = SCCModule.RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateLambda([](ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
	{
		ISourceControlModule::Get().GetAssetDataCache().OnSourceControlProviderChanged(OldProvider, NewProvider);
	}));

#if WITH_EDITOR
	ActorLabelChangedDelegateHandle = FCoreDelegates::OnActorLabelChanged.AddLambda([](AActor* ChangedActor)
	{
		if (ensure(ChangedActor))
		{
			if (UPackage* Package = ChangedActor->GetPackage())
			{
				FString Filename = USourceControlHelpers::PackageFilename(Package);
				ISourceControlModule::Get().GetAssetDataCache().ClearAssetData(Filename);
			}
		}
	});
#endif

	bIsSourceControlDialogShown = false;
}

void FSourceControlAssetDataCache::Shutdown()
{
#if WITH_EDITOR
	FCoreDelegates::OnActorLabelChanged.Remove(ActorLabelChangedDelegateHandle);
#endif

	ISourceControlModule& SCCModule = ISourceControlModule::Get();
	SCCModule.UnregisterProviderChanged(ProviderChangedDelegateHandle);

	ClearPendingTasks();
}

bool FSourceControlAssetDataCache::GetAssetDataArray(FSourceControlStateRef InFileState, FAssetDataArrayPtr& OutAssetDataArrayPtr)
{
	if (bIsSourceControlDialogShown)
	{
		return false;
	}

	FSourceControlAssetDataEntry* AssetDataEntry = AssetDataCache.Find(InFileState->GetFilename());

	if (AssetDataEntry == nullptr)
	{
		AssetDataEntry = AddAssetInformationEntry(InFileState);
	}

	check(AssetDataEntry != nullptr);

	if (AssetDataEntry->bInitialized)
	{
		OutAssetDataArrayPtr = AssetDataEntry->AssetDataArrayPtr;
		return true;
	}

	return false;
}

FSourceControlAssetDataEntry* FSourceControlAssetDataCache::AddAssetInformationEntry(FSourceControlStateRef InFileState)
{
	FString Filename = InFileState->GetFilename();
	FSourceControlAssetDataEntry* AssetDataEntry = &AssetDataCache.Add(Filename);

	check(AssetDataEntry != nullptr);
	check(!AssetDataEntry->AssetDataArrayPtr.IsValid());

	AssetDataEntry->bInitialized = false;
	AssetDataEntry->AssetDataArrayPtr = MakeShared<TArray<FAssetData>>();

	/* For deleted items, the file is not on disk anymore so the only way we can get the asset data is by getting the file from the depot.
	 * For shelved files, if the file still exists locally, it will have been found before, otherwise, the history of the shelved file state will point to the remote version
	 */
	if (!InFileState->IsDeleted())
	{
		AssetDataEntry->bInitialized = USourceControlHelpers::GetAssetData(Filename, *AssetDataEntry->AssetDataArrayPtr);
	}

	// At the moment, getting the asset data from non-external assets yields issues with the package path
	if (IAssetRegistry::Get()->IsPathBeautificationNeeded(Filename) && (InFileState->IsDeleted() || (AssetDataEntry->AssetDataArrayPtr->Num() == 0)))
	{
		check(!AssetDataEntry->bInitialized);

		if (InFileState->GetHistorySize() == 0)
		{
			FileHistoryToUpdate.Add(Filename);
		}
		else
		{
			AssetDataToFetch.Enqueue(InFileState);
		}
	}
	else
	{
		// We either already have AssetData or we can't go further with this asset.
		AssetDataEntry->bInitialized = true;
	}

	return AssetDataEntry;
}

void FSourceControlAssetDataCache::Tick()
{
	GetFileHistory();

	LaunchFetchAssetDataTasks();

	UpdatePendingAssetData();
}

void FSourceControlAssetDataCache::OnSourceControlDialogShown()
{
	ClearPendingTasks();
	bIsSourceControlDialogShown = true;
}

void FSourceControlAssetDataCache::OnSourceControlDialogClosed()
{
	bIsSourceControlDialogShown = false;
}

void FSourceControlAssetDataCache::GetFileHistory()
{
	if (FileHistoryToUpdate.Num() == 0)
	{
		return;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);
	UpdateStatusOperation->SetQuiet(true);

	SourceControlProvider.Execute(UpdateStatusOperation,
								  FileHistoryToUpdate,
								  EConcurrency::Asynchronous,
								  FSourceControlOperationComplete::CreateLambda([FileHistoryToUpdateCopy = FileHistoryToUpdate](const FSourceControlOperationRef& Operation, ECommandResult::Type InResult)
	{
		ISourceControlModule::Get().GetAssetDataCache().OnUpdateHistoryComplete(FileHistoryToUpdateCopy, Operation, InResult);
	}));

	FileHistoryToUpdate.Reset();
}

void FSourceControlAssetDataCache::OnUpdateHistoryComplete(const TArray<FString>& InUpdatedFiles, const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	if (InResult == ECommandResult::Failed)
	{
		UE_LOG(LogSourceControl, Warning, TEXT("UpdateHistory Failed."));
		return;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	for (const FString& Filename : InUpdatedFiles)
	{
		// The AssetDataCache container entry might have been removed during FUpdateStatus.
		if (FSourceControlAssetDataEntry* AssetDataEntry = AssetDataCache.Find(Filename))
		{
			FSourceControlStatePtr FileState = SourceControlProvider.GetState(Filename, EStateCacheUsage::Use);
			check(FileState.IsValid());

			if (FileState->GetHistorySize() > 0)
			{
				AssetDataToFetch.Enqueue(FileState);
			}
			else
			{
				// History was not updated we probably cannot go further with this asset.
				AssetDataEntry->bInitialized = true;
			}
		}
	}
}

void FSourceControlAssetDataCache::ClearAssetData(const FString& Filename)
{
	if (FSourceControlAssetDataEntry* AssetDataEntry = AssetDataCache.Find(Filename))
	{
		if (AssetDataEntry->bInitialized)
		{
			AssetDataCache.Remove(Filename);
		}
	}
}

void FSourceControlAssetDataCache::LaunchFetchAssetDataTasks()
{
	FSourceControlStatePtr FileState;
	const uint32 MaxAsyncTask = static_cast<uint32>(CVarSourceControlAssetDataCacheMaxAsyncTask.GetValueOnGameThread());

	check(MaxAsyncTask > 0);

	while ((CurrentAsyncTask < MaxAsyncTask) && AssetDataToFetch.Dequeue(FileState))
	{
		check(FileState.IsValid());
		FSourceControlStateRef FileStateRef = FileState.ToSharedRef();
		FSourceControlAssetDataEntry* AssetDataEntry = AssetDataCache.Find(FileStateRef->GetFilename());

		check(AssetDataEntry != nullptr);
		++CurrentAsyncTask;
		AssetDataEntry->FetchAssetDataTask = Async(EAsyncExecution::TaskGraph,
												   [FileStateRef]() { ISourceControlModule::Get().GetAssetDataCache().FetchAssetData(FileStateRef); },
												   []() { ISourceControlModule::Get().GetAssetDataCache().OnFetchAssetDataComplete(); });
	}
}

void FSourceControlAssetDataCache::FetchAssetData(FSourceControlStateRef InFileState)
{
	check(InFileState->GetHistorySize() > 0);
	
	FString TempFileName;
	TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = InFileState->GetHistoryItem(0);
	FSourceControlAssetDataEntry* AssetInformation = AssetDataCache.Find(InFileState->GetFilename());

	check(AssetInformation != nullptr);
	check(!AssetInformation->bInitialized);
	check(AssetInformation->AssetDataArrayPtr.IsValid());
	check(Revision.IsValid());

	const int64 MaxFetchSize = (1 << 20); // 1MB
	const bool bShouldGetFile = (MaxFetchSize < 0 || MaxFetchSize > static_cast<int64>(Revision->GetFileSize()));

	if (bShouldGetFile && Revision->Get(TempFileName, EConcurrency::Asynchronous))
	{
		FAssetDataToLoad ToLoad = { TempFileName, InFileState->GetFilename() };
		AssetDataToLoad.Enqueue(MoveTemp(ToLoad));
	}
}

void FSourceControlAssetDataCache::OnFetchAssetDataComplete()
{
	--CurrentAsyncTask;
}

void FSourceControlAssetDataCache::UpdatePendingAssetData()
{
	FAssetDataToLoad ToLoad;

	while (AssetDataToLoad.Dequeue(ToLoad))
	{
		FSourceControlAssetDataEntry* AssetDataEntry = AssetDataCache.Find(ToLoad.Filename);

		check(AssetDataEntry != nullptr);
		USourceControlHelpers::GetAssetData(ToLoad.TempFilename, *(AssetDataEntry->AssetDataArrayPtr), nullptr);

		// We either already have AssetData or we can't go further with this asset.
		AssetDataEntry->bInitialized = true;
	}
}

void FSourceControlAssetDataCache::OnSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	ClearPendingTasks();
}

void FSourceControlAssetDataCache::ClearPendingTasks()
{
	TArray<FString> EntriesToRemove;

	// Wait for tasks to stop
	for (auto& Pair : AssetDataCache)
	{
		FAssetDataCache::KeyType& Filename = Pair.Key;
		FAssetDataCache::ValueType& AssetDataEntry = Pair.Value;

		if (AssetDataEntry.bInitialized)
		{
			continue;
		}

		AssetDataEntry.FetchAssetDataTask.Wait();
		AssetDataEntry.FetchAssetDataTask.Reset();
		EntriesToRemove.Add(Filename);
	}

	// We remove all previously pending entries, they will restart the fetching process if needed again
	for (const FString& Entry : EntriesToRemove)
	{
		AssetDataCache.Remove(Entry);
	}

	// Clear queues
	FileHistoryToUpdate.Reset();
	AssetDataToFetch.Empty();
	AssetDataToLoad.Empty();
}
