// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetTokenStoreState.h"

namespace UE::Net
{

FNetTokenStoreState::FNetTokenStoreState()
{
	// We reserve the first token as an invalid token
	TokenInfos.Add(FNetTokenStoreKey());
}

bool FNetTokenStoreState::ReserveTokenCount(uint32 NewCount)
{
	if (NewCount <= FNetToken::MaxNetTokenCount)
	{
		const uint32 NewSize = FMath::Max<uint32>(TokenInfos.Num(), NewCount);
		TokenInfos.SetNumZeroed(NewSize);

		return true;
	}
	else
	{
		return false;
	}
}

}
