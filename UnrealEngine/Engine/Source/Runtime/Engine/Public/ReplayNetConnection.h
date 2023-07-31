// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetConnection.h"
#include "ReplayHelper.h"
#include "ReplayNetConnection.generated.h"

UCLASS(transient, config=Engine)
class ENGINE_API UReplayNetConnection : public UNetConnection
{
	GENERATED_BODY()

public:
	UReplayNetConnection(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSEconds) override;

	// UNetConnection interface.
	virtual void InitConnection(UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed = 0, int32 InMaxPacket=0) override;
	virtual FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	virtual FString LowLevelDescribe() override;
	virtual void LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual int32 IsNetReady(bool Saturate) override;
	virtual void HandleClientPlayer(APlayerController* PC, UNetConnection* NetConnection) override;
	virtual TSharedPtr<const FInternetAddr> GetRemoteAddr() override;
	virtual bool ClientHasInitializedLevelFor(const AActor* TestActor) const override;
	virtual FString RemoteAddressToString() override { return TEXT("Replay"); }
	virtual void CleanUp() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void NotifyActorDestroyed(AActor* Actor, bool IsSeamlessTravel = false) override;
	virtual bool IsReplayReady() const override;

	virtual void NotifyActorChannelCleanedUp(UActorChannel* Channel, EChannelCloseReason CloseReason) override;

	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override {}
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override {}

	void StartRecording();

	void AddEvent(const FString& Group, const FString& Meta, const TArray<uint8>& Data);
	void AddOrUpdateEvent(const FString& EventName, const FString& Group, const FString& Meta, const TArray<uint8>& Data);

	void AddUserToReplay(const FString& UserString);

	FString GetActiveReplayName() const { return ReplayHelper.ActiveReplayName; }
	float GetReplayCurrentTime() const { return ReplayHelper.DemoCurrentTime; }

	bool IsSavingCheckpoint() const;

	void OnSeamlessTravelStart(UWorld* CurrentWorld, const FString& LevelName);

	void SetAnalyticsProvider(TSharedPtr<IAnalyticsProvider> InProvider);

	void SetCheckpointSaveMaxMSPerFrame(const float InCheckpointSaveMaxMSPerFrame);

	void RequestCheckpoint();

	bool SetExternalDataForObject(UObject* OwningObject, const uint8* Src, const int32 NumBits);

private:
	void TrackSendForProfiler(const void* Data, int32 NumBytes);

	FReplayHelper ReplayHelper;

	int32 DemoFrameNum;

	FDelegateHandle OnLevelRemovedFromWorldHandle;
	FDelegateHandle OnLevelAddedToWorldHandle;

	void OnLevelRemovedFromWorld(class ULevel* Level, class UWorld* World);
	void OnLevelAddedToWorld(class ULevel* Level, class UWorld* World);

	FName NetworkRemapPath(FName InPackageName, bool bReading);
};
