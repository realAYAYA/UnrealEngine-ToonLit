// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/Packet/DisplayClusterPacketInternal.h"
#include "Network/Transport/DisplayClusterSocketOperations.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include "Misc/DisplayClusterLog.h"


bool FDisplayClusterPacketInternal::Serialize(FMemoryWriter& Arch)
{
	// Header
	Arch << Name;
	Arch << Type;
	Arch << Protocol;

	// Comm result
	Arch << CommResult;

	// Arguments
	Arch << TextArguments;
	Arch << BinaryArguments;

	// Objects
	Arch << TextObjects;
	Arch << BinaryObjects;

	return true;
}

bool FDisplayClusterPacketInternal::Deserialize(FMemoryReader& Arch)
{
	// Header
	Arch << Name;
	Arch << Type;
	Arch << Protocol;

	// Comm result
	Arch << CommResult;

	// Arguments
	Arch << TextArguments;
	Arch << BinaryArguments;

	// Objects
	Arch << TextObjects;
	Arch << BinaryObjects;

	return true;
}

FString FDisplayClusterPacketInternal::ToString() const
{
	return FString::Printf(TEXT("<Protocol=%s, Type=%s, Name=%s, CommErr=%d Args={%s} Text_Objects=%d Bin_Objects=%d>"),
		*GetProtocol(), *GetType(), *GetName(), CommResult, *ArgsToString(), TextObjects.Num(), BinaryObjects.Num());
}

FString FDisplayClusterPacketInternal::ArgsToString() const
{
	FString TmpStr;
	TmpStr.Reserve(512);
	
	for (const auto& Category : TextArguments)
	{
		TmpStr += FString::Printf(TEXT("%s=["), *Category.Key);

		for (const auto& Argument : Category.Value)
		{
			TmpStr += FString::Printf(TEXT("%s=%s "), *Argument.Key, *Argument.Value);
		}

		TmpStr += FString("] ");
	}

	return TmpStr;
}

bool FDisplayClusterPacketInternal::SendPacket(FDisplayClusterSocketOperations& SocketOps)
{
	FScopeLock Lock(&SocketOps.GetSyncObj());

	if (!SocketOps.IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s not connected"), *SocketOps.GetConnectionName());
		return false;
	}

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - sending internal packet..."), *SocketOps.GetConnectionName());

	TArray<uint8> DataBuffer;
	FMemoryWriter MemoryWriter(DataBuffer);

	// Reserve space for packet header
	MemoryWriter.Seek(sizeof(FPacketHeader));

	// Serialize the packet body
	if (!Serialize(MemoryWriter))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - couldn't serialize a packet"), *SocketOps.GetConnectionName());
		return false;
	}

	// Initialize the packet header
	FPacketHeader PacketHeader;
	PacketHeader.PacketBodyLength = static_cast<uint32>(DataBuffer.Num() - sizeof(FPacketHeader));
	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - Outgoing packet header: %s"), *SocketOps.GetConnectionName(), *PacketHeader.ToString());

	// Fill packet header with packet data length
	FMemory::Memcpy(DataBuffer.GetData(), &PacketHeader, sizeof(FPacketHeader));

	// Send the header
	if (!SocketOps.SendChunk(DataBuffer, DataBuffer.Num(), FString("send-internal-msg")))
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s - Couldn't send a packet"), *SocketOps.GetConnectionName());
		return false;
	}

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - Packet sent"), *SocketOps.GetConnectionName());

	return true;
}

bool FDisplayClusterPacketInternal::RecvPacket(FDisplayClusterSocketOperations& SocketOps)
{
	FScopeLock Lock(&SocketOps.GetSyncObj());

	if (!SocketOps.IsOpen())
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s - not connected"), *SocketOps.GetConnectionName());
		return false;
	}

	// We'll be working with internal buffer to save some time on memcpy operation
	TArray<uint8>& DataBuffer = SocketOps.GetPersistentBuffer();

	// Read packet header
	DataBuffer.Reset();
	if (!SocketOps.RecvChunk(DataBuffer, sizeof(FPacketHeader), FString("recv-internal-chunk-header")))
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s couldn't receive packet header"), *SocketOps.GetConnectionName());
		return false;
	}

	// Ok. Now we can extract header data
	FPacketHeader PacketHeader;
	FMemory::Memcpy(&PacketHeader, DataBuffer.GetData(), sizeof(FPacketHeader));
	DataBuffer.Reset();

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - packet header received: %s"), *SocketOps.GetConnectionName(), *PacketHeader.ToString());
	check(PacketHeader.PacketBodyLength > 0);

	// Read packet body
	if (!SocketOps.RecvChunk(DataBuffer, PacketHeader.PacketBodyLength, FString("recv-internal-chunk-body")))
	{
		UE_LOG(LogDisplayClusterNetwork, Warning, TEXT("%s couldn't receive packet body"), *SocketOps.GetConnectionName());
		return false;
	}

	// We need to set a correct value for array size before deserialization
	DataBuffer.SetNumUninitialized(PacketHeader.PacketBodyLength, false);

	UE_LOG(LogDisplayClusterNetwork, VeryVerbose, TEXT("%s - packet body received"), *SocketOps.GetConnectionName());

	// Deserialize packet from buffer
	FMemoryReader Arch = FMemoryReader(DataBuffer, false);
	if (!Deserialize(Arch))
	{
		UE_LOG(LogDisplayClusterNetwork, Error, TEXT("%s couldn't deserialize a packet"), *SocketOps.GetConnectionName());
		return false;
	}

	// Succeeded
	return true;
}

FString FDisplayClusterPacketInternal::ToLogString(bool bDetailed) const
{
	return bDetailed ? ToString() : FString::Printf(TEXT("<Protocol=%s, Type=%s, Name=%s, CommErr=%d>"), *Protocol, *Type, *Name, CommResult);
}
