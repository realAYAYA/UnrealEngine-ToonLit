// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "AutoRTFM/AutoRTFM.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/PreprocessorHelpers.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Build.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryLayout.h"
#include "Templates/TypeHash.h"
#include <atomic>
#include <type_traits>

/** A virtual interface for ref counted objects to implement. */
class IRefCountedObject
{
public:
	virtual ~IRefCountedObject() { }
	virtual uint32 AddRef() const = 0;
	virtual uint32 Release() const = 0;
	virtual uint32 GetRefCount() const = 0;
};

/**
 * Base class implementing thread-safe reference counting.
 */
class FRefCountBase
{
public:
			FRefCountBase() = default;
	virtual ~FRefCountBase() = default;

	FRefCountBase(const FRefCountBase& Rhs) = delete;
	FRefCountBase& operator=(const FRefCountBase& Rhs) = delete;

	inline uint32 AddRef() const
	{
		return uint32(FPlatformAtomics::InterlockedIncrement(&NumRefs));
	}

	inline uint32 Release() const
	{
#if DO_GUARD_SLOW
		if (NumRefs == 0)
		{
			CheckRefCount();
		}
#endif

		const int32 Refs = FPlatformAtomics::InterlockedDecrement(&NumRefs);
		if (Refs == 0)
		{
			delete this;
		}

		return uint32(Refs);
	}

	uint32 GetRefCount() const
	{
		return uint32(NumRefs);
	}

private:
	mutable int32 NumRefs = 0;

	CORE_API void CheckRefCount() const;
};

/**
 * The base class of reference counted objects.
 *
 * This class should not be used for new code as it does not use atomic operations to update 
 * the reference count.
 *
 */
class FRefCountedObject
{
public:
	FRefCountedObject(): NumRefs(0) {}
	virtual ~FRefCountedObject() { check(!NumRefs); }
	FRefCountedObject(const FRefCountedObject& Rhs) = delete;
	FRefCountedObject& operator=(const FRefCountedObject& Rhs) = delete;
	uint32 AddRef() const
	{
		return uint32(++NumRefs);
	}
	uint32 Release() const
	{
		uint32 Refs = uint32(--NumRefs);
		if(Refs == 0)
		{
			delete this;
		}
		return Refs;
	}
	uint32 GetRefCount() const
	{
		return uint32(NumRefs);
	}
private:
	mutable int32 NumRefs;
};

/**
 * Like FRefCountedObject, but internal ref count is thread safe
 */
class FThreadSafeRefCountedObject
{
public:
	FThreadSafeRefCountedObject() : NumRefs(0) {}
	virtual ~FThreadSafeRefCountedObject() { check(NumRefs.GetValue() == 0); }
	FThreadSafeRefCountedObject(const FThreadSafeRefCountedObject& Rhs) = delete;
	FThreadSafeRefCountedObject& operator=(const FThreadSafeRefCountedObject& Rhs) = delete;
	uint32 AddRef() const
	{
		return uint32(NumRefs.Increment());
	}
	uint32 Release() const
	{
		uint32 Refs = uint32(NumRefs.Decrement());
		if (Refs == 0)
		{
			delete this;
		}
		return Refs;
	}
	uint32 GetRefCount() const
	{
		return uint32(NumRefs.GetValue());
	}
private:
	mutable FThreadSafeCounter NumRefs;
};

/**
 * ERefCountingMode is used select between either 'fast' or 'thread safe' ref-counting types.
 * This is only used by templates at compile time to generate one code path or another.
 */
enum class ERefCountingMode : uint8
{
	/** Forced to be not thread-safe. */
	NotThreadSafe = 0,

	/** Thread-safe, never spin locks, but slower */
	ThreadSafe = 1
};

/**
 * Ref-counting mixin, designed to add ref-counting to an object without requiring a virtual destructor.
 * Implements support for AutoRTFM, is thread-safe by default, and can support custom deleters via T::StaticDestroyObject.
 * 
 * @note AutoRTFM means that the return value of AddRef/Release is nonsense (as the ref-count doesn't change until the
 *       transaction is committed), but this is fine for use with TRefCountPtr (as it doesn't use those return values).
 * 
 * Basic Example:
 *  struct FMyRefCountedObject : public TRefCountingMixin<FMyRefCountedObject>
 *  {
 *      // ...
 *  };
 * 
 * Deleter Example:
 *  struct FMyRefCountedPooledObject : public TRefCountingMixin<FMyRefCountedPooledObject>
 *  {
 *      static void StaticDestroyObject(const FMyRefCountedPooledObject* Obj)
 *      {
 *          GPool->ReturnToPool(Obj);
 *      }
 *  };
 */
template <typename T, ERefCountingMode Mode = ERefCountingMode::ThreadSafe>
class TRefCountingMixin
{
	using RefCountType = std::conditional_t<Mode == ERefCountingMode::ThreadSafe, std::atomic<uint32>, uint32>;

public:
	TRefCountingMixin() = default;

	TRefCountingMixin(const TRefCountingMixin&) = delete;
	TRefCountingMixin& operator=(const TRefCountingMixin&) = delete;

	uint32 AddRef() const
	{
		if constexpr (Mode == ERefCountingMode::ThreadSafe)
		{
			// Incrementing a reference count with relaxed ordering is always safe because no other action is taken
			// in response to the increment, so there's nothing to order with.

			AutoRTFM::OnCommit([this]
			{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
				// We do a regular SC increment here because it maps to an _InterlockedIncrement (lock inc).
				// The codegen for a relaxed fetch_add is actually much worse under MSVC (lock xadd).
				++RefCount;
#else
				RefCount.fetch_add(1, std::memory_order_relaxed);
#endif
			});
		}
		else
		{
			++RefCount;
		}

		// Note: TRefCountPtr doesn't use the return value
		return 0;
	}

	uint32 Release() const
	{
		if constexpr (Mode == ERefCountingMode::ThreadSafe)
		{
			AutoRTFM::OnCommit([this]
			{
				// std::memory_order_acq_rel is used here so that, if we do end up executing the destructor, it's not possible
				// for side effects from executing the destructor end up being visible before we've determined that the 
				// reference count is actually zero.

				uint32 OldCount = RefCount.fetch_sub(1, std::memory_order_acq_rel);
				checkSlow(OldCount > 0);
				if (OldCount == 1)
				{
					T::StaticDestroyObject(static_cast<const T*>(this));
				}
			});
		}
		else
		{
			checkSlow(RefCount > 0);

			if (--RefCount == 0)
			{
				T::StaticDestroyObject(static_cast<const T*>(this));
			}
		}

		// Note: TRefCountPtr doesn't use the return value
		return 0;
	}

	uint32 GetRefCount() const
	{
		if constexpr (Mode == ERefCountingMode::ThreadSafe)
		{
			// A 'live' reference count is unstable by nature and so there's no benefit
			// to try and enforce memory ordering around the reading of it.
			//
			// This is equivalent to https://en.cppreference.com/w/cpp/memory/shared_ptr/use_count

			uint32 Count = 0;

			UE_AUTORTFM_OPEN(
			{
				// This reference count may be accessed by multiple threads
				Count = RefCount.load(std::memory_order_relaxed);
			});

			return Count;
		}
		else
		{
			return RefCount;
		}
	}

	static void StaticDestroyObject(const T* Obj)
	{
		delete Obj;
	}

private:
	mutable RefCountType RefCount{0};
};

/**
 * A smart pointer to an object which implements AddRef/Release.
 */
template<typename ReferencedType>
class TRefCountPtr
{
	typedef ReferencedType* ReferenceType;

public:

	FORCEINLINE TRefCountPtr():
		Reference(nullptr)
	{ }

	TRefCountPtr(ReferencedType* InReference,bool bAddRef = true)
	{
		Reference = InReference;
		if(Reference && bAddRef)
		{
			Reference->AddRef();
		}
	}

	TRefCountPtr(const TRefCountPtr& Copy)
	{
		Reference = Copy.Reference;
		if(Reference)
		{
			Reference->AddRef();
		}
	}

	template<typename CopyReferencedType>
	explicit TRefCountPtr(const TRefCountPtr<CopyReferencedType>& Copy)
	{
		Reference = static_cast<ReferencedType*>(Copy.GetReference());
		if (Reference)
		{
			Reference->AddRef();
		}
	}

	FORCEINLINE TRefCountPtr(TRefCountPtr&& Move)
	{
		Reference = Move.Reference;
		Move.Reference = nullptr;
	}

	template<typename MoveReferencedType>
	explicit TRefCountPtr(TRefCountPtr<MoveReferencedType>&& Move)
	{
		Reference = static_cast<ReferencedType*>(Move.GetReference());
		Move.Reference = nullptr;
	}

	~TRefCountPtr()
	{
		if(Reference)
		{
			Reference->Release();
		}
	}

	TRefCountPtr& operator=(ReferencedType* InReference)
	{
		if (Reference != InReference)
		{
			// Call AddRef before Release, in case the new reference is the same as the old reference.
			ReferencedType* OldReference = Reference;
			Reference = InReference;
			if (Reference)
			{
				Reference->AddRef();
			}
			if (OldReference)
			{
				OldReference->Release();
			}
		}
		return *this;
	}

	FORCEINLINE TRefCountPtr& operator=(const TRefCountPtr& InPtr)
	{
		return *this = InPtr.Reference;
	}

	template<typename CopyReferencedType>
	FORCEINLINE TRefCountPtr& operator=(const TRefCountPtr<CopyReferencedType>& InPtr)
	{
		return *this = InPtr.GetReference();
	}

	TRefCountPtr& operator=(TRefCountPtr&& InPtr)
	{
		if (this != &InPtr)
		{
			ReferencedType* OldReference = Reference;
			Reference = InPtr.Reference;
			InPtr.Reference = nullptr;
			if(OldReference)
			{
				OldReference->Release();
			}
		}
		return *this;
	}

	template<typename MoveReferencedType>
	TRefCountPtr& operator=(TRefCountPtr<MoveReferencedType>&& InPtr)
	{
		// InPtr is a different type (or we would have called the other operator), so we need not test &InPtr != this
		ReferencedType* OldReference = Reference;
		Reference = InPtr.Reference;
		InPtr.Reference = nullptr;
		if (OldReference)
		{
			OldReference->Release();
		}
		return *this;
	}

	FORCEINLINE ReferencedType* operator->() const
	{
		return Reference;
	}

	FORCEINLINE operator ReferenceType() const
	{
		return Reference;
	}

	FORCEINLINE ReferencedType** GetInitReference()
	{
		*this = nullptr;
		return &Reference;
	}

	FORCEINLINE ReferencedType* GetReference() const
	{
		return Reference;
	}

	FORCEINLINE friend bool IsValidRef(const TRefCountPtr& InReference)
	{
		return InReference.Reference != nullptr;
	}

	FORCEINLINE bool IsValid() const
	{
		return Reference != nullptr;
	}

	FORCEINLINE void SafeRelease()
	{
		*this = nullptr;
	}

	uint32 GetRefCount()
	{
		uint32 Result = 0;
		if (Reference)
		{
			Result = Reference->GetRefCount();
			check(Result > 0); // you should never have a zero ref count if there is a live ref counted pointer (*this is live)
		}
		return Result;
	}

	FORCEINLINE void Swap(TRefCountPtr& InPtr) // this does not change the reference count, and so is faster
	{
		ReferencedType* OldReference = Reference;
		Reference = InPtr.Reference;
		InPtr.Reference = OldReference;
	}

	void Serialize(FArchive& Ar)
	{
		ReferenceType PtrReference = Reference;
		Ar << PtrReference;
		if(Ar.IsLoading())
		{
			*this = PtrReference;
		}
	}

private:

	ReferencedType* Reference;

	template <typename OtherType>
	friend class TRefCountPtr;

public:
	FORCEINLINE bool operator==(const TRefCountPtr& B) const
	{
		return GetReference() == B.GetReference();
	}

	FORCEINLINE bool operator==(ReferencedType* B) const
	{
		return GetReference() == B;
	}
};

ALIAS_TEMPLATE_TYPE_LAYOUT(template<typename T>, TRefCountPtr<T>, void*);

#if !PLATFORM_COMPILER_HAS_GENERATED_COMPARISON_OPERATORS
template<typename ReferencedType>
FORCEINLINE bool operator==(ReferencedType* A, const TRefCountPtr<ReferencedType>& B)
{
	return A == B.GetReference();
}
#endif

template<typename ReferencedType>
FORCEINLINE uint32 GetTypeHash(const TRefCountPtr<ReferencedType>& InPtr)
{
	return GetTypeHash(InPtr.GetReference());
}


template<typename ReferencedType>
FArchive& operator<<(FArchive& Ar,TRefCountPtr<ReferencedType>& Ptr)
{
	Ptr.Serialize(Ar);
	return Ar;
}

template <
	typename T,
	typename... TArgs
	UE_REQUIRES(!std::is_array_v<T>)
>
FORCEINLINE TRefCountPtr<T> MakeRefCount(TArgs&&... Args)
{
	return TRefCountPtr<T>(new T(Forward<TArgs>(Args)...));
}