// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationDataStream.h"
#include "Iris/ReplicationSystem/ReplicationReader.h"
#include "Iris/ReplicationSystem/ReplicationWriter.h"

UReplicationDataStream::UReplicationDataStream()
: ReplicationReader(nullptr)
, ReplicationWriter(nullptr)
{
}

UReplicationDataStream::~UReplicationDataStream()
{
}

UDataStream::EWriteResult UReplicationDataStream::BeginWrite(const FBeginWriteParameters& Params)
{
	if (ReplicationWriter != nullptr)
	{
		return ReplicationWriter->BeginWrite(Params);
	}

	return EWriteResult::NoData;
}

UDataStream::EWriteResult UReplicationDataStream::WriteData(UE::Net::FNetSerializationContext& Context, FDataStreamRecord const*& OutRecord)
{
	if (ReplicationWriter != nullptr)
	{
		return ReplicationWriter->Write(Context);
	}

	return EWriteResult::NoData;
}

void UReplicationDataStream::EndWrite()
{
	if (ReplicationWriter != nullptr)
	{
		return ReplicationWriter->EndWrite();
	}
}

void UReplicationDataStream::ReadData(UE::Net::FNetSerializationContext& Context)
{
	if (ReplicationReader != nullptr)
	{
		ReplicationReader->Read(Context);
	}
}

void UReplicationDataStream::ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus Status, FDataStreamRecord const* Record)
{
	if (ReplicationWriter != nullptr)
	{
		ReplicationWriter->ProcessDeliveryNotification(Status);
	}
}

void UReplicationDataStream::SetReaderAndWriter(UE::Net::Private::FReplicationReader* Reader, UE::Net::Private::FReplicationWriter* Writer)
{
	ReplicationReader = Reader;
	ReplicationWriter = Writer;
}
