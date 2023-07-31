// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"

namespace UE::Net
{

bool FNetSerializationContext::IsBitStreamOverflown() const
{
	if (BitStreamReader != nullptr && BitStreamReader->IsOverflown())
	{
		return true;
	}

	if (BitStreamWriter != nullptr && BitStreamWriter->IsOverflown())
	{
		return true;
	}

	return false;
}

void FNetSerializationContext::SetBitStreamOverflow()
{
	if (BitStreamReader != nullptr && !BitStreamReader->IsOverflown())
	{
		BitStreamReader->DoOverflow();
	}

	if (BitStreamWriter != nullptr && !BitStreamWriter->IsOverflown())
	{
		BitStreamWriter->DoOverflow();
	}
}

UObject* FNetSerializationContext::GetLocalConnectionUserData(uint32 ConnectionId)
{
	if (InternalContext == nullptr)
	{
		return nullptr;
	}

	const UReplicationSystem* ReplicationSystem = InternalContext->ReplicationSystem;
	if (ReplicationSystem == nullptr)
	{
		return nullptr;
	}

	UObject* UserData = ReplicationSystem->GetConnectionUserData(ConnectionId);
	return UserData;
}

}
