// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "AutoRTFM/AutoRTFM.h"
#include "VVMCell.h"
#include "VVMContext.h"
#include "VVMLog.h"
#include "VVMWriteBarrier.h"
#include <Containers/Array.h>
#include <Containers/Set.h>

namespace Verse
{

struct FMarkStack;

struct FTransactionLog
{
public:
	struct FEntry
	{
		void* Key() { return &Slot; }
		VCell& Owner;
		TWriteBarrier<VValue>& Slot;
		VValue OldValue;

		void MarkReferencedCells(FMarkStack&);
	};

	TSet<void*> IsInLog; // TODO: We should probably use something like AutoRTFM's HitSet
	TArray<FEntry> Log;

public:
	void Add(FEntry Entry)
	{
		bool AlreadyHasEntry;
		IsInLog.FindOrAdd(Entry.Key(), &AlreadyHasEntry);
		if (!AlreadyHasEntry)
		{
			Log.Add(MoveTemp(Entry));
		}
	}

	// This version avoids loading from Slot until we need it.
	void Add(VCell& Owner, TWriteBarrier<VValue>& Slot)
	{
		void* Key = &Slot;
		bool AlreadyHasEntry;
		IsInLog.FindOrAdd(Key, &AlreadyHasEntry);
		if (!AlreadyHasEntry)
		{
			Log.Add(FEntry{Owner, Slot, Slot.Get()});
		}
	}

	void Join(FTransactionLog& Child)
	{
		for (FEntry Entry : Child.Log)
		{
			Add(MoveTemp(Entry));
		}
	}

	void Abort(FAccessContext Context)
	{
		for (FEntry Entry : Log)
		{
			Entry.Slot.Set(Context, Entry.OldValue);
		}
	}

	void MarkReferencedCells(FMarkStack&);
};

struct FTransaction
{
	FTransactionLog Log;
	FTransaction* Parent{nullptr};
	bool bHasStarted{false};
	bool bHasCommitted{false};
	bool bHasAborted{false};

	// Note: We can Abort before we Start because of how leniency works. For example, we can't
	// Start the transaction until the effect token is concrete, but the effect token may become
	// concrete after failure occurs.
	void Start(FRunningContext Context)
	{
		V_DIE_IF(bHasCommitted);
		V_DIE_IF(bHasStarted);
		V_DIE_IF(Parent);
		bHasStarted = true;

		if (!bHasAborted)
		{
			AutoRTFM::ForTheRuntime::StartTransaction();
			Parent = Context.CurrentTransaction();
			Context.SetCurrentTransaction(this);
		}
	}

	// We can't call Commit before we Start because we serialize Start then Commit via the effect token.
	void Commit(FRunningContext Context)
	{
		V_DIE_UNLESS(bHasStarted);
		V_DIE_IF(bHasAborted);
		V_DIE_IF(bHasCommitted);
		bHasCommitted = true;
		AutoRTFM::ForTheRuntime::CommitTransaction();
		if (Parent)
		{
			Parent->Log.Join(Log);
		}
		Context.SetCurrentTransaction(Parent);
	}

	// See above comment as to why we might Abort before we start.
	void Abort(FRunningContext Context)
	{
		V_DIE_IF(bHasCommitted);
		V_DIE_IF(bHasAborted);
		bHasAborted = true;
		if (bHasStarted)
		{
			V_DIE_UNLESS(Context.CurrentTransaction() == this);

			AutoRTFM::AbortTransaction();
			AutoRTFM::ForTheRuntime::ClearTransactionStatus();
			Log.Abort(Context);
			Context.SetCurrentTransaction(Parent);
		}
		else
		{
			V_DIE_IF(Parent);
		}
	}

	void LogBeforeWrite(FAccessContext Context, VCell& Owner, TWriteBarrier<VValue>& Slot)
	{
		Log.Add(Owner, Slot);
	}

	static void MarkReferencedCells(FTransaction&, FMarkStack&);
};

} // namespace Verse
#endif // WITH_VERSE_VM
