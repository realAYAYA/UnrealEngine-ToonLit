// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RigVMModule.h: Module definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "RigVMDefines.h"

RIGVM_API DECLARE_LOG_CATEGORY_EXTERN(LogRigVM, Log, All);

namespace RigVMCore
{
	RIGVM_API bool SupportsUObjects();
	RIGVM_API bool SupportsUInterfaces();
}
