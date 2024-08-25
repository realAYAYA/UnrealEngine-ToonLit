// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EBreakBehavior.h"
#include "Replication/Messages/Handshake.h"
#include "Replication/Messages/ChangeAuthority.h"
#include "Replication/Messages/ChangeStream.h"
#include "Replication/Messages/ClientQuery.h"

template<typename ResultType>
class TFuture;

struct FConcertReplicationStream;

namespace UE::ConcertSyncClient::Replication
{
	struct FJoinReplicatedSessionArgs
	{
		/** The streams this client offers. */
		TArray<FConcertReplicationStream> Streams;
	};

	struct FJoinReplicatedSessionResult
	{
		/** Error code sent by the server. */
		EJoinReplicationErrorCode ErrorCode;
		/** Optional error message to help human user resolve the error. */
		FString DetailedErrorMessage;

		FJoinReplicatedSessionResult(EJoinReplicationErrorCode ErrorCode, FString DetailedErrorMessage = TEXT(""))
			: ErrorCode(ErrorCode)
			, DetailedErrorMessage(MoveTemp(DetailedErrorMessage))
		{}
	};
}

/**
 * Handles all communication with the server regarding replication.
 * 
 * Keeps a list of properties to send along with their send rules.
 * Tells the server which properties this client is interested in receiving.
 */
class CONCERTSYNCCLIENT_API IConcertClientReplicationManager
{
public:

	/**
	 * Joins a replication session.
	 * Subsequent calls to JoinReplicationSession will fail until either the resulting TFuture returns or LeaveReplicationSession is called.
	 * @note This future can finish on any thread (e.g. when message endpoint times out); usually it finishes on the game thread.  
	 */
	virtual TFuture<UE::ConcertSyncClient::Replication::FJoinReplicatedSessionResult> JoinReplicationSession(UE::ConcertSyncClient::Replication::FJoinReplicatedSessionArgs Args) = 0;
	/** Leaves the current replication session. */
	virtual void LeaveReplicationSession() = 0;

	/**
	 * Whether it is valid to call JoinReplicationSession right now. Returns false if it was called and its future has not yet concluded.
	 * If this returns true, you can call JoinReplicationSession to attempt joining the session.
	 */
	virtual bool CanJoin() = 0;
	/** Whether JoinReplicationSession completed successfully and LeaveReplicationSession has not yet been called. */
	virtual bool IsConnectedToReplicationSession() = 0;

	enum class EStreamEnumerationResult { NoRegisteredStreams, Iterated };
	/**
	 * Iterates the streams the client has registered with the server.
	 * It only makes sense to call this function the manager has joined a replication session.
	 * @return Whether this manager is connected to a session (Iterated) or not (NoRegisteredStreams).
	 * @note This future can finish on any thread (e.g. when message endpoint times out); usually it finishes on the game thread.  
	 */
	virtual EStreamEnumerationResult ForEachRegisteredStream(TFunctionRef<EBreakBehavior(const FConcertReplicationStream& Stream)> Callback) const = 0;
	/** @return Whether this manager is has any registered streams (basically whether ForEachRegisteredStream returns EStreamEnumerationResult::Iterated). */
	bool HasRegisteredStreams() const;
	/** @return The streams registered with the server. */
	TArray<FConcertReplicationStream> GetRegisteredStreams() const;
	
	/**
	 * Requests from the server to change the authority over some objects.
	 * @note This future can finish on any thread (e.g. when message endpoint times out); usually it finishes on the game thread.  
	 */
	virtual TFuture<FConcertReplication_ChangeAuthority_Response> RequestAuthorityChange(FConcertReplication_ChangeAuthority_Request Args) = 0;
	/** Util function that will request authority for all streams for the given objects. */
	TFuture<FConcertReplication_ChangeAuthority_Response> TakeAuthorityOver(TArrayView<const FSoftObjectPath> Objects);
	/** Util function that will let go over all authority of the given objects. */
	TFuture<FConcertReplication_ChangeAuthority_Response> ReleaseAuthorityOf(TArrayView<const FSoftObjectPath> Objects);

	enum class EAuthorityEnumerationResult { NoAuthorityAvailable, Iterated };
	/**
	 * Iterates through all objects that this client has authority over.
	 * @return Whether this manager is connected to a session (Iterated) or not (NoAuthorityAvailable)
	 */
	virtual EAuthorityEnumerationResult ForEachClientOwnedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object, TSet<FGuid>&& OwningStreams)> Callback) const = 0;
	/** @return All the streams this client has registered that have authority for the given ObjectPath. */
	virtual TSet<FGuid> GetClientOwnedStreamsForObject(const FSoftObjectPath& ObjectPath) const = 0;
	/** @return All owned objects and associated owning streams. */
	TMap<FSoftObjectPath, TSet<FGuid>> GetClientOwnedObjects() const;

	/**
	 * Requests replication info about other clients, including the streams registered and which objects they have authority over (i.e. are sending).
	 * @note This future can finish on any thread (e.g. when message endpoint times out); usually it finishes on the game thread.  
	 */
	virtual TFuture<FConcertReplication_QueryReplicationInfo_Response> QueryClientInfo(FConcertReplication_QueryReplicationInfo_Request Args) = 0;

	/**
	 * Requests to change the client's registered stream.
	 * @note This future can finish on any thread (e.g. when message endpoint times out); usually it finishes on the game thread.  
	 */
	virtual TFuture<FConcertReplication_ChangeStream_Response> ChangeStream(FConcertReplication_ChangeStream_Request Args) = 0;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPreStreamsChanged,
		const FConcertReplication_ChangeStream_Request&,
		const FConcertReplication_ChangeStream_Response&
		);
	/** Called right before the result of GetRegisteredStreams changes. */
	virtual FOnPreStreamsChanged& OnPreStreamsChanged() = 0;
	
	DECLARE_MULTICAST_DELEGATE(FOnPostStreamsChanged);
	/** Called right after the result of GetRegisteredStreams has changed. */
	virtual FOnPostStreamsChanged& OnPostStreamsChanged() = 0;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPreAuthorityChanged,
		const FConcertReplication_ChangeAuthority_Request&,
		const FConcertReplication_ChangeAuthority_Response&
		);
	/** Called right before GetClientOwnedObjects changes. */
	virtual FOnPreAuthorityChanged& OnPreAuthorityChanged() = 0;
	
	DECLARE_MULTICAST_DELEGATE(FOnPostAuthorityChanged);
	/** Called right after GetClientOwnedObjects has changed. */
	virtual FOnPostAuthorityChanged& OnPostAuthorityChanged() = 0;
	
	virtual ~IConcertClientReplicationManager() = default;
};
	