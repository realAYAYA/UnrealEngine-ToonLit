// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetBlob/NetRPCHandler.h"
#include "Iris/ReplicationSystem/NetBlob/NetRPC.h"

UNetRPCHandler::UNetRPCHandler()
: ReplicationSystem(nullptr)
{
}

UNetRPCHandler::~UNetRPCHandler()
{
}

void UNetRPCHandler::Init(UReplicationSystem& InReplicationSystem)
{
	ReplicationSystem = &InReplicationSystem;
}

TRefCountPtr<UE::Net::Private::FNetRPC> UNetRPCHandler::CreateRPC(const UE::Net::FNetObjectReference& ObjectReference, const UFunction* Function, const void* Parameters) const
{
	FNetBlobCreationInfo CreationInfo;
	CreationInfo.Type = GetNetBlobType();
	CreationInfo.Flags = ((Function->FunctionFlags & FUNC_NetReliable) != 0) ? UE::Net::ENetBlobFlags::Reliable : UE::Net::ENetBlobFlags::None;
	FNetRPC* RPC = FNetRPC::Create(ReplicationSystem, CreationInfo, ObjectReference, Function, Parameters);
	return RPC;
}

TRefCountPtr<UE::Net::FNetBlob> UNetRPCHandler::CreateNetBlob(const FNetBlobCreationInfo& CreationInfo) const
{
	FNetRPC* RPC = new FNetRPC(CreationInfo);
	return RPC;
}

void UNetRPCHandler::OnNetBlobReceived(UE::Net::FNetSerializationContext& Context, const TRefCountPtr<FNetBlob>& NetBlob)
{
	FNetRPC* RPC = static_cast<FNetRPC*>(NetBlob.GetReference());
	RPC->CallFunction(Context);
}
