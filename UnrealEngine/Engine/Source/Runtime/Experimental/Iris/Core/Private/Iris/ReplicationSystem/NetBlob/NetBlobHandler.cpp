// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetBlob/NetBlobHandler.h"

namespace UE::Net
{

const FName GNetError_UnsupportedNetBlob("Unsupported NetBlob type");

}

UNetBlobHandler::UNetBlobHandler()
: NetBlobType(UE::Net::InvalidNetBlobType)
{
}

UNetBlobHandler::~UNetBlobHandler()
{
}

void UNetBlobHandler::AddConnection(uint32 ConnectionId)
{
}

void UNetBlobHandler::RemoveConnection(uint32 ConnectionId)
{
}
