// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMMemoryCommon.h"

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

