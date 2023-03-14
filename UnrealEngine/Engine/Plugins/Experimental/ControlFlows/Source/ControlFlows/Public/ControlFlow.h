// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlFlowNode.h"
#include "HAL/Platform.h"
#include "Templates/UnrealTypeTraits.h"

class UObject;
class FControlFlowTask_Branch;
class FControlFlowTask_BranchLegacy;

/**
 *  System/Tool to queue (asynchronous or synchronous) functions for modularity implemented via delegates.
 *  Allows code to be more easily read so there fewer 'Alt+G'-ing around to figure out what and where a class does it's thing
 *
 *  This system only supports non-const functions currently.
 *
 *  'QueueFunction': Queues a 'void (...)' function.
 *                   The Flow will execute this function and continue on with the next function in queue.
 *
 *  'QueueWait': Queues a 'void (FControlFlowNodeRef FlowHandle, ...)' function.
 *               The flow will stop until 'FlowHandle->ContinueFlow()' is called.
 *	             BE RESPONSIBLE and make sure all code paths call to continue, otherwise the flow will hang.
 *
 *  'QueueControlFlow': Queues a 'void (TSharedRef<FControlFlow> Subflow, ...)' function.
 * 
 *  'QueueControlFlowBranch': Queues a 'int32(TSharedRef<FControlFlowBranch> Branch, ...)' function. TODO: Allow any return-type for branch selection, and not restricted to only int32.
 * 
 *  'QueueStep': Usable in #UObject's or classes that derive from #TSharedFromThis<UserClass>. The Control Flow will automatically deduce if this is a
 *				 '#QueueFunction', '#QueueWait', '#QueueControlFlow', '#QueueControlFlowBranch' based on the function signature.
 *				 Returns a ref to the ControlFlow - enables chained step queue-ing.
 *
 *  Using the auto-deduction of 'QueueStep', you can change the queue from a synchronous function (QueueFunction) to an asynchronous one (QueueWait) or vice-versa
 *  by adding/removing the 'FControlFlowNodeRef FlowHandle' as your first parameter. And you can change it to (QueueControlFlow) if need be as well!
 * 
 *  Syntax:
 * 
 *  void MyFunction(...);
 *  void MyFunction(FControlFlowNodeRef FlowHandle, ...);
 *  void MyFunction(TSharedRef<FControlFlow> Subflow, ...);
 *  void MyFunction(int32(TSharedRef<FControlFlowBranch> Branch, ...);
 * 
 *  ControlFlowInstance->QueueStep(this, &UserClass:MyFunction1, ...)
 *		.QueueStep(this, &UserClass:MyFunction2, ...)
 *		.QueueStep(this, &UserClass:MyFunction3, ...);
 * 
 *  This allow ease of going from Synchronous Functionality to Asynchronously Functionality to Subflows as you build out your Flow.
 */

class CONTROLFLOWS_API FControlFlow : public TSharedFromThis<FControlFlow>
{
public:
	FControlFlow(const FString& FlowDebugName = TEXT(""));

public:
	/** This needs to be called, otherwise nothing will happen!! Call after you finish adding functions to the queue. Calling with an empty queue is safe. */
	void ExecuteFlow();
	void Reset();
	bool IsRunning() const { return CurrentNode.IsValid(); }
	size_t NumInQueue() const { return FlowQueue.Num(); }

	/** Will cancel ALL flows, both child ControlFlows and ControlFlows who owns this Flow. You've been warned. */
	void CancelFlow();

	FControlFlow& SetCancelledNodeAsComplete(bool bCancelledNodeIsComplete);

	TOptional<FString> GetCurrentStepDebugName() const;

	TSharedPtr<FTrackedActivity> GetTrackedActivity() const;

public:

	template<typename...ArgsT>
	FControlFlow& QueueStep(const FString& NodeName, ArgsT...Params)
	{
		QueueStep_Internal(FormatOrGetNewNodeDebugName(NodeName), Params...);
		return *this;
	}

	template<typename...ArgsT>
	FControlFlow& QueueStep(const TCHAR* NodeName, ArgsT...Params)
	{
		QueueStep_Internal(FormatOrGetNewNodeDebugName(NodeName), Params...);
		return *this;
	}

	template<typename...ArgsT>
	FControlFlow& QueueStep(const char* NodeName, ArgsT...Params)
	{
		QueueStep_Internal(FormatOrGetNewNodeDebugName(NodeName), Params...);
		return *this;
	}

	template<typename...ArgsT>
	FControlFlow& QueueStep(ArgsT...Params)
	{
		QueueStep_Internal(FormatOrGetNewNodeDebugName(), Params...);
		return *this;
	}

public:
	template<typename FunctionT, typename...ArgsT>
	FControlFlow& BranchFlow(FunctionT InBranchLambda, ArgsT...Params)
	{
		QueueControlFlowBranch(FormatOrGetNewNodeDebugName()).BindLambda(InBranchLambda, Params...);
		return *this;
	}

	template<typename FunctionT, typename...ArgsT>
	FControlFlow& ForkFlow(FunctionT InForkLambda, ArgsT...Params)
	{
		QueueConcurrentFlows(FormatOrGetNewNodeDebugName()).BindLambda(InForkLambda, Params...);
		return *this;
	}

	FControlFlow& TrackActivities(TSharedPtr<FTrackedActivity> InActivity = nullptr);

public:
	FSimpleDelegate& QueueFunction(const FString& FlowNodeDebugName = TEXT(""));
	FControlFlowWaitDelegate& QueueWait(const FString& FlowNodeDebugName = TEXT(""));
	FControlFlowPopulator& QueueControlFlow(const FString& TaskName = TEXT(""), const FString& FlowNodeDebugName = TEXT(""));
	FControlFlowBranchDefiner& QueueControlFlowBranch(const FString& TaskName = TEXT(""), const FString& FlowNodeDebugName = TEXT(""));
	FConcurrentFlowsDefiner& QueueConcurrentFlows(const FString& TaskName = TEXT(""), const FString& FlowNodeDebugName = TEXT(""));

private:
	template<typename BindingObjectT, typename...PayloadParamsT>
	void QueueStep_Internal(const FString& InDebugName, TSharedRef<BindingObjectT> InBindingObject, PayloadParamsT...Params)
	{
		//ensureMsgf(false, TEXT("Deprecated. Pass the raw pointer of the TSharedFromThis Binding Object (e.g. 'this') into QueueStep."));

		QueueStep_Internal_TSharedFromThis(InDebugName, InBindingObject, Params...);
	}

	template<typename BindingObjectT, typename...ArgsT>
	void QueueStep_Internal(const FString& InDebugName, BindingObjectT* InBindingObject, ArgsT...Params)
	{
		QueueStep_Internal_DeduceBindingObject<BindingObjectT>(InDebugName, InBindingObject, Params...);
	}

private:
	template<typename BindingObjectClassT, typename...ArgsT>
	void QueueStep_Internal_DeduceBindingObject(const FString& InDebugName, typename TEnableIf<TIsDerivedFrom<BindingObjectClassT, TSharedFromThis<BindingObjectClassT>>::IsDerived, BindingObjectClassT*>::Type InBindingObject, ArgsT...Params)
	{
		if (static_cast<TSharedFromThis<BindingObjectClassT>*>(InBindingObject)->DoesSharedInstanceExist())
		{
			QueueStep_Internal_TSharedFromThis(InDebugName, InBindingObject->AsShared(), Params...);
		}
	}

	template<typename BindingObjectClassT, typename...ArgsT>
	void QueueStep_Internal_DeduceBindingObject(const FString& InDebugName, typename TEnableIf<TIsDerivedFrom<BindingObjectClassT, UObject>::IsDerived, BindingObjectClassT*>::Type InBindingObject, ArgsT...Params)
	{
		QueueStep_Internal_UObject<BindingObjectClassT>(InDebugName, InBindingObject, Params...);
	}

private:
	
	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_TSharedFromThis(const FString& InDebugName, TSharedRef<BindingObjectClassT> InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(TSharedRef<FConcurrentControlFlows>, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueConcurrentFlows(InDebugName).BindSP(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_TSharedFromThis(const FString& InDebugName, TSharedRef<BindingObjectClassT> InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, int32(TSharedRef<FControlFlowBranch>, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueControlFlowBranch(InDebugName).BindSP(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_TSharedFromThis(const FString& InDebugName, TSharedRef<BindingObjectClassT> InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(FControlFlowNodeRef, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueWait(InDebugName).BindSP(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_TSharedFromThis(const FString& InDebugName, TSharedRef<BindingObjectClassT> InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(TSharedRef<FControlFlow>, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueControlFlow(InDebugName).BindSP(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_TSharedFromThis(const FString& InDebugName, TSharedRef<BindingObjectClassT> InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueFunction(InDebugName).BindSP(InBindingObject, InFunction, Params...);
	}

private:

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_UObject(const FString& InDebugName, BindingObjectClassT* InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(TSharedRef<FConcurrentControlFlows>, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueConcurrentFlows(InDebugName).BindUObject(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_UObject(const FString& InDebugName, BindingObjectClassT* InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, int32(TSharedRef<FControlFlowBranch>, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueControlFlowBranch(InDebugName).BindUObject(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_UObject(const FString& InDebugName, BindingObjectClassT* InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(FControlFlowNodeRef, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueWait(InDebugName).BindUObject(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_UObject(const FString& InDebugName, BindingObjectClassT* InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(TSharedRef<FControlFlow>, PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueControlFlow(InDebugName).BindUObject(InBindingObject, InFunction, Params...);
	}

	template<typename BindingObjectClassT, typename...PayloadParamsT>
	void QueueStep_Internal_UObject(const FString& InDebugName, BindingObjectClassT* InBindingObject, typename TMemFunPtrType<false, BindingObjectClassT, void(PayloadParamsT...)>::Type InFunction, PayloadParamsT...Params)
	{
		QueueFunction(InDebugName).BindUObject(InBindingObject, InFunction, Params...);
	}

private:
	friend class FControlFlowNode;
	friend class FControlFlowNode_RequiresCallback;
	friend class FControlFlowNode_SelfCompleting;
	friend class FControlFlowSimpleSubTask;
	friend class FControlFlowTask_Loop;
	friend class FControlFlowTask_BranchLegacy;
	friend class FControlFlowTask_Branch;
	friend class FControlFlowStatics;
	friend struct FConcurrencySubFlowContainer;

public:
	/** These work, but they are a bit clunky to use. The heart of the issue is that it requires the caller to define two functions. We want only the caller to use one function.
	  * Not making templated versions of them until a better API is figured out. */	
	TSharedRef<FControlFlowTask_BranchLegacy> QueueBranch(FControlFlowBranchDecider_Legacy& BranchDecider, const FString& TaskName = TEXT(""), const FString& FlowNodeDebugName = TEXT(""));

	//TODO: Implement #define QueueLoop_Signature TMemFunPtrType<false, BindingObjectClassType, bool(TSharedRef<FControlFlow> SubFlow, VarTypes...)> and delete

	/** Adds a Loop to your flow. The flow will use FControlFlowLoopComplete - if this returns false, the flow will execute FControlTaskQueuePopulator until true is returned */
	FControlFlowPopulator& QueueLoop(FControlFlowLoopComplete& LoopCompleteDelgate, const FString& TaskName = TEXT(""), const FString& FlowNodeDebugName = TEXT(""));

private:

	void HandleControlFlowNodeCompleted(TSharedRef<const FControlFlowNode> NodeCompleted);

	void ExecuteNextNodeInQueue();

	FSimpleDelegate& OnComplete() const { return OnCompleteDelegate; }
	FSimpleDelegate& OnExecutedWithoutAnyNodes() const { return OnExecutedWithoutAnyNodesDelegate; }
	FSimpleDelegate& OnCancelled() const { return OnCancelledDelegate; }

	mutable FSimpleDelegate OnCompleteDelegate;
	mutable FSimpleDelegate OnExecutedWithoutAnyNodesDelegate;
	mutable FSimpleDelegate OnCancelledDelegate;

private:
	void ExecuteNode(TSharedRef<FControlFlowNode_SelfCompleting> SelfCompletingNode);

private:

	void HandleTaskNodeExecuted(TSharedRef<FControlFlowNode_Task> TaskNode);
	void HandleTaskNodeCancelled(TSharedRef<FControlFlowNode_Task> TaskNode);

	void HandleOnTaskComplete();
	void HandleOnTaskCancelled();

private:

	void LogNodeExecution(const FControlFlowNode& NodeExecuted);

	FString GetFlowPath() const;

	int32 GetRepeatedFlowCount() const;

public:
	const FString& GetDebugName() const { return DebugName; }

private:
	FString FormatOrGetNewNodeDebugName(const FString& FlowNodeDebugName = TEXT(""));

private:

	static int32 UnnamedControlFlowCounter;

	FString DebugName;

	bool bInterpretCancelledNodeAsComplete = false;

	int32 UnnamedNodeCounter = 0;

	int32 UnnamedBranchCounter = 0;

	TSharedPtr<FControlFlowNode_Task> CurrentlyRunningTask = nullptr;

	TSharedPtr<FControlFlowNode> CurrentNode = nullptr;

	//TODO: Put behind some args, because this is expensive.
	TArray<TSharedRef<FControlFlow>> SubFlowStack_ForDebugging;

	TArray<TSharedRef<FControlFlowNode>> FlowQueue;

	TSharedPtr<FTrackedActivity> Activity;
};