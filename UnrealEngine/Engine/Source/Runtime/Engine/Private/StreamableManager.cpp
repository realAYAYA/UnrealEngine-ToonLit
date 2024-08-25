// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/StreamableManager.h"
#include "UObject/ICookInfo.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectThreadContext.h"
#include "HAL/IConsoleManager.h"
#include "Tickable.h"
#include "Serialization/LoadTimeTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"
#include <type_traits>

DEFINE_LOG_CATEGORY_STATIC(LogStreamableManager, Log, All);

// Default to 1 frame, this will cause the delegates to go off on the next tick to avoid recursion issues. Set higher to fake disk lag
static int32 GStreamableDelegateDelayFrames = 1;
static FAutoConsoleVariableRef CVarStreamableDelegateDelayFrames(
	TEXT("s.StreamableDelegateDelayFrames"),
	GStreamableDelegateDelayFrames,
	TEXT("Number of frames to delay StreamableManager delegates "),
	ECVF_Default
);

// CVar to switch back to legacy behavior of non-specifically flushing async loading when waiting on a request handle
static bool GStreamableFlushAllAsyncLoadRequestsOnWait = 0;
static FAutoConsoleVariableRef CVarStreamableFlushAllAsyncLoadRequestsOnWait(
	TEXT("s.StreamableFlushAllAsyncLoadRequestsOnWait"),
	GStreamableFlushAllAsyncLoadRequestsOnWait,
	TEXT("Flush async loading without a specific request ID when waiting on streamable handles."),
	ECVF_Default
);


const FString FStreamableHandle::HandleDebugName_Preloading = FString(TEXT("Preloading"));
const FString FStreamableHandle::HandleDebugName_AssetList = FString(TEXT("LoadAssetList"));
const FString FStreamableHandle::HandleDebugName_CombinedHandle = FString(TEXT("CreateCombinedHandle"));

/** Helper class that defers streamable manager delegates until the next frame */
class FStreamableDelegateDelayHelper : public FTickableGameObject
{
public:

	/** Adds a delegate to deferred list */
	template<typename InStreamableDelegate>
	void AddDelegate(InStreamableDelegate&& Delegate, InStreamableDelegate&& CancelDelegate, TSharedPtr<FStreamableHandle> AssociatedHandle)
	{
		FPendingDelegate* PendingDelegate = new FPendingDelegate(Forward<InStreamableDelegate>(Delegate), Forward<InStreamableDelegate>(CancelDelegate), AssociatedHandle);

		{
			FScopeLock Lock(&DataLock);

			FPendingDelegateList& DelegatesForHandle = PendingDelegatesByHandle.FindOrAdd(AssociatedHandle);
			FPendingDelegateList::AddTail(PendingDelegate, PendingDelegates, DelegatesForHandle);
		}
	}

	/** Cancels delegate for handle, this will either delete the delegate or replace with the cancel delegate */
	void CancelDelegatesForHandle(TSharedPtr<FStreamableHandle> AssociatedHandle)
	{
		FPendingDelegateList PendingDelegatesToDelete;

		{
			FScopeLock _(&DataLock);

			FPendingDelegateList* DelegatesForHandle = PendingDelegatesByHandle.Find(AssociatedHandle);
			if (!DelegatesForHandle)
			{
				return;
			}

			FPendingDelegate* CurrentNode = DelegatesForHandle->Head;
			while (CurrentNode)
			{
				FPendingDelegate* NextNode = CurrentNode->NextByHandle;
				if (CurrentNode->CancelDelegate.IsBound())
				{
					// Replace with cancel delegate
					CurrentNode->Delegate = CurrentNode->CancelDelegate;
					CurrentNode->CancelDelegate.Unbind();
				}
				else
				{
					// Remove entirely
					FPendingDelegateList::Unlink(CurrentNode, PendingDelegates, *DelegatesForHandle);
					if (PendingDelegatesToDelete.Tail)
					{
						PendingDelegatesToDelete.Tail->Next = CurrentNode;
						PendingDelegatesToDelete.Tail = CurrentNode;
					}
					else
					{
						PendingDelegatesToDelete.Head = PendingDelegatesToDelete.Tail = CurrentNode;
					}
					CurrentNode->Next = nullptr;
				}
				CurrentNode = NextNode;
			}
			if (!DelegatesForHandle->Head)
			{
				RemovePendingDelegateInternal(AssociatedHandle);
			}
		}

		FPendingDelegate* NodeToDelete = PendingDelegatesToDelete.Head;
		while (NodeToDelete)
		{
			FPendingDelegate* NextNodeToDelete = NodeToDelete->Next;
			delete NodeToDelete;
			NodeToDelete = NextNodeToDelete;
		}
	}

	/** Calls all delegates, call from synchronous flushes */
	void FlushDelegates()
	{
		while (PendingDelegates.Head)
		{
			Tick(0.0f);
		}
	}

	// FTickableGameObject interface

	void Tick(float DeltaTime) override
	{
		if (!PendingDelegates.Head)
		{
			return;
		}

		FPendingDelegateList DelegatesToCall;
		{
			FScopeLock Lock(&DataLock);

			FPendingDelegate* CurrentNode = PendingDelegates.Head;
			while (CurrentNode)
			{
				FPendingDelegate* NextNode = CurrentNode->Next;
				if (--CurrentNode->DelayFrames <= 0)
				{
					// Add to call array and remove from tracking one
					FPendingDelegateList* DelegatesForHandle = PendingDelegatesByHandle.Find(CurrentNode->RelatedHandle);
					check(DelegatesForHandle);
					FPendingDelegateList::Unlink(CurrentNode, PendingDelegates, *DelegatesForHandle);
					if (DelegatesToCall.Tail)
					{
						DelegatesToCall.Tail->Next = CurrentNode;
						DelegatesToCall.Tail = CurrentNode;
					}
					else
					{
						DelegatesToCall.Head = DelegatesToCall.Tail = CurrentNode;
					}
					if (!DelegatesForHandle->Head)
					{
						RemovePendingDelegateInternal(CurrentNode->RelatedHandle);
					}
				}
				CurrentNode = NextNode;
			}
		}

		FPendingDelegate* DelegateToCall = DelegatesToCall.Head;
		while (DelegateToCall)
		{
			FPendingDelegate* NextDelegateToCall = DelegateToCall->Next;
			DelegateToCall->Delegate.ExecuteIfBound();
			delete DelegateToCall;
			DelegateToCall = NextDelegateToCall;
		}
	}

	virtual ETickableTickType GetTickableTickType() const override
	{
		return ETickableTickType::Always;
	}

	virtual bool IsTickableWhenPaused() const override
	{
		return true;
	}

	virtual bool IsTickableInEditor() const
	{
		return true;
	}

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FStreamableDelegateDelayHelper, STATGROUP_Tickables);
	}

private:

	struct FPendingDelegate
	{
		FPendingDelegate* Prev;
		FPendingDelegate* Next;

		FPendingDelegate* PrevByHandle;
		FPendingDelegate* NextByHandle;

		/** Delegate to call when frames are up */
		FStreamableDelegate Delegate;

		/** Delegate to call if this gets cancelled early */
		FStreamableDelegate CancelDelegate;

		/** Handle related to delegates, needs to keep these around to avoid things GCing before the user callback goes off. This may be null */
		TSharedPtr<FStreamableHandle> RelatedHandle;

		/** Frames left to delay */
		int32 DelayFrames;

		template<typename InStreamableDelegate>
		FPendingDelegate(InStreamableDelegate&& InDelegate, InStreamableDelegate&& InCancelDelegate, TSharedPtr<FStreamableHandle> InHandle)
			: Delegate(Forward<InStreamableDelegate>(InDelegate))
			, CancelDelegate(Forward<InStreamableDelegate>(InCancelDelegate))
			, RelatedHandle(MoveTemp(InHandle))
			, DelayFrames(GStreamableDelegateDelayFrames)
		{}
	};

	struct FPendingDelegateList
	{
		FPendingDelegate* Head = nullptr;
		FPendingDelegate* Tail = nullptr;

		static void Unlink(FPendingDelegate* PendingDelegate, FPendingDelegateList& PendingDelegates, FPendingDelegateList& PendingDelegatesByHandle)
		{
			if (PendingDelegate->Prev)
			{
				PendingDelegate->Prev->Next = PendingDelegate->Next;
			}
			else
			{
				check(PendingDelegate == PendingDelegates.Head);
				PendingDelegates.Head = PendingDelegate->Next;
			}
			if (PendingDelegate->Next)
			{
				PendingDelegate->Next->Prev = PendingDelegate->Prev;
			}
			else
			{
				check(PendingDelegate == PendingDelegates.Tail);
				PendingDelegates.Tail = PendingDelegate->Prev;
			}
			PendingDelegate->Next = PendingDelegate->Prev = nullptr;

			if (PendingDelegate->PrevByHandle)
			{
				PendingDelegate->PrevByHandle->NextByHandle = PendingDelegate->NextByHandle;
			}
			else
			{
				check(PendingDelegate == PendingDelegatesByHandle.Head);
				PendingDelegatesByHandle.Head = PendingDelegate->NextByHandle;
			}
			if (PendingDelegate->NextByHandle)
			{
				PendingDelegate->NextByHandle->PrevByHandle = PendingDelegate->PrevByHandle;
			}
			else
			{
				check(PendingDelegate == PendingDelegatesByHandle.Tail);
				PendingDelegatesByHandle.Tail = PendingDelegate->PrevByHandle;
			}
			PendingDelegate->NextByHandle = PendingDelegate->PrevByHandle = nullptr;

		}

		static void AddTail(FPendingDelegate* PendingDelegate, FPendingDelegateList& PendingDelegates, FPendingDelegateList& PendingDelegatesByHandle)
		{
			if (PendingDelegates.Tail)
			{
				PendingDelegates.Tail->Next = PendingDelegate;
				PendingDelegate->Prev = PendingDelegates.Tail;
				PendingDelegate->Next = nullptr;
				PendingDelegates.Tail = PendingDelegate;
			}
			else
			{
				check(!PendingDelegates.Head);
				PendingDelegates.Head = PendingDelegates.Tail = PendingDelegate;
				PendingDelegate->Next = PendingDelegate->Prev = nullptr;
			}

			if (PendingDelegatesByHandle.Tail)
			{
				PendingDelegatesByHandle.Tail->NextByHandle = PendingDelegate;
				PendingDelegate->PrevByHandle = PendingDelegatesByHandle.Tail;
				PendingDelegate->NextByHandle = nullptr;
				PendingDelegatesByHandle.Tail = PendingDelegate;
			}
			else
			{
				check(!PendingDelegatesByHandle.Head);
				PendingDelegatesByHandle.Head = PendingDelegatesByHandle.Tail = PendingDelegate;
				PendingDelegate->NextByHandle = PendingDelegate->PrevByHandle = nullptr;
			}
		}
	};

	inline void RemovePendingDelegateInternal(TSharedPtr<FStreamableHandle> Handle)
	{
		// requires DataLock by caller
		PendingDelegatesByHandle.Remove(Handle);
		if (PendingDelegatesByHandle.IsEmpty())
		{
			// release potentially big allocation after huge batches of streaming requests
			PendingDelegatesByHandle.Empty(128);
		}
	}

	FPendingDelegateList PendingDelegates;
	TMap<TSharedPtr<FStreamableHandle>, FPendingDelegateList> PendingDelegatesByHandle;

	FCriticalSection DataLock;
};

static FStreamableDelegateDelayHelper* StreamableDelegateDelayHelper = nullptr;


TStreamableHandleContextDataTypeID FStreamableHandleContextDataBase::AllocateClassTypeId()
{
	static std::atomic<TStreamableHandleContextDataTypeID> TypeNum{0};
	TStreamableHandleContextDataTypeID Result = TypeNum++;

	checkf(Result != TStreamableHandleContextDataTypeIDInvalid, TEXT("Overflow in TypeNum: too many TStreamableHandleContextData subclasses. Change the TStreamableHandleContextDataTypeID typedef if more subclasses are needed."));
	return Result;
}


FStreamableHandle::FStreamableHandle()
	: bLoadCompleted(false)
	, bReleased(false)
	, bCanceled(false)
	, bStalled(false)
	, bReleaseWhenLoaded(false)
	, bIsCombinedHandle(false)
	, Priority(0)
	, StreamablesLoading(0)
	, OwningManager(nullptr)
#if WITH_EDITOR
	, CookLoadType(ECookLoadType::Unexpected)
#endif
{
}

bool FStreamableHandle::BindCompleteDelegate(FStreamableDelegate NewDelegate)
{
	if (!IsLoadingInProgress())
	{
		// Too late!
		return false;
	}

	CompleteDelegate = MoveTemp(NewDelegate);
	return true;
}

bool FStreamableHandle::BindCancelDelegate(FStreamableDelegate NewDelegate)
{
	if (!IsLoadingInProgress())
	{
		// Too late!
		return false;
	}

	CancelDelegate = MoveTemp(NewDelegate);
	return true;
}

bool FStreamableHandle::BindUpdateDelegate(FStreamableUpdateDelegate NewDelegate)
{
	if (!IsLoadingInProgress())
	{
		// Too late!
		return false;
	}

	UpdateDelegate = MoveTemp(NewDelegate);
	return true;
}

EAsyncPackageState::Type FStreamableHandle::WaitUntilComplete(float Timeout, bool bStartStalledHandles)
{
	if (HasLoadCompleted())
	{
		return EAsyncPackageState::Complete;
	}

	// We need to recursively start any stalled handles
	if (bStartStalledHandles)
	{
		TArray<TSharedRef<FStreamableHandle>> HandlesToStart;

		HandlesToStart.Add(AsShared());

		for (int32 i = 0; i < HandlesToStart.Num(); i++)
		{
			TSharedRef<FStreamableHandle> Handle = HandlesToStart[i];

			if (Handle->IsStalled())
			{
				// If we were stalled, start us now to avoid deadlocks
				UE_LOG(LogStreamableManager, Warning, TEXT("FStreamableHandle::WaitUntilComplete called on stalled handle %s, forcing load even though resources may not have been acquired yet"), *Handle->GetDebugName());
				Handle->StartStalledHandle();
			}

			for (const TSharedPtr<FStreamableHandle>& ChildHandle : Handle->ChildHandles)
			{
				if (ChildHandle.IsValid())
				{
					HandlesToStart.Add(ChildHandle.ToSharedRef());
				}
			}
		}
	}

	// If we have have a timeout we can't call FlushAsyncLoading so use ProcessAsyncLoadingUntilComplete 
	if (Timeout == 0.0f && !GStreamableFlushAllAsyncLoadRequestsOnWait)
	{
		TArray<int32> RequestIds = OwningManager->GetAsyncLoadRequestIds(AsShared());
		FlushAsyncLoading(RequestIds);	
		if (ensureMsgf(HasLoadCompletedOrStalled(), TEXT("Flushing async loading by request id did not complete loading for streamable handle %s"), *GetDebugName()))
		{
			return EAsyncPackageState::Complete;
		}
		// If for some reason the streamables don't consider themselves complete, fall back to old codepath which flushes asyncing loading without specific IDs
	}
	 
	// Finish when all handles are completed or stalled. If we started stalled above then there will be no stalled handles
	EAsyncPackageState::Type State = ProcessAsyncLoadingUntilComplete([this]() { return HasLoadCompletedOrStalled(); }, Timeout);
	
	if (State == EAsyncPackageState::Complete)
	{
		ensureMsgf(HasLoadCompletedOrStalled() || WasCanceled(), TEXT("WaitUntilComplete failed for streamable handle %s, async loading is done but handle is not complete"), *GetDebugName());
	}
	return State;
}

bool FStreamableHandle::HasLoadCompletedOrStalled() const
{
	// Check this handle
	if (!IsCombinedHandle() && !HasLoadCompleted() && !IsStalled())
	{
		return false;
	}

	// Check children recursively
	for (const TSharedPtr<FStreamableHandle>& ChildHandle : ChildHandles)
	{
		if (ChildHandle.IsValid() && !ChildHandle->HasLoadCompletedOrStalled())
		{
			return false;
		}
	}

	return true;
}

bool FStreamableHandle::IsHandleNameEmptyOrDefault() const
{
	// empty or "None"
	if (DebugName.IsEmpty() || DebugName.Equals(FName(NAME_None).ToString()))
	{
		return true;
	}

	// a default value...
	if (DebugName == HandleDebugName_AssetList || DebugName == HandleDebugName_CombinedHandle)
	{
		return true;
	}
	// a default generated debug name
	else if (DebugName.StartsWith(HandleDebugName_Preloading))
	{
		return true;
	}

	return false;
}

void FStreamableHandle::SetDebugNameIfEmptyOrDefault(const FString& NewName)
{
	if (IsHandleNameEmptyOrDefault())
	{
		DebugName = NewName;
	}
}

void FStreamableHandle::GetRequestedAssets(TArray<FSoftObjectPath>& AssetList, bool bIncludeChildren /*= true*/) const
{
	AssetList = RequestedAssets;

	// Check child handles

	if (bIncludeChildren)
	{
		for (const TSharedPtr<FStreamableHandle>& ChildHandle : ChildHandles)
		{
			TArray<FSoftObjectPath> ChildAssetList;

			ChildHandle->GetRequestedAssets(ChildAssetList);

			for (const FSoftObjectPath& ChildRef : ChildAssetList)
			{
				AssetList.AddUnique(ChildRef);
			}
		}
	}
}

UObject* FStreamableHandle::GetLoadedAsset() const
{
	TArray<UObject *> LoadedAssets;

	GetLoadedAssets(LoadedAssets);

	if (LoadedAssets.Num() > 0)
	{
		return LoadedAssets[0];
	}

	return nullptr;
}

void FStreamableHandle::GetLoadedAssets(TArray<UObject *>& LoadedAssets) const
{
	ForEachLoadedAsset([&LoadedAssets](UObject* LoadedAsset)
	{
		LoadedAssets.Add(LoadedAsset);
	});
}

void FStreamableHandle::GetLoadedCount(int32& LoadedCount, int32& RequestedCount) const
{
	RequestedCount = RequestedAssets.Num();
	LoadedCount = RequestedCount - StreamablesLoading;
	
	// Check child handles
	for (const TSharedPtr<FStreamableHandle>& ChildHandle : ChildHandles)
	{
		int32 ChildRequestedCount = 0;
		int32 ChildLoadedCount = 0;

		ChildHandle->GetLoadedCount(ChildLoadedCount, ChildRequestedCount);

		LoadedCount += ChildLoadedCount;
		RequestedCount += ChildRequestedCount;
	}
}

float FStreamableHandle::GetProgress() const
{
	if (HasLoadCompleted())
	{
		return 1.0f;
	}

	int32 Loaded, Total;

	GetLoadedCount(Loaded, Total);

	return (float)Loaded / Total;
}

FStreamableManager* FStreamableHandle::GetOwningManager() const
{
	check(IsInGameThread());

	if (IsActive())
	{
		return OwningManager;
	}
	return nullptr;
}

void FStreamableHandle::CancelHandle()
{
	check(IsInGameThread());

	if (bCanceled || !OwningManager)
	{
		// Too late to cancel it
		return;
	}

	TSharedRef<FStreamableHandle> SharedThis = AsShared();
	if (bLoadCompleted || bReleased)
	{
		// Cancel if it's in the pending queue
		if (StreamableDelegateDelayHelper)
		{
			StreamableDelegateDelayHelper->CancelDelegatesForHandle(SharedThis);
		}

		if (!bReleased)
		{
			ReleaseHandle();
		}

		bCanceled = true;
		NotifyParentsOfCancellation();
		return;
	}

	bCanceled = true;
	NotifyParentsOfCancellation();

	ExecuteDelegate(MoveTemp(CancelDelegate), SharedThis);
	UnbindDelegates();

	// Remove from referenced list. If it is stalled then it won't have been registered with
	// the manager yet
	if (!bStalled)
	{
		for (const FSoftObjectPath& AssetRef : RequestedAssets)
		{
			OwningManager->RemoveReferencedAsset(AssetRef, SharedThis);
		}
	}

	// Remove from explicit list
	OwningManager->ManagedActiveHandles.Remove(SharedThis);

	// Remove child handles
	for (TSharedPtr<FStreamableHandle>& ChildHandle : ChildHandles)
	{
		ChildHandle->ParentHandles.Remove(SharedThis);
	}

	ChildHandles.Empty();
	CompletedChildCount = 0;
	CanceledChildCount = 0;

	OwningManager = nullptr;

	if (ParentHandles.Num() > 0)
	{
		// Update any meta handles that are still active. Copy the array first as elements may be removed from original while iterating
		TArray<TWeakPtr<FStreamableHandle>> ParentHandlesCopy = ParentHandles;
		for (TWeakPtr<FStreamableHandle>& WeakHandle : ParentHandlesCopy)
		{
			TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

			if (Handle.IsValid())
			{
				Handle->UpdateCombinedHandle();
			}
		}
	}
}

void FStreamableHandle::ReleaseHandle()
{
	check(IsInGameThread());

	if (bReleased || bCanceled)
	{
		// Too late to release it
		return;
	}

	check(OwningManager);

	if (bLoadCompleted)
	{
		bReleased = true;

		TSharedRef<FStreamableHandle> SharedThis = AsShared();

		// Remove from referenced list
		for (const FSoftObjectPath& AssetRef : RequestedAssets)
		{
			OwningManager->RemoveReferencedAsset(AssetRef, SharedThis);
		}

		// Remove from explicit list
		OwningManager->ManagedActiveHandles.Remove(SharedThis);

		// Remove child handles
		for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
		{
			ChildHandle->ParentHandles.Remove(SharedThis);
		}

		ChildHandles.Empty();
		CompletedChildCount = 0;
		CanceledChildCount = 0;

		OwningManager = nullptr;
	}
	else
	{
		// Set to release on complete
		bReleaseWhenLoaded = true;
	}
}

void FStreamableHandle::StartStalledHandle()
{
	if (!bStalled || !IsActive())
	{
		// Cannot start
		return;
	}

	check(OwningManager);

	bStalled = false;
	OwningManager->StartHandleRequests(AsShared());
}

bool FStreamableHandle::HasCompleteDelegate() const
{
	return CompleteDelegate.IsBound();
}

bool FStreamableHandle::HasCancelDelegate() const
{
	return CancelDelegate.IsBound();
}

bool FStreamableHandle::HasUpdateDelegate() const
{
	return UpdateDelegate.IsBound();
}

static void RemoveActiveHandle(FStreamable& Streamable, FStreamableHandle& Handle);		

FStreamableHandle::~FStreamableHandle()
{
	checkf(IsInGameThread(), TEXT("Streamable handles aren't thread-safe and must be destroyed on the game thread"));

	if (IsActive())
	{
		if (!bStalled)
		{
			for (const FSoftObjectPath&  RequestedAsset : RequestedAssets)
			{
				if (FStreamable* Streamable = OwningManager->FindStreamable(RequestedAsset))
				{
					RemoveActiveHandle(*Streamable, *this);
				}
			}
		}

		bReleased = true;
		OwningManager = nullptr;
		
		// The weak pointers in FStreamable will be nulled, but they're fixed on next GC, and actively canceling is not safe as we're halfway destroyed
	}
}

void FStreamableHandle::CompleteLoad()
{
	// Only complete if it's still active
	if (IsActive())
	{
		bLoadCompleted = true;

		ExecuteDelegate(MoveTemp(CompleteDelegate), AsShared(), MoveTemp(CancelDelegate));
		UnbindDelegates();

		NotifyParentsOfCompletion();

		if (ParentHandles.Num() > 0)
		{
			// Update any meta handles that are still active. Copy the array first as elements may be removed from original while iterating
			TArray<TWeakPtr<FStreamableHandle>> ParentHandlesCopy = ParentHandles;
			for (TWeakPtr<FStreamableHandle>& WeakHandle : ParentHandlesCopy)
			{
				TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

				if (Handle.IsValid())
				{
					Handle->UpdateCombinedHandle();
				}
			}
		}
	}
}

void FStreamableHandle::NotifyParentsOfCompletion()
{
	if (ParentHandles.Num() > 0)
	{
		// Update any meta handles that are still active. Copy the array first as elements may be removed from original while iterating
		TArray<TWeakPtr<FStreamableHandle>> ParentHandlesCopy = ParentHandles;
		for (TWeakPtr<FStreamableHandle>& WeakHandle : ParentHandlesCopy)
		{
			TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

			if (Handle.IsValid())
			{
				++Handle->CompletedChildCount;
			}
		}
	}
}

void FStreamableHandle::NotifyParentsOfCancellation()
{
	if (ParentHandles.Num() > 0)
	{
		// Update any meta handles that are still active. Copy the array first as elements may be removed from original while iterating
		TArray<TWeakPtr<FStreamableHandle>> ParentHandlesCopy = ParentHandles;
		for (TWeakPtr<FStreamableHandle>& WeakHandle : ParentHandlesCopy)
		{
			TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

			if (Handle.IsValid())
			{
				++Handle->CanceledChildCount;
			}
		}
	}
}

void FStreamableHandle::UpdateCombinedHandle()
{
	if (!IsActive())
	{
		return;
	}

	if (!ensure(IsCombinedHandle()))
	{
		return;
	}

	if (CompletedChildCount + CanceledChildCount < ChildHandles.Num())
	{
		return;
	}

	// Check all our children, complete if done
	bool bAllCompleted = true;
	bool bAllCanceled = true;
	for (const TSharedPtr<FStreamableHandle>& ChildHandle : ChildHandles)
	{
		bAllCompleted = bAllCompleted && !ChildHandle->IsLoadingInProgress();
		bAllCanceled = bAllCanceled && ChildHandle->WasCanceled();

		if (!bAllCompleted && !bAllCanceled)
		{
			return;
		}
	}

	// If all our sub handles were canceled, cancel us. Otherwise complete us if at least one was completed and there are none in progress
	if (bAllCanceled)
	{
		if (OwningManager)
		{
			OwningManager->PendingCombinedHandles.Remove(AsShared());
		}

		CancelHandle();
	}
	else if (bAllCompleted)
	{
		if (OwningManager)
		{
			OwningManager->PendingCombinedHandles.Remove(AsShared());
		}

		CompleteLoad();

		if (bReleaseWhenLoaded)
		{
			ReleaseHandle();
		}
	}
}

void FStreamableHandle::CallUpdateDelegate()
{
	UpdateDelegate.ExecuteIfBound(AsShared());

	// Update any meta handles that are still active
	for (TWeakPtr<FStreamableHandle>& WeakHandle : ParentHandles)
	{
		TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();

		if (Handle.IsValid())
		{
			Handle->CallUpdateDelegate();
		}
	}
}

void FStreamableHandle::UnbindDelegates()
{
	CancelDelegate.Unbind();
	UpdateDelegate.Unbind();
	CompleteDelegate.Unbind();
}

void FStreamableHandle::AsyncLoadCallbackWrapper(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result, FSoftObjectPath TargetName)
{
	check(IsInGameThread());

	// Needed so we can bind with a shared pointer for safety
	if (OwningManager)
	{
		OwningManager->AsyncLoadCallback(TargetName, Package);

		if (!HasLoadCompleted())
		{
			CallUpdateDelegate();
		}
	}
	else if (!bCanceled)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("FStreamableHandle::AsyncLoadCallbackWrapper called on request %s with result %d with no active manager!"), *DebugName, (int32)Result);
	}
}

namespace UE::StreamableManager::Private
{
	// Internal helper for executing delegates. This avoids code duplication by allowing the delegate to be moved or copied when deferred depending on which overload of ExecuteDelegate is called.
	template<typename InStreamableDelegate>
	void ExecuteDelegateInternal(InStreamableDelegate&& Delegate, TSharedPtr<FStreamableHandle> AssociatedHandle, InStreamableDelegate&& CancelDelegate)
	{
		using UnRefCvStreamableDelegate = std::remove_cv_t<std::remove_reference_t<InStreamableDelegate>>;
		static_assert(std::is_same_v<UnRefCvStreamableDelegate, FStreamableDelegate>, "ExecuteDelegateInternal is only valid to be called with FStreamableDelegate.");

		if (Delegate.IsBound())
		{
			if (GStreamableDelegateDelayFrames == 0)
			{
				// Execute it immediately
				Delegate.Execute();
			}
			else
			{
				// Add to execution queue for next tick
				if (!StreamableDelegateDelayHelper)
				{
					StreamableDelegateDelayHelper = new FStreamableDelegateDelayHelper;
				}

				StreamableDelegateDelayHelper->AddDelegate(Forward<InStreamableDelegate>(Delegate), Forward<InStreamableDelegate>(CancelDelegate), MoveTemp(AssociatedHandle));
			}
		}
	}
}

void FStreamableHandle::ExecuteDelegate(const FStreamableDelegate& Delegate, TSharedPtr<FStreamableHandle> AssociatedHandle, const FStreamableDelegate& CancelDelegate)
{
	UE::StreamableManager::Private::ExecuteDelegateInternal(Delegate, MoveTemp(AssociatedHandle), CancelDelegate);
}

void FStreamableHandle::ExecuteDelegate(FStreamableDelegate&& Delegate, TSharedPtr<FStreamableHandle> AssociatedHandle, FStreamableDelegate&& CancelDelegate)
{
	UE::StreamableManager::Private::ExecuteDelegateInternal(MoveTemp(Delegate), MoveTemp(AssociatedHandle), MoveTemp(CancelDelegate));
}

TSharedPtr<FStreamableHandle> FStreamableHandle::FindMatchingHandle(TFunction<bool(const FStreamableHandle&)> Predicate) const
{
	if (Predicate(*this))
	{
		return ConstCastSharedPtr<FStreamableHandle, const FStreamableHandle, ESPMode::ThreadSafe>(AsShared());
	}

	for (const TSharedPtr<FStreamableHandle>& ChildHandle : ChildHandles)
	{
		if (TSharedPtr<FStreamableHandle> MatchingHandleFromChildren = ChildHandle ? ChildHandle->FindMatchingHandle(Predicate) : nullptr)
		{
			return MatchingHandleFromChildren;
		}
	}

	return nullptr;
}

TSharedPtr<FStreamableHandle> FStreamableHandle::CreateCombinedHandle(const TConstArrayView<TSharedPtr<FStreamableHandle>>& OtherHandles)
{
	static const EStreamableManagerCombinedHandleOptions MergeOptions 
	= EStreamableManagerCombinedHandleOptions::MergeDebugNames | EStreamableManagerCombinedHandleOptions::SkipNulls | EStreamableManagerCombinedHandleOptions::RedirectParents;

	TSharedPtr<FStreamableHandle> ThisAsShared{AsShared()};

	if (FStreamableManager* Manager = GetOwningManager())
	{
		TArray<TSharedPtr<FStreamableHandle>> HandlesToUse{OtherHandles};
		HandlesToUse.AddUnique(ThisAsShared);

		return Manager->CreateCombinedHandle(HandlesToUse, HandleDebugName_CombinedHandle, MergeOptions);		
	}

	return nullptr;
}

TSharedPtr<FStreamableHandle> FStreamableHandle::GetOutermostHandle()
{
	if (ParentHandles.Num())
	{
		// have at least one parent handle.

		if (ParentHandles.Num() == 1)
		{
			// can just return the outermost of our one parent
			return ParentHandles[0].Pin()->GetOutermostHandle();
		}

		TArray<TWeakPtr<FStreamableHandle>> IterationArray;
		IterationArray = ParentHandles;
		TArray<TWeakPtr<FStreamableHandle>> NextItrArray;
		bool bFoundParents = true;

		while (bFoundParents)
		{
			for (const TWeakPtr<FStreamableHandle>& WeakHandle : IterationArray)
			{
				TSharedPtr<FStreamableHandle> Handle = WeakHandle.Pin();
				if (Handle.IsValid())
				{
					// Append does the right thing with the allocation already, no need to handle it ourselves
					NextItrArray.Append(Handle->ParentHandles);
				}
			}

			// we didn't find parents if none of our handles we just checked had parent handles
			bFoundParents = NextItrArray.Num() > 0;

			if (!bFoundParents)
			{
				// we can't be in the loop if this array is empty...
				// possible this can actually be more than one handle at this point - that's okay!
				return IterationArray[0].Pin();
			}

			IterationArray = NextItrArray;
			NextItrArray.Reset();
		}
	}
	
	// weren't able to resolve the maximum outermost (likely due to no parent)
	return AsShared();
}

/** Internal object, one of these per object paths managed by this system */
struct FStreamable
{
	/** Hard GC pointer to object */
	TObjectPtr<UObject> Target = nullptr;

	/** Live handles keeping this alive. Handles may be duplicated. */
	TArray<FStreamableHandle*> ActiveHandles;

	/** Handles waiting for this to load. Handles may be duplicated with redirectors. */
	TArray< TSharedRef< FStreamableHandle> > LoadingHandles;

	/** Id for async load request with async loading system */
	int32 RequestId = INDEX_NONE;

	/** If this object is currently being loaded */
	bool bAsyncLoadRequestOutstanding = false;

	/** If this object failed to load, don't try again */
	bool bLoadFailed = false;


	void FreeHandles()
	{
		// Clear the loading handles 
		for (TSharedRef<FStreamableHandle>& LoadingHandle : LoadingHandles)
		{
			LoadingHandle->StreamablesLoading--;
		}
		LoadingHandles.Empty();

		// Cancel active handles, this list includes the loading handles
		while (ActiveHandles.Num() > 0)
		{
			FStreamableHandle* ActiveHandle = ActiveHandles.Pop(EAllowShrinking::No);
			if (!ActiveHandle->bCanceled)
			{
				// Full cancel isn't safe any more

				ActiveHandle->bCanceled = true;
				ActiveHandle->OwningManager = nullptr;

				if (ActiveHandle->bReleased)
				{
					ActiveHandle->NotifyParentsOfCancellation();
				}
				else
				{
					// Keep handle alive to stop the cancel callback from dropping the last reference
					TSharedPtr<FStreamableHandle> SharedHandle = ActiveHandle->AsShared();

					FStreamableHandle::ExecuteDelegate(MoveTemp(ActiveHandle->CancelDelegate), SharedHandle);
					ActiveHandle->UnbindDelegates();
					ActiveHandle->NotifyParentsOfCancellation();
				}
			}
		}
	}

	void AddLoadingRequest(TSharedRef<FStreamableHandle>& NewRequest)
	{
		// With redirectors we can end up adding the same handle multiple times, this is unusual but supported
		ActiveHandles.Add(&NewRequest.Get());

		LoadingHandles.Add(NewRequest);
		NewRequest->StreamablesLoading++;
	}
};

void RemoveActiveHandle(FStreamable& Streamable, FStreamableHandle& Handle)
{
	TArray<FStreamableHandle*>& ActiveHandles = Streamable.ActiveHandles;
	for (int32 Idx = Streamable.ActiveHandles.Num() - 1; Idx >= 0; --Idx)
	{
		if (ActiveHandles[Idx] == &Handle)
		{
			ActiveHandles.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
		}
	}
}
	

FStreamableManager::FStreamableManager()
	: FGCObject(EFlags::AddStableNativeReferencesOnly)
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FStreamableManager::OnPreGarbageCollect);
	bForceSynchronousLoads = false;
	ManagerName = TEXT("StreamableManager");
}

FStreamableManager::~FStreamableManager()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);

	for (const TPair<FSoftObjectPath, FStreamable*>& Pair : StreamableItems)
	{
		Pair.Value->FreeHandles();
	}
	
	TStreamableMap TempStreamables;
	Swap(TempStreamables, StreamableItems);
	for (const TPair<FSoftObjectPath, FStreamable*>& TempPair : TempStreamables)
	{
		delete TempPair.Value;
	}
}

void FStreamableManager::OnPreGarbageCollect()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamableManager::OnPreGarbageCollect);

	// Find streamables without active handles
	TBitArray<> InactiveStreamables;
	InactiveStreamables.SetNumUninitialized(StreamableItems.Num());
	int32 Idx = 0;
	for (const TPair<FSoftObjectPath, FStreamable*>& Pair : StreamableItems)
	{
		InactiveStreamables[Idx++] = Pair.Value->ActiveHandles.IsEmpty();
	}

	int32 NumInactive = InactiveStreamables.CountSetBits();
	if (NumInactive == 0)
	{
		return;
	}

	// Remove redirects
	if (StreamableRedirects.Num() > 0)
	{
		TSet<FSoftObjectPath> RedirectsToRemove;
		RedirectsToRemove.Reserve(NumInactive);

		Idx = 0;
		for (const TPair<FSoftObjectPath, FStreamable*>& Pair : StreamableItems)
		{
			if (InactiveStreamables[Idx++])
			{
				RedirectsToRemove.Add(Pair.Key);
			}
		}

		for (TStreamableRedirects::TIterator It(StreamableRedirects); It; ++It)
		{
			if (RedirectsToRemove.Contains(It.Value().NewPath))
			{
				It.RemoveCurrent();
			}
		}
	}

	// Delete and remove inactive streamables
	Idx = 0;
	for (TStreamableMap::TIterator It(StreamableItems); It; ++It)
	{
		if (InactiveStreamables[Idx++])
		{
			It->Value->FreeHandles();
			delete It->Value;
			It.RemoveCurrent();
		}
	}
}

void FStreamableManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	// If there are active streamable handles in the editor, this will cause the user to Force Delete, which is irritating but necessary because weak pointers cannot be used here
	for (auto& Pair : StreamableItems)
	{
		Collector.AddStableReference(&Pair.Value->Target);
	}
	
	for (TPair<FSoftObjectPath, FRedirectedPath>& Pair : StreamableRedirects)
	{
		Collector.AddStableReference(&Pair.Value.LoadedRedirector);
	}
}

const FString& FStreamableManager::GetManagerName() const
{
	return ManagerName;
}

void FStreamableManager::SetManagerName(FString InName)
{
	ManagerName = MoveTemp(InName);
}

bool FStreamableManager::GetReferencerPropertyName(UObject* Object, FString& OutPropertyName) const
{
	if (Object)
	{
		// Report the first handle
		for (TStreamableMap::TConstIterator It(StreamableItems); It; ++It)
		{
			const FStreamable* Existing = It.Value();
			check(Existing);
			if (Existing->Target == Object)
			{
				if (Existing->ActiveHandles.Num() > 0)
				{
					OutPropertyName = Existing->ActiveHandles[0]->GetDebugName();
					return true;
				}

				if (Existing->LoadingHandles.Num() > 0)
				{
					OutPropertyName = Existing->LoadingHandles[0]->GetDebugName();
					return true;
				}
			}
		}
	}

	return false;
}

FStreamable* FStreamableManager::FindStreamable(const FSoftObjectPath& Target) const
{
	FStreamable* Existing = StreamableItems.FindRef(Target);

	if (!Existing)
	{
		return StreamableItems.FindRef(ResolveRedirects(Target));
	}

	return Existing;
}

FStreamable* FStreamableManager::StreamInternal(const FSoftObjectPath& InTargetName, TAsyncLoadPriority Priority, TSharedRef<FStreamableHandle> Handle)
{
	check(IsInGameThread());
	UE_LOG(LogStreamableManager, Verbose, TEXT("Asynchronous load %s"), *InTargetName.ToString());

	FSoftObjectPath TargetName = ResolveRedirects(InTargetName);
	FStreamable* Existing = StreamableItems.FindRef(TargetName);
	if (Existing)
	{
		if (Existing->bAsyncLoadRequestOutstanding)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Already in progress %s"), *TargetName.ToString());
			check(!Existing->Target); // should not be a load request unless the target is invalid
			check(Existing->RequestId != INDEX_NONE);
			ensure(IsAsyncLoading()); // Nothing should be pending if there is no async loading happening

			// Don't return as we potentially want to sync load it
		}
		if (Existing->Target)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Already Loaded %s"), *TargetName.ToString());
			return Existing;
		}
	}
	else
	{
		Existing = StreamableItems.Add(TargetName, new FStreamable());
	}

	if (!Existing->bAsyncLoadRequestOutstanding)
	{
		FindInMemory(TargetName, Existing);
	}
	
	if (!Existing->Target)
	{
		// Disable failed flag as it may have been added at a later point
		Existing->bLoadFailed = false;

		FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();

		// If async loading isn't safe or it's forced on, we have to do a sync load which will flush all async loading
		if (GIsInitialLoad || ThreadContext.IsInConstructor > 0 || bForceSynchronousLoads)
		{
			FRedirectedPath RedirectedPath;
			UE_LOG(LogStreamableManager, Verbose, TEXT("     Static loading %s"), *TargetName.ToString());
			Existing->Target = StaticLoadObject(UObject::StaticClass(), nullptr, *TargetName.ToString());

			// Need to manually detect redirectors because the above call only expects to load a UObject::StaticClass() type
			UObjectRedirector* Redir = Cast<UObjectRedirector>(Existing->Target);
			if (Redir)
			{
				TargetName = HandleLoadedRedirector(Redir, TargetName, Existing);
			}

			if (Existing->Target)
			{
				UE_LOG(LogStreamableManager, Verbose, TEXT("     Static loaded %s"), *Existing->Target->GetFullName());
			}
			else
			{
				Existing->bLoadFailed = true;
				UE_LOG(LogStreamableManager, Log, TEXT("Failed attempt to load %s"), *TargetName.ToString());
			}
			Existing->bAsyncLoadRequestOutstanding = false;
		}
		else
		{
			// We always queue a new request in case the existing one gets cancelled
			FString Package = TargetName.ToString();
			int32 FirstDot = Package.Find(TEXT("."), ESearchCase::CaseSensitive);
			if (FirstDot != INDEX_NONE)
			{
				Package.LeftInline(FirstDot,EAllowShrinking::No);
			}

			FPackagePath PackagePath;
			if (!FPackagePath::TryFromPackageName(Package, PackagePath))
			{
				UE_LOG(LogStreamableManager, Error, TEXT("Failed attempt to load %s; it is not a valid LongPackageName"), *Package);
				Existing->bLoadFailed = true;
				Existing->bAsyncLoadRequestOutstanding = false;
			}
			else
			{
				Existing->bLoadFailed = false;
				Existing->bAsyncLoadRequestOutstanding = true;
#if UE_WITH_PACKAGE_ACCESS_TRACKING
				UE_TRACK_REFERENCING_PACKAGE_SCOPED(Handle->GetReferencerPackage(), Handle->GetRefencerPackageOp());
#endif
#if WITH_EDITOR
				FCookLoadScope CookLoadScope(Handle->GetCookLoadType());
#endif
				// This may overwrite an existing request id, this is intentional - see comment above re: cancellation
				Existing->RequestId = LoadPackageAsync(PackagePath,
					NAME_None /* PackageNameToCreate */,
					FLoadPackageAsyncDelegate::CreateSP(Handle, &FStreamableHandle::AsyncLoadCallbackWrapper, TargetName),
					PKG_None /* InPackageFlags */,
					INDEX_NONE /* InPIEInstanceID */,
					Priority /* InPackagePriority */);
			}
		}
	}
	return Existing;
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestAsyncLoad(TArray<FSoftObjectPath> TargetsToStream, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, FString DebugName)
{
	LLM_SCOPE(ELLMTag::StreamingManager);

	// Schedule a new callback, this will get called when all related async loads are completed
	TSharedRef<FStreamableHandle> NewRequest = MakeShareable(new FStreamableHandle());
	NewRequest->CompleteDelegate = MoveTemp(DelegateToCall);
	NewRequest->OwningManager = this;
	NewRequest->RequestedAssets = MoveTemp(TargetsToStream);
#if (!PLATFORM_IOS && !PLATFORM_ANDROID)
	NewRequest->DebugName = MoveTemp(DebugName);
#endif
	NewRequest->Priority = Priority;
#if UE_WITH_PACKAGE_ACCESS_TRACKING
	PackageAccessTracking_Private::FTrackedData* AccumulatedScopeData = PackageAccessTracking_Private::FPackageAccessRefScope::GetCurrentThreadAccumulatedData();
	if (AccumulatedScopeData)
	{
		NewRequest->ReferencerPackage = AccumulatedScopeData->PackageName;
		NewRequest->ReferencerPackageOp = AccumulatedScopeData->OpName;
	}
#endif
#if WITH_EDITOR
	NewRequest->CookLoadType = FCookLoadScope::GetCurrentValue();
#endif

	int32 NumValidRequests = NewRequest->RequestedAssets.Num();
	
	TSet<FSoftObjectPath> TargetSet;
	TargetSet.Reserve(NumValidRequests);

	for (const FSoftObjectPath& TargetName : NewRequest->RequestedAssets)
	{
		if (TargetName.IsNull())
		{
			--NumValidRequests;
			continue;
		}
		else if (FPackageName::IsShortPackageName(TargetName.GetLongPackageFName()))
		{
			UE_LOG(LogStreamableManager, Error, TEXT("RequestAsyncLoad(%s) called with invalid package name %s"), *DebugName, *TargetName.ToString());
			NewRequest->CancelHandle();
			return nullptr;
		}
		TargetSet.Add(TargetName);
	}

	if (NumValidRequests == 0)
	{
		// Original array was empty or all null
		UE_LOG(LogStreamableManager, Display, TEXT("RequestAsyncLoad(%s) called with empty or only null assets!"), *DebugName);
		NewRequest->CancelHandle();
		return nullptr;
	} 
	else if (NewRequest->RequestedAssets.Num() != NumValidRequests)
	{
		FString RequestedSet;

		for (auto It = NewRequest->RequestedAssets.CreateIterator(); It; ++It)
		{
			FSoftObjectPath& Asset = *It;
			if (!RequestedSet.IsEmpty())
			{
				RequestedSet += TEXT(", ");
			}
			RequestedSet += Asset.ToString();

			// Remove null entries
			if (Asset.IsNull())
			{
				It.RemoveCurrent();
			}
		}

		// Some valid, some null
		UE_LOG(LogStreamableManager, Display, TEXT("RequestAsyncLoad(%s) called with both valid and null assets, null assets removed from %s!"), *DebugName, *RequestedSet);
	}

	if (TargetSet.Num() != NewRequest->RequestedAssets.Num())
	{
#if UE_BUILD_DEBUG
		FString RequestedSet;

		for (const FSoftObjectPath& Asset : NewRequest->RequestedAssets)
		{
			if (!RequestedSet.IsEmpty())
			{
				RequestedSet += TEXT(", ");
			}
			RequestedSet += Asset.ToString();
		}

		UE_LOG(LogStreamableManager, Verbose, TEXT("RequestAsyncLoad(%s) called with duplicate assets, duplicates removed from %s!"), *DebugName, *RequestedSet);
#endif

		NewRequest->RequestedAssets = TargetSet.Array();
	}

	if (bManageActiveHandle)
	{
		// This keeps a reference around until explicitly released
		ManagedActiveHandles.Add(NewRequest);
	}

	if (bStartStalled)
	{
		NewRequest->bStalled = true;
	}
	else
	{
		StartHandleRequests(NewRequest);
	}

	return NewRequest;
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestAsyncLoad(const FSoftObjectPath& TargetToStream, FStreamableDelegate DelegateToCall, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, FString DebugName)
{
	return RequestAsyncLoad(TArray<FSoftObjectPath>{TargetToStream}, MoveTemp(DelegateToCall), Priority, bManageActiveHandle, bStartStalled, MoveTemp(DebugName));
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestAsyncLoad(TArray<FSoftObjectPath> TargetsToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, FString DebugName)
{
	return RequestAsyncLoad(MoveTemp(TargetsToStream), FStreamableDelegate::CreateLambda( MoveTemp( Callback ) ), Priority, bManageActiveHandle, bStartStalled, MoveTemp(DebugName));
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestAsyncLoad(const FSoftObjectPath& TargetToStream, TFunction<void()>&& Callback, TAsyncLoadPriority Priority, bool bManageActiveHandle, bool bStartStalled, FString DebugName)
{
	return RequestAsyncLoad(TargetToStream, FStreamableDelegate::CreateLambda( MoveTemp( Callback ) ), Priority, bManageActiveHandle, bStartStalled, MoveTemp(DebugName));
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestSyncLoad(TArray<FSoftObjectPath> TargetsToStream, bool bManageActiveHandle, FString DebugName)
{
	// If in async loading thread or from callback always do sync as recursive tick is unsafe
	// If in EDL always do sync as EDL internally avoids flushing
	// Otherwise, only do a sync load if there are no background sync loads, this is faster but will cause a sync flush
	bForceSynchronousLoads = IsInAsyncLoadingThread() || IsEventDrivenLoaderEnabled() || !IsAsyncLoading();

	// Do an async load and wait to complete. In some cases this will do a sync load due to safety issues
	TSharedPtr<FStreamableHandle> Request = RequestAsyncLoad(MoveTemp(TargetsToStream), FStreamableDelegate(), AsyncLoadHighPriority, bManageActiveHandle, false, MoveTemp(DebugName));

	bForceSynchronousLoads = false;

	if (Request.IsValid())
	{
		EAsyncPackageState::Type Result = Request->WaitUntilComplete();

		ensureMsgf(Result == EAsyncPackageState::Complete, TEXT("RequestSyncLoad of %s resulted in bad async load result %d"), *Request->DebugName, Result);
		ensureMsgf(Request->HasLoadCompleted(), TEXT("RequestSyncLoad of %s completed early, not actually completed!"), *Request->DebugName);
	}

	return Request;
}

TSharedPtr<FStreamableHandle> FStreamableManager::RequestSyncLoad(const FSoftObjectPath& TargetToStream, bool bManageActiveHandle, FString DebugName)
{
	return RequestSyncLoad(TArray<FSoftObjectPath>{TargetToStream}, bManageActiveHandle, MoveTemp(DebugName));
}

void FStreamableManager::StartHandleRequests(TSharedRef<FStreamableHandle> Handle)
{
	TRACE_LOADTIME_REQUEST_GROUP_SCOPE(TEXT("StreamableManager - %s"), *Handle->GetDebugName());

	TArray<FStreamable *> ExistingStreamables;
	ExistingStreamables.Reserve(Handle->RequestedAssets.Num());

	for (int32 i = 0; i < Handle->RequestedAssets.Num(); i++)
	{
		FStreamable* Existing = StreamInternal(Handle->RequestedAssets[i], Handle->Priority, Handle);
		check(Existing);

		ExistingStreamables.Add(Existing);
		Existing->AddLoadingRequest(Handle);
	}

	// Go through and complete loading anything that's already in memory, this may call the callback right away
	for (int32 i = 0; i < Handle->RequestedAssets.Num(); i++)
	{
		FStreamable* Existing = ExistingStreamables[i];

		if (Existing && (Existing->Target || Existing->bLoadFailed))
		{
			Existing->bAsyncLoadRequestOutstanding = false;

			CheckCompletedRequests(Handle->RequestedAssets[i], Existing);
		}
	}
}

TArray<int32> FStreamableManager::GetAsyncLoadRequestIds(TSharedRef<FStreamableHandle> InitialHandle)
{
	TArray<TSharedPtr<FStreamableHandle>> Queue;
	Queue.Reserve(1 + InitialHandle->ChildHandles.Num());
	Queue.Add(InitialHandle.ToSharedPtr());	
	
	TArray<int32> RequestIds;
	for (int32 i=0; i < Queue.Num(); ++i)
	{
		TSharedPtr<FStreamableHandle> Handle = Queue[i];
		if (!Handle.IsValid()) 
		{
			 continue; 
		}

		Queue.Append(Handle->ChildHandles);	
		
		for (const FSoftObjectPath& Path : Handle->RequestedAssets)
		{
			FStreamable* Streamable = StreamableItems.FindRef(Path);
			if (Streamable && Streamable->RequestId != INDEX_NONE)
			{
				RequestIds.Add(Streamable->RequestId);				
			}
		}
	}
	return RequestIds;
}

UE_TRACE_EVENT_BEGIN(Cpu, StreamableManager_LoadSynchronous, NoSync)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, AssetPath)
UE_TRACE_EVENT_END()

UObject* FStreamableManager::LoadSynchronous(const FSoftObjectPath& Target, bool bManageActiveHandle, TSharedPtr<FStreamableHandle>* RequestHandlePointer)
{
#if CPUPROFILERTRACE_ENABLED
	UE_TRACE_LOG_SCOPED_T(Cpu, StreamableManager_LoadSynchronous, CpuChannel)
		<< StreamableManager_LoadSynchronous.AssetPath(*WriteToWideString<FName::StringBufferSize>(Target));
#endif // CPUPROFILERTRACE_ENABLED
	TSharedPtr<FStreamableHandle> Request = RequestSyncLoad(Target, bManageActiveHandle, FString::Printf(TEXT("LoadSynchronous of %s"), *Target.ToString()));

	if (RequestHandlePointer)
	{
		(*RequestHandlePointer) = Request;
	}

	if (Request.IsValid())
	{
		UObject* Result = Request->GetLoadedAsset();

		if (!Result)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("LoadSynchronous failed for load of %s! File is missing or there is a loading system problem"), *Target.ToString());
		}

		return Result;
	}

	return nullptr;
}

void FStreamableManager::FindInMemory(FSoftObjectPath& InOutTargetName, struct FStreamable* Existing, UPackage* Package)
{
	check(Existing);
	check(!Existing->bAsyncLoadRequestOutstanding);
	UE_LOG(LogStreamableManager, Verbose, TEXT("     Searching in memory for %s"), *InOutTargetName.ToString());
	Existing->Target = nullptr;

	UObject* Asset = Package ? 
		StaticFindObjectFast(UObject::StaticClass(), Package, InOutTargetName.GetAssetFName()) :
		StaticFindObject(UObject::StaticClass(), InOutTargetName.GetAssetPath(), /*ExactClass*/false);	
	if (Asset)
	{
		if (InOutTargetName.IsAsset())
		{
			Existing->Target = Asset;
		}
		else if (InOutTargetName.IsSubobject())
		{
			Existing->Target = StaticFindObject(UObject::StaticClass(), Asset, *InOutTargetName.GetSubPathString());
		}
	}
	checkSlow(Existing->Target == StaticFindObject(UObject::StaticClass(), nullptr, *InOutTargetName.ToString()));

	if (Existing->Target && Existing->Target->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
	{
		// This can get called from PostLoad on async loaded objects, if it is we do not want to return partially loaded objects and instead want to register for their full load
		Existing->Target = nullptr;
	}

	UObjectRedirector* Redir = Cast<UObjectRedirector>(Existing->Target);

	if (Redir)
	{
		InOutTargetName = HandleLoadedRedirector(Redir, InOutTargetName, Existing);
	}

	if (Existing->Target)
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("     Found in memory %s"), *Existing->Target->GetFullName());
		Existing->bLoadFailed = false;
	}
}

void FStreamableManager::AsyncLoadCallback(FSoftObjectPath TargetName, UPackage* Package)
{
	check(IsInGameThread());

	FStreamable* Existing = FindStreamable(TargetName);

	UE_LOG(LogStreamableManager, Verbose, TEXT("Stream Complete callback %s"), *TargetName.ToString());
	if (Existing)
	{
		if (Existing->bAsyncLoadRequestOutstanding)
		{
			Existing->bAsyncLoadRequestOutstanding = false;
			if (!Existing->Target)
			{
				FindInMemory(TargetName, Existing, Package);
			}

			CheckCompletedRequests(TargetName, Existing);
		}
		else
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("AsyncLoadCallback called for %s when not waiting on a load request, was loaded early by sync load"), *TargetName.ToString());
		}
		if (Existing->Target)
		{
			UE_LOG(LogStreamableManager, Verbose, TEXT("    Found target %s"), *Existing->Target->GetFullName());
		}
		else
		{
			// Async load failed to find the object
			Existing->bLoadFailed = true;
			UE_LOG(LogStreamableManager, Verbose, TEXT("    Failed async load for %s"), *TargetName.ToString());
		}
	}
	else
	{
		UE_LOG(LogStreamableManager, Error, TEXT("Can't find streamable for %s in AsyncLoadCallback!"), *TargetName.ToString());
	}
}

void FStreamableManager::CheckCompletedRequests(const FSoftObjectPath& Target, struct FStreamable* Existing)
{
	static int32 RecursiveCount = 0;

	ensure(RecursiveCount == 0);

	RecursiveCount++;

	// Release these handles at end
	TArray<TSharedRef<FStreamableHandle>> HandlesToComplete;
	TArray<TSharedRef<FStreamableHandle>> HandlesToRelease;

	for (TSharedRef<FStreamableHandle>& Handle : Existing->LoadingHandles)
	{
		ensure(Handle->WasCanceled() || Handle->OwningManager == this);

		// Decrement related requests, and call delegate if all are done and request is still active
		Handle->StreamablesLoading--;
		if (Handle->StreamablesLoading == 0)
		{
			if (Handle->bReleaseWhenLoaded)
			{
				HandlesToRelease.Add(Handle);
			}

			HandlesToComplete.Add(Handle);
		}		
	}
	Existing->LoadingHandles.Empty();

	for (TSharedRef<FStreamableHandle>& Handle : HandlesToComplete)
	{
		Handle->CompleteLoad();
	}

	for (TSharedRef<FStreamableHandle>& Handle : HandlesToRelease)
	{
		Handle->ReleaseHandle();
	}

	// HandlesToRelease might get deleted when function ends

	RecursiveCount--;
}

void FStreamableManager::RemoveReferencedAsset(const FSoftObjectPath& Target, TSharedRef<FStreamableHandle> Handle)
{
	if (Target.IsNull())
	{
		return;
	}

	ensureMsgf(Handle->OwningManager == this, TEXT("RemoveReferencedAsset called on wrong streamable manager for target %s"), *Target.ToString());

	FStreamable* Existing = FindStreamable(Target);

	// This should always be in the active handles list
	if (ensureMsgf(Existing, TEXT("Failed to find existing streamable for %s"), *Target.ToString()))
	{
		ensureMsgf(Existing->ActiveHandles.RemoveSwap(&Handle.Get(), EAllowShrinking::No) > 0, TEXT("Failed to remove active handle for %s"), *Target.ToString());

		// Try removing from loading list if it's still there, this won't call the callback as it's being called from cancel
		// This may remove more than one copy if streamables were merged
		int32 LoadingRemoved = Existing->LoadingHandles.RemoveSwap(Handle);
		if (LoadingRemoved > 0)
		{
			Handle->StreamablesLoading -= LoadingRemoved;

			if (Existing->LoadingHandles.Num() == 0)
			{
				// All requests cancelled, remove loading flag
				Existing->bAsyncLoadRequestOutstanding = false;
			}
		}
	}
}

bool FStreamableManager::AreAllAsyncLoadsComplete() const
{
	check(IsInGameThread());
	for (const TPair<FSoftObjectPath, FStreamable*>& Item : StreamableItems)
	{
		const FStreamable* Streamable = Item.Value;
		if (Streamable && Streamable->bAsyncLoadRequestOutstanding)
		{
			return false;
		}
	}
	return true;
}

bool FStreamableManager::IsAsyncLoadComplete(const FSoftObjectPath& Target) const
{
	// Failed loads count as success
	check(IsInGameThread());
	FStreamable* Existing = FindStreamable(Target);
	return !Existing || !Existing->bAsyncLoadRequestOutstanding;
}

UObject* FStreamableManager::GetStreamed(const FSoftObjectPath& Target) const
{
	check(IsInGameThread());
	FStreamable* Existing = FindStreamable(Target);
	if (Existing && Existing->Target)
	{
		return Existing->Target;
	}
	return nullptr;
}

void FStreamableManager::Unload(const FSoftObjectPath& Target)
{
	check(IsInGameThread());

	TArray<TSharedRef<FStreamableHandle>> HandleList;

	if (GetActiveHandles(Target, HandleList, true))
	{
		for (TSharedRef<FStreamableHandle> Handle : HandleList)
		{
			Handle->ReleaseHandle();
		}
	}
	else
	{
		UE_LOG(LogStreamableManager, Verbose, TEXT("Attempt to unload %s, but it isn't loaded"), *Target.ToString());
	}
}

TSharedPtr<FStreamableHandle> FStreamableManager::CreateCombinedHandle(const TConstArrayView<TSharedPtr<FStreamableHandle> >& ChildHandles, const FString& DebugName, EStreamableManagerCombinedHandleOptions Options)
{
	if (!ensure(ChildHandles.Num() > 0))
	{
		return nullptr;
	}

	TSharedRef<FStreamableHandle> NewRequest = MakeShareable(new FStreamableHandle());
	NewRequest->OwningManager = this;
	NewRequest->bIsCombinedHandle = true;
	NewRequest->DebugName = DebugName;

	static const auto RemapHandleParentRelationship = [](TSharedPtr<FStreamableHandle>& IndividualHandle, TSharedRef<FStreamableHandle>& MergedHandle)
	{
		// break backlinking, inject new forelinking (such that our old parent handles now reference the new MergedHandle - our old parent's ChildHandles array will contain MergedHandle, and MergedHandle is our new Parent)...
		for (int32 Index = 0, Num = IndividualHandle->ParentHandles.Num(); Index < Num; ++Index)
		{
			TSharedPtr<FStreamableHandle> Parent = IndividualHandle->ParentHandles[Index].Pin();

			// even though we should run this check (via invoking this lambda) prior to adding MergedHandle to our ParentHandles, it is still possible if the array is not unique
			if (Parent == MergedHandle)
			{
				continue;
			}

			if (Parent.IsValid())
			{
				const int32 FoundIndex = Parent->ChildHandles.Find(IndividualHandle);
				if (ensure(Parent->ChildHandles.IsValidIndex(FoundIndex)))
				{
					// stomp the old link with the new merged handle
					Parent->ChildHandles[FoundIndex] = MergedHandle;
				}
				else
				{
					// should be impossible, but at least we can add the merged handle for tracking purposes...
					Parent->ChildHandles.Add(MergedHandle);
				}
				
				MergedHandle->ParentHandles.Add(Parent);
			}

			// no longer need the parent handle reference in this handle's parents. The merged handle is within our parent handles, and has us in its ChildHandles.
			IndividualHandle->ParentHandles.RemoveAtSwap(Index);
			Index--;
			Num--;
		}
	};

	TStringBuilder<256> DebugNameBuilder;

	if (EnumHasAllFlags(Options, EStreamableManagerCombinedHandleOptions::MergeDebugNames))
	{
		DebugNameBuilder.Appendf(TEXT("%s: Merged("), *DebugName);
	}

	bool bFormattingFirstPass = true;
	for (TSharedPtr<FStreamableHandle> ChildHandle : ChildHandles)
	{
		if (!ChildHandle.IsValid())
		{
			if (EnumHasAllFlags(Options, EStreamableManagerCombinedHandleOptions::SkipNulls))
			{
				continue;
			}
			else
			{
				ensureMsgf(ChildHandle.IsValid(), TEXT("A child handle is not valid and SkipNulls flag was not used, returning nullptr"));
				return nullptr;
			}
		}

		ensure(ChildHandle->OwningManager == this);

		if (EnumHasAllFlags(Options, EStreamableManagerCombinedHandleOptions::MergeDebugNames))
		{
			if (bFormattingFirstPass)
			{
				DebugNameBuilder.Appendf(TEXT("%s"), *ChildHandle->DebugName);
				bFormattingFirstPass = false;
			}
			else
			{
				DebugNameBuilder.Appendf(TEXT(", %s"), *ChildHandle->DebugName);
			}
		}

		if (EnumHasAllFlags(Options, EStreamableManagerCombinedHandleOptions::RedirectParents))
		{
			RemapHandleParentRelationship(ChildHandle, NewRequest);
		}

		ChildHandle->ParentHandles.Add(NewRequest);
		NewRequest->ChildHandles.Add(ChildHandle);

		if (ChildHandle->bLoadCompleted)
		{
			++NewRequest->CompletedChildCount;
		}
		if (ChildHandle->bCanceled)
		{
			++NewRequest->CanceledChildCount;
		}
	}

	if (EnumHasAllFlags(Options, EStreamableManagerCombinedHandleOptions::MergeDebugNames))
	{
		DebugNameBuilder += TEXT(")");

		NewRequest->DebugName = *DebugNameBuilder;
	}

	// Add to pending list so these handles don't free when not referenced
	PendingCombinedHandles.Add(NewRequest);

	// This may already be complete
	NewRequest->UpdateCombinedHandle();

	return NewRequest;
}

bool FStreamableManager::GetActiveHandles(const FSoftObjectPath& Target, TArray<TSharedRef<FStreamableHandle>>& HandleList, bool bOnlyManagedHandles) const
{
	check(IsInGameThread());
	FStreamable* Existing = FindStreamable(Target);
	if (Existing && Existing->ActiveHandles.Num() > 0)
	{
		for (FStreamableHandle* ActiveHandle : Existing->ActiveHandles)
		{
			ensure(ActiveHandle->OwningManager == this);

			TSharedRef<FStreamableHandle> HandleRef = ActiveHandle->AsShared();
			if (!bOnlyManagedHandles || ManagedActiveHandles.Contains(HandleRef))
			{
				// Only add each handle once, we can have duplicates in the source list
				HandleList.AddUnique(MoveTemp(HandleRef));
			}
		}

		return HandleList.Num() > 0;
	}

	return false;
}

FSoftObjectPath FStreamableManager::ResolveRedirects(const FSoftObjectPath& Target) const
{
	FRedirectedPath const* Redir = StreamableRedirects.Find(Target);
	if (Redir)
	{
		check(Target != Redir->NewPath);
		UE_LOG(LogStreamableManager, Verbose, TEXT("Redirected %s -> %s"), *Target.ToString(), *Redir->NewPath.ToString());
		return Redir->NewPath;
	}
	return Target;
}

FSoftObjectPath FStreamableManager::HandleLoadedRedirector(UObjectRedirector* LoadedRedirector, FSoftObjectPath RequestedPath, FStreamable* RequestedStreamable)
{
	UE_LOG(LogStreamableManager, Verbose, TEXT("     Found redirect %s"), *LoadedRedirector->GetPathName());

	// Need to follow the redirector chain
	while (UObjectRedirector* Redirector = Cast<UObjectRedirector>(RequestedStreamable->Target))
	{
		RequestedStreamable->Target = Redirector->DestinationObject;

		if (!RequestedStreamable->Target)
		{
			RequestedStreamable->bLoadFailed = true;
			UE_LOG(LogStreamableManager, Warning, TEXT("Destination of redirector was not found %s -> %s."), *RequestedPath.ToString(), *Redirector->GetPathName());

			return RequestedPath;
		}
	}

	FSoftObjectPath NewPath(RequestedStreamable->Target->GetPathName());
	UE_LOG(LogStreamableManager, Verbose, TEXT("     Which redirected to %s"), *NewPath.ToString());

	// We add the originally loaded redirector to GC keep alive, this will keep entire chain alive
	StreamableRedirects.Add(RequestedPath, FRedirectedPath(NewPath, LoadedRedirector));

	// Remove the requested streamable from it's old location
	StreamableItems.Remove(RequestedPath);

	if (FStreamable** FoundStreamable = StreamableItems.Find(NewPath))
	{
		// We found an existing streamable, need to merge the handles and delete old streamable
		// This may result in the same handle being in the list twice! But the rest of the code is ready for that
		// We let LoadFailed and InProgress stay as false on the new streamable because we've successfully loaded an object

		RequestedStreamable->LoadingHandles.Append(MoveTemp((*FoundStreamable)->LoadingHandles));
		RequestedStreamable->ActiveHandles.Append(MoveTemp((*FoundStreamable)->ActiveHandles));

		// No handles remain so need to FreeHandles()
		delete *FoundStreamable;
	}

	// Add requested streamable with new path
	StreamableItems.Add(NewPath, RequestedStreamable);
	
	return NewPath;
}
