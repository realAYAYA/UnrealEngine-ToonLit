// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RigVMModule.h: Module definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "RigVMDefines.h"

RIGVM_API DECLARE_LOG_CATEGORY_EXTERN(LogRigVM, Log, All);

/**
* The public interface to this module
*/
class RIGVM_API FRigVMModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

namespace RigVMCore
{
	RIGVM_API bool SupportsUObjects();
	RIGVM_API bool SupportsUInterfaces();
}
