// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

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

template<typename OwningObjectT, bool /*bDerivedFromUObject*/, bool /*bDerivedFromTSharedFromThis*/>
class FControlFlowContainerInternal;

template<typename OwningObjectT>
class FControlFlowContainerInternal<OwningObjectT, true, false> : public FControlFlowContainerBase
{
public:
	FControlFlowContainerInternal() = delete;
	FControlFlowContainerInternal(OwningObjectT* InOwner, TSharedRef<FControlFlow> InFlow, const FString& FlowId)
		: FControlFlowContainerBase(InFlow, FlowId)
		, OwningObject(InOwner)
	{}
protected:
	virtual const void* const GetOwningObject() const override { return OwningObject.IsValid() ? OwningObject.Get() : nullptr; }
private:
	TWeakObjectPtr<const OwningObjectT> OwningObject;
};

template<typename OwningObjectT>
class FControlFlowContainerInternal<OwningObjectT, false, true> : public FControlFlowContainerBase
{
public:
	FControlFlowContainerInternal() = delete;
	FControlFlowContainerInternal(OwningObjectT* InOwner, TSharedRef<FControlFlow> InFlow, const FString& FlowId)
		: FControlFlowContainerBase(InFlow, FlowId)
		, OwningObject(InOwner->AsShared())
	{}
protected:
	virtual const void* const GetOwningObject() const override { return OwningObject.IsValid() ? OwningObject.Pin().Get() : nullptr; }
private:
	TWeakPtr<const OwningObjectT> OwningObject;
};

#define FControlFlowContainerInternal_Decl FControlFlowContainerInternal<ExternalObjectT, TIsDerivedFrom<ExternalObjectT, UObject>::IsDerived, TIsDerivedFrom<ExternalObjectT, TSharedFromThis<ExternalObjectT>>::IsDerived>

template<typename ExternalObjectT>
class TControlFlowContainer : public FControlFlowContainerInternal_Decl
{
public:
	TControlFlowContainer() = delete;
	TControlFlowContainer(ExternalObjectT* InOwner, TSharedRef<FControlFlow> InFlow, const FString& FlowId) : FControlFlowContainerInternal_Decl(InOwner, InFlow, FlowId) {}
};

#undef FControlFlowContainerInternal_Decl