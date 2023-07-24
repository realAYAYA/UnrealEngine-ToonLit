// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Iris/ReplicationSystem/NetBlob/NetRPC.h"
#include "Iris/ReplicationSystem/NetBlob/NetBlobHandler.h"
#include "NetRPCHandler.generated.h"

UCLASS(transient, MinimalAPI)
class UNetRPCHandler final : public UNetBlobHandler
{
	GENERATED_BODY()

	using FNetRPC = UE::Net::Private::FNetRPC;

public:
	UNetRPCHandler();
	virtual ~UNetRPCHandler();

	void Init(UReplicationSystem& ReplicationSystem);

	TRefCountPtr<UE::Net::Private::FNetRPC> CreateRPC(const UE::Net::FNetObjectReference& ObjectReference, const UFunction* Function, const void* Parameters) const;

private:
	virtual TRefCountPtr<FNetBlob> CreateNetBlob(const FNetBlobCreationInfo&) const override;
	virtual void OnNetBlobReceived(UE::Net::FNetSerializationContext& Context, const TRefCountPtr<FNetBlob>& NetBlob) override;

	UReplicationSystem* ReplicationSystem;
};
