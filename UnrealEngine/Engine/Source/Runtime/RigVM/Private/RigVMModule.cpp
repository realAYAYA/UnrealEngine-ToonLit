// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RigVM.h: Module implementation.
=============================================================================*/

#include "RigVMModule.h"
#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, RigVM);

DEFINE_LOG_CATEGORY(LogRigVM);

#if UE_RIGVM_UOBJECT_PROPERTIES_ENABLED
TAutoConsoleVariable<bool> CVarRigVMEnableUObjects(TEXT("RigVM.UObjectSupport"), true, TEXT("When true the RigVMCompiler will allow UObjects."));
#endif

#if UE_RIGVM_UINTERFACE_PROPERTIES_ENABLED
TAutoConsoleVariable<bool> CVarRigVMEnableUInterfaces(TEXT("RigVM.UInterfaceSupport"), true, TEXT("When true the RigVMCompiler will allow UInterfaces."));
#endif

bool RigVMCore::SupportsUObjects()
{
#if UE_RIGVM_UOBJECT_PROPERTIES_ENABLED
	return CVarRigVMEnableUObjects.GetValueOnGameThread();
#else
	return false;
#endif
}

bool RigVMCore::SupportsUInterfaces()
{
#if UE_RIGVM_UINTERFACE_PROPERTIES_ENABLED
	return CVarRigVMEnableUInterfaces.GetValueOnGameThread();
#else
	return false;
#endif
}