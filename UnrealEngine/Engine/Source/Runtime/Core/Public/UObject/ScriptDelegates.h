// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/UnrealTypeTraits.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Delegates/DelegateAccessHandler.h"

namespace UE::Core::Private
{
	template <typename InThreadSafetyMode>
	struct TScriptDelegateTraits
	{
		// Although templated, WeakPtrType is not intended to be anything other than FWeakObjectPtr,
		// and is only a template for module organization reasons.
		using WeakPtrType = FWeakObjectPtr;

		using ThreadSafetyMode = InThreadSafetyMode;
		using UnicastThreadSafetyModeForMulticasts = FNotThreadSafeNotCheckedDelegateMode;
	};

	template <>
	struct UE_DEPRECATED(5.3, "TScriptDelegate<FWeakObjectPtr> and TMulticastScriptDelegate<FWeakObjectPtr> have been deprecated, please use FScriptDelegate or FMulticastScriptDelegate respectively.") TScriptDelegateTraits<FWeakObjectPtr>
	{
		// After this deprecated specialization has been removed, all of the functions inside
		// TMulticastScriptDelegate which take OtherDummy parameters should also be removed,
		// and also the TScriptDelegate(const TScriptDelegate<FWeakObjectPtr>&) constructor.

		using WeakPtrType = FWeakObjectPtr;
		using ThreadSafetyMode = FNotThreadSafeDelegateMode;
		using UnicastThreadSafetyModeForMulticasts = FNotThreadSafeNotCheckedDelegateMode;
	};

	// This function only exists to allow compatibility between multicast and unicast delegate types which use an explicit FWeakObjectPtr template parameter
	template <typename From, typename To>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	inline constexpr bool BackwardCompatibilityCheck()
	{
		if constexpr (std::is_same_v<From, FNotThreadSafeDelegateMode>)
		{
			return std::is_same_v<To, FWeakObjectPtr>;
		}
		else if constexpr (std::is_same_v<From, FWeakObjectPtr>)
		{
			return std::is_same_v<To, FNotThreadSafeDelegateMode>;
		}
		else
		{
			return false;
		}
	}
}

/**
 * Script delegate base class.
 */
template <typename InThreadSafetyMode>
class TScriptDelegate : public TDelegateAccessHandlerBase<typename UE::Core::Private::TScriptDelegateTraits<InThreadSafetyMode>::ThreadSafetyMode>
{
public:
	using ThreadSafetyMode = typename UE::Core::Private::TScriptDelegateTraits<InThreadSafetyMode>::ThreadSafetyMode;
	using WeakPtrType = typename UE::Core::Private::TScriptDelegateTraits<InThreadSafetyMode>::WeakPtrType;

private:
	template <typename>
	friend class TScriptDelegate;

	template<typename>
	friend class TMulticastScriptDelegate;

	using Super = TDelegateAccessHandlerBase<ThreadSafetyMode>;
	using typename Super::FReadAccessScope;
	using Super::GetReadAccessScope;
	using typename Super::FWriteAccessScope;
	using Super::GetWriteAccessScope;

public:
	/** Default constructor. */
	TScriptDelegate() 
		: Object( nullptr )
		, FunctionName( NAME_None )
	{ }

	TScriptDelegate(const TScriptDelegate& Other)
	{
		FReadAccessScope OtherReadScope = Other.GetReadAccessScope();

		Object = Other.Object;
		FunctionName = Other.FunctionName;
	}

	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	TScriptDelegate(const TScriptDelegate<OtherThreadSafetyMode>& Other)
	{
		typename TScriptDelegate<OtherThreadSafetyMode>::FReadAccessScope OtherReadScope = Other.GetReadAccessScope();

		Object = Other.Object;
		FunctionName = Other.FunctionName;
	}

	TScriptDelegate& operator=(const TScriptDelegate& Other)
	{
		WeakPtrType OtherObject;
		FName OtherFunctionName;

		{
			FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			OtherObject = Other.Object;
			OtherFunctionName = Other.FunctionName;
		}

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();
			Object = OtherObject;
			FunctionName = OtherFunctionName;
		}

		return *this;
	}
	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	TScriptDelegate& operator=(const TScriptDelegate<OtherThreadSafetyMode>& Other)
	{
		WeakPtrType OtherObject;
		FName OtherFunctionName;

		{
			typename TScriptDelegate<OtherThreadSafetyMode>::FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			OtherObject = Other.Object;
			OtherFunctionName = Other.FunctionName;
		}

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();
			Object = OtherObject;
			FunctionName = OtherFunctionName;
		}

		return *this;
	}

private:

	template <class UObjectTemplate>
	inline bool IsBound_Internal() const
	{
		if (FunctionName != NAME_None)
		{
			if (UObject* ObjectPtr = Object.Get())
			{
				return ((UObjectTemplate*)ObjectPtr)->FindFunction(FunctionName) != nullptr;
			}
		}

		return false;
	}

public:

	/**
	 * Binds a UFunction to this delegate.
	 *
	 * @param InObject The object to call the function on.
	 * @param InFunctionName The name of the function to call.
	 */
	void BindUFunction( UObject* InObject, const FName& InFunctionName )
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		Object = InObject;
		FunctionName = InFunctionName;
	}

	/** 
	 * Checks to see if the user object bound to this delegate is still valid
	 *
	 * @return  True if the object is still valid and it's safe to execute the function call
	 */
	inline bool IsBound() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
		return IsBound_Internal<UObject>();
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object.
	 *
	 * @return	True if this delegate is bound to InUserObject, false otherwise.
	 */
	inline bool IsBoundToObject(void const* InUserObject) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
		return InUserObject && (InUserObject == GetUObject());
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object, even if the object is unreachable.
	 *
	 * @return	True if this delegate is bound to InUserObject, false otherwise.
	 */
	bool IsBoundToObjectEvenIfUnreachable(void const* InUserObject) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
		return InUserObject && InUserObject == GetUObjectEvenIfUnreachable();
	}

	/** 
	 * Checks to see if the user object bound to this delegate will ever be valid again
	 *
	 * @return  True if the object is still valid and it's safe to execute the function call
	 */
	inline bool IsCompactable() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
		return FunctionName == NAME_None || !Object.Get(true);
	}

	/**
	 * Unbinds this delegate
	 */
	void Unbind()
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		Object = nullptr;
		FunctionName = NAME_None;
	}

	/**
	 * Unbinds this delegate (another name to provide a similar interface to TMulticastScriptDelegate)
	 */
	void Clear()
	{
		Unbind();
	}

	/**
	 * Converts this delegate to a string representation
	 *
	 * @return	Delegate in string format
	 */
	template <class UObjectTemplate>
	inline FString ToString() const
	{
		if( IsBound() )
		{
			FReadAccessScope ReadScope = GetReadAccessScope();
			return ((UObjectTemplate*)GetUObject())->GetPathName() + TEXT(".") + GetFunctionName().ToString();
		}
		return TEXT( "<Unbound>" );
	}

	/** Delegate serialization */
	friend FArchive& operator<<( FArchive& Ar, TScriptDelegate& D )
	{
		FReadAccessScope ReadScope = D.GetReadAccessScope();

		Ar << D.Object << D.FunctionName;
		return Ar;
	}

	/** Delegate serialization */
	friend void operator<<(FStructuredArchive::FSlot Slot, TScriptDelegate& D)
	{
		FReadAccessScope ReadScope = D.GetReadAccessScope();

		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << SA_VALUE(TEXT("Object"), D.Object) << SA_VALUE(TEXT("FunctionName"),D.FunctionName);
	}

	/** Comparison operators */
	FORCEINLINE bool operator==( const TScriptDelegate& Other ) const
	{
		WeakPtrType OtherObject;
		FName OtherFunctionName;

		{
			FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			OtherObject = Other.Object;
			OtherFunctionName = Other.FunctionName;
		}

		bool bResult;

		{
			FReadAccessScope ThisReadScope = GetReadAccessScope();
			bResult = Object == OtherObject && FunctionName == OtherFunctionName;
		}

		return bResult;
	}
	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	FORCEINLINE bool operator==(const TScriptDelegate<OtherThreadSafetyMode>& Other) const
	{
		WeakPtrType OtherObject;
		FName OtherFunctionName;

		{
			typename TScriptDelegate<OtherThreadSafetyMode>::FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			OtherObject = Other.Object;
			OtherFunctionName = Other.FunctionName;
		}

		bool bResult;

		{
			FReadAccessScope ThisReadScope = GetReadAccessScope();
			bResult = Object == OtherObject && FunctionName == OtherFunctionName;
		}

		return bResult;
	}

	FORCEINLINE bool operator!=( const TScriptDelegate& Other ) const
	{
		return !operator==(Other);
	}
	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	FORCEINLINE bool operator!=(const TScriptDelegate<OtherThreadSafetyMode>& Other) const
	{
		return !operator==(Other);
	}

	/** 
	 * Gets the object bound to this delegate
	 *
	 * @return	The object
	 */
	UObject* GetUObject()
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		// Downcast UObjectBase to UObject
		return static_cast< UObject* >( Object.Get() );
	}

	/**
	 * Gets the object bound to this delegate (const)
	 *
	 * @return	The object
	 */
	const UObject* GetUObject() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		// Downcast UObjectBase to UObject
		return static_cast< const UObject* >( Object.Get() );
	}

	/** 
	 * Gets the object bound to this delegate, even if the object is unreachable
	 *
	 * @return	The object
	 */
	UObject* GetUObjectEvenIfUnreachable()
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		// Downcast UObjectBase to UObject
		return static_cast< UObject* >( Object.GetEvenIfUnreachable() );
	}

	/**
	 * Gets the object bound to this delegate (const), even if the object is unreachable
	 *
	 * @return	The object
	 */
	const UObject* GetUObjectEvenIfUnreachable() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		// Downcast UObjectBase to UObject
		return static_cast< const UObject* >( Object.GetEvenIfUnreachable() );
	}

	WeakPtrType& GetUObjectRef()
	{
		return Object;
	}

	/**
	 * Gets the name of the function to call on the bound object
	 *
	 * @return	Function name
	 */
	FName GetFunctionName() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
		return FunctionName;
	}

	/**
	 * Executes a delegate by calling the named function on the object bound to the delegate.  You should
	 * always first verify that the delegate is safe to execute by calling IsBound() before calling this function.
	 * In general, you should never call this function directly.  Instead, call Execute() on a derived class.
	 *
	 * @param	Parameters		Parameter structure
	 */
	//CORE_API void ProcessDelegate(void* Parameters) const;
	template <class UObjectTemplate>
	void ProcessDelegate( void* Parameters ) const
	{
		UObjectTemplate* ObjectPtr;
		UFunction* Function;

		{	// to avoid MT access check if the delegate is deleted from inside of its callback, we don't cover the callback execution
			// by access protection scope
			// the `const` on the method is a lie
			FWriteAccessScope WriteScope = const_cast<TScriptDelegate*>(this)->GetWriteAccessScope();

			checkf( Object.IsValid() != false, TEXT( "ProcessDelegate() called with no object bound to delegate!" ) );
			checkf( FunctionName != NAME_None, TEXT( "ProcessDelegate() called with no function name set!" ) );

			// Object was pending kill, so we cannot execute the delegate.  Note that it's important to assert
			// here and not simply continue execution, as memory may be left uninitialized if the delegate is
			// not able to execute, resulting in much harder-to-detect code errors.  Users should always make
			// sure IsBound() returns true before calling ProcessDelegate()!
			ObjectPtr = static_cast<UObjectTemplate*>(Object.Get());	// Down-cast
			checkSlow( IsValid(ObjectPtr) );

			// Object *must* implement the specified function
			Function = ObjectPtr->FindFunctionChecked(FunctionName);
		}

		// Execute the delegate!
		ObjectPtr->ProcessEvent(Function, Parameters);
	}

	friend uint32 GetTypeHash(const TScriptDelegate& Delegate)
	{
		FReadAccessScope ReadScope = Delegate.GetReadAccessScope();

		return HashCombine(GetTypeHash(Delegate.Object), GetTypeHash(Delegate.GetFunctionName()));
	}

	template<typename OtherThreadSafetyMode>
	static TScriptDelegate CopyFrom(const TScriptDelegate<OtherThreadSafetyMode>& Other)
	{
		static_assert(std::is_same_v<ThreadSafetyMode, typename UE::Core::Private::TScriptDelegateTraits<ThreadSafetyMode>::UnicastThreadSafetyModeForMulticasts>);

		typename TScriptDelegate<OtherThreadSafetyMode>::FReadAccessScope OtherReadScope = Other.GetReadAccessScope();

		TScriptDelegate Copy;
		Copy.Object = Other.Object;
		Copy.FunctionName = Other.FunctionName;

		return Copy;
	}

protected:

	/** The object bound to this delegate, or nullptr if no object is bound */
	WeakPtrType Object;

	/** Name of the function to call on the bound object */
	FName FunctionName;

	// 
	friend class FCallDelegateHelper;

	friend struct TIsZeroConstructType<TScriptDelegate>;
};

template<typename ThreadSafetyMode>
struct TIsZeroConstructType<TScriptDelegate<ThreadSafetyMode>>
{
	static constexpr bool Value = 
		TIsZeroConstructType<typename UE::Core::Private::TScriptDelegateTraits<ThreadSafetyMode>::WeakPtrType>::Value &&
		TIsZeroConstructType<typename TScriptDelegate<ThreadSafetyMode>::Super>::Value;
};

/**
 * Script multi-cast delegate base class
 */
template <typename InThreadSafetyMode>
class TMulticastScriptDelegate : public TDelegateAccessHandlerBase<typename UE::Core::Private::TScriptDelegateTraits<InThreadSafetyMode>::ThreadSafetyMode>
{
private:
	using Super = TDelegateAccessHandlerBase<InThreadSafetyMode>;
	using typename Super::FReadAccessScope;
	using Super::GetReadAccessScope;
	using typename Super::FWriteAccessScope;
	using Super::GetWriteAccessScope;

	using UnicastDelegateType = TScriptDelegate<typename UE::Core::Private::TScriptDelegateTraits<InThreadSafetyMode>::UnicastThreadSafetyModeForMulticasts>;

public:
	using ThreadSafetyMode = typename UE::Core::Private::TScriptDelegateTraits<InThreadSafetyMode>::ThreadSafetyMode;
	using InvocationListType = TArray<UnicastDelegateType>;

	TMulticastScriptDelegate() = default;

	TMulticastScriptDelegate(const TMulticastScriptDelegate& Other)
	{
		InvocationListType LocalCopy;

		{
			FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			LocalCopy = Other.InvocationList;
		}

		InvocationList = MoveTemp(LocalCopy);
	}

	TMulticastScriptDelegate& operator=(const TMulticastScriptDelegate& Other)
	{
		InvocationListType LocalCopy;
		{
			FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			LocalCopy = Other.InvocationList;
		}

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();
			InvocationList = MoveTemp(LocalCopy);
		}

		return *this;
	}

	TMulticastScriptDelegate(TMulticastScriptDelegate&& Other)
	{
		InvocationListType LocalStorage;

		{
			FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			LocalStorage = MoveTemp(Other.InvocationList);
		}

		InvocationList = MoveTemp(LocalStorage);
	}

	TMulticastScriptDelegate& operator=(TMulticastScriptDelegate&& Other)
	{
		InvocationListType LocalStorage;

		{
			FReadAccessScope OtherReadScope = Other.GetReadAccessScope();
			LocalStorage = MoveTemp(Other.InvocationList);
		}

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();
			InvocationList = MoveTemp(LocalStorage);
		}

		return *this;
	}

public:

	/**
	 * Checks to see if any functions are bound to this multi-cast delegate
	 *
	 * @return	True if any functions are bound
	 */
	inline bool IsBound() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		return InvocationList.Num() > 0;
	}

	/**
	 * Checks whether a function delegate is already a member of this multi-cast delegate's invocation list
	 *
	 * @param	InDelegate	Delegate to check
	 * @return	True if the delegate is already in the list.
	 */
	bool Contains( const TScriptDelegate<ThreadSafetyMode>& InDelegate ) const
	{
		const UObject* Object;
		FName FunctionName;

		{
			FReadAccessScope OtherReadScope = InDelegate.GetReadAccessScope();
			Object = InDelegate.Object.Get();
			FunctionName = InDelegate.FunctionName;
		}

		return Contains(Object, FunctionName);
	}
	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	bool Contains(const TScriptDelegate<OtherThreadSafetyMode>& InDelegate) const
	{
		const UObject* Object;
		FName FunctionName;

		{
			typename TScriptDelegate<OtherThreadSafetyMode>::FReadAccessScope OtherReadScope = InDelegate.GetReadAccessScope();
			Object = InDelegate.Object.Get();
			FunctionName = InDelegate.FunctionName;
		}

		return Contains(Object, FunctionName);
	}

	/**
	 * Checks whether a function delegate is already a member of this multi-cast delegate's invocation list
	 *
	 * @param	InObject		Object of the delegate to check
	 * @param	InFunctionName	Function name of the delegate to check
	 * @return	True if the delegate is already in the list.
	 */
	bool Contains( const UObject* InObject, FName InFunctionName ) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		return InvocationList.ContainsByPredicate( [=]( const UnicastDelegateType& Delegate ){
			return Delegate.GetFunctionName() == InFunctionName && Delegate.IsBoundToObjectEvenIfUnreachable(InObject);
		} );
	}

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list
	 *
	 * @param	InDelegate	Delegate to add
	 */
	void Add( const TScriptDelegate<ThreadSafetyMode>& InDelegate )
	{
		UnicastDelegateType LocalCopy = UnicastDelegateType::CopyFrom(InDelegate);

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();

			// First check for any objects that may have expired
			CompactInvocationList();

			// Add the delegate
			AddInternal(MoveTemp(LocalCopy));
		}
	}
	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	void Add(const TScriptDelegate<OtherThreadSafetyMode>& InDelegate)
	{
		UnicastDelegateType LocalCopy = UnicastDelegateType::CopyFrom(InDelegate);

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();

			// First check for any objects that may have expired
			CompactInvocationList();

			// Add the delegate
			AddInternal(MoveTemp(LocalCopy));
		}
	}

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list if a delegate with the same signature
	 * doesn't already exist in the invocation list
	 *
	 * @param	InDelegate	Delegate to add
	 */
	void AddUnique( const TScriptDelegate<ThreadSafetyMode>& InDelegate )
	{
		UnicastDelegateType LocalCopy = UnicastDelegateType::CopyFrom(InDelegate);

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();

			// Add the delegate, if possible
			AddUniqueInternal(MoveTemp(LocalCopy));

			// Then check for any objects that may have expired
			CompactInvocationList();
		}
	}
	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	void AddUnique(const TScriptDelegate<OtherThreadSafetyMode>& InDelegate)
	{
		UnicastDelegateType LocalCopy = UnicastDelegateType::CopyFrom(InDelegate);

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();

			// Add the delegate
			AddUniqueInternal(MoveTemp(LocalCopy));

			// Then check for any objects that may have expired
			CompactInvocationList();
		}
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InDelegate	Delegate to remove
	 */
	void Remove( const TScriptDelegate<ThreadSafetyMode>& InDelegate )
	{
		UnicastDelegateType LocalCopy = UnicastDelegateType::CopyFrom(InDelegate);

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();

			// Remove the delegate
			RemoveInternal(LocalCopy);

			// Check for any delegates that may have expired
			CompactInvocationList();
		}
	}
	template <
		typename OtherThreadSafetyMode
		UE_REQUIRES(UE::Core::Private::BackwardCompatibilityCheck<InThreadSafetyMode, OtherThreadSafetyMode>())
	>
	/* UE_DEPRECATED(5.3, "Deprecated - remove after TScriptDelegateTraits<FWeakObjectPtr> is removed") */
	void Remove(const TScriptDelegate<OtherThreadSafetyMode>& InDelegate)
	{
		UnicastDelegateType LocalCopy = UnicastDelegateType::CopyFrom(InDelegate);

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();

			// Remove the delegate
			RemoveInternal(LocalCopy);

			// Check for any delegates that may have expired
			CompactInvocationList();
		}
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InObject		Object of the delegate to remove
	 * @param	InFunctionName	Function name of the delegate to remove
	 */
	void Remove( const UObject* InObject, FName InFunctionName )
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		// Remove the delegate
		RemoveInternal( InObject, InFunctionName );

		// Check for any delegates that may have expired
		CompactInvocationList();
	}

	/**
	 * Removes all delegate bindings from this multicast delegate's
	 * invocation list that are bound to the specified object.
	 *
	 * This method also compacts the invocation list.
	 *
	 * @param InObject The object to remove bindings for.
	 */
	void RemoveAll(const UObject* Object)
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		for (int32 BindingIndex = InvocationList.Num() - 1; BindingIndex >= 0; --BindingIndex)
		{
			const UnicastDelegateType& Binding = InvocationList[BindingIndex];

			if (Binding.IsBoundToObject(Object) || Binding.IsCompactable())
			{
				InvocationList.RemoveAtSwap(BindingIndex);
			}
		}
	}

	/**
	 * Removes all functions from this delegate's invocation list
	 */
	void Clear()
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();
		InvocationList.Empty();
	}

	/**
	 * Converts this delegate to a string representation
	 *
	 * @return	Delegate in string format
	 */
	template <typename UObjectTemplate>
	inline FString ToString() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		if( IsBound() )
		{
			FString AllDelegatesString = TEXT( "[" );
			bool bAddComma = false;
			for( typename InvocationListType::TConstIterator CurDelegate( InvocationList ); CurDelegate; ++CurDelegate )
			{
				if (bAddComma)
				{
					AllDelegatesString += TEXT( ", " );
				}
				bAddComma = true;
				AllDelegatesString += CurDelegate->template ToString<UObjectTemplate>();
			}
			AllDelegatesString += TEXT( "]" );
			return AllDelegatesString;
		}
		return TEXT( "<Unbound>" );
	}

	/** Multi-cast delegate serialization */
	friend FArchive& operator<<( FArchive& Ar, TMulticastScriptDelegate& D )
	{
		// Special case to avoid taking a lock on empty script delegate.
		// This is required for avoiding asserts on EmptyDelegate serialization.
		if (Ar.IsSaving() && D.InvocationList.Num() == 0)
		{
			FReadAccessScope ReadScope = D.GetReadAccessScope();

			Ar << D.InvocationList;
		}
		else
		{
			FWriteAccessScope WriteScope = D.GetWriteAccessScope();

			if( Ar.IsSaving() )
			{
				// When saving the delegate, clean up the list to make sure there are no bad object references
				D.CompactInvocationList();
			}

			Ar << D.InvocationList;

			if( Ar.IsLoading() )
			{
				// After loading the delegate, clean up the list to make sure there are no bad object references
				D.CompactInvocationList();
			}
		}

		return Ar;
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, TMulticastScriptDelegate& D)
	{
		FWriteAccessScope WriteScope = D.GetWriteAccessScope();

		FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

		if (UnderlyingArchive.IsSaving())
		{
			// When saving the delegate, clean up the list to make sure there are no bad object references
			D.CompactInvocationList();
		}

		Slot << D.InvocationList;

		if (UnderlyingArchive.IsLoading())
		{
			// After loading the delegate, clean up the list to make sure there are no bad object references
			D.CompactInvocationList();
		}
	}

	/**
	 * Executes a multi-cast delegate by calling all functions on objects bound to the delegate.  Always
	 * safe to call, even if when no objects are bound, or if objects have expired.  In general, you should
	 * never call this function directly.  Instead, call Broadcast() on a derived class.
	 *
	 * @param	Params				Parameter structure
	 */
	template <class UObjectTemplate>
	void ProcessMulticastDelegate(void* Parameters) const
	{
		// the `const` on the method is a lie
		FWriteAccessScope WriteScope = const_cast<TMulticastScriptDelegate*>(this)->GetWriteAccessScope();

		if( InvocationList.Num() > 0 )
		{
			// Create a copy of the invocation list, just in case the list is modified by one of the callbacks during the broadcast
			typedef TArray< UnicastDelegateType, TInlineAllocator< 4 > > FInlineInvocationList;
			FInlineInvocationList InvocationListCopy = FInlineInvocationList(InvocationList);
	
			// Invoke each bound function
			for( typename FInlineInvocationList::TConstIterator FunctionIt( InvocationListCopy ); FunctionIt; ++FunctionIt )
			{
				if( FunctionIt->IsBound() )
				{
					// Invoke this delegate!
					FunctionIt->template ProcessDelegate<UObjectTemplate>(Parameters);
				}
				else if ( FunctionIt->IsCompactable() )
				{
					// Function couldn't be executed, so remove it.  Note that because the original list could have been modified by one of the callbacks, we have to search for the function to remove here.
					RemoveInternal( *FunctionIt );
				}
			}
		}
	}

	/**
	 * Returns all objects associated with this multicast-delegate.
	 * For advanced uses only -- you should never need call this function in normal circumstances.
 	 * @return	List of objects bound to this delegate
	 */
	TArray< UObject* > GetAllObjects() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		TArray< UObject* > OutputList;
		for( typename InvocationListType::TIterator CurDelegate( InvocationList ); CurDelegate; ++CurDelegate )
		{
			UObject* CurObject = CurDelegate->GetUObject();
			if( CurObject != nullptr )
			{
				OutputList.Add( CurObject );
			}
		}
		return OutputList;
	}

	/**
	 * Returns all objects associated with this multicast-delegate, even if unreachable.
	 * For advanced uses only -- you should never need call this function in normal circumstances.
 	 * @return	List of objects bound to this delegate
	 */
	TArray< UObject* > GetAllObjectsEvenIfUnreachable() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();    
		TArray<UObject*> Result;
		for (auto* Ref : GetAllObjectRefsEvenIfUnreachable())
		{
			Result.Add(Ref->GetEvenIfUnreachable());
		}
		return Result;
	}
	
	TArray< typename UnicastDelegateType::WeakPtrType* > GetAllObjectRefsEvenIfUnreachable() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();        
		using WeakPtrType = typename UnicastDelegateType::WeakPtrType;
		TArray< WeakPtrType* > OutputList;
		for( typename InvocationListType::TIterator CurDelegate( InvocationList ); CurDelegate; ++CurDelegate )
		{
			WeakPtrType& CurObject = CurDelegate->GetUObjectRef();
			if( CurObject.GetEvenIfUnreachable() != nullptr )
			{
				OutputList.Add( &CurObject );
			}
		}
		return OutputList;
	}

	/**
	 * Returns the amount of memory allocated by this delegate's invocation list, not including sizeof(*this).
	 */
	SIZE_T GetAllocatedSize() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();
		return InvocationList.GetAllocatedSize();
	}

protected:

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list
	 *
	 * @param	InDelegate	Delegate to add
	*/
	void AddInternal(UnicastDelegateType&& InDelegate)
	{
#if DO_ENSURE
		// Verify same function isn't already bound
		const int32 NumFunctions = InvocationList.Num();
		for( int32 CurFunctionIndex = 0; CurFunctionIndex < NumFunctions; ++CurFunctionIndex )
		{
			(void)ensure( InvocationList[ CurFunctionIndex ] != InDelegate );
		}
#endif // DO_CHECK
		InvocationList.Add(MoveTemp(InDelegate));
	}

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list, if a delegate with that signature
	 * doesn't already exist
	 *
	 * @param	InDelegate	Delegate to add
	 */
	void AddUniqueInternal(UnicastDelegateType&& InDelegate)
	{
		// Add the item to the invocation list only if it is unique
		InvocationList.AddUnique(MoveTemp(InDelegate));
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InDelegate	Delegate to remove
	*/
	void RemoveInternal( const UnicastDelegateType& InDelegate ) const
	{
		InvocationList.RemoveSingleSwap(InDelegate);
	}

	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).  Note that the
	 * order of the delegates may not be preserved!
	 *
	 * @param	InObject		Object of the delegate to remove
	 * @param	InFunctionName	Function name of the delegate to remove
	*/
	void RemoveInternal( const UObject* InObject, FName InFunctionName ) const
	{
		int32 FoundDelegate = InvocationList.IndexOfByPredicate([=](const UnicastDelegateType& Delegate) {
			return Delegate.GetFunctionName() == InFunctionName && Delegate.IsBoundToObjectEvenIfUnreachable(InObject);
		});

		if (FoundDelegate != INDEX_NONE)
		{
			InvocationList.RemoveAtSwap(FoundDelegate, 1, EAllowShrinking::No);
		}
	}

	/** Cleans up any delegates in our invocation list that have expired (performance is O(N)) */
	void CompactInvocationList() const
	{
		InvocationList.RemoveAllSwap([](const UnicastDelegateType& Delegate){
			return Delegate.IsCompactable();
		});
	}

protected:

	/** Ordered list functions to invoke when the Broadcast function is called */
	mutable InvocationListType InvocationList;		// Mutable so that we can housekeep list even with 'const' broadcasts

	// Declare ourselves as a friend of FMulticastDelegateProperty so that it can access our function list
	friend class FMulticastDelegateProperty;
	friend class FMulticastInlineDelegateProperty;
	friend class FMulticastSparseDelegateProperty;

	// 
	friend class FCallDelegateHelper;

	friend struct TIsZeroConstructType<TMulticastScriptDelegate>;
};


template<typename ThreadSafetyMode> 
struct TIsZeroConstructType<TMulticastScriptDelegate<ThreadSafetyMode> >
{ 
	static constexpr bool Value = TIsZeroConstructType<typename TMulticastScriptDelegate<ThreadSafetyMode>::InvocationListType>::Value;
};
