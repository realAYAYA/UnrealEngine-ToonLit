// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMTransaction.h"

namespace Verse
{

void FTransactionLog::FEntry::MarkReferencedCells(FMarkStack& MarkStack)
{
	MarkStack.MarkNonNull(&Owner);

	if (VCell* Cell = OldValue.ExtractCell())
	{
		MarkStack.MarkNonNull(Cell);
	}
}

void FTransactionLog::MarkReferencedCells(FMarkStack& MarkStack)
{
	for (FEntry& Entry : Log)
	{
		Entry.MarkReferencedCells(MarkStack);
	}
}

// TODO: We should treat the owner as a weak reference and only mark the old value
// if the owner is marked. However, to do that, we also need to make sure we can prune
// dead entries from the log during census, which runs concurrent to the mutator.
// Therefore, we need a concurrent algorithm for this. For now, since it's abundantly
// likely that the "var" cell is alive when used in the middle of a transaction,
// we just treat it as a root.
void FTransaction::MarkReferencedCells(FTransaction& _Transaction, FMarkStack& MarkStack)
{
	for (FTransaction* Transaction = &_Transaction; Transaction; Transaction = Transaction->Parent)
	{
		Transaction->Log.MarkReferencedCells(MarkStack);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)