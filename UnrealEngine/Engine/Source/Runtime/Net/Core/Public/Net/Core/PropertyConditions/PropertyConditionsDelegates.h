// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Kept as iris only until we we know if we even need this delegate. 
// We are considering to move Iris into NetCore when we leave the experimental stage at that point we can call directly into iris code
// rather than going though a delegate.

#if UE_WITH_IRIS

#include "Delegates/Delegate.h"

enum ELifetimeCondition : int;

namespace UE::Net::Private
{

class FPropertyConditionDelegates
{
public:
	using FOnPropertyCustomConditionChanged = TMulticastDelegate<void(const UObject* OwningObject, const uint16 RepIndex, const bool bActive)>;
	using FOnPropertyDynamicConditionChanged = TMulticastDelegate<void(const UObject* OwningObject, const uint16 RepIndex, const ELifetimeCondition Condition)>;
	
	static FOnPropertyCustomConditionChanged& GetOnPropertyCustomConditionChangedDelegate() { return OnPropertyCustomConditionChangedDelegate; }
	static FOnPropertyDynamicConditionChanged& GetOnPropertyDynamicConditionChangedDelegate() { return OnPropertyDynamicConditionChangedDelegate; }

private:
	static NETCORE_API FOnPropertyCustomConditionChanged OnPropertyCustomConditionChangedDelegate;
	static NETCORE_API FOnPropertyDynamicConditionChanged OnPropertyDynamicConditionChangedDelegate;
};

} // UE::Net::Private

#endif
