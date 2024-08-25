// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Math/UnrealMathUtility.h"
#include "Delegates/IDelegateInstance.h" // IWYU pragma: export
#include "Delegates/DelegateBase.h" // IWYU pragma: export

#if !defined(NUM_MULTICAST_DELEGATE_INLINE_ENTRIES) || NUM_MULTICAST_DELEGATE_INLINE_ENTRIES == 0
	typedef FHeapAllocator FMulticastInvocationListAllocatorType;
#elif NUM_MULTICAST_DELEGATE_INLINE_ENTRIES < 0
	#error NUM_MULTICAST_DELEGATE_INLINE_ENTRIES must be positive
#else
	typedef TInlineAllocator<NUM_MULTICAST_DELEGATE_INLINE_ENTRIES> FMulticastInvocationListAllocatorType;
#endif

#define UE_MULTICAST_DELEGATE_DEFAULT_COMPACTION_THRESHOLD 2

/**
 * Abstract base class for multicast delegates.
 */
template<typename UserPolicy>
class TMulticastDelegateBase : public TDelegateAccessHandlerBase<typename UserPolicy::FThreadSafetyMode>
{
protected:
	using Super = TDelegateAccessHandlerBase<typename UserPolicy::FThreadSafetyMode>;
	using typename Super::FReadAccessScope;
	using Super::GetReadAccessScope;
	using typename Super::FWriteAccessScope;
	using Super::GetWriteAccessScope;

	// individual bindings are not checked for races as it's done for the parent delegate
	using UnicastDelegateType = TDelegateBase<FNotThreadSafeNotCheckedDelegateMode>;

	using InvocationListType = TArray<UnicastDelegateType, FMulticastInvocationListAllocatorType>;

public:
	TMulticastDelegateBase(TMulticastDelegateBase&& Other)
	{
		*this = MoveTemp(Other);
	}

	TMulticastDelegateBase& operator=(TMulticastDelegateBase&& Other)
	{
		if (&Other == this)
		{
			return *this;
		}

		InvocationListType LocalInvocationList;
		int32 LocalCompactionThreshold;

		{
			FWriteAccessScope OtherWriteScope = Other.GetWriteAccessScope();

			LocalInvocationList = MoveTemp(Other.InvocationList);
			LocalCompactionThreshold = Other.CompactionThreshold;
			Other.CompactionThreshold = UE_MULTICAST_DELEGATE_DEFAULT_COMPACTION_THRESHOLD;
		}

		{
			FWriteAccessScope ThisWriteScope = GetWriteAccessScope();

			ClearUnchecked();
			InvocationList = MoveTemp(LocalInvocationList);
			CompactionThreshold = LocalCompactionThreshold;
		}

		return *this;
	}

	/** Removes all functions from this delegate's invocation list. */
	void Clear( )
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		ClearUnchecked();
	}

	/**
	 * Checks to see if any functions are bound to this multi-cast delegate.
	 *
	 * @return true if any functions are bound, false otherwise.
	 */
	inline bool IsBound( ) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
	
		for (const UnicastDelegateType& DelegateBaseRef : InvocationList)
		{
			if (DelegateBaseRef.GetDelegateInstanceProtected())
			{
				return true;
			}
		}
		return false;
	}

	/** 
	 * Checks to see if any functions are bound to the given user object.
	 *
	 * @return	True if any functions are bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject( void const* InUserObject ) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		for (const UnicastDelegateType& DelegateBaseRef : InvocationList)
		{
			const IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
			if ((DelegateInstance != nullptr) && DelegateInstance->HasSameObject(InUserObject))
			{
				return true;
			}
		}

		return false;
	}

	/**
	 * Removes all functions from this multi-cast delegate's invocation list that are bound to the specified UserObject.
	 * Note that the order of the delegates may not be preserved!
	 *
	 * @param InUserObject The object to remove all delegates for.
	 * @return  The number of delegates successfully removed.
	 */
	int32 RemoveAll( const void* InUserObject )
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		int32 Result = 0;
		if (InvocationListLockCount > 0)
		{
			for (UnicastDelegateType& DelegateBaseRef : InvocationList)
			{
				IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
				if ((DelegateInstance != nullptr) && DelegateInstance->HasSameObject(InUserObject))
				{
					// Manually unbind the delegate here so the compaction will find and remove it.
					DelegateBaseRef.Unbind();
					++Result;
				}
			}

			// can't compact at the moment as the invocation list is locked, but set out threshold to zero so the next add will do it
			if (Result > 0)
			{
				CompactionThreshold = 0;
			}
		}
		else
		{
			// compact us while shuffling in later delegates to fill holes
			for (int32 InvocationListIndex = 0; InvocationListIndex < InvocationList.Num();)
			{
				UnicastDelegateType& DelegateBaseRef = InvocationList[InvocationListIndex];

				IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
				if (DelegateInstance == nullptr
					|| DelegateInstance->HasSameObject(InUserObject)
					|| DelegateInstance->IsCompactable())
				{
					InvocationList.RemoveAtSwap(InvocationListIndex, 1, EAllowShrinking::No);
					++Result;
				}
				else
				{
					InvocationListIndex++;
				}
			}

			CompactionThreshold = FMath::Max(UE_MULTICAST_DELEGATE_DEFAULT_COMPACTION_THRESHOLD, 2 * InvocationList.Num());

			InvocationList.Shrink();
		}

		return Result;
	}

	/**
	 * Returns the amount of memory allocated by this delegate's invocation list and the delegates stored within it, not including sizeof(*this).
	 */
	SIZE_T GetAllocatedSize() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
	
		SIZE_T Size = 0;
		Size += InvocationList.GetAllocatedSize();
		for (const UnicastDelegateType& DelegateBaseRef : InvocationList)
		{
			Size += DelegateBaseRef.GetAllocatedSize();
		}
		return Size;
	}

protected:

	/** Hidden default constructor. */
	inline TMulticastDelegateBase( )
		: CompactionThreshold(UE_MULTICAST_DELEGATE_DEFAULT_COMPACTION_THRESHOLD)
		, InvocationListLockCount(0)
	{ }

protected:
	template<typename DelegateInstanceInterfaceType>
	void CopyFrom(const TMulticastDelegateBase& Other)
	{
		InvocationListType TempInvocationList;

		{
			FReadAccessScope OtherReadScope = Other.GetReadAccessScope();

		    for (const UnicastDelegateType& OtherDelegateRef : Other.GetInvocationList())
		    {
				if (const IDelegateInstance* OtherInstance = OtherDelegateRef.GetDelegateInstanceProtected())
			    {
				    UnicastDelegateType TempDelegate;
				    static_cast<const DelegateInstanceInterfaceType*>(OtherInstance)->CreateCopy(TempDelegate);
				    TempInvocationList.Add(MoveTemp(TempDelegate));
			    }
		    }
		}

		{
			FWriteAccessScope ThisWriteScope = GetWriteAccessScope();

			ClearUnchecked();
			InvocationList = MoveTemp(TempInvocationList);
		}
	}

	template<typename DelegateInstanceInterfaceType, typename... ParamTypes>
	void Broadcast(ParamTypes... Params) const
	{
		// the `const` on the method is a lie
		FWriteAccessScope WriteScope = const_cast<TMulticastDelegateBase*>(this)->GetWriteAccessScope();

		bool NeedsCompaction = false;

		LockInvocationList();
		{
			const InvocationListType& LocalInvocationList = GetInvocationList();

			// call bound functions in reverse order, so we ignore any instances that may be added by callees
			for (int32 InvocationListIndex = LocalInvocationList.Num() - 1; InvocationListIndex >= 0; --InvocationListIndex)
			{
				// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
				const UnicastDelegateType& DelegateBase = LocalInvocationList[InvocationListIndex];

				const IDelegateInstance* DelegateInstanceInterface = DelegateBase.GetDelegateInstanceProtected();
				if (DelegateInstanceInterface == nullptr || !static_cast<const DelegateInstanceInterfaceType*>(DelegateInstanceInterface)->ExecuteIfSafe(Params...))
				{
					NeedsCompaction = true;
				}
			}
		}
		UnlockInvocationList();

		if (NeedsCompaction)
		{
			const_cast<TMulticastDelegateBase*>(this)->CompactInvocationList();
		}
	}

	/**
	 * Adds the given delegate instance to the invocation list.
	 *
	 * @param NewDelegateBaseRef The delegate instance to add.
	 */
	template <typename NewDelegateType>
	inline FDelegateHandle AddDelegateInstance(NewDelegateType&& NewDelegateBaseRef)
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		FDelegateHandle Result;
		if (NewDelegateBaseRef.IsBound())
		{
			// compact but obey threshold of when this will trigger
			CompactInvocationList(true);
			Result = NewDelegateBaseRef.GetHandle();
			InvocationList.Emplace(Forward<NewDelegateType>(NewDelegateBaseRef));
		}
		return Result;
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * @param Handle The handle of the delegate instance to remove.
	 * @return  true if the delegate was successfully removed.
	 */
	bool RemoveDelegateInstance(FDelegateHandle Handle)
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		for (int32 InvocationListIndex = 0; InvocationListIndex < InvocationList.Num(); ++InvocationListIndex)
		{
			UnicastDelegateType& DelegateBase = InvocationList[InvocationListIndex];

			IDelegateInstance* DelegateInstance = DelegateBase.GetDelegateInstanceProtected();
			if (DelegateInstance && DelegateInstance->GetHandle() == Handle)
			{
				DelegateBase.Unbind();
				CompactInvocationList();
				return true; // each delegate binding has a unique handle, so once we find it, we can stop
			}
		}

		return false;
	}

private:
	/**
	 * Removes any expired or deleted functions from the invocation list.
	 *
	 * @see RequestCompaction
	 */
	void CompactInvocationList(bool CheckThreshold=false)
	{
		// if locked and no object, just return
		if (InvocationListLockCount > 0)
		{
			return;
		}

		// if checking threshold, obey but decay. This is to ensure that even infrequently called delegates will
		// eventually compact during an Add()
		if (CheckThreshold 	&& --CompactionThreshold > InvocationList.Num())
		{
			return;
		}

		int32 OldNumItems = InvocationList.Num();

		// Find anything null or compactable and remove it
		for (int32 InvocationListIndex = 0; InvocationListIndex < InvocationList.Num();)
		{
			auto& DelegateBaseRef = InvocationList[InvocationListIndex];

			IDelegateInstance* DelegateInstance = DelegateBaseRef.GetDelegateInstanceProtected();
			if (DelegateInstance == nullptr	|| DelegateInstance->IsCompactable())
			{
				InvocationList.RemoveAtSwap(InvocationListIndex);
			}
			else
			{
				InvocationListIndex++;
			}
		}

		CompactionThreshold = FMath::Max(UE_MULTICAST_DELEGATE_DEFAULT_COMPACTION_THRESHOLD, 2 * InvocationList.Num());

		if (OldNumItems > CompactionThreshold)
		{
			// would be nice to shrink down to threshold, but reserve only grows..?
			InvocationList.Shrink();
		}
	}

	/**
	 * Gets a read-only reference to the invocation list.
	 *
	 * @return The invocation list.
	 */
	inline InvocationListType& GetInvocationList( )
	{
		return InvocationList;
	}

	inline const InvocationListType& GetInvocationList( ) const
	{
		return InvocationList;
	}

	/** Increments the lock counter for the invocation list. */
	inline void LockInvocationList( ) const
	{
		++InvocationListLockCount;
	}

	/** Decrements the lock counter for the invocation list. */
	inline void UnlockInvocationList( ) const
	{
		--InvocationListLockCount;
	}

	/** Returns the lock counter for the invocation list. */
	inline int32 GetInvocationListLockCount() const
	{
		return InvocationListLockCount;
	}

private:
	void ClearUnchecked()
	{
		for (UnicastDelegateType& DelegateBaseRef : InvocationList)
		{
			DelegateBaseRef.Unbind();
		}

		CompactInvocationList(false);
	}

	/** Holds the collection of delegate instances to invoke. */
	InvocationListType InvocationList;

	/** Used to determine when a compaction should happen. */
	int32 CompactionThreshold;

	/** Holds a lock counter for the invocation list. */
	mutable int32 InvocationListLockCount;
};
