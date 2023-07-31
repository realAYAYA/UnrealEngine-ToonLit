// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Field.h"
#include "Templates/ChooseClass.h"
#include "SubclassOf.h"

// So we can construct uninitialized TNonNullSubclassOf
enum class EDefaultConstructNonNullSubclassOf { UnsafeDoNotUse };

/**
 * Template to allow TClassType's to be passed around with type safety 
 */
template<class TClass>
class TNonNullSubclassOf : public TSubclassOf<TClass>
{
public:
	
	using TClassType = typename TSubclassOf<TClass>::TClassType;
	using TBaseType = typename TSubclassOf<TClass>::TBaseType;

	/** Default Constructor, defaults to null */
	FORCEINLINE TNonNullSubclassOf(EDefaultConstructNonNullSubclassOf) :
		TSubclassOf<TClass>(nullptr)
	{}

	/** Constructor that takes a UClass and does a runtime check to make sure this is a compatible class */
	FORCEINLINE TNonNullSubclassOf(TClassType* From) :
		TSubclassOf<TClass>(From)
	{
		checkf(From, TEXT("Initializing TNonNullSubclassOf with null"));
	}

	/** Copy Constructor, will only compile if types are compatible */
	template <class TClassA, class = decltype(ImplicitConv<TClass*>((TClassA*)nullptr))>
	FORCEINLINE TNonNullSubclassOf(const TSubclassOf<TClassA>& From) :
		TSubclassOf<TClass>(From)
	{}

	/** Assignment operator, will only compile if types are compatible */
	template <class TClassA, class = decltype(ImplicitConv<TClass*>((TClassA*)nullptr))>
	FORCEINLINE TNonNullSubclassOf& operator=(const TSubclassOf<TClassA>& From)
	{
		checkf(*From, TEXT("Assigning null to TNonNullSubclassOf"));
		TSubclassOf<TClass>::operator=(From);
		return *this;
	}
	
	/** Assignment operator from UClass, the type is checked on get not on set */
	FORCEINLINE TNonNullSubclassOf& operator=(TClassType* From)
	{
		checkf(From, TEXT("Assigning null to TNonNullSubclassOf"));
		TSubclassOf<TClass>::operator=(From);
		return *this;
	}
};


/**
 * Specialization of TOptional for TNonNullSubclassOf value types
 */
template<typename TClass>
struct TOptional<TNonNullSubclassOf<TClass>>
{
public:

	using TClassType = typename TSubclassOf<TClass>::TClassType;

	/** Construct an OptionaType with a valid value. */
	TOptional(const TNonNullSubclassOf<TClass>& InPointer)
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

	TOptional& operator=(TClass* InPointer)
	{
		Pointer = InPointer;
		return *this;
	}

	void Reset()
	{
		Pointer = nullptr;
	}

	TClass* Emplace(TClass* InPointer)
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
	TClassType* GetValue() const { checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead.")); return Pointer; }

	TClassType* operator->() const { return Pointer; }

	TClassType& operator*() const { return *Pointer; }

	/** @return The optional value when set; DefaultValue otherwise. */
	TClassType* Get(TClassType* DefaultPointer) const { return IsSet() ? Pointer : DefaultPointer; }

private:
	TClassType* Pointer;
};
