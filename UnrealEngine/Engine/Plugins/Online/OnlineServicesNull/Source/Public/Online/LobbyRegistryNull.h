// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/LobbiesCommon.h"

namespace UE::Online {
using IOnlineLobbyIdRegistry = IOnlineIdRegistry<OnlineIdHandleTags::FLobby>; // todo: remove this when its added to the global scope
class FOnlineLobbyIdRegistryNull : public IOnlineLobbyIdRegistry
{
public:
	static FOnlineLobbyIdRegistryNull& Get();

	const FLobbyId* Find(FString LobbyIdStr);
	FLobbyId FindOrAdd(FString LobbyIdStr);
	FLobbyId GetNext();

	// Begin IOnlineAccountIdRegistry
	virtual FString ToLogString(const FLobbyId& LobbyId) const override;
	virtual TArray<uint8> ToReplicationData(const FLobbyId& LobbyId) const override;
	virtual FLobbyId FromReplicationData(const TArray<uint8>& ReplicationString) override;
	// End IOnlineAccountIdRegistry

	virtual ~FOnlineLobbyIdRegistryNull() = default;

private:
	const FString* GetInternal(const FLobbyId& LobbyId) const;
	TArray<FString> Ids;
	TMap<FString, FLobbyId> StringToId; 

};

} // namespace UE::Online