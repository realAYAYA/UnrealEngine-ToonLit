// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LazyObjectPtr.h: Lazy, guid-based weak pointer to a UObject, mostly useful for actors
=============================================================================*/

#pragma once

#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "HAL/Platform.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/Guid.h"
#include "Serialization/Archive.h"
#include "Serialization/StructuredArchive.h"
#include "Templates/Casts.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Object.h"
#include "UObject/PersistentObjectPtr.h"

template <typename T> struct TIsPODType;
template <typename T> struct TIsWeakPointerType;

/**
 * Wrapper structure for a GUID that uniquely identifies registered UObjects.
 * The actual GUID is stored in an object annotation that is updated when a new reference is made.
 */
struct FUniqueObjectGuid
{
	FUniqueObjectGuid()
	{}

	FUniqueObjectGuid(const FGuid& InGuid)
		: Guid(InGuid)
	{}

	/** Reset the guid pointer back to the invalid state */
	FORCEINLINE void Reset()
	{
		Guid.Invalidate();
	}

	/** Construct from an existing object */
	COREUOBJECT_API explicit FUniqueObjectGuid(const class UObject* InObject);

	/** Converts into a string */
	COREUOBJECT_API FString ToString() const;

	/** Converts from a string */
	COREUOBJECT_API void FromString(const FString& From);

	/** Fixes up this UniqueObjectID to add or remove the PIE prefix depending on what is currently active */
	COREUOBJECT_API FUniqueObjectGuid FixupForPIE(int32 PlayInEditorID = GPlayInEditorID) const;

	/**
	 * Attempts to find a currently loaded object that matches this object ID
	 *
	 * @return Found UObject, or nullptr if not currently loaded
	 */
	COREUOBJECT_API class UObject *ResolveObject() const;

	/** Test if this can ever point to a live UObject */
	FORCEINLINE bool IsValid() const
	{
		return Guid.IsValid();
	}

	FORCEINLINE bool operator==(const FUniqueObjectGuid& Other) const
	{
		return Guid == Other.Guid;
	}

	FORCEINLINE bool operator!=(const FUniqueObjectGuid& Other) const
	{
		return Guid != Other.Guid;
	}

	/** Returns true is this is the default value */
	FORCEINLINE bool IsDefault() const
	{
		// A default GUID is 0,0,0,0 and this is "invalid"
		return !IsValid(); 
	}

	FORCEINLINE friend uint32 GetTypeHash(const FUniqueObjectGuid& ObjectGuid)
	{
		return GetTypeHash(ObjectGuid.Guid);
	}
	/** Returns wrapped Guid */
	FORCEINLINE const FGuid& GetGuid() const
	{
		return Guid;
	}

	friend FArchive& operator<<(FArchive& Ar,FUniqueObjectGuid& ObjectGuid)
	{
		Ar << ObjectGuid.Guid;
		return Ar;
	}

	friend void operator<<(FStructuredArchive::FSlot Slot, FUniqueObjectGuid& ObjectGuid)
	{
		Slot << ObjectGuid.Guid;
	}

	UE_DEPRECATED(5.4, "The current object tag is no longer used by TSoftObjectPtr, you can remove all calls")
	static int32 GetCurrentTag()
	{
		return 0;
	}
	UE_DEPRECATED(5.4, "The current object tag is no longer used by TSoftObjectPtr, you can remove all calls")
	static int32 InvalidateTag()
	{
		return 0;
	}

	static COREUOBJECT_API FUniqueObjectGuid GetOrCreateIDForObject(const class UObject *Object);

private:
	/** Guid representing the object, should be unique */
	FGuid Guid;
};

template<> struct TIsPODType<FUniqueObjectGuid> { enum { Value = true }; };

/**
 * FLazyObjectPtr is a type of weak pointer to a UObject that uses a GUID created at save time.
 * Objects will only have consistent GUIDs if they are referenced by a lazy pointer and then saved.
 * It will change back and forth between being valid or pending as the referenced object loads or unloads.
 * It has no impact on if the object is garbage collected or not.
 * It can't be directly used across a network.
 *
 * NOTE: Because this only stores a GUID, it does not know how to load the destination object and does not work with Play In Editor.
 * This will be deprecated in a future engine version and new features should use FSoftObjectPtr instead.
 */
struct FLazyObjectPtr : public TPersistentObjectPtr<FUniqueObjectGuid>
{
public:	
	/** Default constructor, sets to null */
	FORCEINLINE FLazyObjectPtr()
	{
	}

	/** Construct from object already in memory */
	explicit FORCEINLINE FLazyObjectPtr(const UObject* Object)
	{
		(*this)=Object;
	}
	
	/** Copy from an object already in memory */
	FORCEINLINE void operator=(const UObject *Object)
	{
		TPersistentObjectPtr<FUniqueObjectGuid>::operator=(Object);
	}

	/** Copy from a unique object identifier */
	FORCEINLINE void operator=(const FUniqueObjectGuid& InObjectID)
	{
		TPersistentObjectPtr<FUniqueObjectGuid>::operator=(InObjectID);
	}

	/** Fixes up this FLazyObjectPtr to target the right UID as set in PIEGuidMap, this only works for directly serialized pointers */
	FORCEINLINE void FixupForPIE(int32 PIEInstance)
	{
		*this = GetUniqueID().FixupForPIE(PIEInstance);
	}
	
	/** Called by UObject::Serialize so that we can save / load the Guid possibly associated with an object */
	COREUOBJECT_API static void PossiblySerializeObjectGuid(UObject* Object, FStructuredArchive::FRecord Record);

	/** Called when entering PIE to prepare it for PIE-specific fixups */
	COREUOBJECT_API static void ResetPIEFixups();
};

template <> struct TIsPODType<FLazyObjectPtr> { enum { Value = TIsPODType<TPersistentObjectPtr<FUniqueObjectGuid> >::Value }; };
template <> struct TIsWeakPointerType<FLazyObjectPtr> { enum { Value = TIsWeakPointerType<TPersistentObjectPtr<FUniqueObjectGuid> >::Value }; };

/**
 * TLazyObjectPtr is the templatized version of the generic FLazyObjectPtr.
 * NOTE: This will be deprecated in a future engine version and new features should use TSoftObjectPtr instead.
 */
template<class T=UObject>
struct TLazyObjectPtr : private FLazyObjectPtr
{
public:
	using ElementType = T;
	
	TLazyObjectPtr() = default;

	TLazyObjectPtr(TLazyObjectPtr<T>&&) = default;
	TLazyObjectPtr(const TLazyObjectPtr<T>&) = default;
	TLazyObjectPtr<T>& operator=(TLazyObjectPtr<T>&&) = default;
	TLazyObjectPtr<T>& operator=(const TLazyObjectPtr<T>&) = default;

	/** Construct from another lazy pointer with implicit upcasting allowed */
	template<typename U, typename = decltype(ImplicitConv<T*>((U*)nullptr))>
	FORCEINLINE TLazyObjectPtr(const TLazyObjectPtr<U>& Other) :
		FLazyObjectPtr((const FLazyObjectPtr&)Other)
	{
	}
	
	/** Assign from another lazy pointer with implicit upcasting allowed */
	template<typename U, typename = decltype(ImplicitConv<T*>((U*)nullptr))>
	FORCEINLINE TLazyObjectPtr<T>& operator=(const TLazyObjectPtr<U>& Other)
	{
		FLazyObjectPtr::operator=((const FLazyObjectPtr&)Other);
		return *this;
	}

	/** Construct from an object pointer */
	FORCEINLINE TLazyObjectPtr(T* Object)
	{
		FLazyObjectPtr::operator=(Object);
	}

	/** Reset the lazy pointer back to the null state */
	FORCEINLINE void Reset()
	{
		FLazyObjectPtr::Reset();
	}

	/** Copy from an object pointer */
	FORCEINLINE void operator=(T* Object)
	{
		FLazyObjectPtr::operator=(Object);
	}

	/**
	 * Copy from a unique object identifier
	 * WARNING: this doesn't check the type of the object is correct,
	 * because the object corresponding to this ID may not even be loaded!
	 *
	 * @param ObjectID Object identifier to create a lazy pointer to
	 */
	FORCEINLINE void operator=(const FUniqueObjectGuid& InObjectID)
	{
		FLazyObjectPtr::operator=(InObjectID);
	}

	/**
	 * Gets the unique object identifier associated with this lazy pointer. Valid even if pointer is not currently valid
	 *
	 * @return Unique ID for this object, or an invalid FUniqueObjectID if this pointer isn't set to anything
	 */
	FORCEINLINE const FUniqueObjectGuid& GetUniqueID() const
	{
		return FLazyObjectPtr::GetUniqueID();
	}

	/**
	 * Dereference the lazy pointer.
	 *
	 * @return nullptr if this object is gone or the lazy pointer was null, otherwise a valid UObject pointer
	 */
	FORCEINLINE T* Get() const
	{
		// there are cases where a TLazyObjectPtr can get an object of the wrong type assigned to it which are difficult to avoid
		// e.g. operator=(const FUniqueObjectGuid& ObjectID)
		// "WARNING: this doesn't check the type of the object is correct..."
		return dynamic_cast<T*>(FLazyObjectPtr::Get());
	}

	/** Dereference the lazy pointer */
	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/** Dereference the lazy pointer */
	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	/** Test if this points to a live UObject */
	FORCEINLINE bool IsValid() const
	{
		return FLazyObjectPtr::IsValid();
	}

	/**
	 * Slightly different than !IsValid(), returns true if this used to point to a UObject, but doesn't any more and has not been assigned or reset in the mean time.
	 *
	 * @return true if this used to point at a real object but no longer does.
	 */
	FORCEINLINE bool IsStale() const
	{
		return FLazyObjectPtr::IsStale();
	}

	/**
	 * Test if this does not point to a live UObject, but may in the future
	 *
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return FLazyObjectPtr::IsPending();
	}

	/**
	 * Test if this can never point to a live UObject
	 *
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return FLazyObjectPtr::IsNull();
	}

	/** Dereference lazy pointer to see if it points somewhere valid */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function. */
	FORCEINLINE uint32 GetLazyObjecPtrTypeHash() const
	{
		return GetTypeHash(static_cast<const FLazyObjectPtr&>(*this));
	}

	FORCEINLINE void SerializePtr(FArchive& Ar)
	{
		Ar << static_cast<FLazyObjectPtr&>(*this);
	}

	/** Compare with another TLazyObjectPtr of related type */
	template<typename U, typename = decltype((T*)nullptr == (U*)nullptr)>
	FORCEINLINE bool operator==(const TLazyObjectPtr<U>& Rhs) const
	{
		return (const FLazyObjectPtr&)*this == (const FLazyObjectPtr&)Rhs;
	}
	template<typename U, typename = decltype((T*)nullptr != (U*)nullptr)>
	FORCEINLINE bool operator!=(const TLazyObjectPtr<U>& Rhs) const
	{
		return (const FLazyObjectPtr&)*this != (const FLazyObjectPtr&)Rhs;
	}

	/** Compare for equality with a raw pointer **/
	template<typename U, typename = decltype((T*)nullptr == (U*)nullptr)>
	FORCEINLINE bool operator==(const U* Rhs) const
	{
		return Get() == Rhs;
	}

	/** Compare to null */
	FORCEINLINE bool operator==(TYPE_OF_NULLPTR) const
	{
		return !IsValid();
	}
	/** Compare for inequality with a raw pointer	**/
	template<typename U, typename = decltype((T*)nullptr != (U*)nullptr)>
	FORCEINLINE bool operator!=(const U* Rhs) const
	{
		return Get() != Rhs;
	}

	/** Compare for inequality with null **/
	FORCEINLINE bool operator!=(TYPE_OF_NULLPTR) const
	{
		return IsValid();
	}
};

/** Hash function. */
template<typename T>
FORCEINLINE uint32 GetTypeHash(const TLazyObjectPtr<T>& LazyObjectPtr)
{
	return LazyObjectPtr.GetLazyObjecPtrTypeHash();
}

template<typename T>
FArchive& operator<<(FArchive& Ar, TLazyObjectPtr<T>& LazyObjectPtr)
{
	LazyObjectPtr.SerializePtr(Ar);
	return Ar;
}

/** Compare for equality with a raw pointer **/
template<typename T, typename U, typename = decltype((T*)nullptr == (U*)nullptr)>
FORCEINLINE bool operator==(const U* Lhs, const TLazyObjectPtr<T>& Rhs)
{
	return Lhs == Rhs.Get();
}

/** Compare to null */
template<typename T>
FORCEINLINE bool operator==(TYPE_OF_NULLPTR, const TLazyObjectPtr<T>& Rhs)
{
	return !Rhs.IsValid();
}

/** Compare for inequality with a raw pointer	**/
template<typename T, typename U, typename = decltype((T*)nullptr != (U*)nullptr)>
FORCEINLINE bool operator!=(const U* Lhs, const TLazyObjectPtr<T>& Rhs)
{
	return Lhs != Rhs.Get();
}

/** Compare for inequality with null **/
template<typename T>
FORCEINLINE bool operator!=(TYPE_OF_NULLPTR, const TLazyObjectPtr<T>& Rhs)
{
	return Rhs.IsValid();
}

template<class T> struct TIsPODType<TLazyObjectPtr<T> > { enum { Value = TIsPODType<FLazyObjectPtr>::Value }; };
template<class T> struct TIsWeakPointerType<TLazyObjectPtr<T> > { enum { Value = TIsWeakPointerType<FLazyObjectPtr>::Value }; };

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
