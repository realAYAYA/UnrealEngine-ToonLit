// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMMemoryCommon.h"
#include "RigVMModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMMemoryCommon)

#if DEBUG_RIGVMMEMORY
	DEFINE_LOG_CATEGORY(LogRigVMMemory);
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FRigVMOperand::Serialize(FArchive& Ar)
{
	Ar << MemoryType;
	Ar << RegisterIndex;
	Ar << RegisterOffset;
}

void FRigVMMemoryStorageImportErrorContext::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	if (bLogErrors)
	{
#if WITH_EDITOR
		UE_LOG(LogRigVM, Display, TEXT("Skipping Importing To MemoryStorage: %s"), V);
#else
		UE_LOG(LogRigVM, Error, TEXT("Error Importing To MemoryStorage: %s"), V);
#endif
	}
	NumErrors++;
}
