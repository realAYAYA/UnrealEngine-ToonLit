// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * This module provides managers to dynamically configure and create state graphs for various use cases.
 * This includes a set of common managers for engine-level classes, but plugins and game code can define
 * their own by creating subsystems that inherit from FStateGraphManager below.
 */

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "StateGraphFwd.h"
#include "UObject/NameTypes.h"

namespace UE
{

DECLARE_DELEGATE_RetVal_OneParam(bool, FStateGraphManagerCreateDelegate, UE::FStateGraph& /*StateGraph*/);

class STATEGRAPHMANAGER_API FStateGraphManager
{
public:
	virtual ~FStateGraphManager();
	virtual FName GetStateGraphName() const = 0;
	virtual void AddCreateDelegate(const FStateGraphManagerCreateDelegate& Delegate);
	virtual UE::FStateGraphPtr Create(const FString& ContextName = FString());

private:
	TArray<FStateGraphManagerCreateDelegate> CreateDelegates;
};

} // UE
