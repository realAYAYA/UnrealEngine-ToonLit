// Copyright Epic Games, Inc. All Rights Reserved.

// Channel for sending and receiving all Iris data

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Channel.h"
#include "Net/Core/Misc/ResizableCircularQueue.h"
#include "DataStreamChannel.generated.h"

class UDataStreamManager;

UCLASS(transient, customConstructor)
class ENGINE_API UDataStreamChannel final : public UChannel
{
	GENERATED_BODY()

	UDataStreamChannel(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	static void AddReferencedObjects(UObject* Object, FReferenceCollector& Collector);

	// UChannel interface
	virtual void Init(UNetConnection* InConnection, int32 InChIndex, EChannelCreateFlags CreateFlags) override;

	virtual bool CleanUp(const bool bForDestroy, EChannelCloseReason CloseReason) override;

	/**
	 * Processes the in bound bunch to extract the data streams
	 */
	virtual void ReceivedBunch(FInBunch& Bunch) override;

	virtual void Tick() override;

	/** Always ticks */
	virtual bool CanStopTicking() const override;

	/** Human readable information about the channel */
	virtual FString Describe() override;

	/** Packet delivery status handling */
	virtual void ReceivedAck(int32 PacketId) override;
	virtual void ReceivedNak(int32 PacketId) override;
	
private:
	enum : uint32
	{
		MaxPacketsInFlightCount = 256U,
		BitStreamBufferByteCount = 2048U,
	};

	struct FDataStreamChannelRecord
	{
		const void* Record = nullptr;
		uint32 PacketId = 0U;
	};

	bool IsPacketWindowFull() const;
	void DiscardAllRecords();

	void SendOpenBunch();

	TObjectPtr<UDataStreamManager> DataStreamManager = nullptr;

	TResizableCircularQueue<FDataStreamChannelRecord> WriteRecords;

	uint32 BitStreamBuffer[BitStreamBufferByteCount];

	uint32 bIsReadyToHandshake : 1U;
	uint32 bHandshakeSent : 1U;
	uint32 bHandshakeComplete : 1U;
};
