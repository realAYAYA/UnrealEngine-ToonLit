// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/NetToken.h"

namespace UE::Net
{

class FNetTokenStoreState
{
public:
	// The size of the array is managed by FNetTokenDataStream
	FNetTokenStoreState();

	// Reserve space for tokens
	bool ReserveTokenCount(uint32 NewCount);

public:

	// This basically maps from token to tokenkey
	TArray<FNetTokenStoreKey> TokenInfos;
};

}
