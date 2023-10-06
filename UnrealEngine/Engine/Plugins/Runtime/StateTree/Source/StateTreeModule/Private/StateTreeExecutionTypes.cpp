// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeExecutionTypes.h"

const FStateTreeExternalDataHandle FStateTreeExternalDataHandle::Invalid = FStateTreeExternalDataHandle();

#if WITH_STATETREE_DEBUGGER
const FStateTreeInstanceDebugId FStateTreeInstanceDebugId::Invalid = FStateTreeInstanceDebugId();
#endif // WITH_STATETREE_DEBUGGER