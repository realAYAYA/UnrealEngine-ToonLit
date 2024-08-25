// Copyright Epic Games, Inc. All Rights Reserved.

template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::ThenServer(TFunction<void(NetworkDataType&)> Action)
{
	CommandBuilder->Do([this, Action]() { Action(static_cast<NetworkDataType&>(*ServerState)); });		
	return *this;
}
template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::ThenServer(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action)
{
	CommandBuilder->Do(Description, [this, Action] { Action(static_cast<NetworkDataType&>(*ServerState)); });
	return *this;
}

template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::ThenClients(TFunction<void(NetworkDataType&)> Action)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Do([this, Action, Index]() { Action(static_cast<NetworkDataType&>(*ClientStates[Index])); });
	}
	return *this;
}
template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::ThenClients(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Do(Description, [this, Action, Index]() { Action(static_cast<NetworkDataType&>(*ClientStates[Index])); });
	}
	return *this;
}

template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::ThenClient(int32 ClientIndex, TFunction<void(NetworkDataType&)> Action)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Do([this, Action, ClientIndex]() { Action(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); });
	return *this;
}
template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::ThenClient(const TCHAR* Description, int32 ClientIndex, TFunction<void(NetworkDataType&)> Action)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Do(Description, [this, Action, ClientIndex]() { Action(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); });
	return *this;
}

template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::UntilServer(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	CommandBuilder->Until(
		[this, Query]() { return Query(static_cast<NetworkDataType&>(*ServerState)); }, Timeout);
	return *this;
}
template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::UntilServer(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	CommandBuilder->Until(
		Description, [this, Query]() { return Query(static_cast<NetworkDataType&>(*ServerState)); }, Timeout);
	return *this;
}

template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::UntilClients(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Until(
			[this, Query, Index]() { return Query(static_cast<NetworkDataType&>(*ClientStates[Index])); }, Timeout);
	}
	return *this;
}
template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::UntilClients(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	for (int32 Index = 0; Index < ClientStates.Num(); Index++)
	{
		CommandBuilder->Until(
			Description, [this, Query, Index]() { return Query(static_cast<NetworkDataType&>(*ClientStates[Index])); }, Timeout);
	}
	return *this;
}
template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::UntilClient(int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Until(
		[this, Query, ClientIndex]() { return Query(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); }, Timeout);
	return *this;
}
template <typename NetworkDataType>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::UntilClient(const TCHAR* Description, int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout)
{
	if (!ClientStates.IsValidIndex(ClientIndex))
	{
		TestRunner->AddError(FString::Printf(TEXT("Invalid client index specified.  Requested Index %d out of %d"), ClientIndex, ClientStates.Num()));
		return *this;
	}

	CommandBuilder->Until(
		Description, [this, Query, ClientIndex]() { return Query(static_cast<NetworkDataType&>(*ClientStates[ClientIndex])); }, Timeout);
	return *this;
}

template <typename NetworkDataType>
template <typename ActorToSpawn, ActorToSpawn* NetworkDataType::*ResultStorage>
inline FPIENetworkComponent<NetworkDataType>& FPIENetworkComponent<NetworkDataType>::SpawnAndReplicate()
{
	static_assert(std::is_convertible_v<ActorToSpawn*, AActor*>, "ActorToSpawn must derive from AActor");

	TSharedPtr<ActorToSpawn*> SharedStorage = MakeShareable(new ActorToSpawn*(nullptr));
	TSharedRef<FNetworkGUID> SharedGuid = MakeShareable(new FNetworkGUID());

	ThenServer(TEXT("Spawning Actor On Server"), [SharedStorage](NetworkDataType& State) {
		*SharedStorage = State.World->template SpawnActor<ActorToSpawn>();
		if (ResultStorage != nullptr)
		{
			State.*ResultStorage = *SharedStorage;
		}
	}).UntilServer(TEXT("Waiting for NetGUID"), [SharedStorage, SharedGuid](NetworkDataType& State) {
		*SharedGuid = State.World->GetNetDriver()->GuidCache->GetNetGUID(*SharedStorage);
		return SharedGuid->IsValid();
	}).UntilClients(TEXT("Waiting for Replication on Clients"), [SharedGuid](NetworkDataType& State) {
		ActorToSpawn* ClientActor = Cast<ActorToSpawn>(State.World->GetNetDriver()->GuidCache->GetObjectFromNetGUID(*SharedGuid, true));
		if (ClientActor == nullptr)
		{
			return false;
		}
		if (ResultStorage != nullptr)
		{
			State.*ResultStorage = ClientActor;
		}
		return true;
	});

	return *this;
}

//////////////////////

template <typename NetworkDataType>
inline FNetworkComponentBuilder<NetworkDataType>::FNetworkComponentBuilder()
{
	static_assert(std::is_convertible_v<NetworkDataType*, FBasePIENetworkComponentState*>, "NetworkDataType must derive from FBaseNetworkComponentState");
}

template <typename NetworkDataType>
inline FNetworkComponentBuilder<NetworkDataType>& FNetworkComponentBuilder<NetworkDataType>::WithClients(int32 InClientCount)
{
	checkf(InClientCount > 0, TEXT("Client count must be greater than 0.  Server only tests should simply use a Spawner"));
	ClientCount = InClientCount;
	return *this;
}

template<typename NetworkDataType>
inline FNetworkComponentBuilder<NetworkDataType>& FNetworkComponentBuilder<NetworkDataType>::WithPacketSimulationSettings(FPacketSimulationSettings* InPacketSimulationSettings)
{
	PacketSimulationSettings = InPacketSimulationSettings;
	return *this;
}

template <typename NetworkDataType>
inline FNetworkComponentBuilder<NetworkDataType>& FNetworkComponentBuilder<NetworkDataType>::AsDedicatedServer()
{
	bIsDedicatedServer = true;
	return *this;
}

template <typename NetworkDataType>
inline FNetworkComponentBuilder<NetworkDataType>& FNetworkComponentBuilder<NetworkDataType>::AsListenServer()
{
	bIsDedicatedServer = false;
	return *this;
}

template <typename NetworkDataType>
inline FNetworkComponentBuilder<NetworkDataType>& FNetworkComponentBuilder<NetworkDataType>::WithGameMode(TSubclassOf<AGameModeBase> InGameMode)
{
	GameMode = InGameMode;
	return *this;
}

template <typename NetworkDataType>
inline FNetworkComponentBuilder<NetworkDataType>& FNetworkComponentBuilder<NetworkDataType>::WithGameInstanceClass(FSoftClassPath InGameInstanceClass)
{
	GameInstanceClass = InGameInstanceClass;
	return *this;
}

template<typename NetworkDataType>
inline void FNetworkComponentBuilder<NetworkDataType>::Build(FPIENetworkComponent<NetworkDataType>& OutNetwork)
{
	NetworkDataType DefaultState{};
	DefaultState.ClientCount = ClientCount;
	DefaultState.bIsDedicatedServer = bIsDedicatedServer;

	OutNetwork.ServerState = MakeUnique<NetworkDataType>(DefaultState);
	OutNetwork.ServerState->ClientConnections.SetNum(ClientCount);

	for (int32 ClientIndex = 0; ClientIndex < ClientCount; ClientIndex++)
	{
		OutNetwork.ClientStates.Add(MakeUnique<NetworkDataType>(DefaultState));
		OutNetwork.ClientStates.Last()->ClientIndex = ClientIndex;
	}

	OutNetwork.PacketSimulationSettings = PacketSimulationSettings;
	OutNetwork.GameMode = GameMode;
	OutNetwork.StateRestorer = FPIENetworkTestStateRestorer{GameInstanceClass, GameMode};
}
