// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/LosesQualifiersFromTo.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtrTemplatesFwd.h"

#include <type_traits>


struct FWeakObjectPtr;

/**
 * TWeakObjectPtr is the templated version of the generic FWeakObjectPtr
 */
template<class T, class TWeakObjectPtrBase>
struct TWeakObjectPtr : private TWeakObjectPtrBase
{
	friend struct FFieldPath;

	// Although templated, these parameters are not intended to be anything other than the default,
	// and are only templates for module organization reasons.
	static_assert(std::is_same_v<TWeakObjectPtrBase*, FWeakObjectPtr*>, "TWeakObjectPtrBase should not be overridden");

public:
	using ElementType = T;
	
	TWeakObjectPtr() = default;
	TWeakObjectPtr(const TWeakObjectPtr&) = default;
	TWeakObjectPtr& operator=(const TWeakObjectPtr&) = default;
	~TWeakObjectPtr() = default;

	/**
	 * Construct from a null pointer
	 */
	FORCEINLINE TWeakObjectPtr(TYPE_OF_NULLPTR) :
		TWeakObjectPtrBase((UObject*)nullptr)
	{
	}

	/**
	 * Construct from an object pointer
	 * @param Object object to create a weak pointer to
	 */
	template <
		typename U,
		typename = decltype(ImplicitConv<T*>(std::declval<U>()))
	>
	FORCEINLINE TWeakObjectPtr(U Object) :
		TWeakObjectPtrBase((const UObject*)Object)
	{
		// This static assert is in here rather than in the body of the class because we want
		// to be able to define TWeakObjectPtr<UUndefinedClass>.
		static_assert(std::is_convertible_v<T*, const volatile UObject*>, "TWeakObjectPtr can only be constructed with UObject types");
	}

	/**
	 * Construct from another weak pointer of another type, intended for derived-to-base conversions
	 * @param Other weak pointer to copy from
	 */
	template <
		typename OtherT,
		typename = decltype(ImplicitConv<T*>((OtherT*)nullptr))
	>
	FORCEINLINE TWeakObjectPtr(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other) :
		TWeakObjectPtrBase(*(TWeakObjectPtrBase*)&Other) // we do a C-style cast to private base here to avoid clang 3.6.0 compilation problems with friend declarations
	{
	}

	/**
	 * Reset the weak pointer back to the null state
	 */
	FORCEINLINE void Reset()
	{
		TWeakObjectPtrBase::Reset();
	}

	/**  
	 * Copy from an object pointer
	 * @param Object object to create a weak pointer to
	 */
	template <
		typename U
		UE_REQUIRES(!TLosesQualifiersFromTo<U, T>::Value)
	>
	FORCEINLINE TWeakObjectPtr& operator=(U* Object)
	{
		T* TempObject = Object;
		TWeakObjectPtrBase::operator=(TempObject);
		return *this;
	}

	/**  
	 * Assign from another weak pointer, intended for derived-to-base conversions
	 * @param Other weak pointer to copy from
	 */
	template <
		typename OtherT,
		typename = decltype(ImplicitConv<T*>((OtherT*)nullptr))
	>
	FORCEINLINE TWeakObjectPtr& operator=(const TWeakObjectPtr<OtherT, TWeakObjectPtrBase>& Other)
	{
		*(TWeakObjectPtrBase*)this = *(TWeakObjectPtrBase*)&Other; // we do a C-style cast to private base here to avoid clang 3.6.0 compilation problems with friend declarations

		return *this;
	}

	/**  
	 * Dereference the weak pointer
	 * @param bEvenIfPendingKill if this is true, pendingkill objects are considered valid
	 * @return nullptr if this object is gone or the weak pointer is explicitly null, otherwise a valid uobject pointer
	 */
	FORCEINLINE T* Get(bool bEvenIfPendingKill) const
	{
		return (T*)TWeakObjectPtrBase::Get(bEvenIfPendingKill);
	}

	/**  
	 * Dereference the weak pointer. This is an optimized version implying bEvenIfPendingKill=false.
	 */
	FORCEINLINE T* Get(/*bool bEvenIfPendingKill = false*/) const
	{
		return (T*)TWeakObjectPtrBase::Get();
	}

	/** Deferences the weak pointer even if its marked RF_Unreachable. This is needed to resolve weak pointers during GC (such as ::AddReferenceObjects) */
	FORCEINLINE T* GetEvenIfUnreachable() const
	{
		return (T*)TWeakObjectPtrBase::GetEvenIfUnreachable();
	}

	/**  
	 * Dereference the weak pointer
	 */
	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/**  
	 * Dereference the weak pointer
	 */
	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	// This is explicitly not added to avoid resolving weak pointers too often - use Get() once in a function.
	explicit operator bool() const = delete;

	/**  
	 * Test if this points to a live UObject.
	 * This should be done only when needed as excess resolution of the underlying pointer can cause performance issues.
	 * 
	 * @param bEvenIfPendingKill if this is true, pendingkill objects are considered valid
	 * @param bThreadsafeTest if true then function will just give you information whether referenced
	 *							UObject is gone forever (return false) or if it is still there (return true, no object flags checked).
	 *							This is required as without it IsValid can return false during the mark phase of the GC
	 *							due to the presence of the Unreachable flag.
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid(bool bEvenIfPendingKill, bool bThreadsafeTest = false) const
	{
		return TWeakObjectPtrBase::IsValid(bEvenIfPendingKill, bThreadsafeTest);
	}

	/**
	 * Test if this points to a live UObject. This is an optimized version implying bEvenIfPendingKill=false, bThreadsafeTest=false.
	 * This should be done only when needed as excess resolution of the underlying pointer can cause performance issues.
	 * Note that IsValid can not be used on another thread as it will incorrectly return false during the mark phase of the GC
	 * due to the Unreachable flag being set. (see bThreadsafeTest above)
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid(/*bool bEvenIfPendingKill = false, bool bThreadsafeTest = false*/) const
	{
		return TWeakObjectPtrBase::IsValid();
	}

	/**  
	 * Slightly different than !IsValid(), returns true if this used to point to a UObject, but doesn't any more and has not been assigned or reset in the mean time.
	 * @param bIncludingIfPendingKill if this is true, pendingkill objects are considered stale
	 * @param bThreadsafeTest set it to true when testing outside of Game Thread. Results in false if WeakObjPtr point to an existing object (no flags checked)
	 * @return true if this used to point at a real object but no longer does.
	 */
	FORCEINLINE bool IsStale(bool bIncludingIfPendingKill = true, bool bThreadsafeTest = false) const
	{
		return TWeakObjectPtrBase::IsStale(bIncludingIfPendingKill, bThreadsafeTest);
	}
	
	/**
	 * Returns true if this pointer was explicitly assigned to null, was reset, or was never initialized.
	 * If this returns true, IsValid() and IsStale() will both return false.
	 */
	FORCEINLINE bool IsExplicitlyNull() const
	{
		return TWeakObjectPtrBase::IsExplicitlyNull();
	}

	/**
	 * Returns true if two weak pointers were originally set to the same object, even if they are now stale
	 * @param Other weak pointer to compare to
	 */
	FORCEINLINE bool HasSameIndexAndSerialNumber(const TWeakObjectPtr& Other) const
	{
		return static_cast<const TWeakObjectPtrBase&>(*this).HasSameIndexAndSerialNumber(static_cast<const TWeakObjectPtrBase&>(Other));
	}

	/**
	 * Weak object pointer serialization, this forwards to FArchive::operator<<(struct FWeakObjectPtr&) or an override
	 */
	FORCEINLINE	void Serialize(FArchive& Ar)
	{
		Ar << static_cast<TWeakObjectPtrBase&>(*this);
	}

	/** Hash function. */
	FORCEINLINE uint32 GetWeakPtrTypeHash() const
	{
		return static_cast<const TWeakObjectPtrBase&>(*this).GetTypeHash();
	}

	/**
	 * Compare weak pointers for equality.
	 * If both pointers would return nullptr from Get() they count as equal even if they were not initialized to the same object.
	 * @param Other weak pointer to compare to
	 */
	template <typename RhsT, typename = decltype((T*)nullptr == (RhsT*)nullptr)>
	FORCENOINLINE bool operator==(const TWeakObjectPtr<RhsT, TWeakObjectPtrBase>& Rhs) const
	{
		return (const TWeakObjectPtrBase&)*this == (const TWeakObjectPtrBase&)Rhs;
	}

	template <typename RhsT, typename = decltype((T*)nullptr == (RhsT*)nullptr)>
	FORCENOINLINE bool operator==(const RhsT* Rhs) const
	{
		// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
		return (const TWeakObjectPtrBase&)*this == TWeakObjectPtrBase(Rhs);
	}

	FORCENOINLINE bool operator==(TYPE_OF_NULLPTR) const
	{
		return !IsValid();
	}

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
	/**
	 * Compare weak pointers for inequality
	 * @param Other weak pointer to compare to
	 */
	template <typename RhsT, typename = decltype((T*)nullptr != (RhsT*)nullptr)>
	FORCENOINLINE bool operator!=(const TWeakObjectPtr<RhsT, TWeakObjectPtrBase>& Rhs) const
	{
		return (const TWeakObjectPtrBase&)*this != (const TWeakObjectPtrBase&)Rhs;
	}

	template <typename RhsT, typename = decltype((T*)nullptr != (RhsT*)nullptr)>
	FORCENOINLINE bool operator!=(const RhsT* Rhs) const
	{
		// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
		return (const TWeakObjectPtrBase&)*this != TWeakObjectPtrBase(Rhs);
	}

	FORCENOINLINE bool operator!=(TYPE_OF_NULLPTR) const
	{
		return IsValid();
	}
#endif
};

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
/**
 * Compare weak pointers for equality.
 * If both pointers would return nullptr from Get() they count as equal even if they were not initialized to the same object.
 * @param Other weak pointer to compare to
 */
template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase, typename = decltype((LhsT*)nullptr == (RhsT*)nullptr)>
FORCENOINLINE bool operator==(const LhsT* Lhs, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
	return OtherTWeakObjectPtrBase(Lhs) == (const OtherTWeakObjectPtrBase&)Rhs;
}
template <typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator==(TYPE_OF_NULLPTR, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	return !Rhs.IsValid();
}

/**
 * Compare weak pointers for inequality
 * @param Other weak pointer to compare to
 */
template <typename LhsT, typename RhsT, typename OtherTWeakObjectPtrBase, typename = decltype((LhsT*)nullptr != (RhsT*)nullptr)>
FORCENOINLINE bool operator!=(const LhsT* Lhs, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	// NOTE: this constructs a TWeakObjectPtrBase, which has some amount of overhead, so this may not be an efficient operation
	return OtherTWeakObjectPtrBase(Lhs) != (const OtherTWeakObjectPtrBase&)Rhs;
}
template <typename RhsT, typename OtherTWeakObjectPtrBase>
FORCENOINLINE bool operator!=(TYPE_OF_NULLPTR, const TWeakObjectPtr<RhsT, OtherTWeakObjectPtrBase>& Rhs)
{
	return Rhs.IsValid();
}
#endif

// Helper function which deduces the type of the initializer
template <typename T>
FORCEINLINE TWeakObjectPtr<T> MakeWeakObjectPtr(T* Ptr)
{
	return TWeakObjectPtr<T>(Ptr);
}


/**
 * SetKeyFuncs for TWeakObjectPtrs which allow the key to become stale without invalidating the set.
 */
template <typename ElementType, bool bInAllowDuplicateKeys = false>
struct TWeakObjectPtrSetKeyFuncs : DefaultKeyFuncs<ElementType, bInAllowDuplicateKeys>
{
	typedef typename DefaultKeyFuncs<ElementType, bInAllowDuplicateKeys>::KeyInitType KeyInitType;

	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.HasSameIndexAndSerialNumber(B);
	}

	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

/**
 * MapKeyFuncs for TWeakObjectPtrs which allow the key to become stale without invalidating the map.
 */
template <typename KeyType, typename ValueType, bool bInAllowDuplicateKeys = false>
struct TWeakObjectPtrMapKeyFuncs : public TDefaultMapKeyFuncs<KeyType, ValueType, bInAllowDuplicateKeys>
{
	typedef typename TDefaultMapKeyFuncs<KeyType, ValueType, bInAllowDuplicateKeys>::KeyInitType KeyInitType;

	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A.HasSameIndexAndSerialNumber(B);
	}

	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

template <typename T>
struct TCallTraits<TWeakObjectPtr<T>> : public TCallTraitsBase<TWeakObjectPtr<T>>
{
	using ConstPointerType = TWeakObjectPtr<const T>;
};

/** Utility function to fill in a TArray<ClassName*> from a TArray<TWeakObjectPtr<ClassName>> */
template<typename DestArrayType, typename SourceArrayType>
void CopyFromWeakArray(DestArrayType& Dest, const SourceArrayType& Src)
{
	Dest.Empty(Src.Num());
	for (int32 Index = 0; Index < Src.Num(); Index++)
	{
		if (auto Value = Src[Index].Get())
		{
			Dest.Add(Value);
		}
	}
}

/** Hash function. */
template <typename T>
FORCEINLINE uint32 GetTypeHash(const TWeakObjectPtr<T>& WeakObjectPtr)
{
	return WeakObjectPtr.GetWeakPtrTypeHash();
}


/**
* Weak object pointer serialization, this forwards to FArchive::operator<<(struct FWeakObjectPtr&) or an override
*/
template<class T, class TWeakObjectPtrBase>
FArchive& operator<<( FArchive& Ar, TWeakObjectPtr<T, TWeakObjectPtrBase>& WeakObjectPtr )
{
	WeakObjectPtr.Serialize(Ar);
	return Ar;
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Templates/AndOrNot.h"
#include "Templates/IsPointer.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#endif
