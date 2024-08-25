// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertPropertyChainWrapper.h"

uint32 GetTypeHash(const FConcertPropertyChainWrapper& ChainWrapper)
{
	return GetTypeHash(ChainWrapper.PropertyChain);
}