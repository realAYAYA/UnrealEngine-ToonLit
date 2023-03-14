// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlFlow.h"

#include "UObject/StrongObjectPtr.h"
#include "ControlFlowContainer.h"

class CONTROLFLOWS_API FControlFlowStatics
{
public:
	template<typename OwningObjectT>
	static FControlFlow& Create(OwningObjectT* OwningObject, const FString& FlowId)
	{
		ensureAlwaysMsgf(FlowId.Len() > 0, TEXT("All Flows need a non-empty ID!"));

		FControlFlow* AlreadyExistedFlow = Find(OwningObject, FlowId);
		if (!ensureAlwaysMsgf(!AlreadyExistedFlow, TEXT("Flow already exists! All flows should have a unique ID. If there are multiple instances of the owning object, that might cause this! You can call MakeShared<FControlFlow> instead of using FControlFlowStatics!")))
		{
			AlreadyExistedFlow->Reset();
		}

		return AlreadyExistedFlow ? *AlreadyExistedFlow : Create_Internal(OwningObject, FlowId, false);
	}

public:
	template<typename OwningObjectT>
	static void StopFlow(OwningObjectT* OwningObject, const FString& FlowId)
	{
		FControlFlow* FlowToStop = Find(OwningObject, FlowId);
		if (ensureAlwaysMsgf(FlowToStop && FlowToStop->IsRunning(), TEXT("Called to stop flow when it doesn't exist or not running!")))
		{
			FlowToStop->CancelFlow();

			CheckForInvalidFlows();
		}
	}

public:
	template<typename OwningObjectT>
	static FControlFlow* Find(OwningObjectT* OwningObject, const FString& FlowId)
	{
		TSharedPtr<FControlFlow> FoundFlow = Find_Internal(OwningObject, FlowId, GetNewlyCreatedFlows());
		if (!FoundFlow.IsValid()) { FoundFlow = Find_Internal(OwningObject, FlowId, GetExecutingFlows()); }
		if (!FoundFlow.IsValid()) { FoundFlow = Find_Internal(OwningObject, FlowId, GetFinishedFlows()); }
		if (!FoundFlow.IsValid()) { FoundFlow = Find_Internal(OwningObject, FlowId, GetPersistentFlows()); }

		return FoundFlow.Get();
	}

public:
	template<typename OwningObjectT>
	static FControlFlow& FindOrCreate(OwningObjectT* OwningObject, const FString& FlowId, bool bResetIfFound = false)
	{
		FControlFlow* AlreadyExistedFlow = Find(OwningObject, FlowId);
		if (AlreadyExistedFlow && bResetIfFound)
		{
			AlreadyExistedFlow->Reset();
		}

		return AlreadyExistedFlow ? *AlreadyExistedFlow : Create_Internal(OwningObject, FlowId, true);
	}

public:
	template<typename OwningObjectT>
	static bool IsRunning(OwningObjectT* OwningObject, const FString& FlowId)
	{
		FControlFlow* FoundFlow = Find(OwningObject, FlowId);
		return FoundFlow && FoundFlow->IsRunning();
	}

private:
	// These flows will be checked the next frame to make sure they are executed. If they are not, it will execute the flow and remove from this array.
	static TArray<TSharedRef<FControlFlowContainerBase>>& GetNewlyCreatedFlows();

	// These flows will not be checked the next frame, and will be moved to executing when we find ourselves executing.
	static TArray<TSharedRef<FControlFlowContainerBase>>& GetPersistentFlows();

	// Flows that are actively running.
	static TArray<TSharedRef<FControlFlowContainerBase>>& GetExecutingFlows();

	// Final Array that flows are moved to before being deleted.
	static TArray<TSharedRef<FControlFlowContainerBase>>& GetFinishedFlows();

private:
	template<typename OwningObjectT>
	static FControlFlow& Create_Internal(OwningObjectT* OwningObject, const FString& FlowId, bool bIsPersistent)
	{
		checkf(OwningObject, TEXT("Invalid Object!"));

		TSharedRef<TControlFlowContainer<OwningObjectT>> NewContainer = MakeShared<TControlFlowContainer<OwningObjectT>>(OwningObject, MakeShared<FControlFlow>(FlowId), FlowId);

		if (bIsPersistent)
		{
			GetPersistentFlows().Add(NewContainer);
		}
		else
		{
			GetNewlyCreatedFlows().Add(NewContainer);
		}

		NewContainer->GetControlFlow()->Reset();

		CheckNewlyCreatedFlows();

		return NewContainer->GetControlFlow().Get();
	}

private:
	template<typename OwningObjectT>
	static TSharedPtr<FControlFlow> Find_Internal(OwningObjectT* OwningObject, const FString& FlowId, TArray<TSharedRef<FControlFlowContainerBase>>& InArray)
	{
		if (ensure(OwningObject))
		{
			for (TSharedRef<FControlFlowContainerBase> ControlFlowContainer : InArray)
			{
				if (ControlFlowContainer->OwningObjectIsValid())
				{
					if (ControlFlowContainer->GetOwningObject() == OwningObject && ControlFlowContainer->GetFlowName() == FlowId)
					{
						return ControlFlowContainer->GetControlFlow();
					}
				}
				else
				{
					CheckForInvalidFlows();
				}
			}
		}

		return nullptr;
	}

private:
	friend class FControlFlow;

	static void HandleControlFlowStartedNotification(TSharedRef<const FControlFlow> InFlow);
	static void HandleControlFlowFinishedNotification() { CheckForInvalidFlows(); }

private:
	static void CheckNewlyCreatedFlows();
	static void CheckForInvalidFlows();

private:
	static bool IterateThroughNewlyCreatedFlows(float DeltaTime);
	static bool IterateForInvalidFlows(float DeltaTime);
};
