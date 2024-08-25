// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationClient.h"

#include "Assets/MultiUserReplicationClientPreset.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/ReplicationWidgetFactories.h"
#include "Replication/Stream/Discovery/MultiUserStreamExtender.h"
#include "Replication/Stream/StreamChangeTracker.h"
#include "Replication/Submission/Data/AuthoritySubmission.h"
#include "Settings/MultiUserReplicationSettings.h"

#include "Misc/CoreDelegates.h"
#include "Replication/ClientReplicationWidgetFactories.h"

namespace UE::MultiUserClient
{
	FReplicationClient::FReplicationClient(
		const FGuid& EndpointId,
		FReplicationDiscoveryContainer& InDiscoveryContainer,
		FGlobalAuthorityCache& InAuthorityCache,
		UMultiUserReplicationClientPreset& InSessionContent,
		TUniquePtr<IClientStreamSynchronizer> InStreamSynchronizer,
		TUniquePtr<IClientAuthoritySynchronizer> InAuthoritySynchronizer,
		TUniquePtr<ISubmissionWorkflow> InSubmissionWorkflow
		)
		: EndpointId(EndpointId)
		, ClientContentStorage(&InSessionContent)
		, StreamSynchronizer(MoveTemp(InStreamSynchronizer))
		, AuthoritySynchronizer(MoveTemp(InAuthoritySynchronizer))
		, SubmissionWorkflow(MoveTemp(InSubmissionWorkflow))
		, SubmissionQueue(*SubmissionWorkflow)
		, ExternalRequestHandler(
			StreamSynchronizer->GetStreamId(),
			FGetStreamContent::CreateLambda([this](){ return &StreamSynchronizer->GetServerState(); }),
			SubmissionQueue
			)
		, LocalClientEditModel(
			ConcertClientSharedSlate::CreateTransactionalStreamModel(
				CreateBaseStreamModel(
					ClientContentStorage->Stream->MakeReplicationMapGetterAttribute(),
					MakeShared<FMultiUserStreamExtender>(EndpointId, InDiscoveryContainer)
					),
				*ClientContentStorage->Stream
				)
			)
		, LocalClientStreamDiffer(
			GetStreamSynchronizer(),
			ClientContentStorage->Stream->MakeReplicationMapGetterAttribute(),
			FStreamChangeTracker::FOnModifyReplicationMap::CreateLambda([this](){ ClientContentStorage->Stream->Modify(); })
			)
		, LocalAuthorityDiffer(EndpointId, *AuthoritySynchronizer, InAuthorityCache)
		, LocalFrequencyChangeTracker(*StreamSynchronizer)
		, ChangeRequestBuilder(EndpointId, InAuthorityCache, *StreamSynchronizer, LocalClientStreamDiffer, LocalAuthorityDiffer, LocalFrequencyChangeTracker)
		, AutoSubmissionPolicy(SubmissionQueue, ChangeRequestBuilder, LocalClientEditModel.Get(), LocalAuthorityDiffer, LocalFrequencyChangeTracker)
		, LevelModificationHandler(GetClientEditModel().Get())
	{
		LocalClientEditModel->OnObjectsChanged().AddRaw(this, &FReplicationClient::OnObjectsChanged);
		LocalClientEditModel->OnPropertiesChanged().AddRaw(this, &FReplicationClient::OnPropertiesChanged);
		LocalAuthorityDiffer.OnChangedOwnedObjects().AddRaw(this, &FReplicationClient::DeferOnModelChanged);

		SubmissionWorkflow->OnAuthorityRequestCompleted_AnyThread().AddRaw(this, &FReplicationClient::OnAuthoritySubmissionCompleted);
		StreamSynchronizer->OnServerStateChanged().AddRaw(this, &FReplicationClient::OnServerStateChanged);

		LevelModificationHandler.OnHierarchyNeedsRefresh().AddLambda([this]()
		{
			OnHierarchyNeedsRefreshDelegate.Broadcast();
		});
	}

	FReplicationClient::~FReplicationClient()
	{
		FCoreDelegates::OnEndFrame.RemoveAll(this);
	}

	bool FReplicationClient::AllowsEditing() const
	{
		return CanEverSubmit(SubmissionWorkflow->GetUploadability()); 
	}

	void FReplicationClient::OnObjectsChanged(
		TConstArrayView<UObject*> AddedObjects,
		TConstArrayView<FSoftObjectPath> RemovedObjects,
		ConcertSharedSlate::EReplicatedObjectChangeReason ReplicatedObjectChangeReason
		)
	{
		DeferOnModelChanged(AddedObjects);
	}

	void FReplicationClient::OnPropertiesChanged()
	{
		DeferOnModelChanged();
	}

	void FReplicationClient::OnServerStateChanged()
	{
		// Whenever this client's server state changes, the UI must be refreshed.

		// The UI adds empty actors. However, we never send them to the server...
		TSet<FSoftObjectPath> StagedObjects;
		for (const TPair<FSoftObjectPath, FConcertReplicatedObjectInfo>& Pair : GetClientContent()->Stream->ReplicationMap.ReplicatedObjects)
		{
			if (Pair.Value.PropertySelection.ReplicatedProperties.IsEmpty())
			{
				StagedObjects.Add(Pair.Key);
			}
		}
		
		// ... if the user removes the last property from the entire actor-component hierarchy, we want the hierarchy to continue to displayed...
		GetClientContent()->Stream->ReplicationMap = GetStreamSynchronizer().GetServerState();
		// ... so add back the staged objects
		for (const FSoftObjectPath& StagedObject : StagedObjects)
		{
			if (const UObject* Object = StagedObject.ResolveObject())
			{
				GetClientContent()->Stream->ReplicationMap.ReplicatedObjects.Add(StagedObject) = FConcertReplicatedObjectInfo::Make(*Object);
			}
		}
		// To remove the hierarchy, the user must click the actor and delete it explicitly, which will call IEditableReplicationStream::RemoveObjects on the staged objects.
		
		DeferOnModelChanged();
	}

	void FReplicationClient::DeferOnModelChanged(TConstArrayView<UObject*> AddedObjects)
	{
		if (!DeferredOnModelChangedData)
		{
			DeferredOnModelChangedData.Emplace();
			FCoreDelegates::OnEndFrame.AddRaw(this, &FReplicationClient::ProcessOnModelChanged);
		}

		Algo::Transform(AddedObjects, DeferredOnModelChangedData->AccumulatedAddedObjects, [](UObject* Object)
		{
			return Object;
		});
	}

	void FReplicationClient::ProcessOnModelChanged()
	{
		check(DeferredOnModelChangedData);
		const FDeferredOnModelChangedData ChangeData = MoveTemp(*DeferredOnModelChangedData);
		DeferredOnModelChangedData.Reset();
		FCoreDelegates::OnEndFrame.RemoveAll(this);
		
		// Could improve performance by just considering what actually changed instead of doing a full rebuild
		// This must be done before SetAuthorityIfAllowed because it uses the cache for checking whether the object has properties assigned
		LocalClientStreamDiffer.RefreshChangesCache();
		
		// Better UX for user: automatically take authority for newly added objects (but only if it is allowed and causes no conflicts)
		TakeAuthorityOverNewlyAddedObjects(ChangeData);
		// Refresh because local authority changes may no longer be valid after modifying the stream
		LocalAuthorityDiffer.RefreshChanges();
		// Check whether added objects are supposed to default to some special frequency settings
		ApplyDefaultFrequencySettings(ChangeData);

		// Finally, let everybody else know.
		OnModelChangedDelegate.Broadcast();
		AutoSubmissionPolicy.ProcessAccumulatedChangesAndSubmit();
	}
	
	void FReplicationClient::TakeAuthorityOverNewlyAddedObjects(const FDeferredOnModelChangedData& ChangeData)
	{
		TArray<FSoftObjectPath> ObjectPaths;
		Algo::TransformIf(ChangeData.AccumulatedAddedObjects, ObjectPaths,
			[](const TWeakObjectPtr<UObject>& Object)
			{
				// The object might have been made invalid last frame.
				return Object.IsValid();
			},
			[](const TWeakObjectPtr<UObject>& Object)
			{
				return FSoftObjectPath(Object.Get()) ;
			});
		LocalAuthorityDiffer.SetAuthorityIfAllowed(ObjectPaths, true);
	}

	void FReplicationClient::ApplyDefaultFrequencySettings(const FDeferredOnModelChangedData& ChangeData)
	{
		UMultiUserReplicationSettings* Settings = UMultiUserReplicationSettings::Get();
		for (const TWeakObjectPtr<UObject>& AddedObject : ChangeData.AccumulatedAddedObjects)
		{
			if (!AddedObject.IsValid())
			{
				continue;
			}
			
			const TOptional<FConcertObjectReplicationSettings> FrequencyOverride = Settings->DetermineObjectFrequencySettings(*AddedObject);
			if (FrequencyOverride)
			{
				LocalFrequencyChangeTracker.AddOverride(AddedObject.Get(), *FrequencyOverride);
			}
		}
	}

	void FReplicationClient::OnAuthoritySubmissionCompleted(const FSubmitAuthorityChangesRequest& Request, const FSubmitAuthorityChangesResponse& Response)
	{
		if (!Response.Response)
		{
			return;
		}

		// Use case: You and another client submit at the same time. You lose. Revert your local changes so the checkboxes accurately reflect the authority state.
		for (const TPair<FSoftObjectPath, FConcertStreamArray>& RejectedObjectPair : Response.Response.GetValue().RejectedObjects)
		{
			LocalAuthorityDiffer.ClearAuthorityChange({ RejectedObjectPair.Key });
		}
	}
}
