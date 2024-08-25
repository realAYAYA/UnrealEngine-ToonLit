// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMMarkStack.h"
#include "VerseVM/VVMLog.h"

namespace Verse
{

FMarkStack::~FMarkStack()
{
	V_DIE_UNLESS(Stack.IsEmpty());
}

template <std::memory_order MemoryOrder>
void FMarkStack::MarkSlowImpl(const VCell* Cell)
{
	std::atomic<uint32>* Word = FHeap::GetMarkBitWord(Cell);
	const uint32 Mask = FHeap::GetMarkBitMask(Cell);
	if (!(Word->fetch_or(Mask, MemoryOrder) & Mask))
	{
		Stack.Push(const_cast<VCell*>(Cell));
	}
}

void FMarkStack::MarkSlow(const VCell* Cell)
{
	MarkSlowImpl<std::memory_order_relaxed>(Cell);
}

void FMarkStack::FencedMarkSlow(const VCell* Cell)
{
	MarkSlowImpl<std::memory_order_seq_cst>(Cell);
}

template <std::memory_order MemoryOrder>
void FMarkStack::MarkAuxSlowImpl(const void* Aux)
{
	std::atomic<uint32>* Word = FHeap::GetMarkBitWord(Aux);
	const uint32 Mask = FHeap::GetMarkBitMask(Aux);
	Word->fetch_or(Mask, MemoryOrder);
}

void FMarkStack::MarkAuxSlow(const void* Aux)
{
	MarkAuxSlowImpl<std::memory_order_relaxed>(Aux);
}

void FMarkStack::FencedMarkAuxSlow(const void* Aux)
{
	MarkAuxSlowImpl<std::memory_order_seq_cst>(Aux);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)