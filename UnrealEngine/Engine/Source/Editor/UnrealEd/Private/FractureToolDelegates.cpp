// Copyright Epic Games, Inc. All Rights Reserved.

#include "FractureToolDelegates.h"

/** Return a single FFractureToolDelegates object */
FFractureToolDelegates& FFractureToolDelegates::Get()
{
	// return the singleton object
	static FFractureToolDelegates Singleton;
	return Singleton;
}

