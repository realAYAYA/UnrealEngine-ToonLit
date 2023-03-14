// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameDelegates.h"

FGameDelegates& FGameDelegates::Get()
{
	// return the singleton object
	static FGameDelegates Singleton;
	return Singleton;
}
