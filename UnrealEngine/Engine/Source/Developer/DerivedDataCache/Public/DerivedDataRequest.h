// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include <atomic>

namespace UE::DerivedData { enum class EPriority : uint8; }

namespace UE::DerivedData
{

/**
 * Interface to an asynchronous request that can be prioritized or canceled.
 *
 * Use IRequestOwner, typically FRequestOwner, to reference requests between its Begin and End.
 *
 * Requests typically invoke a callback on completion, and must not return from Cancel or Wait on
 * any thread until any associated callbacks have finished executing. This property is crucial to
 * allowing requests to be chained or nested by creating new requests from within callbacks.
 *
 * @see FRequestBase
 * @see FRequestOwner
 * @see IRequestOwner
 */
class IRequest
{
public:
	virtual ~IRequest() = default;

	/**
	 * Set the priority of the request.
	 *
	 * @param Priority   The priority, which may be higher or lower than the existing priority.
	 */
	virtual void SetPriority(EPriority Priority) = 0;

	/**
	 * Cancel the request and invoke any associated callback.
	 *
	 * Must not return until any callback for the request has finished executing, and the request
	 * has been removed from its owner by calling IRequestOwner::End.
	 */
	virtual void Cancel() = 0;

	/**
	 * Block the calling thread until the request is complete.
	 *
	 * Must not return until any callback for the request has finished executing, and the request
	 * has been removed from its owner by calling IRequestOwner::End.
	 */
	virtual void Wait() = 0;

	/** Add a reference to the request. */
	virtual void AddRef() const = 0;

	/** Release a reference. The request is deleted when the last reference is released. */
	virtual void Release() const = 0;
};

/** An asynchronous request base type that provides reference counting. */
class FRequestBase : public IRequest
{
public:
	FRequestBase() = default;
	~FRequestBase() override = default;
	FRequestBase(const FRequestBase&) = delete;
	FRequestBase& operator=(const FRequestBase&) = delete;

	inline void AddRef() const final
	{
		ReferenceCount.fetch_add(1, std::memory_order_relaxed);
	}

	inline void Release() const final
	{
		if (ReferenceCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			delete this;
		}
	}

private:
	mutable std::atomic<uint32> ReferenceCount{0};
};

} // UE::DerivedData
