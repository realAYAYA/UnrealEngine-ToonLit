// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SoftObjectPtr.h: Pointer to UObject asset, keeps extra information so that it is works even if the asset is not in memory
=============================================================================*/

#pragma once

#include "UObject/Object.h"
#include "Templates/Casts.h"
#include "UObject/PersistentObjectPtr.h"
#include "UObject/SoftObjectPath.h"

/**
 * TIsSoftObjectPointerType
 * Trait for recognizing 'soft' (path-based) object pointer types
 */
template<typename T> 
struct TIsSoftObjectPointerType
{ 
	enum { Value = false };
};

/**
 * FSoftObjectPtr is a type of weak pointer to a UObject, that also keeps track of the path to the object on disk.
 * It will change back and forth between being Valid and Pending as the referenced object loads or unloads.
 * It has no impact on if the object is garbage collected or not.
 *
 * This is useful to specify assets that you may want to asynchronously load on demand.
 */
struct FSoftObjectPtr : public TPersistentObjectPtr<FSoftObjectPath>
{
public:	
	FORCEINLINE FSoftObjectPtr() = default;
	FORCEINLINE FSoftObjectPtr(const FSoftObjectPtr& Other) = default;
	FORCEINLINE FSoftObjectPtr(FSoftObjectPtr&& Other) = default;
	FORCEINLINE ~FSoftObjectPtr() = default;
	FORCEINLINE FSoftObjectPtr& operator=(const FSoftObjectPtr& Other) = default;
	FORCEINLINE FSoftObjectPtr& operator=(FSoftObjectPtr&& Other) = default;

	explicit FORCEINLINE FSoftObjectPtr(const FSoftObjectPath& ObjectPath)
		: TPersistentObjectPtr<FSoftObjectPath>(ObjectPath)
	{
	}

	explicit FORCEINLINE FSoftObjectPtr(const UObject* Object)
	{
		(*this)=Object;
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	UObject* LoadSynchronous() const
	{
		UObject* Asset = Get();
		if (Asset == nullptr && !IsNull())
		{
			ToSoftObjectPath().TryLoad();
			
			// TryLoad will have loaded this pointer if it is valid
			Asset = Get();
		}
		return Asset;
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname */
	FORCEINLINE FString ToString() const
	{
		return ToSoftObjectPath().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToSoftObjectPath().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path. part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToSoftObjectPath().GetAssetName();
	}

#if WITH_EDITOR
	/** Overridden to deal with PIE lookups */
	FORCEINLINE UObject* Get() const
	{
		if (GPlayInEditorID != INDEX_NONE)
		{
			// Cannot use or set the cached value in PIE as it may affect other PIE instances or the editor
			TWeakObjectPtr<UObject> Result = GetUniqueID().ResolveObject();
			// If this object is pending kill or otherwise invalid, this will return nullptr just like TPersistentObjectPtr<FSoftObjectPath>::Get()
			return Result.Get();
		}
		return TPersistentObjectPtr<FSoftObjectPath>::Get();
	}
#endif

	using TPersistentObjectPtr<FSoftObjectPath>::operator=;
};

template <> struct TIsPODType<FSoftObjectPtr> { enum { Value = TIsPODType<TPersistentObjectPtr<FSoftObjectPath> >::Value }; };
template <> struct TIsWeakPointerType<FSoftObjectPtr> { enum { Value = TIsWeakPointerType<TPersistentObjectPtr<FSoftObjectPath> >::Value }; };
template <> struct TIsSoftObjectPointerType<FSoftObjectPtr> { enum { Value = true }; };

/**
 * TSoftObjectPtr is templatized wrapper of the generic FSoftObjectPtr, it can be used in UProperties
 */
template<class T=UObject>
struct TSoftObjectPtr
{
	template <class U>
	friend struct TSoftObjectPtr;

public:
	using ElementType = T;
	
	FORCEINLINE TSoftObjectPtr() = default;
	FORCEINLINE TSoftObjectPtr(const TSoftObjectPtr& Other) = default;
	FORCEINLINE TSoftObjectPtr(TSoftObjectPtr&& Other) = default;
	FORCEINLINE ~TSoftObjectPtr() = default;
	FORCEINLINE TSoftObjectPtr& operator=(const TSoftObjectPtr& Other) = default;
	FORCEINLINE TSoftObjectPtr& operator=(TSoftObjectPtr&& Other) = default;
	
	/** Construct from another soft pointer */
	template <class U, class = decltype(ImplicitConv<T*>((U*)nullptr))>
	FORCEINLINE TSoftObjectPtr(const TSoftObjectPtr<U>& Other)
		: SoftObjectPtr(Other.SoftObjectPtr)
	{
	}

	/** Construct from a moveable soft pointer */
	template <class U, class = decltype(ImplicitConv<T*>((U*)nullptr))>
	FORCEINLINE TSoftObjectPtr(TSoftObjectPtr<U>&& Other)
		: SoftObjectPtr(MoveTemp(Other.SoftObjectPtr))
	{
	}

	/** Construct from an object already in memory */
	template <typename U>
	FORCEINLINE TSoftObjectPtr(const U* Object)
		: SoftObjectPtr(Object)
	{
	}

	/** Construct from a TObjectPtr<U> which may or may not be in memory. */
	template <typename U>
	FORCEINLINE TSoftObjectPtr(const TObjectPtr<U> Object)
		: SoftObjectPtr(Object.Get())
	{
	}

	/** Construct from a nullptr */
	FORCEINLINE TSoftObjectPtr(TYPE_OF_NULLPTR)
		: SoftObjectPtr(nullptr)
	{
	}

	/** Construct from a soft object path */
	explicit FORCEINLINE TSoftObjectPtr(FSoftObjectPath ObjectPath)
		: SoftObjectPtr(MoveTemp(ObjectPath))
	{
	}

	/** Reset the soft pointer back to the null state */
	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** Resets the weak ptr only, call this when ObjectId may change */
	FORCEINLINE void ResetWeakPtr()
	{
		SoftObjectPtr.ResetWeakPtr();
	}

	/** Copy from an object already in memory */
	template <typename U>
	FORCEINLINE TSoftObjectPtr& operator=(const U* Object)
	{
		SoftObjectPtr = Object;
		return *this;
	}

	/** Copy from a TObjectPtr<U> which may or may not be in memory. */
	template <typename U>
	FORCEINLINE TSoftObjectPtr& operator=(const TObjectPtr<U> Object)
	{
		SoftObjectPtr = Object.Get();
		return *this;
	}

	/** Assign from a nullptr */
	FORCEINLINE TSoftObjectPtr& operator=(TYPE_OF_NULLPTR)
	{
		SoftObjectPtr = nullptr;
		return *this;
	}

	/** Copy from a soft object path */
	FORCEINLINE TSoftObjectPtr& operator=(FSoftObjectPath ObjectPath)
	{
		SoftObjectPtr = MoveTemp(ObjectPath);
		return *this;
	}

	/** Copy from a weak pointer to an object already in memory */
	template <class U, class = decltype(ImplicitConv<T*>((U*)nullptr))>
	FORCEINLINE TSoftObjectPtr& operator=(const TWeakObjectPtr<U>& Other)
	{
		SoftObjectPtr = Other;
		return *this;
	}

	/** Copy from another soft pointer */
	template <class U, class = decltype(ImplicitConv<T*>((U*)nullptr))>
	FORCEINLINE TSoftObjectPtr& operator=(TSoftObjectPtr<U> Other)
	{
		SoftObjectPtr = MoveTemp(Other.SoftObjectPtr);
		return *this;
	}

	/**
	 * Compare soft pointers for equality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE bool operator==(const TSoftObjectPtr& Rhs) const
	{
		return SoftObjectPtr == Rhs.SoftObjectPtr;
	}

	/**
	 * Compare soft pointers for equality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE bool operator==(TYPE_OF_NULLPTR) const
	{
		return SoftObjectPtr == nullptr;
	}

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
	/**
	 * Compare soft pointers for inequality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE bool operator!=(const TSoftObjectPtr& Rhs) const
	{
		return SoftObjectPtr != Rhs.SoftObjectPtr;
	}

	/**
	 * Compare soft pointers for inequality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE bool operator!=(TYPE_OF_NULLPTR) const
	{
		return SoftObjectPtr != nullptr;
	}
#endif

	/**
	 * Dereference the soft pointer.
	 *
	 * @return nullptr if this object is gone or the lazy pointer was null, otherwise a valid UObject pointer
	 */
	T* Get() const;

	/** Dereference the soft pointer */
	FORCEINLINE T& operator*() const
	{
		return *Get();
	}

	/** Dereference the soft pointer */
	FORCEINLINE T* operator->() const
	{
		return Get();
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	T* LoadSynchronous() const
	{
		UObject* Asset = SoftObjectPtr.LoadSynchronous();
		return Cast<T>(Asset);
	}

	/**  
	 * Test if this points to a live UObject
	 *
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		// This does the runtime type check
		return Get() != nullptr;
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 *
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return SoftObjectPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 *
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& GetUniqueID() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname */
	FORCEINLINE FString ToString() const
	{
		return ToSoftObjectPath().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToSoftObjectPath().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToSoftObjectPath().GetAssetName();
	}

	/** Dereference soft pointer to see if it points somewhere valid */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function */
	FORCEINLINE uint32 GetPtrTypeHash() const
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FSoftObjectPath>&>(SoftObjectPtr));
	}

	FORCEINLINE void Serialize(FArchive& Ar)
	{
		Ar << SoftObjectPtr;
	}

private:
	FSoftObjectPtr SoftObjectPtr;
};

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
template<class T>
FORCEINLINE bool operator==(TYPE_OF_NULLPTR, const TSoftObjectPtr<T>& Rhs)
{
	return Rhs == nullptr;
}

template<class T>
FORCEINLINE bool operator!=(TYPE_OF_NULLPTR, const TSoftObjectPtr<T>& Rhs)
{
	return Rhs != nullptr;
}

template<class T>
FORCEINLINE bool operator==(const T* Lhs, const TSoftObjectPtr<T>& Rhs)
{
	return Rhs == Lhs;
}

template<class T>
FORCEINLINE bool operator!=(const T* Lhs, const TSoftObjectPtr<T>& Rhs)
{
	return Rhs != Lhs;
}
#endif

/** Hash function */
template<class T>
FORCEINLINE uint32 GetTypeHash(const TSoftObjectPtr<T>& Ptr)
{
	return Ptr.GetPtrTypeHash();
}

template<class T>
FArchive& operator<<(FArchive& Ar, TSoftObjectPtr<T>& Ptr)
{
	Ptr.Serialize(Ar);
	return Ar;
}


template<class T> struct TIsPODType<TSoftObjectPtr<T> > { enum { Value = TIsPODType<FSoftObjectPtr>::Value }; };
template<class T> struct TIsWeakPointerType<TSoftObjectPtr<T> > { enum { Value = TIsWeakPointerType<FSoftObjectPtr>::Value }; };
template<class T> struct TIsSoftObjectPointerType<TSoftObjectPtr<T>> { enum { Value = TIsSoftObjectPointerType<FSoftObjectPtr>::Value }; };

template <typename T>
struct TCallTraits<TSoftObjectPtr<T>> : public TCallTraitsBase<TSoftObjectPtr<T>>
{
	using ConstPointerType = TSoftObjectPtr<const T>;
};

/** Utility to create a TSoftObjectPtr without specifying the type */
template <class T>
TSoftObjectPtr<std::remove_cv_t<T>> MakeSoftObjectPtr(T* Object)
{
	static_assert(std::is_base_of_v<UObject, T>, "Type must derive from UObject");
	return TSoftObjectPtr<std::remove_cv_t<T>>(Object);
}

template <class T>
TSoftObjectPtr<std::remove_cv_t<T>> MakeSoftObjectPtr(TObjectPtr<T> Object)
{
	static_assert(std::is_base_of_v<UObject, T>, "Type must derive from UObject");
	return TSoftObjectPtr<std::remove_cv_t<T>>(ToRawPtr(Object));
}

/**
 * TSoftClassPtr is a templatized wrapper around FSoftObjectPtr that works like a TSubclassOf, it can be used in UProperties for blueprint subclasses
 */
template<class TClass=UObject>
class TSoftClassPtr
{
	template <class TClassA>
	friend class TSoftClassPtr;

public:
	using ElementType = TClass;
	
	FORCEINLINE TSoftClassPtr() = default;
	FORCEINLINE TSoftClassPtr(const TSoftClassPtr& Other) = default;
	FORCEINLINE TSoftClassPtr(TSoftClassPtr&& Other) = default;
	FORCEINLINE ~TSoftClassPtr() = default;
	FORCEINLINE TSoftClassPtr& operator=(const TSoftClassPtr& Other) = default;
	FORCEINLINE TSoftClassPtr& operator=(TSoftClassPtr&& Other) = default;
		
	/** Construct from another soft pointer */
	template <class TClassA, class = decltype(ImplicitConv<TClass*>((TClassA*)nullptr))>
	FORCEINLINE TSoftClassPtr(const TSoftClassPtr<TClassA>& Other)
		: SoftObjectPtr(Other.SoftObjectPtr)
	{
	}

	/** Construct from a class already in memory */
	FORCEINLINE TSoftClassPtr(const UClass* From)
		: SoftObjectPtr(From)
	{
	}

	/** Construct from a soft object path */
	explicit FORCEINLINE TSoftClassPtr(const FSoftObjectPath& ObjectPath)
		: SoftObjectPtr(ObjectPath)
	{
	}

	/** Reset the soft pointer back to the null state */
	FORCEINLINE void Reset()
	{
		SoftObjectPtr.Reset();
	}

	/** Resets the weak ptr only, call this when ObjectId may change */
	FORCEINLINE void ResetWeakPtr()
	{
		SoftObjectPtr.ResetWeakPtr();
	}

	/** Copy from a class already in memory */
	FORCEINLINE void operator=(const UClass* From)
	{
		SoftObjectPtr = From;
	}

	/** Copy from a soft object path */
	FORCEINLINE void operator=(const FSoftObjectPath& ObjectPath)
	{
		SoftObjectPtr = ObjectPath;
	}

	/** Copy from a weak pointer already in memory */
	template<class TClassA, class = decltype(ImplicitConv<TClass*>((TClassA*)nullptr))>
	FORCEINLINE TSoftClassPtr& operator=(const TWeakObjectPtr<TClassA>& Other)
	{
		SoftObjectPtr = Other;
		return *this;
	}

	/** Copy from another soft pointer */
	template<class TClassA, class = decltype(ImplicitConv<TClass*>((TClassA*)nullptr))>
	FORCEINLINE TSoftClassPtr& operator=(const TSoftObjectPtr<TClassA>& Other)
	{
		SoftObjectPtr = Other.SoftObjectPtr;
		return *this;
	}

	/**  
	 * Compare soft pointers for equality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to 
	 */
	FORCEINLINE bool operator==(const TSoftClassPtr& Rhs) const
	{
		return SoftObjectPtr == Rhs.SoftObjectPtr;
	}
#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
	/**  
	 * Compare soft pointers for inequality
	 * Caution: Two soft pointers might not be equal to each other, but they both might return nullptr
	 *
	 * @param Other soft pointer to compare to
	 */
	FORCEINLINE bool operator!=(const TSoftClassPtr& Rhs) const
	{
		return SoftObjectPtr != Rhs.SoftObjectPtr;
	}
#endif

	/**  
	 * Dereference the soft pointer
	 *
	 * @return nullptr if this object is gone or the soft pointer was null, otherwise a valid UClass pointer
	 */
	FORCEINLINE UClass* Get() const
	{
		UClass* Class = dynamic_cast<UClass*>(SoftObjectPtr.Get());
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}

	/** Dereference the soft pointer */
	FORCEINLINE UClass& operator*() const
	{
		return *Get();
	}

	/** Dereference the soft pointer */
	FORCEINLINE UClass* operator->() const
	{
		return Get();
	}

	/**  
	 * Test if this points to a live UObject
	 *
	 * @return true if Get() would return a valid non-null pointer
	 */
	FORCEINLINE bool IsValid() const
	{
		// This also does the UClass type check
		return Get() != nullptr;
	}

	/**  
	 * Test if this does not point to a live UObject, but may in the future
	 *
	 * @return true if this does not point to a real object, but could possibly
	 */
	FORCEINLINE bool IsPending() const
	{
		return SoftObjectPtr.IsPending();
	}

	/**  
	 * Test if this can never point to a live UObject
	 *
	 * @return true if this is explicitly pointing to no object
	 */
	FORCEINLINE bool IsNull() const
	{
		return SoftObjectPtr.IsNull();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& GetUniqueID() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns the StringObjectPath that is wrapped by this SoftObjectPtr */
	FORCEINLINE const FSoftObjectPath& ToSoftObjectPath() const
	{
		return SoftObjectPtr.GetUniqueID();
	}

	/** Returns string representation of reference, in form /package/path.assetname  */
	FORCEINLINE FString ToString() const
	{
		return ToSoftObjectPath().ToString();
	}

	/** Returns /package/path string, leaving off the asset name */
	FORCEINLINE FString GetLongPackageName() const
	{
		return ToSoftObjectPath().GetLongPackageName();
	}
	
	/** Returns assetname string, leaving off the /package/path part */
	FORCEINLINE FString GetAssetName() const
	{
		return ToSoftObjectPath().GetAssetName();
	}

	/** Dereference soft pointer to see if it points somewhere valid */
	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	/** Hash function */
	FORCEINLINE uint32 GetPtrTypeHash() const
	{
		return GetTypeHash(static_cast<const TPersistentObjectPtr<FSoftObjectPath>&>(SoftObjectPtr));
	}

	/** Synchronously load (if necessary) and return the asset object represented by this asset ptr */
	UClass* LoadSynchronous() const
	{
		UObject* Asset = SoftObjectPtr.LoadSynchronous();
		UClass* Class = dynamic_cast<UClass*>(Asset);
		if (!Class || !Class->IsChildOf(TClass::StaticClass()))
		{
			return nullptr;
		}
		return Class;
	}

	inline void Serialize(FArchive& Ar)
	{
		Ar << static_cast<FSoftObjectPtr&>(SoftObjectPtr);
	}

private:
	FSoftObjectPtr SoftObjectPtr;
};

template <class T> struct TIsPODType<TSoftClassPtr<T> > { enum { Value = TIsPODType<FSoftObjectPtr>::Value }; };
template <class T> struct TIsWeakPointerType<TSoftClassPtr<T> > { enum { Value = TIsWeakPointerType<FSoftObjectPtr>::Value }; };

template <typename T>
struct TCallTraits<TSoftClassPtr<T>> : public TCallTraitsBase<TSoftClassPtr<T>>
{
	using ConstPointerType = TSoftClassPtr<const T>;
};

/** Utility to create a TSoftObjectPtr without specifying the type */
template <class T>
TSoftClassPtr<std::remove_cv_t<T>> MakeSoftClassPtr(T* Object)
{
	static_assert(std::is_base_of_v<UClass, T>, "Type must derive from UClass");
	return TSoftClassPtr<std::remove_cv_t<T>>(Object);
}

template <class T>
TSoftClassPtr<std::remove_cv_t<T>> MakeSoftClassPtr(TObjectPtr<T> Object)
{
	static_assert(std::is_base_of_v<UClass, T>, "Type must derive from UClass");
	return TSoftClassPtr<std::remove_cv_t<T>>(ToRawPtr(Object));
}

/** Fast non-alphabetical order that is only stable during this process' lifetime. */
struct FSoftObjectPtrFastLess : private FSoftObjectPathFastLess
{
	template <typename SoftObjectPtrType>
	bool operator()(const SoftObjectPtrType& Lhs, const SoftObjectPtrType& Rhs) const
	{
		return FSoftObjectPathFastLess::operator()(Lhs.ToSoftObjectPath(), Rhs.ToSoftObjectPath());
	}
};

/** Slow alphabetical order that is stable / deterministic over process runs. */
struct FSoftObjectPtrLexicalLess : private FSoftObjectPathLexicalLess
{
	template <typename SoftObjectPtrType>
	bool operator()(const SoftObjectPtrType& Lhs, const SoftObjectPtrType& Rhs) const
	{
		return FSoftObjectPathLexicalLess::operator()(Lhs.ToSoftObjectPath(), Rhs.ToSoftObjectPath());
	}
};

template<class T=UObject>
using TAssetPtr UE_DEPRECATED(5.0, "TAssetPtr was renamed to TSoftObjectPtr as it is not necessarily an asset") = TSoftObjectPtr<T>;

template<class TClass = UObject>
using TAssetSubclassOf UE_DEPRECATED(5.0, "TAssetSubclassOf was renamed to TSoftClassPtr") = TSoftClassPtr<TClass>;

/** Not directly inlined on purpose so compiler have the option of not inlining it. (and it also works with extern template) */
template<class T>
T* TSoftObjectPtr<T>::Get() const
{
	return dynamic_cast<T*>(SoftObjectPtr.Get());
}

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
template<class TClass>
FORCEINLINE bool operator==(const UClass* Lhs, const TSoftClassPtr<TClass>& Rhs)
{
	return Rhs == Lhs;
}

template<class TClass>
FORCEINLINE bool operator!=(const UClass* Lhs, const TSoftClassPtr<TClass>& Rhs)
{
	return Rhs != Lhs;
}
#endif

/** Hash function */
template<class TClass>
FORCEINLINE uint32 GetTypeHash(const TSoftClassPtr<TClass>& Ptr)
{
	return Ptr.GetPtrTypeHash();
}

template<class TClass>
FArchive& operator<<(FArchive& Ar, TSoftClassPtr<TClass>& Ptr)
{
	Ptr.Serialize(Ar);
	return Ar;
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
