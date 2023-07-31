// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Delegates/DelegateCombinations.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"

class FControlFlow;
class FConcurrentControlFlowBehavior;
struct FConcurrencySubFlowContainer;

// All Flows will be executed concurrently (Single-Threaded). WARNING: Having a non-terminating loop within a fork can cause a forever hang for the forked step
class FConcurrentControlFlows : public TSharedFromThis<FConcurrentControlFlows>
{
public:
	CONTROLFLOWS_API FControlFlow& AddOrGetProng(int32 InIdentifier, const FString& DebugSubFlowName = TEXT(""));
	CONTROLFLOWS_API FControlFlow& AddOrGetFlow(int32 InIdentifier, const FString& DebugSubFlowName = TEXT(""));

	// TODO: Multi-threaded-like behavior
	// void Set(const FConcurrentControlFlowBehavior& InBehavior);

private:
	friend class FControlFlowTask_ConcurrentFlows;

	bool AreAllSubFlowsCompletedOrCancelled() const;
	bool HasAnySubFlowBeenExecuted() const;

	void HandleConcurrentFlowCompleted(int32 FlowIndex);
	void HandleConcurrentFlowCancelled(int32 FlowIndex);

	void Execute();
	void CancelAll(); // Do not make public

	void OnAllCompleted();
	void OnAllCancelled();

	FSimpleDelegate OnConcurrencyCompleted;
	FSimpleDelegate OnConcurrencyCancelled;

	bool bCancelAllHasBegun = false;
	TMap<int32, TSharedRef<FConcurrencySubFlowContainer>> ConcurrentFlows;

private:
	FConcurrentControlFlowBehavior GetConcurrencyBehavior() const;
};

struct FConcurrencySubFlowContainer : public TSharedFromThis<FConcurrencySubFlowContainer>
{
	FConcurrencySubFlowContainer(const FString& InDebugName);

private:
	friend class FConcurrentControlFlows;

	bool HasBeenExecuted() const;
	bool HasBeenCancelled() const;
	bool IsCompleteOrCancelled() const;

	void Execute();
	void Cancel();
	FSimpleDelegate& OnComplete();
	FSimpleDelegate& OnExecutedWithoutAnyNodes();
	FSimpleDelegate& OnCancelled();
	const FString& GetDebugName();

	TSharedRef<FControlFlow> GetControlFlow() const;

	bool bHasBeenExecuted = false;
	bool bHasBeenCancelled = false;

	TSharedRef<FControlFlow> SubFlow;
};

// Placeholder class to extend Concurrency behavior. TODO: Read up more on multi-threaded processing
class FConcurrentControlFlowBehavior
{
	friend class FConcurrentControlFlows;
private:
	enum class EContinueConditions
	{
		ContinueOnAll_CompletedOrCancelled, // Completed or Canceled
		// Can add behavior here as needed before having an evaluation method
	};

	EContinueConditions GetContinueCondition() const { return EContinueConditions::ContinueOnAll_CompletedOrCancelled; }

	// TODO: Customized continue conditions
	// void EvaluateContinueCondition();
	//
	// Examples:
	//		ContinueOnFirstCompleted_CancelOthers,
	//		ContinueOnFirstCanceled_CancelOthers,
	//		ContinueOnFirstCompletedOrCanceled_CancelOthers,
	//		ContinueOnSpecificFlowsCompleted_CancelOthers,
	//		ContinueOnSpecificFlowsCancelled_CancelOthers,
	//		ContinueOnSpecificFlowsCompletedOrCancelled_CancelOthers

	// TODO: Determine how a Forked Flow handles canceling
	// Having a cancelled sub-flow affect other flows sort of goes against the idea of "Fork" flow, where each Subflow behaves independently from each other. Further debate required.
};