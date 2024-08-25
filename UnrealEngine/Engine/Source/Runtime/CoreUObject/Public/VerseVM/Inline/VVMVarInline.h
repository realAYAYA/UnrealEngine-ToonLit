// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMTransaction.h"
#include "VerseVM/VVMVar.h"
#include "VerseVM/VVMWriteBarrier.h"

namespace Verse
{
template <typename T>
template <typename TResult>
auto TWriteBarrier<T>::SetTransactionally(FAccessContext Context, VCell& Owner, TValue NewValue) -> std::enable_if_t<bIsVValue, TResult>
{
	RunBarrier(Context, NewValue);
	Context.CurrentTransaction()->LogBeforeWrite(Context, Owner, *this);
	Value = NewValue;
}

void VRestValue::SetTransactionally(FAccessContext Context, VCell& Owner, VValue NewValue)
{
	checkSlow(!NewValue.IsRoot());
	Value.SetTransactionally(Context, Owner, NewValue);
}

void VVar::Set(FAccessContext Context, VValue NewValue)
{
	return Value.SetTransactionally(Context, *this, NewValue);
}
} // namespace Verse
#endif // WITH_VERSE_VM
