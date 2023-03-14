// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystemTypes.h"

namespace UE
{
namespace MovieScene
{


/** Facade for any component data type */
template<typename T>
struct TComponentPtr
{
	/** Value type is either a T or const T& for read-only types, or T& for writeable types */
	using ValueType = typename TChooseClass<TIsConst<T>::Value, typename TCallTraits<T>::ParamType, T&>::Result;

	TComponentPtr()
		: ComponentPtr(nullptr)
	{}

	explicit TComponentPtr(T* InComponentPtr)
		: ComponentPtr(InComponentPtr)
	{}

	TComponentPtr(const TComponentPtr& RHS) = default;
	TComponentPtr& operator=(const TComponentPtr& RHS) = default;

	TComponentPtr(TComponentPtr&& RHS)
		: ComponentPtr(RHS.ComponentPtr)
	{
		RHS.ComponentPtr = nullptr;
	}

	TComponentPtr& operator=(TComponentPtr&& RHS)
	{
		ComponentPtr = RHS.ComponentPtr;
		RHS.ComponentPtr = nullptr;
		return *this;
	}

	FORCEINLINE explicit operator bool() const
	{
		return IsValid();
	}

	FORCEINLINE bool IsValid() const
	{
		return ComponentPtr != nullptr;
	}

	/** Explicitly convert this component data to its underlying pointer type */
	FORCEINLINE T* AsPtr() const
	{
		return ComponentPtr;
	}

	/** Retrieve this component data as an array view of the specified size (usually the size will be FEntityAllocation::Num()) */
	FORCEINLINE TArrayView<T> AsArray(int32 Num) const
	{
		return MakeArrayView(ComponentPtr, Num);
	}

	/** Retrieve a slice of this component data as an array view of the specified size and offset (usually the size will be FEntityAllocation::Num()) */
	FORCEINLINE TArrayView<T> Slice(int32 StartIndex, int32 Num) const
	{
		return MakeArrayView(ComponentPtr + StartIndex, Num);
	}

	FORCEINLINE T* operator->() const
	{
		return ComponentPtr;
	}

	FORCEINLINE ValueType operator*() const
	{
		return *ComponentPtr;
	}

	FORCEINLINE ValueType operator[](int32 Index) const
	{
		return ComponentPtr[Index];
	}

	FORCEINLINE operator T*() const
	{
		return ComponentPtr;
	}

protected:

	T* ComponentPtr;
};


/**
 * Typed write-lock for component data
 */
template<typename Accessor> struct TComponentLock;
template<typename LockType> struct TComponentLockMixin;

template<>
struct TComponentLockMixin<FScopedHeaderReadLock>
{
	TComponentLockMixin() = default;
	explicit TComponentLockMixin(const FComponentHeader* InHeader)
		: Lock(InHeader)
	{}

private:
	FScopedHeaderReadLock Lock;
};

template<>
struct TComponentLockMixin<FScopedHeaderWriteLock>
{
	TComponentLockMixin() = default;
	explicit TComponentLockMixin(const FComponentHeader* InHeader, FEntityAllocationWriteContext InWriteContext)
		: Lock(InHeader, InWriteContext)
	{}

private:
	FScopedHeaderWriteLock Lock;
};

struct FReadErased
{
	FReadErased(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
		: ComponentPtr(InHeader->Components + ComponentOffset*InHeader->Sizeof)
		, Sizeof(InHeader->Sizeof)
	{}

	FORCEINLINE const void* AsPtr() const
	{
		return ComponentPtr;
	}
	FORCEINLINE const void* operator[](int32 Index) const
	{
		return ComponentPtr + Sizeof*Index;
	}
	FORCEINLINE const void* ComponentAtIndex(int32 Index) const
	{
		return ComponentPtr + Sizeof*Index;
	}
	FORCEINLINE bool IsValid() const
	{
		return ComponentPtr != nullptr;
	}

protected:
	FReadErased()
		: ComponentPtr(nullptr)
		, Sizeof(0)
	{}

	const uint8* ComponentPtr;
	int32 Sizeof;
};

struct FReadErasedOptional : FReadErased
{
	FReadErasedOptional() = default;
	explicit FReadErasedOptional(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			ComponentPtr = InHeader->Components + ComponentOffset*InHeader->Sizeof;
			Sizeof = InHeader->Sizeof;
		}
	}
};

template<typename T>
struct TRead : TComponentPtr<const T>
{
	using ValueType = typename TCallTraits<T>::ParamType;

	TRead() = default;
	explicit TRead(const T* ComponentPtr)
		: TComponentPtr<const T>(ComponentPtr)
	{}
	explicit TRead(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
		: TComponentPtr<const T>(reinterpret_cast<const T*>(InHeader->Components) + ComponentOffset)
	{}
	ValueType ComponentAtIndex(int32 Index) const
	{
		return (*this)[Index];
	}

	UE_DEPRECATED(4.27, "It is no longer necessary to call Resolve - please change to using the TComponentPtr API.")
	const T* Resolve(const FEntityAllocation* Allocation) const
	{
		return this->AsPtr();
	}

	UE_DEPRECATED(4.27, "It is no longer necessary to call ResolveAsArray - please change to TComponentPtr::AsArray.")
	TArrayView<const T> ResolveAsArray(const FEntityAllocation* Allocation) const
	{
		return MakeArrayView(this->AsPtr(), Allocation->Num());
	}
};
using FReadEntityIDs = TRead<FMovieSceneEntityID>;

template<typename T>
struct TReadOptional : TComponentPtr<const T>
{
	TReadOptional() = default;
	explicit TReadOptional(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			this->ComponentPtr = reinterpret_cast<const T*>(InHeader->Components) + ComponentOffset;
		}
	}

	UE_DEPRECATED(4.27, "It is no longer necessary to call Resolve - please change to using the TComponentPtr API.")
	const T* Resolve(const FEntityAllocation* Allocation) const
	{
		return this->AsPtr();
	}

	UE_DEPRECATED(4.27, "It is no longer necessary to call ResolveAsArray - please change to TComponentPtr::AsArray.")
	TArrayView<const T> ResolveAsArray(const FEntityAllocation* Allocation) const
	{
		const T* Ptr = this->AsPtr();
		return MakeArrayView(Ptr, Ptr ? Allocation->Num() : 0);
	}
};


struct FWriteErased
{
	FWriteErased()
		: ComponentPtr(nullptr)
		, Sizeof(0)
	{}
	FWriteErased(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
		: ComponentPtr(InHeader->Components + ComponentOffset*InHeader->Sizeof)
		, Sizeof(InHeader->Sizeof)
	{}
	FORCEINLINE void* AsPtr() const
	{
		return ComponentPtr;
	}
	FORCEINLINE void* operator[](int32 Index) const
	{
		return ComponentPtr + Sizeof*Index;
	}
	FORCEINLINE bool IsValid() const
	{
		return ComponentPtr != nullptr;
	}

protected:
	uint8* ComponentPtr;
	int32 Sizeof;
};

struct FWriteErasedOptional : FWriteErased
{
	FWriteErasedOptional() = default;
	FWriteErasedOptional(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			ComponentPtr = InHeader->Components + ComponentOffset*InHeader->Sizeof;
			Sizeof = InHeader->Sizeof;
		}
	}
};

template<typename T>
struct TWrite : TComponentPtr<T>
{
	TWrite() = default;
	explicit TWrite(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
		: TComponentPtr<T>(reinterpret_cast<T*>(InHeader->Components) + ComponentOffset)
	{}

	UE_DEPRECATED(4.27, "It is no longer necessary to call Resolve - please change to using the TComponentPtr API.")
	T* Resolve(const FEntityAllocation* Allocation) const
	{
		return this->AsPtr();
	}

	UE_DEPRECATED(4.27, "It is no longer necessary to call ResolveAsArray - please change to TComponentPtr::AsArray.")
	TArrayView<T> ResolveAsArray(const FEntityAllocation* Allocation) const
	{
		return MakeArrayView(this->AsPtr(), Allocation->Num());
	}
};

template<typename T>
struct TWriteOptional : TComponentPtr<T>
{
	TWriteOptional() = default;
	explicit TWriteOptional(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			this->ComponentPtr = reinterpret_cast<T*>(InHeader->Components) + ComponentOffset;
		}
	}

	UE_DEPRECATED(4.27, "It is no longer necessary to call Resolve - please change to using the TComponentPtr API.")
	T* Resolve(const FEntityAllocation* Allocation) const
	{
		return this->AsPtr();
	}

	UE_DEPRECATED(4.27, "It is no longer necessary to call ResolveAsArray - please change to TComponentPtr::AsArray.")
	TArrayView<T> ResolveAsArray(const FEntityAllocation* Allocation) const
	{
		T* Ptr = this->AsPtr();
		return MakeArrayView(Ptr, Ptr ? Allocation->Num() : 0);
	}
};



template<typename AccessorType> struct TComponentLock;

template<>
struct TComponentLock<FReadErased> : TComponentLockMixin<FScopedHeaderReadLock>, FReadErased
{
	explicit TComponentLock(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
		: TComponentLockMixin<FScopedHeaderReadLock>(InHeader)
		, FReadErased(InHeader, ComponentOffset)
	{}

	const void* ComponentAtIndex(int32 Index) const
	{
		return (*this)[Index];
	}
};
template<>
struct TComponentLock<FReadErasedOptional> : TComponentLockMixin<FScopedHeaderReadLock>, FReadErasedOptional
{
	TComponentLock() = default;
	explicit TComponentLock(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			*static_cast<TComponentLockMixin<FScopedHeaderReadLock>*>(this) = TComponentLockMixin<FScopedHeaderReadLock>(InHeader);
			*static_cast<FReadErasedOptional*>(this) = FReadErasedOptional(InHeader, ComponentOffset);
		}
	}

	explicit operator bool() const
	{
		return this->ComponentPtr != nullptr;
	}

	const void* ComponentAtIndex(int32 Index) const
	{
		return this->ComponentPtr != nullptr ? (*this)[Index] : nullptr;
	}
};
template<>
struct TComponentLock<FWriteErased> : TComponentLockMixin<FScopedHeaderWriteLock>, FWriteErased
{
	explicit TComponentLock(const FComponentHeader* InHeader, FEntityAllocationWriteContext InWriteContext, int32 ComponentOffset = 0)
		: TComponentLockMixin<FScopedHeaderWriteLock>(InHeader, InWriteContext)
		, FWriteErased(InHeader, ComponentOffset)
	{}

	void* ComponentAtIndex(int32 Index) const
	{
		return (*this)[Index];
	}
};
template<>
struct TComponentLock<FWriteErasedOptional> : TComponentLockMixin<FScopedHeaderWriteLock>, FWriteErasedOptional
{
	TComponentLock() = default;
	explicit TComponentLock(const FComponentHeader* InHeader, FEntityAllocationWriteContext InWriteContext, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			*static_cast<TComponentLockMixin<FScopedHeaderWriteLock>*>(this) = TComponentLockMixin<FScopedHeaderWriteLock>(InHeader, InWriteContext);
			*static_cast<FWriteErasedOptional*>(this) = FWriteErasedOptional(InHeader, ComponentOffset);
		}
	}

	explicit operator bool() const
	{
		return this->ComponentPtr != nullptr;
	}

	void* ComponentAtIndex(int32 Index) const
	{
		return this->ComponentPtr != nullptr ? (*this)[Index] : nullptr;
	}
};
template<typename T>
struct TComponentLock<TRead<T>> : TComponentLockMixin<FScopedHeaderReadLock>, TRead<T>
{
	explicit TComponentLock(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
		: TComponentLockMixin<FScopedHeaderReadLock>(InHeader)
		, TRead<T>(InHeader, ComponentOffset)
	{}
};

template<typename T>
struct TComponentLock<TReadOptional<T>> : TComponentLockMixin<FScopedHeaderReadLock>, TReadOptional<T>
{
	TComponentLock() = default;
	explicit TComponentLock(const FComponentHeader* InHeader, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			*static_cast<TComponentLockMixin<FScopedHeaderReadLock>*>(this) = TComponentLockMixin<FScopedHeaderReadLock>(InHeader);
			*static_cast<TReadOptional<T>*>(this) = TReadOptional<T>(InHeader, ComponentOffset);
		}
	}

	explicit operator bool() const
	{
		return this->ComponentPtr != nullptr;
	}

	const T* ComponentAtIndex(int32 Index) const
	{
		return this->ComponentPtr != nullptr ? this->AsPtr() + Index : nullptr;
	}
};
template<typename T>
struct TComponentLock<TWrite<T>> : TComponentLockMixin<FScopedHeaderWriteLock>, TWrite<T>
{
	explicit TComponentLock(const FComponentHeader* InHeader, FEntityAllocationWriteContext InWriteContext, int32 ComponentOffset = 0)
		: TComponentLockMixin<FScopedHeaderWriteLock>(InHeader, InWriteContext)
		, TWrite<T>(InHeader, ComponentOffset)
	{}

	T& ComponentAtIndex(int32 Index) const
	{
		return (*this)[Index];
	}
};

template<typename T>
struct TComponentLock<TWriteOptional<T>> : TComponentLockMixin<FScopedHeaderWriteLock>, TWriteOptional<T>
{
	TComponentLock() = default;
	explicit TComponentLock(const FComponentHeader* InHeader, FEntityAllocationWriteContext InWriteContext, int32 ComponentOffset = 0)
	{
		if (InHeader)
		{
			*static_cast<TComponentLockMixin<FScopedHeaderWriteLock>*>(this) = TComponentLockMixin<FScopedHeaderWriteLock>(InHeader, InWriteContext);
			*static_cast<TWriteOptional<T>*>(this) = TWriteOptional<T>(InHeader, ComponentOffset);
		}
	}

	explicit operator bool() const
	{
		return this->ComponentPtr != nullptr;
	}

	T* ComponentAtIndex(int32 Index) const
	{
		return this->ComponentPtr != nullptr ? this->AsPtr() + Index : nullptr;
	}
};


using FComponentReader = TComponentLock<FReadErased>;
using FOptionalComponentReader = TComponentLock<FReadErasedOptional>;
using FComponentWriter = TComponentLock<FWriteErased>;
using FOptionalComponentWriter = TComponentLock<FWriteErasedOptional>;
template<typename T> using TComponentReader = TComponentLock<TRead<T>>;
template<typename T> using TOptionalComponentReader = TComponentLock<TReadOptional<T>>;
template<typename T> using TComponentWriter = TComponentLock<TWrite<T>>;
template<typename T> using TOptionalComponentWriter = TComponentLock<TWriteOptional<T>>;


template<typename... T> using TMultiComponentLock = TTuple<TComponentLock<T>...>;

template<typename... T>
struct TMultiComponentData
{
	TMultiComponentData(const TMultiComponentLock<T...>& InAggregateLock)
	{
		auto Init = [](const auto& InLock, auto& OutData)
		{
			if (InLock)
			{
				OutData = InLock;
			}
		};
		VisitTupleElements(Init, InAggregateLock, this->Data);
	}

	template<int Index>
	auto* Get() const
	{
		return Data.template Get<Index>().AsPtr();
	}

	template<int Index>
	auto GetAsArray(int32 ArraySize) const
	{
		auto* ComponentPtr = Data.template Get<Index>().AsPtr();
		return MakeArrayView(ComponentPtr, ComponentPtr ? ArraySize : 0);
	}

private:

	TTuple<T...> Data;
};

template<typename... T> using TMultiReadOptional = TMultiComponentData<TReadOptional<T>...>;
template<typename... T> using TReadOneOrMoreOf = TMultiComponentData<TReadOptional<T>...>;
template<typename... T> using TReadOneOf = TMultiComponentData<TReadOptional<T>...>;

} // namespace UE
} // namespace MovieScene
