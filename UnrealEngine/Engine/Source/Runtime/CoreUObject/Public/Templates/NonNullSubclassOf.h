// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "SubclassOf.h"

// So we can construct uninitialized TNonNullSubclassOf
enum class EDefaultConstructNonNullSubclassOf { UnsafeDoNotUse };

/**
 * Template to allow TClassType's to be passed around with type safety 
 */
template <typename T>
class TNonNullSubclassOf : public TSubclassOf<T>
{
	using Super = TSubclassOf<T>;

public:
	/** Default Constructor, defaults to null */
	FORCEINLINE TNonNullSubclassOf(EDefaultConstructNonNullSubclassOf)
		: Super(nullptr)
	{
	}

	/** Constructor that takes a UClass* or FFieldClass* if T is a UObject or a FField type respectively. */
	template <typename PtrType>
	FORCEINLINE TNonNullSubclassOf(PtrType* From)
		: Super(From)
	{
		checkf(From, TEXT("Initializing TNonNullSubclassOf with null"));
	}

	/** Copy Constructor, will only compile if types are compatible */
	template <
		typename U,
		decltype(ImplicitConv<T*>((U*)nullptr))* = nullptr
	>
	FORCEINLINE TNonNullSubclassOf(const TSubclassOf<U>& From)
		: Super(From)
	{
	}

	/** Assignment operator, will only compile if types are compatible */
	template
	<
		typename U,
		decltype(ImplicitConv<T*>((U*)nullptr))* = nullptr
	>
	FORCEINLINE TNonNullSubclassOf& operator=(const TSubclassOf<U>& From)
	{
		checkf(*From, TEXT("Assigning null to TNonNullSubclassOf"));
		Super::operator=(From);
		return *this;
	}
	
	/** Assignment operator from UClass, the type is checked on get not on set */
	template <typename PtrType>
	FORCEINLINE TNonNullSubclassOf& operator=(PtrType* From)
	{
		checkf(From, TEXT("Assigning null to TNonNullSubclassOf"));
		Super::operator=(From);
		return *this;
	}
};


/**
 * Specialization of TOptional for TNonNullSubclassOf value types
 */
template<typename T>
struct TOptional<TNonNullSubclassOf<T>>
{
	using ClassType = std::remove_pointer_t<decltype(std::declval<TNonNullSubclassOf<T>>().Get())>;

public:
	/** Construct an OptionaType with a valid value. */
	TOptional(const TNonNullSubclassOf<T>& InPointer)
		: Pointer(InPointer)
	{
	}

	/** Construct an TClass with no value; i.e. unset */
	TOptional()
		: Pointer(nullptr)
	{
	}

	/** Construct an TClass with an invalid value. */
	TOptional(FNullOpt)
		: TOptional()
	{
	}

	TOptional& operator=(const TOptional& Other)
	{
		Pointer = Other.Pointer;
		return *this;
	}

	TOptional& operator=(T* InPointer)
	{
		Pointer = InPointer;
		return *this;
	}

	void Reset()
	{
		Pointer = nullptr;
	}

	T* Emplace(T* InPointer)
	{
		Pointer = InPointer;
		return InPointer;
	}

	friend bool operator==(const TOptional& lhs, const TOptional& rhs)
	{
		return lhs.Pointer == rhs.Pointer;
	}

	friend bool operator!=(const TOptional& lhs, const TOptional& rhs)
	{
		return !(lhs == rhs);
	}

	friend FArchive& operator<<(FArchive& Ar, TOptional& Optional)
	{
		Ar << Optional.Pointer;
		return Ar;
	}

	/** @return true when the value is meaningful; false if calling GetValue() is undefined. */
	bool IsSet() const { return Pointer != nullptr; }
	FORCEINLINE explicit operator bool() const { return Pointer != nullptr; }

	/** @return The optional value; undefined when IsSet() returns false. */
	ClassType* GetValue() const
	{
		checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead."));
		return Pointer;
	}

	ClassType* operator->() const
	{
		return Pointer;
	}

	ClassType& operator*() const
	{
		return *Pointer;
	}

	/** @return The optional value when set; DefaultValue otherwise. */
	ClassType* Get(ClassType* DefaultPointer) const
	{
		return IsSet() ? Pointer : DefaultPointer;
	}

private:
	ClassType* Pointer;
};
