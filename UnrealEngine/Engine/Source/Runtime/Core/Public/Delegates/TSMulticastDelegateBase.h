// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/ScopeLock.h"
#include "Delegates/MulticastDelegateBase.h"

/**
 * Abstract base class for thread-safe multicast delegates.
 */
template<typename UserPolicy>
class TTSMulticastDelegateBase : protected TMulticastDelegateBase<UserPolicy>
{
protected:
	using Super = TMulticastDelegateBase<UserPolicy>;
	using UserPolicyType = UserPolicy;

public:
	/** Removes all functions from this delegate's invocation list. */
	void Clear()
	{
		FScopeLock Lock(&CS);
		Super::Clear();
	}

	/**
	 * Checks to see if any functions are bound to this multi-cast delegate.
	 *
	 * @return true if any functions are bound, false otherwise.
	 */
	inline bool IsBound() const
	{
		FScopeLock Lock(&CS);
		return Super::IsBound();
	}

	/**
	 * Checks to see if any functions are bound to the given user object.
	 *
	 * @return	True if any functions are bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject(void const* InUserObject) const
	{
		FScopeLock Lock(&CS);
		return Super::IsBoundToObject(InUserObject);
	}

	/**
	 * Removes all functions from this multi-cast delegate's invocation list that are bound to the specified UserObject.
	 * Note that the order of the delegates may not be preserved!
	 *
	 * @param InUserObject The object to remove all delegates for.
	 * @return  The number of delegates successfully removed.
	 */
	int32 RemoveAll(const void* InUserObject)
	{
		FScopeLock Lock(&CS);
		return Super::RemoveAll(InUserObject);
	}

protected:

	/** Hidden default constructor. */
	TTSMulticastDelegateBase() = default;

protected:
	template<typename DelegateInstanceInterfaceType, typename DelegateType>
	void CopyFrom(const TTSMulticastDelegateBase& Other)
	{
		FScopeLock Lock(&CS);
		Super::template CopyFrom<DelegateInstanceInterfaceType, DelegateType>(Other);
	}

	template<typename DelegateInstanceInterfaceType, typename DelegateBaseType, typename... ParamTypes>
	void Broadcast(ParamTypes... Params) const
	{
		FScopeLock Lock(&const_cast<TTSMulticastDelegateBase*>(this)->CS);
		Super::template Broadcast<DelegateInstanceInterfaceType, DelegateBaseType, ParamTypes...>(Params...);
	}

	/**
	 * Adds the given delegate instance to the invocation list.
	 *
	 * @param NewDelegateBaseRef The delegate instance to add.
	 */
	inline FDelegateHandle AddDelegateInstance(TDelegateBase<UserPolicy>&& NewDelegateBaseRef)
	{
		FScopeLock Lock(&CS);
		return Super::AddDelegateInstance(MoveTemp(NewDelegateBaseRef));
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * @param Handle The handle of the delegate instance to remove.
	 * @return  true if the delegate was successfully removed.
	 */
	bool RemoveDelegateInstance(FDelegateHandle Handle)
	{
		FScopeLock Lock(&CS);
		return Super::RemoveDelegateInstance(Handle);
	}

private:
	mutable FCriticalSection CS;
};