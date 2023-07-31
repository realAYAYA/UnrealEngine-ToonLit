// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/Invoke.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

#define UE_API DERIVEDDATACACHE_API

template <typename FuncType> class TUniqueFunction;

namespace UE::DerivedData { class IRequest; }
namespace UE::DerivedData { enum class EPriority : uint8; }

namespace UE::DerivedData
{

/** Flags to control the behavior of request barriers. */
enum class ERequestBarrierFlags : uint32
{
	/** A value without any flags set. */
	None            = 0,
	/** Within the barrier, calls to SetPriority are replayed to requests in calls to Begin. */
	Priority        = 1 << 0,
};

ENUM_CLASS_FLAGS(ERequestBarrierFlags);

/**
 * A request owner manages requests throughout their execution.
 *
 * Requests are expected to call Begin when created, and End when finished, with their completion
 * callback and arguments passed to End when possible. If a request needs to invoke more than one
 * callback, have a request barrier in scope when executing callbacks to ensure that new requests
 * are prioritized properly when the priority is being changed from another thread.
 *
 * The owner is responsible for keeping itself alive while it has active requests or barriers.
 *
 * @see FRequestBarrier
 * @see FRequestOwner
 */
class IRequestOwner
{
public:
	/**
	 * Begin tracking for the request.
	 *
	 * The owner will hold a reference to the request until End is called, and forward any cancel
	 * operation or priority change to the request. Begin must return before End is called.
	 */
	virtual void Begin(IRequest* Request) = 0;

	/**
	 * End tracking for the request.
	 *
	 * Requests with a single completion callback should use the callback overload of End.
	 *
	 * @return The reference to the request that was held by the owner.
	 */
	virtual TRefCountPtr<IRequest> End(IRequest* Request) = 0;

	/**
	 * End tracking of the request.
	 *
	 * Begins a barrier, ends the request, invokes the callback, then ends the barrier. Keeps its
	 * reference to the request until the callback has returned.
	 *
	 * @return The reference to the request that was held by the owner.
	 */
	template <typename CallbackType, typename... CallbackArgTypes>
	inline TRefCountPtr<IRequest> End(IRequest* Request, CallbackType&& Callback, CallbackArgTypes&&... CallbackArgs);

	/** See FRequestBarrier. */
	virtual void BeginBarrier(ERequestBarrierFlags Flags) = 0;
	virtual void EndBarrier(ERequestBarrierFlags Flags) = 0;

	/** Returns the priority that new requests are expected to inherit. */
	virtual EPriority GetPriority() const = 0;
	/** Returns whether the owner has been canceled, which new requests are expected to check. */
	virtual bool IsCanceled() const = 0;

	/**
	 * Launches a task that executes the task body when scheduled.
	 *
	 * The task inherits the current priority of this request owner.
	 * The task is launched as a new request within this request owner.
	 *
	 * @note The debug name is not copied and must remain valid for the duration of the task.
	 */
	UE_API void LaunchTask(const TCHAR* DebugName, TUniqueFunction<void ()>&& TaskBody);
};

/**
 * A concrete request owner that also presents as a request.
 *
 * Request owners may be moved but not copied, and cancel any outstanding requests on destruction
 * unless KeepAlive has been called.
 *
 * @see IRequestOwner
 */
class FRequestOwner
{
public:
	/** Construct a request owner with the given priority. */
	UE_API explicit FRequestOwner(EPriority Priority);

	FRequestOwner(FRequestOwner&&) = default;
	FRequestOwner& operator=(FRequestOwner&&) = default;

	FRequestOwner(const FRequestOwner&) = delete;
	FRequestOwner& operator=(const FRequestOwner&) = delete;

	/** Keep requests in the owner alive until complete, even after destruction of the owner. */
	UE_API void KeepAlive();

	/** Returns the priority that new requests are expected to inherit. */
	UE_API EPriority GetPriority() const;
	/** Set the priority of active and future requests in the owner. */
	UE_API void SetPriority(EPriority Priority);
	/** Cancel any active and future requests in the owner. */
	UE_API void Cancel();
	/** Wait for any active and future requests or barriers in the owner. */
	UE_API void Wait();
	/** Poll whether the owner has any active requests or barriers. */
	UE_API bool Poll() const;

	/** Returns whether the owner has been canceled, which new requests are expected to check. */
	inline bool IsCanceled() const { return Owner->IsCanceled(); }

	/** Launches a task that executes the task body when scheduled. See IRequestOwner::LaunchTask. */
	inline void LaunchTask(const TCHAR* DebugName, TUniqueFunction<void ()>&& TaskBody)
	{
		Owner->LaunchTask(DebugName, MoveTemp(TaskBody));
	}

	/** Access as a request owner. */
	inline operator IRequestOwner&() { return *Owner; }
	/** Access as a request. */
	UE_API operator IRequest*();

private:
	UE_API static void Destroy(IRequestOwner& SharedOwner);

	struct FDeleteOwner { inline void operator()(IRequestOwner* O) const { if (O) { Destroy(*O); } } };
	TUniquePtr<IRequestOwner, FDeleteOwner> Owner;
};

/**
 * A request barrier is expected to be used when an owner may have new requests added to it.
 *
 * An owner may not consider its execution to be complete in the presence of a barrier, and needs
 * to take note of priority changes that occur while within a barrier, since newly-added requests
 * may have been created with the previous priority value.
 */
class FRequestBarrier
{
public:
	inline explicit FRequestBarrier(IRequestOwner& InOwner, ERequestBarrierFlags InFlags = ERequestBarrierFlags::None)
		: Owner(InOwner)
		, Flags(InFlags)
	{
		Owner.BeginBarrier(Flags);
	}

	inline ~FRequestBarrier()
	{
		Owner.EndBarrier(Flags);
	}

	FRequestBarrier(const FRequestBarrier&) = delete;
	FRequestBarrier& operator=(const FRequestBarrier&) = delete;

private:
	IRequestOwner& Owner;
	ERequestBarrierFlags Flags;
};

template <typename CallbackType, typename... CallbackArgTypes>
TRefCountPtr<IRequest> IRequestOwner::End(IRequest* Request, CallbackType&& Callback, CallbackArgTypes&&... CallbackArgs)
{
	FRequestBarrier Barrier(*this, ERequestBarrierFlags::Priority);
	TRefCountPtr<IRequest> RequestRef = End(Request);
	Invoke(Forward<CallbackType>(Callback), Forward<CallbackArgTypes>(CallbackArgs)...);
	return RequestRef;
}

} // UE::DerivedData

#undef UE_API
