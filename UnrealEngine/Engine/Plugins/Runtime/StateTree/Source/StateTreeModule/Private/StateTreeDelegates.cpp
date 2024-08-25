// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeDelegates.h"

namespace UE::StateTree::Delegates
{
	
#if WITH_EDITOR
FOnIdentifierChanged OnIdentifierChanged;
FOnSchemaChanged OnSchemaChanged;
FOnParametersChanged OnParametersChanged;
FOnGlobalDataChanged OnGlobalDataChanged;
FOnStateParametersChanged OnStateParametersChanged;
FOnBreakpointsChanged OnBreakpointsChanged;
FOnPostCompile OnPostCompile;
FOnRequestCompile OnRequestCompile;
#endif // WITH_EDITOR

#if WITH_STATETREE_DEBUGGER
FOnTracingStateChanged OnTracingStateChanged;
#endif // WITH_STATETREE_DEBUGGER

}; // UE::StateTree::Delegates
