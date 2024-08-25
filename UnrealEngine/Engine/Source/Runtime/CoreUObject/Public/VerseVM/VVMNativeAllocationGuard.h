// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMHeap.h"

namespace Verse
{

template <typename THolder>
struct TNativeAllocationGuard
{
	TNativeAllocationGuard(THolder* InHolder)
		: Holder(InHolder)
		, PreviousAllocatedSize(Holder->GetAllocatedSize())
	{
	}

	~TNativeAllocationGuard()
	{
		FHeap::ReportAllocatedNativeBytes(Holder->GetAllocatedSize() - PreviousAllocatedSize);
	}

private:
	THolder* Holder;
	size_t PreviousAllocatedSize;
};

} // namespace Verse
#endif // WITH_VERSE_VM
