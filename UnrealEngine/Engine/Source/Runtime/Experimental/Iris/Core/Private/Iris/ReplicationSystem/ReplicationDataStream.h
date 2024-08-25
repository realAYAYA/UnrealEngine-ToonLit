// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/DataStream/DataStream.h"
#include "ReplicationDataStream.generated.h"

namespace UE::Net::Private
{
	class FReplicationReader;
	class FReplicationWriter;
}

UCLASS()
class UReplicationDataStream final : public UDataStream
{
	GENERATED_BODY()

public:
	void SetReaderAndWriter(UE::Net::Private::FReplicationReader* Reader, UE::Net::Private::FReplicationWriter* Writer);

private:
	UReplicationDataStream();
	virtual ~UReplicationDataStream();

	// UDataStream interface
	virtual EWriteResult BeginWrite(const FBeginWriteParameters& Params) override;
	virtual EWriteResult WriteData(UE::Net::FNetSerializationContext& Context, FDataStreamRecord const*& OutRecord) override;
	virtual void EndWrite() override;
	virtual void ReadData(UE::Net::FNetSerializationContext& Context) override;
	virtual void ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus Status, FDataStreamRecord const* Record) override;

private:
	UE::Net::Private::FReplicationReader* ReplicationReader;
	UE::Net::Private::FReplicationWriter* ReplicationWriter;
};
