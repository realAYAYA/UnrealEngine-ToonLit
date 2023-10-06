// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Context.h"
#include "TransactionInlines.h"

namespace AutoRTFM
{

UE_AUTORTFM_FORCEINLINE void FContext::CheckOpenRecordWrite(void* LogicalAddress)
{
    // We don't record any writes to unscoped transactions when the write-address
    // is within the nearest closed C++ function call nest. This may seem weird, but we
    // have to prohibit this because we can't track all the comings and goings of
    // local variables and all child C++ scopes. It's easy to get into a situation
    // where a rollback will undo the recorded writes to stack-locals that actually
    // corrupts the undo process itself.
    ASSERT(CurrentTransaction->IsScopedTransaction() || !IsInnerTransactionStack(LogicalAddress));
}

UE_AUTORTFM_FORCEINLINE void FContext::RecordWrite(void* LogicalAddress, size_t Size)
{
    CurrentTransaction->RecordWrite(LogicalAddress, Size);
}

UE_AUTORTFM_FORCEINLINE void FContext::DidAllocate(void* LogicalAddress, size_t Size)
{
    CurrentTransaction->DidAllocate(LogicalAddress, Size);
}

} // namespace AutoRTFM
