// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScriptDelegateFwd.h: Delegate forward declarations
=============================================================================*/

#pragma once

#include "UObject/WeakObjectPtrFwd.h"

struct FNotThreadSafeDelegateMode;
struct FThreadSafeDelegateMode;

template <typename ThreadSafetyMode = FNotThreadSafeDelegateMode> class TScriptDelegate;
template <typename ThreadSafetyMode = FNotThreadSafeDelegateMode> class TMulticastScriptDelegate;

// Typedef script delegates for convenience.
typedef TScriptDelegate<> FScriptDelegate;
typedef TMulticastScriptDelegate<> FMulticastScriptDelegate;

typedef TScriptDelegate<FThreadSafeDelegateMode> FTSScriptDelegate;
typedef TMulticastScriptDelegate<FThreadSafeDelegateMode> FTSMulticastScriptDelegate;
