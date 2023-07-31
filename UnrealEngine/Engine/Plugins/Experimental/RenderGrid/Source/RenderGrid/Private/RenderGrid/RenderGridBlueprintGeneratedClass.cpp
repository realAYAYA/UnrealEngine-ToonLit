// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGrid/RenderGridBlueprintGeneratedClass.h"


uint8* URenderGridBlueprintGeneratedClass::GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const
{
	if (!IsInGameThread())
	{
		// we cant use the persistent frame if we are executing in parallel (as we could potentially thunk to BP)
		return nullptr;
	}
	return Super::GetPersistentUberGraphFrame(Obj, FuncToCheck);
}
