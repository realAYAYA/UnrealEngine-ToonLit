// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiStreamModel.h"

#include "IClientSelectionModel.h"
#include "Replication/Client/ReplicationClient.h"
#include "Replication/Client/ReplicationClientManager.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"

namespace UE::MultiUserClient
{
	FMultiStreamModel::FMultiStreamModel(IClientSelectionModel& InClientSelectionModel, FReplicationClientManager& InClientManager)
		: ClientSelectionModel(InClientSelectionModel)
		, ClientManager(InClientManager)
	{
		ClientSelectionModel.OnSelectionChanged().AddRaw(this, &FMultiStreamModel::RebuildStreamsSets);
		RebuildStreamsSets();
	}

	void FMultiStreamModel::ForEachClient(TFunctionRef<EBreakBehavior(const FReplicationClient*)> ProcessClient) const
	{
		for (const FReplicationClient* ReadOnlyClient : GetCachedReadOnlyClients())
		{
			if (ProcessClient(ReadOnlyClient) == EBreakBehavior::Break)
			{
				return;
			}
		}
		for (const FReplicationClient* WritableClient : GetCachedWritableClients())
		{
			if (ProcessClient(WritableClient) == EBreakBehavior::Break)
			{
				return;
			}
		}
	}

	TSet<TSharedRef<ConcertSharedSlate::IReplicationStreamModel>> FMultiStreamModel::GetReadOnlyStreams() const
	{
		TSet<TSharedRef<ConcertSharedSlate::IReplicationStreamModel>> Result;
		Algo::Transform(CachedReadOnlyClients, Result, [](const FReplicationClient* Client){ return Client->GetClientEditModel(); });
		return Result;
	}

	TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> FMultiStreamModel::GetEditableStreams() const
	{
		TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> Result;
		Algo::Transform(CachedWritableClients, Result, [](const FReplicationClient* Client){ return Client->GetClientEditModel(); });
		return Result;
	}

	void FMultiStreamModel::RebuildStreamsSets()
	{
		// Cannot just iterate through CachedReadOnlyClients because it may contain stale clients that were just removed
		ClientManager.ForEachClient([this](FReplicationClient& Client)
		{
			Client.OnModelChanged().RemoveAll(this);
			return EBreakBehavior::Continue;
		});
		
		TSet<const FReplicationClient*> ReadOnlyClients;
		TSet<const FReplicationClient*> WritableClients;
		ClientSelectionModel.ForEachSelectedClient([this, &ReadOnlyClients, &WritableClients](FReplicationClient& Client)
		{
			const bool bIsUploadable = CanEverSubmit(Client.GetSubmissionWorkflow().GetUploadability());
			const TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel> Stream = Client.GetClientEditModel();
			Client.OnModelChanged().AddRaw(this, &FMultiStreamModel::OnStreamExternallyChanged, Stream.ToWeakPtr());
			
			if (bIsUploadable)
			{
				WritableClients.Add(&Client);
			}
			else
			{
				ReadOnlyClients.Add(&Client);
			}
			
			return EBreakBehavior::Continue;
		});

		const bool bReadOnlyStayedSame = CachedReadOnlyClients.Num() == ReadOnlyClients.Num() && CachedReadOnlyClients.Includes(ReadOnlyClients);
		if (!bReadOnlyStayedSame)
		{
			CachedReadOnlyClients = MoveTemp(ReadOnlyClients);
		}
		const bool bWritableStayedSame = CachedWritableClients.Num() == WritableClients.Num() && CachedWritableClients.Includes(WritableClients);
		if (!bWritableStayedSame)
		{
			CachedWritableClients = MoveTemp(WritableClients);
		}

		if (!bReadOnlyStayedSame || !bWritableStayedSame)
		{
			OnStreamSetChangedDelegate.Broadcast();
		}
	}

	void FMultiStreamModel::OnStreamExternallyChanged(TWeakPtr<ConcertSharedSlate::IEditableReplicationStreamModel> ChangedStream)
	{
		if (const TSharedPtr<ConcertSharedSlate::IEditableReplicationStreamModel> ChangedStreamPin = ChangedStream.Pin())
		{
			OnReadOnlyStreamChangedDelegate.Broadcast(ChangedStreamPin.ToSharedRef());
		}
	}
}
