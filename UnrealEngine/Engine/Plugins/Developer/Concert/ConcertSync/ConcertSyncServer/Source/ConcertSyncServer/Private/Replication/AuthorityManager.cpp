// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuthorityManager.h"

#include "ConcertLogGlobal.h"
#include "IConcertSession.h"
#include "Replication/AuthorityConflictSharedUtils.h"
#include "Replication/Data/ObjectIds.h"
#include "Replication/Data/ReplicationStream.h"
#include "Replication/Messages/ChangeAuthority.h"

namespace UE::ConcertSyncServer::Replication
{
	namespace Private
	{
		class FServerGroundTruth : public ConcertSyncCore::Replication::AuthorityConflictUtils::IReplicationGroundTruth
		{
			const FAuthorityManager& Owner;
			const IAuthorityManagerGetters& Getters;
		public:
			
			FServerGroundTruth(const FAuthorityManager& Owner, const IAuthorityManagerGetters& Getters)
				: Owner(Owner)
				, Getters(Getters)
			{}

			virtual void ForEachStream(const FGuid& ClientEndpointId, TFunctionRef<EBreakBehavior(const FGuid& StreamId, const FConcertObjectReplicationMap& ReplicationMap)> Callback) const override
			{
				Getters.ForEachStream(ClientEndpointId, [&Callback](const FConcertReplicationStream& Stream)
				{
					return Callback(Stream.BaseDescription.Identifier, Stream.BaseDescription.ReplicationMap);
				});
			}
			
			virtual void ForEachSendingClient(TFunctionRef<EBreakBehavior(const FGuid& ClientEndpointId)> Callback) const override
			{
				Getters.ForEachSendingClient(Callback);
			}
			
			virtual bool HasAuthority(const FGuid& ClientId, const FGuid& StreamId, const FSoftObjectPath& ObjectPath) const override
			{
				return Owner.HasAuthorityToChange({ { StreamId, ObjectPath }, ClientId });
			}
		};
		
		static void ForEachReplicatedObject(
			const TMap<FSoftObjectPath, FConcertStreamArray>& Map,
			TFunctionRef<void(const FGuid& StreamId, const FSoftObjectPath& ObjectPath)> Callback
			)
		{
			for (const TPair<FSoftObjectPath, FConcertStreamArray>& ObjectInfo : Map)
			{
				for (const FGuid& StreamId : ObjectInfo.Value.StreamIds)
				{
					Callback(StreamId, ObjectInfo.Key);
				}
			}
		}
	}
	
	FAuthorityManager::FAuthorityManager(IAuthorityManagerGetters& Getters, TSharedRef<IConcertSession> InSession)
		: Getters(Getters)
		, Session(MoveTemp(InSession))
	{
		Session->RegisterCustomRequestHandler<FConcertReplication_ChangeAuthority_Request, FConcertReplication_ChangeAuthority_Response>(this, &FAuthorityManager::HandleChangeAuthorityRequest);
	}

	FAuthorityManager::~FAuthorityManager()
	{
		Session->UnregisterCustomRequestHandler<FConcertReplication_ChangeAuthority_Request>();
	}

	bool FAuthorityManager::HasAuthorityToChange(const FConcertReplicatedObjectId& ObjectChange) const
	{
		const FClientAuthorityData* AuthorityData = ClientAuthorityData.Find(ObjectChange.SenderEndpointId);
		const TSet<FSoftObjectPath>* OwnedObjects = AuthorityData ? AuthorityData->OwnedObjects.Find(ObjectChange.StreamId) : nullptr;
		return OwnedObjects && OwnedObjects->Contains(ObjectChange.Object);
	}

	void FAuthorityManager::EnumerateAuthority(const FClientId& ClientId, const FStreamId& StreamId, TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Callback) const
	{
		const FClientAuthorityData* AuthorityData = ClientAuthorityData.Find(ClientId);
		const TSet<FSoftObjectPath>* OwnedObjects = AuthorityData ? AuthorityData->OwnedObjects.Find(StreamId) : nullptr;
		if (!OwnedObjects)
		{
			return;
		}
		
		for (const FSoftObjectPath& AuthoredObject : *OwnedObjects)
		{
			if (Callback(AuthoredObject) == EBreakBehavior::Break)
			{
				break;
			}
		}
	}

	FAuthorityManager::EAuthorityResult FAuthorityManager::EnumerateAuthorityConflicts(
		const FConcertReplicatedObjectId& Object, 
		const FConcertPropertySelection* OverwriteProperties,
		FProcessAuthorityConflict ProcessConflict
		) const
	{
		const FClientId& ClientId = Object.SenderEndpointId;
		const FConcertPropertySelection* PropertiesToCheck = OverwriteProperties;
		if (!PropertiesToCheck)
		{
			const FConcertReplicationStream* Description = FindClientStreamById(ClientId, Object.StreamId);
			const FConcertReplicatedObjectInfo* PropertyInfo = Description ? Description->BaseDescription.ReplicationMap.ReplicatedObjects.Find(Object.Object) : nullptr;
			PropertiesToCheck = PropertyInfo ? &PropertyInfo->PropertySelection : nullptr;
		}

		if (!PropertiesToCheck)
		{
			return EAuthorityResult::NoRegisteredProperties;
		}

		using namespace ConcertSyncCore::Replication;
		using namespace ConcertSyncCore::Replication::AuthorityConflictUtils;
		Private::FServerGroundTruth GroundTruth(*this, Getters);
		const EAuthorityConflict Conflict = AuthorityConflictUtils::EnumerateAuthorityConflicts(
			Object.SenderEndpointId,
			Object.Object,
			PropertiesToCheck->ReplicatedProperties,
			GroundTruth,
			[&ProcessConflict](const FClientId& ClientId, const FStreamId& StreamId, const FConcertPropertyChain& Property)
			{
				return ProcessConflict(ClientId, StreamId, Property);
			});
		return Conflict == EAuthorityConflict::Allowed ? EAuthorityResult::Allowed : EAuthorityResult::Conflict;
	}

	bool FAuthorityManager::CanTakeAuthority(const FConcertReplicatedObjectId& Object) const
	{
		return EnumerateAuthorityConflicts(Object) == EAuthorityResult::Allowed;
	}

	void FAuthorityManager::OnClientLeft(const FClientId& ClientEndpointId)
	{
		ClientAuthorityData.Remove(ClientEndpointId);
	}

	void FAuthorityManager::RemoveAuthority(const FConcertReplicatedObjectId& Object)
	{
		const FClientId& ClientId = Object.SenderEndpointId;
		FClientAuthorityData* ClientData = ClientAuthorityData.Find(ClientId);
		if (!ClientData)
		{
			return;
		}

		const FStreamId& StreamId = Object.StreamId;
		TSet<FSoftObjectPath>* AuthoredObjects = ClientData->OwnedObjects.Find(StreamId);
		if (!AuthoredObjects)
		{
			return;
		}

		// This is all that is needed to remove authority.
		AuthoredObjects->Remove(Object.Object);
		
		// Clean-up potentially empty entries
		if (AuthoredObjects->IsEmpty())
		{
			ClientAuthorityData.Remove(StreamId);
			if (ClientData->OwnedObjects.IsEmpty())
			{
				ClientAuthorityData.Remove(ClientId);
			}
		}
	}

	EConcertSessionResponseCode FAuthorityManager::HandleChangeAuthorityRequest(
		const FConcertSessionContext& ConcertSessionContext,
		const FConcertReplication_ChangeAuthority_Request& Request,
		FConcertReplication_ChangeAuthority_Response& Response
		)
	{
		// This log does two things: 1. Identify issues in unit tests / at runtime 2. Warn about possibly malicious attempts when the server runs.
		UE_CLOG(Request.TakeAuthority.IsEmpty() && Request.ReleaseAuthority.IsEmpty(), LogConcert, Warning, TEXT("Received invalid authority request (TakeAuthority.Num() == 0 and ReleaseAuthority.Num() == 0)"));
		
		FClientAuthorityData& AuthorityData = ClientAuthorityData.FindOrAdd(ConcertSessionContext.SourceEndpointId);
		const FClientId& ClientId = ConcertSessionContext.SourceEndpointId;
		Private::ForEachReplicatedObject(Request.TakeAuthority, [this, &Response, &AuthorityData, &ClientId](const FStreamId& StreamId, const FSoftObjectPath& ObjectPath)
		{
			const FConcertReplicatedObjectId ObjectToAuthor{ { StreamId, ObjectPath }, ClientId };
			if (CanTakeAuthority(ObjectToAuthor))
			{
				UE_LOG(LogConcert, Log, TEXT("Transferred authority of %s to client %s for their stream %s"), *ObjectPath.ToString(), *ClientId.ToString(EGuidFormats::Short), *StreamId.ToString(EGuidFormats::Short));
				AuthorityData.OwnedObjects.FindOrAdd(StreamId).Add(ObjectPath);
			}
			else
			{
				UE_LOG(LogConcert, Log, TEXT("Rejected %s request of authority over %s in stream %s"), *ClientId.ToString(EGuidFormats::Short), *ObjectPath.ToString(), *StreamId.ToString(EGuidFormats::Short));
				Response.RejectedObjects.FindOrAdd(ObjectPath).StreamIds.AddUnique(StreamId);
			}
		});
		
		Private::ForEachReplicatedObject(Request.ReleaseAuthority, [this, &AuthorityData](const FStreamId& StreamId, const FSoftObjectPath& ObjectPath)
		{
			TSet<FSoftObjectPath>* OwnedObjects = AuthorityData.OwnedObjects.Find(StreamId);
			// Though dubious, it is a valid request for the client to release non-owned objects
			if (!OwnedObjects)
			{
				return;
			}

			OwnedObjects->Remove(ObjectPath);
			// Avoid memory leaking
			if (OwnedObjects->IsEmpty())
			{
				AuthorityData.OwnedObjects.Remove(StreamId);
			}
		});

		Response.ErrorCode = EReplicationResponseErrorCode::Handled;
		return EConcertSessionResponseCode::Success;
	}
	
	const FConcertReplicationStream* FAuthorityManager::FindClientStreamById(const FClientId& ClientId, const FStreamId& StreamId) const
	{
		const FConcertReplicationStream* StreamDescription = nullptr;
		Getters.ForEachStream(ClientId, [&StreamId, &StreamDescription](const FConcertReplicationStream& Stream) mutable
		{
			if (Stream.BaseDescription.Identifier == StreamId)
			{
				StreamDescription = &Stream;
				return EBreakBehavior::Break;
			}
			return EBreakBehavior::Continue;
		});
		return StreamDescription;
	}
}
