// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertServerReplicationManager.h"

#include "AuthorityManager.h"
#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/ChangeStreamSharedUtils.h"
#include "Replication/ConcertReplicationClient.h"
#include "Replication/Messages/ChangeStream.h"
#include "Replication/Messages/Handshake.h"
#include "Util/LogUtils.h"

namespace UE::ConcertSyncServer::Replication
{
	namespace Private
	{
		static const FConcertReplicationStream* FindExistingStream(const FConcertReplicationClient& Client, const FGuid& StreamId)
		{
			return Client.GetStreamDescriptions().FindByPredicate([&StreamId](const FConcertReplicationStream& Description)
			{
				return Description.BaseDescription.Identifier == StreamId;
			});
		}
		
		/** Validates that ObjectsToPut writes only to pre-existing streams. */
		static void ValidatePutObjectsRequestSemantics(const FConcertReplication_ChangeStream_Request& Request, const FConcertReplicationClient& Client, FConcertReplication_ChangeStream_Response& OutResponse)
		{
			const auto PutObjectHasEnoughData = [](const FConcertReplicationStream* ExistingStream, const FSoftObjectPath& ChangedObjectPath, const FConcertReplication_ChangeStream_PutObject& PutObject)
			{
				const bool bHasProperties = !PutObject.Properties.ReplicatedProperties.IsEmpty();
				const bool bHasClassPath = !PutObject.ClassPath.IsNull();
				const bool bIsEditingExistingObjectDefinition = ExistingStream->BaseDescription.ReplicationMap.ReplicatedObjects.Contains(ChangedObjectPath);
				const bool bHasEnoughData = (!bIsEditingExistingObjectDefinition && bHasProperties && bHasClassPath)
					|| (bIsEditingExistingObjectDefinition && bHasProperties)
					|| (bIsEditingExistingObjectDefinition && bHasClassPath);
				return bHasEnoughData;
			};
			
			for (const TPair<FConcertObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& Change : Request.ObjectsToPut)
			{
				const FConcertObjectInStreamID ChangedObject = Change.Key;
				
				const FGuid& StreamToModify = ChangedObject.StreamId;
				const FConcertReplicationStream* ExistingStream = FindExistingStream(Client, StreamToModify);
				const bool bStreamExists = ExistingStream != nullptr;
				if (bStreamExists)
				{
					// FConcertReplicatedObjectInfo must have non-empty values for its members. Hence we must check that
					// 1. if creating a new entry, both fields are given
					// 2. if writing to pre-existing entry, at least one field is given
					const FConcertReplication_ChangeStream_PutObject& PutObject = Change.Value; 
					if (!PutObjectHasEnoughData(ExistingStream, ChangedObject.Object, PutObject))
					{
						OutResponse.ObjectsToPutSemanticErrors.Add(ChangedObject, EConcertPutObjectErrorCode::MissingData);
					}
				}
				else
				{
					// ObjectsToPut can only write to pre-existing streams
					UE_LOG(LogConcert, Log, TEXT("Semantic error: unknown stream %s"), *StreamToModify.ToString(EGuidFormats::Short));
					OutResponse.ObjectsToPutSemanticErrors.Add(ChangedObject, EConcertPutObjectErrorCode::UnresolvedStream);
				}
			}
		}

		/** Checks that StreamsToAdd do not conflict with pre-existing ones and that all IDs in the request are also unique. */
		static void ValidateAddedStreamsAreUnique(const FConcertReplication_ChangeStream_Request& Request, const FConcertReplicationClient& Client, FConcertReplication_ChangeStream_Response& OutResponse)
		{
			TSet<FGuid> DuplicateEntryDetection;
			for (const FConcertReplicationStream& NewStream : Request.StreamsToAdd)
			{
				// StreamsToAdd is invalid if there is already a stream with the same ID registered ...
				const FGuid& NewStreamId = NewStream.BaseDescription.Identifier;
				const bool bIdAlreadyExists = FindExistingStream(Client, NewStreamId) != nullptr;

				// ... or StreamsToAdd contains the same ID multiple times
				bool bIsDuplicateEntry = false;
				DuplicateEntryDetection.FindOrAdd(NewStreamId, &bIsDuplicateEntry);
				
				if (bIdAlreadyExists || bIsDuplicateEntry)
				{
					UE_LOG(LogConcert, Log, TEXT("Duplicate stream entry %s"), *NewStreamId.ToString(EGuidFormats::Short));
					OutResponse.FailedStreamCreation.Add(NewStreamId);
				}
			}
		}

		/** If the requesting client has authority over a changed object, checks that no other client has authority over the properties being added. */
		static void LookForAuthorityConflicts(const FConcertReplication_ChangeStream_Request& Request, const FConcertReplicationClient& Client, const FAuthorityManager& AuthorityManager, FConcertReplication_ChangeStream_Response& OutResponse)
		{
			const FGuid ClientEndpointId = Client.GetClientEndpointId();
			for (const TPair<FConcertObjectInStreamID, FConcertReplication_ChangeStream_PutObject>& PutObjectPair : Request.ObjectsToPut)
			{
				// No conflict possible if requesting client does not have authority over the changed object
				const FConcertReplicatedObjectId ReplicatedObjectInfo { { PutObjectPair.Key }, ClientEndpointId };
				if (!AuthorityManager.HasAuthorityToChange(ReplicatedObjectInfo))
				{
					continue;
				}

				// Requester not changing properties (must be changing ClassPath)? Also no conflict possible.
				const FConcertPropertySelection& PropertySelection = PutObjectPair.Value.Properties;
				const bool bIsEditingProperties = !PropertySelection.ReplicatedProperties.IsEmpty();
				if (!bIsEditingProperties)
				{
					continue;
				}

				// Simply check whether any other client is already sending any of the requested properties.
				AuthorityManager.EnumerateAuthorityConflicts(ReplicatedObjectInfo, &PropertySelection,
					[&OutResponse, &ReplicatedObjectInfo](const FGuid& ClientId, const FGuid& StreamId, const FConcertPropertyChain& Property)
					{
						const FConcertReplicatedObjectId ConflictingObject = { { StreamId, ReplicatedObjectInfo.Object }, ClientId };
						OutResponse.AuthorityConflicts.Add(ReplicatedObjectInfo, ConflictingObject);
						
						UE_LOG(LogConcert, Log, TEXT("Authority conflict with client %s for stream %s for property %s"), *ClientId.ToString(EGuidFormats::Short), *StreamId.ToString(EGuidFormats::Short), *Property.ToString());
						return EBreakBehavior::Continue;
					});
			}
		}

		/** Checks whether this request is valid to apply. */
		static bool ShouldAcceptRequest(
			const FConcertReplication_ChangeStream_Request& Request,
			const FConcertReplicationClient& Client,
			const FAuthorityManager& AuthorityManager,
			FConcertReplication_ChangeStream_Response& OutResponse
			)
		{
			OutResponse.ErrorCode = EReplicationResponseErrorCode::Handled;
			
			ValidatePutObjectsRequestSemantics(Request, Client, OutResponse);
			ValidateAddedStreamsAreUnique(Request, Client, OutResponse);
			LookForAuthorityConflicts(Request, Client, AuthorityManager, OutResponse);
			ConcertSyncCore::Replication::ChangeStreamUtils::ValidateFrequencyChanges(Request, Client.GetStreamDescriptions(), &OutResponse.FrequencyErrors);
			
			return OutResponse.IsSuccess();
		}
	}
	
	TAutoConsoleVariable<bool> CVarLogStreamRequestsAndResponsesOnServer(
		TEXT("Concert.Replication.LogStreamRequestsAndResponsesOnServer"),
		false,
		TEXT("Whether to log changes to streams.")
		);
	
	EConcertSessionResponseCode FConcertServerReplicationManager::HandleChangeStreamRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertReplication_ChangeStream_Request& Request,
		FConcertReplication_ChangeStream_Response& Response)
	{
		LogNetworkMessage(CVarLogStreamRequestsAndResponsesOnServer, Request, [&](){ return GetClientName(*Session, ConcertSessionContext.SourceEndpointId); });
		Response = {};
		
		const FGuid SendingClientId = ConcertSessionContext.SourceEndpointId;
		const TUniquePtr<FConcertReplicationClient>* SendingClient = Clients.Find(SendingClientId);
		if (SendingClient && Private::ShouldAcceptRequest(Request, *SendingClient->Get(), AuthorityManager.Get(), Response))
		{
			// If the client had authority over any objects that were removed by this request, authority must be cleaned up
			ConcertSyncCore::Replication::ChangeStreamUtils::ForEachObjectLosingAuthority(Request, SendingClient->Get()->GetStreamDescriptions(),
				[this, &SendingClientId](const FConcertObjectInStreamID& RemovedObject)
				{
					AuthorityManager->RemoveAuthority({ RemovedObject, SendingClientId});
					return EBreakBehavior::Continue;
				});
			
			SendingClient->Get()->ApplyValidatedRequest(Request);
		}
		else
		{
			UE_LOG(LogConcert, Warning, TEXT("Rejecting ChangeStream request from %s"), *SendingClientId.ToString(EGuidFormats::Short));
		}
		
		Response.ErrorCode = EReplicationResponseErrorCode::Handled;
		LogNetworkMessage(CVarLogStreamRequestsAndResponsesOnServer, Response, [&](){ return GetClientName(*Session, ConcertSessionContext.SourceEndpointId); });
		return EConcertSessionResponseCode::Success;
	}
}

