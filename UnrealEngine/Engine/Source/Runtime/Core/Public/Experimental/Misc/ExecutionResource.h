// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/RefCounting.h"

/**
 * Simple ref-counted interface that represents a resource that is held or required for the execution of some task item.
 */
class IExecutionResource : public IRefCountedObject
{
public:
	virtual ~IExecutionResource() {};
};

/**
 * Per-thread stack-frame of execution resource currently held.
 * Can be retrieved and passed around to other task until not needed anymore.
 */
class CORE_API FExecutionResourceContext
{
public:
	static TRefCountPtr<IExecutionResource> Get();
};

/**
 * Used to push an execution resource on the stack-frame that can be retrieved by FExecutionResourceContext::Get().
 */
class CORE_API FExecutionResourceContextScope
{
public:
	FExecutionResourceContextScope(TRefCountPtr<IExecutionResource> InExecutionResource);
	~FExecutionResourceContextScope();
};