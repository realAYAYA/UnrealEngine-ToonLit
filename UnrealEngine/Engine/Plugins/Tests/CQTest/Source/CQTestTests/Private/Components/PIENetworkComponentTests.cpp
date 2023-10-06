// Copyright Epic Games, Inc. All Rights Reserved.

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "CQTestUnitTestHelper.h"

#include "TestReplicatedActor.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

#if ENABLE_PIE_NETWORK_TEST

NETWORK_TEST_CLASS(StateTest, "TestFramework.CQTest.Network")
{
	struct DerivedState : public FBasePIENetworkComponentState
	{
		int32 IndependentNumber = 0;
	};

	int32 SharedNumber = 0;
	FPIENetworkComponent<DerivedState> Network{ TestRunner, TestCommandBuilder, bInitializing };

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<DerivedState>()
			.WithClients(3)
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(Network_WithMultipleSteps_TriggersStepsInOrder)
	{
		Network
			.ThenServer([&](DerivedState&) { ASSERT_THAT(AreEqual(0, SharedNumber++)); })
			.ThenClient(0, [&](DerivedState&) { ASSERT_THAT(AreEqual(1, SharedNumber++)); })
			.ThenServer([&](DerivedState&) { ASSERT_THAT(AreEqual(2, SharedNumber++)); })
			.Then([&]() { ASSERT_THAT(AreEqual(3, SharedNumber++)); });
	}

	TEST_METHOD(Network_WithServerCommands_RetainsStateBetweenCalls)
	{
		Network
			.ThenServer([&](DerivedState& State) { ASSERT_THAT(AreEqual(0, State.IndependentNumber++)); })
			.ThenServer([&](DerivedState& State) { ASSERT_THAT(AreEqual(1, State.IndependentNumber++)); });
	}

	TEST_METHOD(Network_WithClientCommands_RetainsStateBetweenCalls)
	{
		Network
			.ThenClient(0, [&](DerivedState& State) { ASSERT_THAT(AreEqual(0, State.IndependentNumber++)); })
			.ThenClient(0, [&](DerivedState& State) { ASSERT_THAT(AreEqual(1, State.IndependentNumber++)); });
	}

	TEST_METHOD(Network_WithClientAndServerCommands_DoNotShareState)
	{
		Network
			.ThenServer([&](DerivedState& State) { State.IndependentNumber++; })
			.ThenClient(0, [&](DerivedState& State) { ASSERT_THAT(AreEqual(0, State.IndependentNumber)); });
	}

	TEST_METHOD(Network_WithMultipleClients_DoNotShareState)
	{
		Network
			.ThenClients([&](DerivedState& State) { State.IndependentNumber = State.ClientIndex; })
			.ThenClients([&](DerivedState& State) { ASSERT_THAT(AreEqual(State.ClientIndex, State.IndependentNumber)); });
	}

	TEST_METHOD(Network_WithTickingServerCommand_TicksUntilDone)
	{
		Network
			.UntilServer([&](DerivedState& State) { return ++State.IndependentNumber > 4; })
			.ThenServer([&](DerivedState& State) { ASSERT_THAT(AreEqual(State.IndependentNumber, 5)); });
	}

	TEST_METHOD(Network_WithTickingClientCommands_TicksEachCommand)
	{
		Network
			.UntilClients([&](DerivedState& State) { SharedNumber++;  return ++State.IndependentNumber > 4; })
			.Then([&]() { ASSERT_THAT(AreEqual(15, SharedNumber)); });
	}
};

NETWORK_TEST_CLASS(ReplicationTest, "TestFramework.CQTest.Network")
{
	struct DerivedState : public FBasePIENetworkComponentState
	{
		ATestReplicatedActor* ReplicatedActor = nullptr;
	};

	FPIENetworkComponent<DerivedState> Network{ TestRunner, TestCommandBuilder, bInitializing };
	BEFORE_EACH() {
		FNetworkComponentBuilder<DerivedState>()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(SpawnAndReplicateActor_WithReplicatedActor_ProvidesActorToClients)
	{
		Network.SpawnAndReplicate<ATestReplicatedActor, &DerivedState::ReplicatedActor>()
			.ThenServer([&](DerivedState& ServerState) {
				ASSERT_THAT(IsNotNull(ServerState.ReplicatedActor));
			})
			.ThenClients([&](DerivedState& ClientState) {
				ASSERT_THAT(IsNotNull(ClientState.ReplicatedActor));
			});
	}

	TEST_METHOD(SpawnAndReplicateActor_ThenUpdateProperty_UpdatesPropertyOnClients)
	{
		Network.SpawnAndReplicate<ATestReplicatedActor, &DerivedState::ReplicatedActor>()
			.ThenServer(TEXT("Server Set Value"), [](DerivedState& ServerState) {
				ServerState.ReplicatedActor->ReplicatedInt = 42;
			})
			.UntilClients(TEXT("Clients Check Value"), [](DerivedState& ClientState) {
				return ClientState.ReplicatedActor->ReplicatedInt == 42;
			});
	}
};

NETWORK_TEST_CLASS(GameInstanceTest, "TestFramework.CQTest.Network")
{
	struct SomeGameInstanceClass : UGameInstance
	{
	};

	FPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder, bInitializing };
	BEFORE_EACH()
	{
		FNetworkComponentBuilder()
			.WithGameInstanceClass(FSoftClassPath(SomeGameInstanceClass::StaticClass()))
			.WithGameMode(AGameModeBase::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(NetworkComponent_WithGameInstanceClass_BuildsNetworkWithProvidedGameInstance)
	{
		Network.ThenServer([&](FBasePIENetworkComponentState& ServerState) {
			auto* GameInstance = Cast<SomeGameInstanceClass>(ServerState.World->GetGameInstance());
			ASSERT_THAT(IsNotNull(GameInstance));
		});
	}
};

NETWORK_TEST_CLASS(GameModeTest, "TestFramework.CQTest.Network")
{
	struct ATestGameMode : AGameModeBase
	{
	};

	FPIENetworkComponent<> Network{ TestRunner, TestCommandBuilder, bInitializing };

	BEFORE_EACH()
	{
		FNetworkComponentBuilder()
			.WithGameInstanceClass(UGameInstance::StaticClass())
			.WithGameMode(ATestGameMode::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(NetworkComponent_WithGameMode_BuildsNetworkWithProvidedGameMode)
	{
		Network.ThenServer([&](auto& ServerState) {
			auto* GameMode = Cast<ATestGameMode>(ServerState.World->GetAuthGameMode());
			ASSERT_THAT(IsNotNull(GameMode));
		});
	}
};

NETWORK_TEST_CLASS(SetupErrorTest, "TestFramework.CQTest.Network")
{
	FPIENetworkComponent<> Network{ TestRunner, TestCommandBuilder, bInitializing };

	AFTER_EACH() 
	{
		ClearExpectedError(*TestRunner, TEXT("Failed to initialize Network Component"));
	}
	TEST_METHOD(NetworkComponent_WithoutUsingBuilder_AddsErrorAndDoesNotCrash)
	{
		Network.ThenServer([&](auto&) { Assert.Fail("Unexpected Error"); });
	}
};

#endif // ENABLE_PIE_NETWORK_TEST