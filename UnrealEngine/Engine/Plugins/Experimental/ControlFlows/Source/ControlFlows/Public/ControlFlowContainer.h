// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_SKIP - Not included directly

#include "Templates/SharedPointer.h"
#include "CastedTo.h"

class FControlFlowContainerBase : public TSharedFromThis<FControlFlowContainerBase>
{
	friend class FControlFlowStatics;
public:

	FControlFlowContainerBase() = delete;
	virtual ~FControlFlowContainerBase() {}

	FControlFlowContainerBase(TSharedRef<FControlFlow> InFlow, const FString& FlowId)
		: ControlFlow(InFlow), FlowName(FlowId)
	{
		checkf(FlowName.Len() > 0, TEXT("All Flows need a non-empty ID!"));
	}

private:
	virtual const void* const GetOwningObject() const = 0;

private:
	bool OwningObjectIsValid() const { return GetOwningObject() != nullptr; }

	const FString& GetFlowName() const { return FlowName; }

	TSharedRef<FControlFlow> GetControlFlow() const { return ControlFlow; }

private:
	TSharedRef<FControlFlow> ControlFlow;

	FString FlowName;
};

template<typename T>
class TControlFlowContainer : public FControlFlowContainerBase
{
public:
	TControlFlowContainer() = delete;
	TControlFlowContainer(T* InOwner, TSharedRef<FControlFlow> InFlow, const FString& FlowId)
		: FControlFlowContainerBase(InFlow, FlowId)
		, OwningObject(InOwner)
	{}

private:
	virtual const void* const GetOwningObject() const override final
	{
		return OwningObject.IsValid() ? OwningObject.Cast() : nullptr;
	}

private:
	TWeakContainer<T> OwningObject;
};