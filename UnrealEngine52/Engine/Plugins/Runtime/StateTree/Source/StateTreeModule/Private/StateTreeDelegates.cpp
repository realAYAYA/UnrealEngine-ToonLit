// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeDelegates.h"

namespace UE::StateTree::Delegates
{
	
#if WITH_EDITOR
FOnIdentifierChanged OnIdentifierChanged;
FOnSchemaChanged OnSchemaChanged;
FOnParametersChanged OnParametersChanged;
FOnStateParametersChanged OnStateParametersChanged;
FOnPostCompile OnPostCompile;
FOnRequestCompile OnRequestCompile;
#endif
	
}; // UE::StateTree::Delegates
