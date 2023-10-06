// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeDelegates.h"

namespace UE::StateTree::Delegates
{
	
#if WITH_EDITOR
FOnIdentifierChanged OnIdentifierChanged;
FOnSchemaChanged OnSchemaChanged;
FOnParametersChanged OnParametersChanged;
FOnStateParametersChanged OnStateParametersChanged;
FOnBreakpointsChanged OnBreakpointsChanged;
FOnPostCompile OnPostCompile;
FOnRequestCompile OnRequestCompile;
#endif
	
}; // UE::StateTree::Delegates
