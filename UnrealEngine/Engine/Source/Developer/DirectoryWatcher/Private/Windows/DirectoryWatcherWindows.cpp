// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirectoryWatcherWindows.h"
#include "DirectoryWatcherPrivate.h"

FDirectoryWatcherWindows::FDirectoryWatcherWindows()
{
	NumRequests = 0;
}

FDirectoryWatcherWindows::~FDirectoryWatcherWindows()
{
	if ( RequestMap.Num() != 0 )
	{
		// Delete any remaining requests here. These requests are likely from modules which are still loaded at the time that this module unloads.
		for (TMap<FDirectoryWithFlags, FDirectoryWatchRequestWindows*>::TConstIterator RequestIt(RequestMap); RequestIt; ++RequestIt)
		{
			if ( ensure(RequestIt.Value()) )
			{
				// make sure we end the watch request, as we may get a callback if a request is in flight
				RequestIt.Value()->EndWatchRequest();
				delete RequestIt.Value();
				NumRequests--;
			}
		}

		RequestMap.Empty();
	}

	if ( RequestsPendingDelete.Num() != 0 )
	{
		for ( int32 RequestIdx = 0; RequestIdx < RequestsPendingDelete.Num(); ++RequestIdx )
		{
			delete RequestsPendingDelete[RequestIdx];
			NumRequests--;
		}
	}

	// Make sure every request that was created is destroyed
	ensure(NumRequests == 0);
}

bool FDirectoryWatcherWindows::RegisterDirectoryChangedCallback_Handle( const FString& Directory, const FDirectoryChanged& InDelegate, FDelegateHandle& Handle, uint32 Flags )
{
	const FDirectoryWithFlags DirectoryKey(Directory, Flags);
	FDirectoryWatchRequestWindows** RequestPtr = RequestMap.Find(DirectoryKey);
	FDirectoryWatchRequestWindows* Request = NULL;
	
	if ( RequestPtr )
	{
		// There should be no NULL entries in the map
		check (*RequestPtr);

		Request = *RequestPtr;
	}
	else
	{
		Request = new FDirectoryWatchRequestWindows(Flags);
		NumRequests++;

		// Begin reading directory changes
		if ( !Request->Init(Directory) )
		{
			uint32 Error = GetLastError();
			TCHAR ErrorMsg[1024];
			FPlatformMisc::GetSystemErrorMessage(ErrorMsg, 1024, Error);
			UE_LOG(LogDirectoryWatcher, Warning, TEXT("Failed to begin reading directory changes for %s. Error: %s (0x%08x)"), *Directory, ErrorMsg, Error);

			delete Request;
			NumRequests--;
			return false;
		}

		RequestMap.Add(DirectoryKey, Request);
	}

	Handle = Request->AddDelegate(InDelegate);

	return true;
}

bool FDirectoryWatcherWindows::UnregisterDirectoryChangedCallback_Handle( const FString& Directory, FDelegateHandle InHandle )
{
	for (const auto& RequestPair : RequestMap)
	{
		if (RequestPair.Key.Key == Directory)
		{
			// There should be no NULL entries in the map
			check(RequestPair.Value);

			if (RequestPair.Value->RemoveDelegate(InHandle))
			{
				if (!RequestPair.Value->HasDelegates())
				{
					// Remove from the active map and add to the pending delete list
					RequestMap.Remove(RequestPair.Key);
					RequestsPendingDelete.AddUnique(RequestPair.Value);

					// Signal to end the watch which will mark this request for deletion
					RequestPair.Value->EndWatchRequest();
				}

				return true;
			}
		}
	}

	return false;
}

void FDirectoryWatcherWindows::Tick( float DeltaSeconds )
{
	TArray<HANDLE> DirectoryHandles;
	TMap<FDirectoryWithFlags, FDirectoryWatchRequestWindows*> InvalidRequestsToDelete;

	// Find all handles to listen to and invalid requests to delete
	for (TMap<FDirectoryWithFlags, FDirectoryWatchRequestWindows*>::TConstIterator RequestIt(RequestMap); RequestIt; ++RequestIt)
	{
		if ( RequestIt.Value()->IsPendingDelete() )
		{
			InvalidRequestsToDelete.Add(RequestIt.Key(), RequestIt.Value());
		}
		else
		{
			DirectoryHandles.Add(RequestIt.Value()->GetDirectoryHandle());
		}
	}

	// Remove all invalid requests from the request map and add them to the pending delete list so they will be deleted below
	for (TMap<FDirectoryWithFlags, FDirectoryWatchRequestWindows*>::TConstIterator RequestIt(InvalidRequestsToDelete); RequestIt; ++RequestIt)
	{
		RequestMap.Remove(RequestIt.Key());
		RequestsPendingDelete.AddUnique(RequestIt.Value());
	}

	// Trigger any file changed delegates that are queued up
	// We need to do this in batches of MAXIMUM_WAIT_OBJECTS-1 as described by the documentation for MsgWaitForMultipleObjectsEx
	if ( DirectoryHandles.Num() > 0 )
	{
		static const int32 BatchSize = MAXIMUM_WAIT_OBJECTS - 1;
		for (int32 HandleOffset = 0; HandleOffset < DirectoryHandles.Num(); HandleOffset += BatchSize)
		{
			MsgWaitForMultipleObjectsEx(FMath::Min(DirectoryHandles.Num() - HandleOffset, BatchSize), DirectoryHandles.GetData() + HandleOffset, 0, QS_ALLEVENTS, MWMO_ALERTABLE);
		}
	}

	// Delete any stale or invalid requests
	for ( int32 RequestIdx = RequestsPendingDelete.Num() - 1; RequestIdx >= 0; --RequestIdx )
	{
		FDirectoryWatchRequestWindows* Request = RequestsPendingDelete[RequestIdx];

		if ( Request->IsPendingDelete() )
		{
			// This request is safe to delete. Delete and remove it from the list
			delete Request;
			NumRequests--;
			RequestsPendingDelete.RemoveAt(RequestIdx);
		}
	}

	// Finally, trigger any file change notification delegates
	for (TMap<FDirectoryWithFlags, FDirectoryWatchRequestWindows*>::TConstIterator RequestIt(RequestMap); RequestIt; ++RequestIt)
	{
		RequestIt.Value()->ProcessPendingNotifications();
	}
}
