// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OptionalFwd.h"
#include "Templates/PointerIsConvertibleFromTo.h"

// So we can construct TNonNullPtrs
enum class EDefaultConstructNonNullPtr { UnsafeDoNotUse };

namespace UE::Core::Private::NonNullPtr {
template <typename...>
using TVoid = void;

/**
 * Version of `::TPointerIsConvertibleFromTo` that produces an incomplete type
 * when either `From` or `To` are incomplete types
 */
template <typename, typename, typename = void>
struct TPointerIsConvertibleFromTo;

/**
 * Specialization of
 * `UE::Core::Private::NonNullPtr::TPointerIsConvertibleFromTo` for complete
 * non-function types
 */
template <typename From, typename To>
struct TPointerIsConvertibleFromTo<From, To, TVoid<decltype(sizeof(From)), decltype(sizeof(To))>>
	: ::TPointerIsConvertibleFromTo<From, To>
{
};

/**
 * Specialization of
 * `UE::Core::Private::NonNullPtr::TPointerIsConvertibleFromTo` for function
 * types, which are always complete types
 */
template <typename Result1, typename... Args1, typename Result2, typename... Args2>
struct TPointerIsConvertibleFromTo<Result1(Args1...), Result2(Args2...)>
	: ::TPointerIsConvertibleFromTo<Result1(Args1...), Result2(Args2...)>
{
};
}

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
		typename = typename TEnableIf<UE::Core::Private::NonNullPtr::TPointerIsConvertibleFromTo<OtherObjectType, ObjectType>::Value>::Type
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
	template <typename OtherObjectType>
	FORCEINLINE typename TEnableIf<UE::Core::Private::NonNullPtr::TPointerIsConvertibleFromTo<OtherObjectType, ObjectType>::Value, TNonNullPtr&>::Type operator=(const TNonNullPtr<OtherObjectType>& Other)
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

	TOptional& operator=(const TOptional& Other)
	{
		Pointer = Other.Pointer;
		return *this;
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
	OptionalType* GetValue() const { checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead.")); return Pointer; }

	OptionalType* operator->() const { return Pointer; }

	OptionalType& operator*() const { return *Pointer; }

	/** @return The optional value when set; DefaultValue otherwise. */
	OptionalType* Get(OptionalType* DefaultPointer) const { return IsSet() ? Pointer : DefaultPointer; }

private:
	OptionalType* Pointer;
};
