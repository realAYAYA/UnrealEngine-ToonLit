// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoRTFM/AutoRTFM.h"
#include "ContextStatus.h"

namespace AutoRTFM
{

class FLineLock;
class FTransaction;
class FCallNest;

class FContext
{
public:
    static FContext* TryGet();
    static FContext* Get();
    static bool IsTransactional();
    
    // This is public API
    ETransactionResult Transact(void (*Function)(void* Arg), void* Arg);
    
	EContextStatus CallClosedNest(void (*ClosedFunction)(void* Arg), void* Arg);

	void AbortByRequestAndThrow();
	void AbortByRequestWithoutThrowing();

	// Open API - no throw
	bool StartTransaction();

	ETransactionResult CommitTransaction();
	ETransactionResult AbortTransaction(bool bIsClosed, bool bIsCascading);
	void ClearTransactionStatus();
	bool IsAborting() const;

	void CheckOpenRecordWrite(void* LogicalAddress);

    // Record that a write is about to occur at the given LogicalAddress of Size bytes.
    void RecordWrite(void* LogicalAddress, size_t Size);
    template<unsigned SIZE> void RecordWrite(void* LogicalAddress);

    void DidAllocate(void* LogicalAddress, size_t Size);
    void DidFree(void* LogicalAddress);

    // The rest of this is internalish.
    void AbortByLanguageAndThrow();

	inline FTransaction* GetCurrentTransaction() const { return CurrentTransaction; }
	inline FCallNest* GetCurrentNest() const { return CurrentNest; }
    inline bool IsTransactionStack(void* LogicalAddress) const { return LogicalAddress >= StackBegin && LogicalAddress < OuterTransactStackAddress; }
	inline bool IsInnerTransactionStack(void* LogicalAddress) const { return LogicalAddress >= StackBegin && LogicalAddress < CurrentTransactStackAddress; }
	inline EContextStatus GetStatus() const { return Status; }
	void Throw();
	
    void DumpState() const;

    static void InitializeGlobalData();

private:
    FContext();
    FContext(const FContext&) = delete;

	void PushCallNest(FCallNest* NewCallNest);
	void PopCallNest();

	void PushTransaction(FTransaction* NewTransaction);
	void PopTransaction();

	ETransactionResult ResolveNestedTransaction(FTransaction* NewTransaction);
	bool AttemptToCommitTransaction(FTransaction* const Transaction);

    void Set();
    
    // All of this other stuff ought to be private?
    void Reset();
    
    FTransaction* CurrentTransaction{nullptr};
	FCallNest* CurrentNest{nullptr};

    void* StackBegin{nullptr}; // begin as in the smaller of the two
    void* StackEnd{nullptr};
    void* OuterTransactStackAddress{nullptr};
    void* CurrentTransactStackAddress{nullptr};
    EContextStatus Status{EContextStatus::Idle};
};

} // namespace AutoRTFM
