// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/Model/IEditableMultiReplicationStreamModel.h"

#include "Containers/Set.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

enum class EBreakBehavior : uint8;

namespace UE::MultiUserClient
{
	class FReplicationClient;
	class FReplicationClientManager;
	class IClientSelectionModel;

	/** Checks whether clients accept remote changes and categorizes them into read-only and writable. */
	class FMultiStreamModel : public ConcertSharedSlate::IEditableMultiReplicationStreamModel
	{
	public:
		
		FMultiStreamModel(IClientSelectionModel& InClientSelectionModel, FReplicationClientManager& InClientManager);
		
		const TSet<const FReplicationClient*>& GetCachedReadOnlyClients() const { return CachedReadOnlyClients; }
		const TSet<const FReplicationClient*>& GetCachedWritableClients() const { return CachedWritableClients; }
		void ForEachClient(TFunctionRef<EBreakBehavior(const FReplicationClient*)> ProcessClient) const;

		//~ Begin IEditableMultiReplicationStreamModel Interface
		virtual TSet<TSharedRef<ConcertSharedSlate::IReplicationStreamModel>> GetReadOnlyStreams() const override;
		virtual TSet<TSharedRef<ConcertSharedSlate::IEditableReplicationStreamModel>> GetEditableStreams() const override;
		virtual FOnStreamExternallyChanged& OnStreamExternallyChanged() override { return OnReadOnlyStreamChangedDelegate; }
		virtual FOnStreamSetChanged& OnStreamSetChanged() override { return OnStreamSetChangedDelegate; }
		//~ End IEditableMultiReplicationStreamModel Interface

	private:
		
		/** Gets all clients to display and informs when they change. */
		IClientSelectionModel& ClientSelectionModel;
		/** Used to obtain a list of clients for unsubscribing */
		FReplicationClientManager& ClientManager;

		TSet<const FReplicationClient*> CachedReadOnlyClients;
		TSet<const FReplicationClient*> CachedWritableClients;

		FOnStreamExternallyChanged OnReadOnlyStreamChangedDelegate;
		FOnStreamSetChanged OnStreamSetChangedDelegate;
		
		void RebuildStreamsSets();

		/** Handle read-only streams changing */
		void OnStreamExternallyChanged(TWeakPtr<ConcertSharedSlate::IEditableReplicationStreamModel> ChangedStream);
	};
}

