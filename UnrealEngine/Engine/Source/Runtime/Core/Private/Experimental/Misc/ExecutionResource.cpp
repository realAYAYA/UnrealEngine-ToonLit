// Copyright Epic Games, Inc. All Rights Reserved.
#include "Experimental/Misc/ExecutionResource.h"

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Templates/UnrealTemplate.h"

namespace ExecutionResourceImpl
{
	static thread_local TArray<TRefCountPtr<IExecutionResource>> ExecutionResourceStack;
}

FExecutionResourceContextScope::FExecutionResourceContextScope(TRefCountPtr<IExecutionResource> ExecutionResource)
{
	ExecutionResourceImpl::ExecutionResourceStack.Push(MoveTemp(ExecutionResource));
}

FExecutionResourceContextScope::~FExecutionResourceContextScope()
{
	ExecutionResourceImpl::ExecutionResourceStack.Pop();
}

class FCompositeExecutionResource : public IExecutionResource, public FThreadSafeRefCountedObject
{
public:
	FCompositeExecutionResource(const TArray<TRefCountPtr<IExecutionResource>>& InExecutionResources)
		: ExecutionResources(InExecutionResources)
	{
	}

	uint32 AddRef() const override
	{
		return FThreadSafeRefCountedObject::AddRef();
	}

	uint32 Release() const override
	{
		return FThreadSafeRefCountedObject::Release();
	}
	
	uint32 GetRefCount() const override
	{
		return FThreadSafeRefCountedObject::GetRefCount();
	}

private:
	TArray<TRefCountPtr<IExecutionResource>> ExecutionResources;
};

TRefCountPtr<IExecutionResource> FExecutionResourceContext::Get()
{
	using namespace ExecutionResourceImpl;
	if (ExecutionResourceStack.IsEmpty())
	{
		return nullptr;
	}

	return new FCompositeExecutionResource(ExecutionResourceStack);
}
