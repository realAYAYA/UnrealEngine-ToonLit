// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineBeacon.h"
#include "Engine/NetConnection.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OnlineBeacon)

DEFINE_LOG_CATEGORY(LogBeacon);

AOnlineBeacon::AOnlineBeacon(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	NetDriver(nullptr),
	BeaconState(EBeaconState::DenyRequests)
{
	NetDriverName = FName(TEXT("BeaconDriver"));
	NetDriverDefinitionName = NAME_BeaconNetDriver;
	bRelevantForNetworkReplays = false;
}

bool AOnlineBeacon::InitBase()
{
	NetDriver = GEngine->CreateNetDriver(GetWorld(), NetDriverDefinitionName);
	if (NetDriver != nullptr)
	{
		HandleNetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &AOnlineBeacon::HandleNetworkFailure);
		SetNetDriverName(NetDriver->NetDriverName);
		return true;
	}

	return false;
}

void AOnlineBeacon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CleanupNetDriver();
	Super::EndPlay(EndPlayReason);
}

bool AOnlineBeacon::HasNetOwner() const
{
    // Beacons are their own net owners
	return true;
}

void AOnlineBeacon::DestroyBeacon()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Destroying beacon %s, netdriver %s"), *GetName(), NetDriver ? *NetDriver->GetDescription() : TEXT("NULL"));
	GEngine->OnNetworkFailure().Remove(HandleNetworkFailureDelegateHandle);

	CleanupNetDriver();
	Destroy();
}

void AOnlineBeacon::CleanupNetDriver()
{
	if (NetDriver)
	{
		if (NetDriver->IsInTick())
		{
			NetDriver->SetPendingDestruction(true);
		}
		else
		{
			GEngine->DestroyNamedNetDriver(GetWorld(), NetDriverName);
		}
		// If the net connection is currently in the middle of processing messages it is
		// possible for the notifier to be fired again if not cleared.
		NetDriver->Notify = nullptr;
		NetDriver = nullptr;
	}
}

void AOnlineBeacon::HandleNetworkFailure(UWorld *World, UNetDriver *InNetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (InNetDriver && InNetDriver->NetDriverName == NetDriverName)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("NetworkFailure %s: %s"), *GetName(), ENetworkFailure::ToString(FailureType));
		OnFailure();
	}
}

void AOnlineBeacon::OnFailure()
{
	GEngine->OnNetworkFailure().Remove(HandleNetworkFailureDelegateHandle);
	CleanupNetDriver();
}

void AOnlineBeacon::OnActorChannelOpen(FInBunch& Bunch, UNetConnection* Connection)
{
	Connection->OwningActor = this;
	Super::OnActorChannelOpen(Bunch, Connection);
}

bool AOnlineBeacon::IsRelevancyOwnerFor(const AActor* ReplicatedActor, const AActor* ActorOwner, const AActor* ConnectionActor) const
{
	bool bRelevantOwner = (ConnectionActor == ReplicatedActor);
	return bRelevantOwner;
}

bool AOnlineBeacon::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// Only replicate to the owner or to connections of the same beacon type (possible that multiple UNetConnections come from the same client)
	bool bIsOwner = GetNetConnection() == ViewTarget->GetNetConnection();
	bool bSameBeaconType = GetClass() == RealViewer->GetClass();
	return bOnlyRelevantToOwner ? bIsOwner : bSameBeaconType;
}

EAcceptConnection::Type AOnlineBeacon::NotifyAcceptingConnection()
{
	check(NetDriver);
	if(NetDriver->ServerConnection)
	{
		// We are a client and we don't welcome incoming connections.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Client refused"));
		return EAcceptConnection::Reject;
	}
	else if(BeaconState == EBeaconState::DenyRequests)
	{
		// Server is down
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s refused"), *GetName());
		return EAcceptConnection::Reject;
	}
	else //if(BeaconState == EBeaconState::AllowRequests)
	{
		// Server is up and running.
		UE_CLOG(!NetDriver->DDoS.CheckLogRestrictions(), LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s accept"), *GetName());
		return EAcceptConnection::Accept;
	}
}

void AOnlineBeacon::NotifyAcceptedConnection(UNetConnection* Connection)
{
	check(NetDriver != nullptr);
	check(NetDriver->ServerConnection == nullptr);
	UE_LOG(LogNet, Log, TEXT("NotifyAcceptedConnection: Name: %s, TimeStamp: %s, %s"), *GetName(), FPlatformTime::StrTimestamp(), *Connection->Describe());
}

bool AOnlineBeacon::NotifyAcceptingChannel(UChannel* Channel)
{
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;
	check(NetDriver == Driver);

	if (Driver->ServerConnection)
	{
		// We are a client and the server has just opened up a new channel.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel %i/%s client %s"), Channel->ChIndex, *Channel->ChName.ToString(), *GetName());
		if (Driver->ChannelDefinitionMap[Channel->ChName].bServerOpen)
		{
			UE_LOG(LogNet, Log, TEXT("Client accepting %s channel"), *Channel->ChName.ToString());
			return true;
		}
		else
		{
			// Unwanted channel type.
			UE_LOG(LogNet, Log, TEXT("Client refusing unwanted channel of type %s"), *Channel->ChName.ToString());
			return false;
		}
	}
	else
	{
		// We are the server.
		if (Driver->ChannelDefinitionMap[Channel->ChName].bClientOpen)
		{
			// The client has opened initial channel.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel Control %i server %s: Accepted"), Channel->ChIndex, *GetFullName());
			return true;
		}
		else
		{
			// Client can't open any other kinds of channels.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel %s %i server %s: Refused"), *Channel->ChName.ToString(), Channel->ChIndex, *GetFullName());
			return false;
		}
	}
}

void AOnlineBeacon::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch)
{
}

