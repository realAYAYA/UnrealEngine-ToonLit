// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NetExports.h"
#include "Iris/PacketControl/PacketNotification.h"

namespace UE::Net::Private
{

void FNetExports::InitExportRecordForPacket()
{
	CurrentExportInfo.NetHandleExportCount = NetHandleExports.Count();
	CurrentExportInfo.NetTokenExportCount = NetTokenExports.Count();
}

void FNetExports::CommitExportsToRecord(FExportScope& ExportScope)
{
	const FNetExportContext::FBatchExports& BatchExports = ExportScope.ExportContext.GetBatchExports();
	for (FNetHandle Handle : BatchExports.HandlesExportedInCurrentBatch)
	{
		NetHandleExports.Enqueue(Handle);
	}
	for (FNetToken Token : BatchExports.NetTokensExportedInCurrentBatch)
	{
		NetTokenExports.Enqueue(Token);
	}
}

void FNetExports::PushExportRecordForPacket()
{
	FExportInfo Info;
	Info.NetHandleExportCount = NetHandleExports.Count() - CurrentExportInfo.NetHandleExportCount;
	Info.NetTokenExportCount = NetTokenExports.Count() - CurrentExportInfo.NetTokenExportCount;
	ExportRecord.Enqueue(Info);
}

void FNetExports::AcknowledgeBatchExports(FNetExportContext::FBatchExports& BatchExports)
{
	AcknowledgedExports.AcknowledgedExportedHandles.Append(BatchExports.HandlesExportedInCurrentBatch);
	AcknowledgedExports.AcknowledgedExportedNetTokens.Append(BatchExports.NetTokensExportedInCurrentBatch);
}

FNetExports::FExportInfo FNetExports::PopExportRecord()
{
	check(ExportRecord.Count());

	const FExportInfo ExportInfo = ExportRecord.Peek();
	ExportRecord.Pop();

	return ExportInfo;
}

void FNetExports::ProcessPacketDeliveryStatus(UE::Net::EPacketDeliveryStatus Status)
{
	FExportInfo ExportInfo = PopExportRecord();
	if (Status == UE::Net::EPacketDeliveryStatus::Delivered)
	{
		uint32 NetHandleExportCount = ExportInfo.NetHandleExportCount;
		while (NetHandleExportCount)
		{
			FNetHandle Handle = NetHandleExports.PeekNoCheck();
			AcknowledgedExports.AcknowledgedExportedHandles.Add(Handle);
			NetHandleExports.PopNoCheck();
			--NetHandleExportCount;
		}
		uint32 NetTokenExportCount = ExportInfo.NetTokenExportCount;
		while (NetTokenExportCount)
		{
			FNetToken Token = NetTokenExports.PeekNoCheck();
			AcknowledgedExports.AcknowledgedExportedNetTokens.Add(Token);
			NetTokenExports.PopNoCheck();
			--NetTokenExportCount;
		}
	}
	else
	{
		NetHandleExports.PopNoCheck(ExportInfo.NetHandleExportCount);
		NetTokenExports.PopNoCheck(ExportInfo.NetTokenExportCount);
	}
}

}
