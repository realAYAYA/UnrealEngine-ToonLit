// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

#define ENABLE_PIE_NETWORK_TEST WITH_EDITOR && (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

#if ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "PIENetworkTestStateRestorer.h"

#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"

struct FBasePIENetworkComponentState
{
	UWorld* World = nullptr;
	TArray<UNetConnection*> ClientConnections;
	int32 ClientIndex = INDEX_NONE;
	int32 ClientCount = 2;
	bool bIsDedicatedServer = true;
};

class CQTEST_API FBasePIENetworkComponent
{
public:
	FBasePIENetworkComponent(FAutomationTestBase* InTestRunner, FTestCommandBuilder& InCommandBuilder, bool IsInitializing);

	FBasePIENetworkComponent& Then(TFunction<void()> Action);
	FBasePIENetworkComponent& Then(const TCHAR* Description, TFunction<void()> Action);

	FBasePIENetworkComponent& Do(TFunction<void()> Action);
	FBasePIENetworkComponent& Do(const TCHAR* Description, TFunction<void()> Action);

	FBasePIENetworkComponent& Until(TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	FBasePIENetworkComponent& Until(TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	
	FBasePIENetworkComponent& StartWhen(TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	FBasePIENetworkComponent& StartWhen(TCHAR* Description, TFunction<bool()> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

protected:
	void StopPie();
	void StartPie();
	bool CollectPieWorlds();
	bool AwaitConnections();
	void RestoreState();

	TUniquePtr<FBasePIENetworkComponentState> ServerState = nullptr;
	TArray<TUniquePtr<FBasePIENetworkComponentState>> ClientStates;
	FAutomationTestBase* TestRunner = nullptr;
	FTestCommandBuilder* CommandBuilder = nullptr;
	FPacketSimulationSettings* PacketSimulationSettings = nullptr;
	TSubclassOf<AGameModeBase> GameMode = TSubclassOf<AGameModeBase>(nullptr);
	FPIENetworkTestStateRestorer StateRestorer;
};

template <typename NetworkDataType>
class FNetworkComponentBuilder;

template <typename NetworkDataType = FBasePIENetworkComponentState>
class FPIENetworkComponent : public FBasePIENetworkComponent
{
public:
	FPIENetworkComponent(FAutomationTestBase* InTestRunner, FTestCommandBuilder& InCommandBuilder, bool IsInitializing)
		: FBasePIENetworkComponent(InTestRunner, InCommandBuilder, IsInitializing) {}

	FPIENetworkComponent& ThenServer(TFunction<void(NetworkDataType&)> Action);
	FPIENetworkComponent& ThenServer(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action);

	FPIENetworkComponent& ThenClients(TFunction<void(NetworkDataType&)> Action);
	FPIENetworkComponent& ThenClients(const TCHAR* Description, TFunction<void(NetworkDataType&)> Action);

	FPIENetworkComponent& ThenClient(int32 ClientIndex, TFunction<void(NetworkDataType&)> Action);
	FPIENetworkComponent& ThenClient(const TCHAR* Description, int32 ClientIndex, TFunction<void(NetworkDataType&)> Action);

	FPIENetworkComponent& UntilServer(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	FPIENetworkComponent& UntilServer(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	FPIENetworkComponent& UntilClients(TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	FPIENetworkComponent& UntilClients(const TCHAR* Description, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	
	FPIENetworkComponent& UntilClient(int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));
	FPIENetworkComponent& UntilClient(const TCHAR* Description, int32 ClientIndex, TFunction<bool(NetworkDataType&)> Query, FTimespan Timeout = FTimespan::FromSeconds(10));

	template<typename ActorToSpawn, ActorToSpawn* NetworkDataType::*ResultStorage>
	FPIENetworkComponent& SpawnAndReplicate();

private:
	friend class FNetworkComponentBuilder<NetworkDataType>;
};


template <typename NetworkDataType = FBasePIENetworkComponentState>
class FNetworkComponentBuilder
{
public:
	FNetworkComponentBuilder();

	FNetworkComponentBuilder& WithClients(int32 ClientCount);
	FNetworkComponentBuilder& AsDedicatedServer();
	FNetworkComponentBuilder& AsListenServer();
	FNetworkComponentBuilder& WithPacketSimulationSettings(FPacketSimulationSettings* InPacketSImulationSettings);
	FNetworkComponentBuilder& WithGameMode(TSubclassOf<AGameModeBase> InGameMode);
	FNetworkComponentBuilder& WithGameInstanceClass(FSoftClassPath InGameInstanceClass);

	void Build(FPIENetworkComponent<NetworkDataType>& OutNetwork);

private:
	FPacketSimulationSettings* PacketSimulationSettings = nullptr;
	TSubclassOf<AGameModeBase> GameMode = TSubclassOf<AGameModeBase>(nullptr);
	FSoftClassPath GameInstanceClass = FSoftClassPath{};
	int32 ClientCount = 2;
	bool bIsDedicatedServer = true;
};

constexpr static uint32 NetworkTestContext = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

#define NETWORK_TEST_CLASS(_ClassName, _TestDir) TEST_CLASS_WITH_FLAGS(_ClassName, _TestDir, NetworkTestContext)

#include "Impl/PIENetworkComponent.inl"

#endif // ENABLE_PIE_NETWORK_TEST