// Copyright Epic Games, Inc. All Rights Reserved.


#include "VirtualScoutingBlueprintFunctionLibrary.h"

#include "Editor.h"
#include "ScopedTransaction.h"

bool UVirtualScoutingBlueprintFunctionLibrary::CheckIsWithEditor()
{
#if WITH_EDITOR
	return true;
#else
	return false;
#endif
}
