// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Kept as iris only until we we know if we even need this delegate. 
// We are considering to move Iris into NetCore when we leave the experimental stage at that point we can call directly into iris code
// rather than going though a delegate.

#if UE_WITH_IRIS

#include "Delegates/Delegate.h"

class FRepChangedPropertyTracker;

namespace UE::Net::Private
{

class NETCORE_API FPropertyConditionDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPropertyCustomConditionChanged, const UObject*, const uint16, const bool);
	
	static FOnPropertyCustomConditionChanged& GetOnPropertyCustomConditionChangedDelegate() { return OnPropertyCustomConditionChangedDelegate; }

private:
	static FOnPropertyCustomConditionChanged OnPropertyCustomConditionChangedDelegate;
};

}; // UE::Net::Private

#endif