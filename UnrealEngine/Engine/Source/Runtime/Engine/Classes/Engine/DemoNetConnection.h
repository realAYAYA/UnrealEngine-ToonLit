// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetConnection.h"
#include "ReplayTypes.h"
#include "DemoNetConnection.generated.h"

class APlayerController;
class FObjectReplicator;
class UDemoNetDriver;

/**
 * Simulated network connection for recording and playing back game sessions.
 */
UCLASS(transient, config=Engine)
class ENGINE_API UDemoNetConnection : public UNetConnection
{
	GENERATED_BODY()

public:
	UDemoNetConnection(const FObjectInitializer& ObjectInitializer);

	// UNetConnection interface.

	virtual void InitConnection( class UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed = 0, int32 InMaxPacket=0) override;
	virtual FString LowLevelGetRemoteAddress( bool bAppendPort = false ) override;
	virtual FString LowLevelDescribe() override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual int32 IsNetReady( bool Saturate ) override;
	virtual void FlushNet( bool bIgnoreSimulation = false ) override;
	virtual void HandleClientPlayer( APlayerController* PC, class UNetConnection* NetConnection ) override;
	virtual TSharedPtr<const FInternetAddr> GetRemoteAddr() override;
	virtual bool ClientHasInitializedLevelFor( const AActor* TestActor ) const override;
	virtual TSharedPtr<FObjectReplicator> CreateReplicatorForNewActorChannel(UObject* Object);
	virtual FString RemoteAddressToString() override { return TEXT("Demo"); }

	virtual void NotifyActorNetGUID(UActorChannel* Channel) override;
	virtual void NotifyActorChannelCleanedUp(UActorChannel* Channel, EChannelCloseReason CloseReason) override;

	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override {}
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override {}

public:

	virtual void Serialize(FArchive& Ar) override;

	/** @return The DemoRecording driver object */
	FORCEINLINE class UDemoNetDriver* GetDriver()
	{
		return (UDemoNetDriver*)Driver;
	}

	/** @return The DemoRecording driver object */
	FORCEINLINE const class UDemoNetDriver* GetDriver() const
	{
		return (UDemoNetDriver*)Driver;
	}

	TMap<FNetworkGUID, UActorChannel*>& GetOpenChannelMap() { return OpenChannelMap; }

protected:
	virtual void DestroyIgnoredActor(AActor* Actor) override;

	void QueueNetStartupActorForRewind(AActor* Actor);

private:
	void TrackSendForProfiler(const void* Data, int32 NumBytes);

	TMap<FNetworkGUID, UActorChannel*> OpenChannelMap;
};
