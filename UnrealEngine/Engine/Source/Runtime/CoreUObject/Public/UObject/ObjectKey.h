// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtr.h"

#include <type_traits>

struct FObjectKey;

namespace UE::CoreUObject::Private
{
	FObjectKey MakeObjectKey(int32 ObjectIndex, int32 ObjectSerialNumber);
}

/** FObjectKey is an immutable, copyable key which can be used to uniquely identify an object for the lifetime of the application */
struct FObjectKey
{
public:
	/** Default constructor */
	FORCEINLINE FObjectKey()
		: ObjectIndex(UE::Core::Private::InvalidWeakObjectIndex)
		, ObjectSerialNumber(0)
	{
	}

	/** Construct from an object pointer */
	FORCEINLINE FObjectKey(const UObject* Object)
		: ObjectIndex(UE::Core::Private::InvalidWeakObjectIndex)
		, ObjectSerialNumber(0)
	{
		if (Object)
		{
			FWeakObjectPtr Weak(Object);
			ObjectIndex = Weak.ObjectIndex;
			ObjectSerialNumber = Weak.ObjectSerialNumber;
		}
	}
	template <
		typename U,
		decltype(ImplicitConv<const UObject*>(std::declval<U>()))* = nullptr
	>
	FORCEINLINE FObjectKey(U Object)
		: FObjectKey(ImplicitConv<const UObject*>(Object))
	{
	}

	/** Compare this key with another */
	FORCEINLINE bool operator==(const FObjectKey& Other) const
	{
		return ObjectIndex == Other.ObjectIndex && ObjectSerialNumber == Other.ObjectSerialNumber;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator!=(const FObjectKey& Other) const
	{
		return ObjectIndex != Other.ObjectIndex || ObjectSerialNumber != Other.ObjectSerialNumber;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator<(const FObjectKey& Other) const
	{
		return ObjectIndex < Other.ObjectIndex || (ObjectIndex == Other.ObjectIndex && ObjectSerialNumber < Other.ObjectSerialNumber);
	}

	/** Compare this key with another */
	FORCEINLINE bool operator<=(const FObjectKey& Other) const
	{
		return ObjectIndex <= Other.ObjectIndex || (ObjectIndex == Other.ObjectIndex && ObjectSerialNumber <= Other.ObjectSerialNumber);
	}

	/** Compare this key with another */
	FORCEINLINE bool operator>(const FObjectKey& Other) const
	{
		return ObjectIndex > Other.ObjectIndex || (ObjectIndex == Other.ObjectIndex && ObjectSerialNumber > Other.ObjectSerialNumber);
	}

	/** Compare this key with another */
	FORCEINLINE bool operator>=(const FObjectKey& Other) const
	{
		return ObjectIndex > Other.ObjectIndex || (ObjectIndex == Other.ObjectIndex && ObjectSerialNumber >= Other.ObjectSerialNumber);
	}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FObjectKey& Key)
	{
		check(!Ar.IsPersistent());
		return Ar << Key.ObjectIndex << Key.ObjectSerialNumber;
	}

	/**
	 * Attempt to access the object from which this key was constructed.
	 * @return The object used to construct this key, or nullptr if it is no longer valid
	 */
	UObject* ResolveObjectPtr() const
	{
		FWeakObjectPtr WeakPtr;
		WeakPtr.ObjectIndex = ObjectIndex;
		WeakPtr.ObjectSerialNumber = ObjectSerialNumber;

		return WeakPtr.Get();
	}

	/**
	 * Attempt to access the object from which this key was constructed, even if it is marked as pending kill.
	 * @return The object used to construct this key, or nullptr if it is no longer valid
	 */
	UObject* ResolveObjectPtrEvenIfPendingKill() const
	{
		FWeakObjectPtr WeakPtr;
		WeakPtr.ObjectIndex = ObjectIndex;
		WeakPtr.ObjectSerialNumber = ObjectSerialNumber;

		constexpr bool bEvenIfPendingKill = true;
		return WeakPtr.Get(bEvenIfPendingKill);
	}

	/**
	 * Attempt to access the object from which this key was constructed, even if it is RF_PendingKill or RF_Unreachable
	 * @return The object used to construct this key, or nullptr if it is no longer valid
	 */
	UObject* ResolveObjectPtrEvenIfUnreachable() const
	{
		FWeakObjectPtr WeakPtr;
		WeakPtr.ObjectIndex = ObjectIndex;
		WeakPtr.ObjectSerialNumber = ObjectSerialNumber;

		return WeakPtr.GetEvenIfUnreachable();
	}

	/** Hash function */
	friend uint32 GetTypeHash(const FObjectKey& Key)
	{
		return HashCombine(Key.ObjectIndex, Key.ObjectSerialNumber);
	}

private:
	FObjectKey(int32 Index, int32 Serial)
		: ObjectIndex(Index)
		, ObjectSerialNumber(Serial)
	{ }

	friend FObjectKey UE::CoreUObject::Private::MakeObjectKey(int32 ObjectIndex, int32 ObjectSerialNumber);

	int32		ObjectIndex;
	int32		ObjectSerialNumber;
};

/** TObjectKey is a strongly typed, immutable, copyable key which can be used to uniquely identify an object for the lifetime of the application */
template<typename InElementType>
class TObjectKey
{
public:
	typedef InElementType ElementType;

	/** Default constructor */
	FORCEINLINE TObjectKey() = default;

	/** Construct from an object pointer */
	template <
		typename U,
		decltype(ImplicitConv<const InElementType*>(std::declval<U>()))* = nullptr
	>
	FORCEINLINE TObjectKey(U Object)
		: ObjectKey(ImplicitConv<const InElementType*>(Object))
	{
	}

	/** Compare this key with another */
	FORCEINLINE bool operator==(const TObjectKey& Other) const
	{
		return ObjectKey == Other.ObjectKey;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator!=(const TObjectKey& Other) const
	{
		return ObjectKey != Other.ObjectKey;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator<(const TObjectKey& Other) const
	{
		return ObjectKey < Other.ObjectKey;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator<=(const TObjectKey& Other) const
	{
		return ObjectKey <= Other.ObjectKey;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator>(const TObjectKey& Other) const
	{
		return ObjectKey > Other.ObjectKey;
	}

	/** Compare this key with another */
	FORCEINLINE bool operator>=(const TObjectKey& Other) const
	{
		return ObjectKey >= Other.ObjectKey;
	}

	//** Hash function */
	friend uint32 GetTypeHash(const TObjectKey& Key)
	{
		return GetTypeHash(Key.ObjectKey);
	}

	/**
	 * Attempt to access the object from which this key was constructed.
	 * @return The object used to construct this key, or nullptr if it is no longer valid
	 */
	InElementType* ResolveObjectPtr() const
	{
		return (InElementType*)ObjectKey.ResolveObjectPtr();
	}

	/**
	 * Attempt to access the object from which this key was constructed, even if it is marked as pending kill.
	 * @return The object used to construct this key, or nullptr if it is no longer valid
	 */
	InElementType* ResolveObjectPtrEvenIfPendingKill() const
	{
		return static_cast<InElementType*>(ObjectKey.ResolveObjectPtrEvenIfPendingKill());
	}

private:
	FObjectKey ObjectKey;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
