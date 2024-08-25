// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/OptionalFwd.h"
#include "Templates/UnrealTypeTraits.h"

#include <type_traits>

class FArchive;
enum class EDefaultConstructNonNullPtr { UnsafeDoNotUse }; // So we can construct TNonNullPtrs

/**
 * TNonNullPtr is a non-nullable, non-owning, raw/naked/unsafe pointer.
 */
template<typename ObjectType>
class TNonNullPtr
{
public:

	/**
	 * Hack that can be used under extraordinary circumstances
	 */
	FORCEINLINE TNonNullPtr(EDefaultConstructNonNullPtr)
		: Object(nullptr)
	{	
	}

	/**
	 * nullptr constructor - not allowed.
	 */
	FORCEINLINE TNonNullPtr(TYPE_OF_NULLPTR)
	{
		// Essentially static_assert(false), but this way prevents GCC/Clang from crying wolf by merely inspecting the function body
		static_assert(sizeof(ObjectType) == 0, "Tried to initialize TNonNullPtr with a null pointer!");
	}

	/**
	 * Constructs a non-null pointer from the provided pointer. Must not be nullptr.
	 */
	FORCEINLINE TNonNullPtr(ObjectType* InObject)
		: Object(InObject)
	{
		ensureMsgf(InObject, TEXT("Tried to initialize TNonNullPtr with a null pointer!"));
	}

	/**
	 * Constructs a non-null pointer from another non-null pointer
	 */
	template <
		typename OtherObjectType,
		decltype(ImplicitConv<ObjectType*>((OtherObjectType*)nullptr))* = nullptr
	>
	FORCEINLINE TNonNullPtr(const TNonNullPtr<OtherObjectType>& Other)
		: Object(Other.Object)
	{
	}

	/**
	 * Assignment operator taking a nullptr - not allowed.
	 */
	FORCEINLINE TNonNullPtr& operator=(TYPE_OF_NULLPTR)
	{
		// Essentially static_assert(false), but this way prevents GCC/Clang from crying wolf by merely inspecting the function body
		static_assert(sizeof(ObjectType) == 0, "Tried to assign a null pointer to a TNonNullPtr!");
		return *this;
	}

	/**
	 * Assignment operator taking a pointer
	 */
	FORCEINLINE TNonNullPtr& operator=(ObjectType* InObject)
	{
		ensureMsgf(InObject, TEXT("Tried to assign a null pointer to a TNonNullPtr!"));
		Object = InObject;
		return *this;
	}

	/**
	 * Assignment operator taking another TNonNullPtr
	 */
	template <
		typename OtherObjectType,
		decltype(ImplicitConv<ObjectType*>((OtherObjectType*)nullptr))* = nullptr
	>
	FORCEINLINE TNonNullPtr& operator=(const TNonNullPtr<OtherObjectType>& Other)
	{
		Object = Other.Object;
		return *this;
	}

	/**
	 * Returns the internal pointer
	 */
	FORCEINLINE operator ObjectType*() const
	{
		ensureMsgf(Object, TEXT("Tried to access null pointer!"));
		return Object;
	}

	/**
	 * Returns the internal pointer
	 */
	FORCEINLINE ObjectType* Get() const
	{
		ensureMsgf(Object, TEXT("Tried to access null pointer!"));
		return Object;
	}

	/**
	 * Dereference operator returns a reference to the object this pointer points to
	 */
	FORCEINLINE ObjectType& operator*() const
	{
		ensureMsgf(Object, TEXT("Tried to access null pointer!"));
		return *Object;
	}

	/**
	 * Arrow operator returns a pointer to this pointer's object
	 */
	FORCEINLINE ObjectType* operator->() const
	{
		ensureMsgf(Object, TEXT("Tried to access null pointer!"));
		return Object;
	}

	/*
	 * WARNING: Hack that can be used under extraordinary circumstances. Pointers here 
	 * should always be valid but might be in the EDefaultConstructNonNullPtr state 
	 * during initialization.
	 */
	FORCEINLINE bool IsInitialized() const
	{
		return Object != nullptr;
	}

private:

	/** The object we're holding a reference to. */
	ObjectType* Object;

};


/**
 * Specialization of TOptional for TNonNullPtr value types
 */
template<typename OptionalType>
struct TOptional<TNonNullPtr<OptionalType>>
{
public:
	/** Construct an OptionaType with a valid value. */
	TOptional(const TNonNullPtr<OptionalType>& InPointer)
		: Pointer(InPointer)
	{
	}

	/** Construct an OptionalType with no value; i.e. unset */
	TOptional()
		: Pointer(nullptr)
	{
	}

	/** Construct an OptionalType with an invalid value. */
	TOptional(FNullOpt)
		: TOptional()
	{
	}

	TOptional& operator=(OptionalType* InPointer)
	{
		Pointer = InPointer;
		return *this;
	}

	void Reset()
	{
		Pointer = nullptr;
	}

	OptionalType* Emplace(OptionalType* InPointer)
	{
		Pointer = InPointer;
		return InPointer;
	}

	bool operator==(const TOptional& rhs) const
	{
		return Pointer == rhs.Pointer;
	}

	bool operator!=(const TOptional& rhs) const
	{
		return Pointer != rhs.Pointer;
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
	OptionalType* GetValue() const { checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead.")); return Pointer; }

	OptionalType* operator->() const { return Pointer; }

	OptionalType& operator*() const { return *Pointer; }

	/** @return The optional value when set; DefaultValue otherwise. */
	OptionalType* Get(OptionalType* DefaultPointer) const { return IsSet() ? Pointer : DefaultPointer; }

private:
	OptionalType* Pointer;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Templates/EnableIf.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#endif
