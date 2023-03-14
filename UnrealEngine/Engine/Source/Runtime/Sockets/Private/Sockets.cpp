// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sockets.h"
#include "SocketSubsystem.h"
//#include "Net/NetworkProfiler.h"


/**
 * FRecvMulti
 */

FRecvMulti::FRecvMulti(ISocketSubsystem* SocketSubsystem, int32 InMaxNumPackets, int32 InMaxPacketSize, ERecvMultiFlags InitFlags)
	: Packets(MakeUnique<FRecvData[]>(InMaxNumPackets))
	, NumPackets(0)
	, MaxNumPackets(InMaxNumPackets)
	, MaxPacketSize(InMaxPacketSize)
{
}

void FRecvMulti::CountBytes(FArchive& Ar) const
{
	Ar.CountBytes(sizeof(*this), sizeof(*this));

	// Packets
	Ar.CountBytes(MaxNumPackets * sizeof(FRecvData), MaxNumPackets * sizeof(FRecvData));
}

//
// FSocket stats implementation
//

bool FSocket::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
//	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(this,Data,BytesSent,Destination));
	UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' SendTo %i Bytes"), *SocketDescription, BytesSent );
	return true;
}


bool FSocket::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
//	NETWORK_PROFILER(GNetworkProfiler.TrackSocketSend(this,Data,BytesSent));
	UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' Send %i Bytes"), *SocketDescription, BytesSent );
	return true;
}


bool FSocket::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	if( BytesRead > 0 )
	{
		UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' RecvFrom %i Bytes"), *SocketDescription, BytesRead );
	}
	return true;
}


bool FSocket::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	if( BytesRead > 0 )
	{
		UE_LOG(LogSockets, Verbose, TEXT("Socket '%s' Recv %i Bytes"), *SocketDescription, BytesRead );
	}
	return true;
}

bool FSocket::RecvMulti(FRecvMulti& MultiData, ESocketReceiveFlags::Type Flags/*=ESocketReceiveFlags::None*/)
{
	return false;
}

bool FSocket::SetRetrieveTimestamp(bool bRetrieveTimestamp/*=true*/)
{
	return false;
}

bool FSocket::SetIpPktInfo(bool bEnable)
{
	return false;
}

bool FSocket::RecvFromWithPktInfo(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, FInternetAddr& Destination, ESocketReceiveFlags::Type Flags)
{
	return false;
}
