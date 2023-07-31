// Copyright Epic Games, Inc. All Rights Reserved.

#include "NullDrv.h"
#include "Modules/ModuleManager.h"


bool FNullDynamicRHIModule::IsSupported()
{
	return true;
}


IMPLEMENT_MODULE(FNullDynamicRHIModule, NullDrv);
