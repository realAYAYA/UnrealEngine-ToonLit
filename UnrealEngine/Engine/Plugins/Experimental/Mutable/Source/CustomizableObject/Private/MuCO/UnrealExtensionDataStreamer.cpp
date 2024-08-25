// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealExtensionDataStreamer.h"

#include "Algo/Find.h"
#include "Algo/IndexOf.h"
#include "Async/Async.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "Templates/SharedPointer.h"

FUnrealExtensionDataStreamer::FUnrealExtensionDataStreamer(const TWeakObjectPtr<UCustomizableObjectSystemPrivate>& InSystemPrivateWeak)
{
	check(IsInGameThread());

	SystemPrivate = InSystemPrivateWeak;
	Mutex = new FCriticalSection();
}

FUnrealExtensionDataStreamer::~FUnrealExtensionDataStreamer()
{
	check(IsInGameThread());

	CancelPendingLoads();

	delete Mutex;
}

void FUnrealExtensionDataStreamer::SetActiveObject(UCustomizableObject* InObject)
{
	check(IsInGameThread());
	check(InObject);

	FScopeLock Lock(Mutex);

	ActiveObject = InObject;
}

void FUnrealExtensionDataStreamer::ClearActiveObject()
{
	check(IsInGameThread());

	FScopeLock Lock(Mutex);

	ActiveObject = nullptr;
}

bool FUnrealExtensionDataStreamer::AreAnyLoadsPending() const
{
	FScopeLock Lock(Mutex);

	return PendingLoads.Num() > 0;
}

void FUnrealExtensionDataStreamer::CancelPendingLoads()
{
	// Needs to be called from the Game thread, as FStreamableHandle::CancelHandle requires this.
	//
	// Restricting this to the Game thread also guarantees that any tasks cancelled by this
	// function will definitely not have started yet, since they only run on the Game thread.
	check(IsInGameThread());

	FScopeLock Lock(Mutex);

	bool bAnyTasksNeedCancelling = false;
	for (const FPendingLoad& PendingLoad : PendingLoads)
	{
		if (!PendingLoad.TaskFuture.IsReady())
		{
			// This task was queued to be run on the Game thread but hasn't run yet
			bAnyTasksNeedCancelling = true;
			continue;
		}

		TSharedPtr<FStreamableHandle> Handle = PendingLoad.TaskFuture.Get();
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}

	if (bAnyTasksNeedCancelling)
	{
		// Set the shared cancel value to true so that whenever the tasks actually execute they
		// will cancel immediately.
		check(ShouldCancelPtr.IsValid());
		*ShouldCancelPtr = true;

		// The next task to be queued should allocate a new cancel value to ensure it doesn't
		// interfere with the previous one.
		ShouldCancelPtr = nullptr;
	}

	PendingLoads.Empty();
}

mu::ExtensionDataPtr FUnrealExtensionDataStreamer::CloneExtensionData(const mu::ExtensionDataPtrConst& Source)
{
	unimplemented();
	return nullptr;
}

TSharedRef<const mu::FExtensionDataLoadHandle> FUnrealExtensionDataStreamer::StartLoad(
	const mu::ExtensionDataPtrConst& Data,
	TArray<mu::ExtensionDataPtrConst>& OutUnloadedConstants)
{
	// Note that this can be called from any thread

	OutUnloadedConstants.Reset();

	TSharedRef<mu::FExtensionDataLoadHandle> LoadHandle = MakeShared<mu::FExtensionDataLoadHandle>();

	LoadHandle->Data = Data;
	LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::Pending;

	{
		FScopeLock Lock(Mutex);

		if (!ShouldCancelPtr.IsValid())
		{
			ShouldCancelPtr = MakeShared<bool>(false);
		}

		TFuture<TSharedPtr<FStreamableHandle>> LoadHandleFuture = Async(EAsyncExecution::TaskGraphMainThread,
			[
				ShouldCancelRef = ShouldCancelPtr.ToSharedRef(),
				LoadHandle,
				ObjectToLoadFor = ActiveObject,
				SystemPrivate = SystemPrivate
			]()
			{
				if (*ShouldCancelRef)
				{
					// Load was cancelled
					LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::FailedToLoad;
					return TSharedPtr<FStreamableHandle>();
				}

				return StartLoadOnGameThread(SystemPrivate, ObjectToLoadFor, LoadHandle);
			});


		PendingLoads.Emplace(LoadHandle, MoveTemp(LoadHandleFuture));
	}

	return LoadHandle;
}

// Note that this function only returns an FStreamableHandle if it needed to start a load.
//
// If the requested object is already loaded, this function can return nullptr and still be
// considered successful.
TSharedPtr<FStreamableHandle> FUnrealExtensionDataStreamer::StartLoadOnGameThread(
	const TWeakObjectPtr<UCustomizableObjectSystemPrivate>& SystemPrivateWeak,
	const TWeakObjectPtr<UCustomizableObject>& ObjectToLoadFor,
	const TSharedRef<mu::FExtensionDataLoadHandle>& LoadHandle)
{
	check(IsInGameThread());
	check(LoadHandle->Data->Origin == mu::ExtensionData::EOrigin::ConstantStreamed);

	UCustomizableObjectSystemPrivate* SystemPrivate = SystemPrivateWeak.Get();
	if (!SystemPrivate)
	{
		LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::FailedToLoad;
		return nullptr;
	}
	
	UCustomizableObject* Object = ObjectToLoadFor.Get();
	if (!Object)
	{
		// CO doesn't exist anymore
		LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::FailedToLoad;
		return nullptr;
	}

	if (!Object->GetPrivate()->GetStreamedExtensionData().IsValidIndex(LoadHandle->Data->Index))
	{
		// The compiled data appears to be out of sync with the CO's properties

		UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data with index %d in %s. Compiled data may be stale."),
			LoadHandle->Data->Index, *Object->GetFullName());

		LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::FailedToLoad;
		return nullptr;
	}

	FCustomizableObjectStreamedResourceData& StreamedData = Object->GetPrivate()->GetStreamedExtensionData()[LoadHandle->Data->Index];
	if (StreamedData.IsLoaded())
	{
		// Already loaded

		// Need to call NotifyLoadCompleted to mark the request as complete and remove it from
		// PendingLoads.
		SystemPrivate->ExtensionDataStreamer->NotifyLoadCompleted(Object, LoadHandle);

		return nullptr;
	}

	// Note that this just checks if the path is non-null, NOT if the object is loaded
	check(!StreamedData.GetPath().IsNull());

	TArray<FSoftObjectPath> TargetsToStream;
	TargetsToStream.Add(StreamedData.GetPath().ToSoftObjectPath());

	check(SystemPrivate->ExtensionDataStreamer);
	TFunction<void()> OnLoadComplete = [ExtensionDataStreamer = SystemPrivate->ExtensionDataStreamer, ObjectToLoadFor, LoadHandle]()
	{
		UCustomizableObject* Object = ObjectToLoadFor.Get();
		if (!Object)
		{
			// CO doesn't exist anymore
			return;
		}

		ExtensionDataStreamer->NotifyLoadCompleted(Object, LoadHandle);
	};

	const bool bManageActiveHandle = false;
	const bool bStartStalled = false;

	FString DebugString;
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	DebugString = FString::Printf(TEXT("UnrealExtensionDataStreamer for %s"), *Object->GetPathName());
#endif

	FStreamableManager& Manager = UCustomizableObjectSystem::GetInstance()->GetPrivate()->StreamableManager;

	return Manager.RequestAsyncLoad(
		TargetsToStream,
		MoveTemp(OnLoadComplete),
		FStreamableManager::DefaultAsyncLoadPriority,
		bManageActiveHandle,
		bStartStalled,
		DebugString);
}

void FUnrealExtensionDataStreamer::NotifyLoadCompleted(
	UCustomizableObject* Object,
	const TSharedRef<mu::FExtensionDataLoadHandle>& LoadHandle)
{
	check(IsInGameThread());
	check(Object);

	if (!Object->GetPrivate()->GetStreamedExtensionData().IsValidIndex(LoadHandle->Data->Index))
	{
		// The compiled data appears to be out of sync with the CO's properties

		UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data with index %d in %s. Compiled data may be stale."),
			LoadHandle->Data->Index, *Object->GetFullName());

		LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::FailedToLoad;
		return;
	}

	FCustomizableObjectStreamedResourceData& StreamedData = Object->GetPrivate()->GetStreamedExtensionData()[LoadHandle->Data->Index];

	// The object could have been loaded by another request, in which case we can skip updating
	// the StreamedData.
	if (!StreamedData.IsLoaded())
	{
		const UCustomizableObjectResourceDataContainer* LoadedObject = StreamedData.GetPath().Get();
		if (!LoadedObject)
		{
			// Object wasn't loaded for some reason
			LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::FailedToLoad;
			return;
		}

		StreamedData.NotifyLoaded(LoadedObject);
	}

	LoadHandle->LoadState = mu::FExtensionDataLoadHandle::ELoadState::Loaded;

	{
		FScopeLock Lock(Mutex);

		const int32 Index = Algo::IndexOfBy(PendingLoads, LoadHandle, &FPendingLoad::LoadHandle);
		// If the load was previously cancelled, CancelHandle should have prevented this callback from
		// running, so at this point the load should still be in PendingLoads.
		check(Index != INDEX_NONE);

		PendingLoads.RemoveAtSwap(Index);
	}
}
