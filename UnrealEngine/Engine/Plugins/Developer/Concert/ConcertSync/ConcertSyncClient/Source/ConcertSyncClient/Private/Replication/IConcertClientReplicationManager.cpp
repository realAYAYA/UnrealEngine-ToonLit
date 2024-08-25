// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/IConcertClientReplicationManager.h"

#include "ConcertLogGlobal.h"

#include "Async/Future.h"

namespace UE::ConcertSyncClient::Replication::Private
{
	template<typename Callback>
	void ForEachStreamContainingObject(TArrayView<const FSoftObjectPath> Objects, const IConcertClientReplicationManager& Manager, Callback InCallback)
	{
		for (const FSoftObjectPath& ObjectPath : Objects)
		{
			Manager.ForEachRegisteredStream([&InCallback, &ObjectPath](const FConcertReplicationStream& StreamDescription)
			{
				const bool bStreamContainsObject = StreamDescription.BaseDescription.ReplicationMap.ReplicatedObjects.Contains(ObjectPath);
				if (bStreamContainsObject)
				{
					InCallback(ObjectPath, StreamDescription.BaseDescription.Identifier);
				}
				else
				{
					UE_LOG(LogConcert, Warning, TEXT("Object %s is not a valid argument because it is not contained in any stream."), *ObjectPath.ToString());
				}
				return EBreakBehavior::Continue;
			});
		}
	}
}

bool IConcertClientReplicationManager::HasRegisteredStreams() const
{
	return ForEachRegisteredStream([](const auto&){ return EBreakBehavior::Break; }) == EStreamEnumerationResult::Iterated;
}

TArray<FConcertReplicationStream> IConcertClientReplicationManager::GetRegisteredStreams() const
{
	TArray<FConcertReplicationStream> Result;
	ForEachRegisteredStream([&Result](const FConcertReplicationStream& Description){ Result.Add(Description); return EBreakBehavior::Continue; });
	return Result;
}

TFuture<FConcertReplication_ChangeAuthority_Response> IConcertClientReplicationManager::TakeAuthorityOver(TArrayView<const FSoftObjectPath> Objects)
{
	using namespace UE::ConcertSyncClient::Replication;
	
	if (!HasRegisteredStreams())
	{
		UE_LOG(LogConcert, Error, TEXT("Attempted to take authority while not connected!"));
		TMap<FSoftObjectPath, FConcertStreamArray> Result;
		Algo::Transform(Objects, Result, [](const FSoftObjectPath& Path){ return Path; });
		return MakeFulfilledPromise<FConcertReplication_ChangeAuthority_Response>(FConcertReplication_ChangeAuthority_Response{EReplicationResponseErrorCode::Handled ,MoveTemp(Result) }).GetFuture();
	}

	FConcertReplication_ChangeAuthority_Request Request;
	Private::ForEachStreamContainingObject(Objects, *this,
		[&Request](const FSoftObjectPath& ObjectPath, const FGuid& StreamId)
		{
			Request.TakeAuthority.FindOrAdd(ObjectPath).StreamIds.Add(StreamId);
		});

	// Do not send pointless, empty requests to the server
	if (Request.TakeAuthority.IsEmpty())
	{
		// Not only does this warn about incorrect API use at runtime, this also helps debug (incorrectly written) unit tests
		const FString ObjectsAsString = FString::JoinBy(Objects, TEXT(","), [](const FSoftObjectPath& Path){ return Path.ToString(); });
		UE_LOG(LogConcert, Warning, TEXT("Local client did not register any stream for the given objects. This take authority request will not be sent. Objects: %s"), *ObjectsAsString);
		return MakeFulfilledPromise<FConcertReplication_ChangeAuthority_Response>(FConcertReplication_ChangeAuthority_Response{}).GetFuture();
	}
	
	return RequestAuthorityChange(MoveTemp(Request));
}

TFuture<FConcertReplication_ChangeAuthority_Response> IConcertClientReplicationManager::ReleaseAuthorityOf(TArrayView<const FSoftObjectPath> Objects)
{
	using namespace UE::ConcertSyncClient::Replication;
	
	if (!HasRegisteredStreams())
	{
		UE_LOG(LogConcert, Error, TEXT("Attempted to take authority while not connected!"));
		TMap<FSoftObjectPath, FConcertStreamArray> Result;
		Algo::Transform(Objects, Result, [](const FSoftObjectPath& Path){ return Path; });
		return MakeFulfilledPromise<FConcertReplication_ChangeAuthority_Response>(FConcertReplication_ChangeAuthority_Response{ EReplicationResponseErrorCode::Handled, MoveTemp(Result) }).GetFuture();
	}
	
	FConcertReplication_ChangeAuthority_Request Request;
	Private::ForEachStreamContainingObject(Objects, *this,
		[&Request](const FSoftObjectPath& ObjectPath, const FGuid& StreamId)
		{
			Request.ReleaseAuthority.FindOrAdd(ObjectPath).StreamIds.Add(StreamId);
		});

	// Do not send pointless, empty requests to the server
	if (Request.ReleaseAuthority.IsEmpty())
	{
		// Not only does this warn about incorrect API use at runtime, this also helps debug (incorrectly written) unit tests
		const FString ObjectsAsString = FString::JoinBy(Objects, TEXT(","), [](const FSoftObjectPath& Path){ return Path.ToString(); });
		UE_LOG(LogConcert, Warning, TEXT("Local client did not register any stream for the given objects. This release authority request will not be sent. Objects: %s"), *ObjectsAsString);
		return MakeFulfilledPromise<FConcertReplication_ChangeAuthority_Response>(FConcertReplication_ChangeAuthority_Response{}).GetFuture();
	}
	
	return RequestAuthorityChange(MoveTemp(Request));
}

TMap<FSoftObjectPath, TSet<FGuid>> IConcertClientReplicationManager::GetClientOwnedObjects() const
{
	TMap<FSoftObjectPath, TSet<FGuid>> Result;
	ForEachClientOwnedObject([&Result](const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)
	{
		Result.Emplace(Object, MoveTemp(OwningStreams));
		return EBreakBehavior::Continue;
	});
	return Result;
}
