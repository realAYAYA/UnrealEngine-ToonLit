// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/OptionalFwd.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/UnrealTemplate.h"
#include "Serialization/Archive.h"

inline constexpr FNullOpt NullOpt{0};

/**
 * When we have an optional value IsSet() returns true, and GetValue() is meaningful.
 * Otherwise GetValue() is not meaningful.
 */
template<typename OptionalType>
struct TOptional
{
public:
	using ElementType = OptionalType;
	
	/** Construct an OptionalType with a valid value. */
	TOptional(const OptionalType& InValue)
	{
		new(&Value) OptionalType(InValue);
		bIsSet = true;
	}
	TOptional(OptionalType&& InValue)
	{
		new(&Value) OptionalType(MoveTempIfPossible(InValue));
		bIsSet = true;
	}
	template <typename... ArgTypes>
	explicit TOptional(EInPlace, ArgTypes&&... Args)
	{
		new(&Value) OptionalType(Forward<ArgTypes>(Args)...);
		bIsSet = true;
	}
	
	/** Construct an OptionalType with an invalid value. */
	TOptional(FNullOpt)
		: bIsSet(false)
	{
	}

	/** Construct an OptionalType with no value; i.e. unset */
	TOptional()
		: bIsSet(false)
	{
	}

	~TOptional()
	{
		Reset();
	}

	/** Copy/Move construction */
	TOptional(const TOptional& InValue)
		: bIsSet(false)
	{
		if (InValue.bIsSet)
		{
			new(&Value) OptionalType(*(const OptionalType*)&InValue.Value);
			bIsSet = true;
		}
	}
	TOptional(TOptional&& InValue)
		: bIsSet(false)
	{
		if (InValue.bIsSet)
		{
			new(&Value) OptionalType(MoveTempIfPossible(*(OptionalType*)&InValue.Value));
			bIsSet = true;
		}
	}

	TOptional& operator=(const TOptional& InValue)
	{
		if (&InValue != this)
		{
			Reset();
			if (InValue.bIsSet)
			{
				new(&Value) OptionalType(*(const OptionalType*)&InValue.Value);
				bIsSet = true;
			}
		}
		return *this;
	}
	TOptional& operator=(TOptional&& InValue)
	{
		if (&InValue != this)
		{
			Reset();
			if (InValue.bIsSet)
			{
				new(&Value) OptionalType(MoveTempIfPossible(*(OptionalType*)&InValue.Value));
				bIsSet = true;
			}
		}
		return *this;
	}

	TOptional& operator=(const OptionalType& InValue)
	{
		if (&InValue != (OptionalType*)&Value)
		{
			Reset();
			new(&Value) OptionalType(InValue);
			bIsSet = true;
		}
		return *this;
	}
	TOptional& operator=(OptionalType&& InValue)
	{
		if (&InValue != (OptionalType*)&Value)
		{
			Reset();
			new(&Value) OptionalType(MoveTempIfPossible(InValue));
			bIsSet = true;
		}
		return *this;
	}

	void Reset()
	{
		if (bIsSet)
		{
			bIsSet = false;

			// We need a typedef here because VC won't compile the destructor call below if OptionalType itself has a member called OptionalType
			typedef OptionalType OptionalDestructOptionalType;
			((OptionalType*)&Value)->OptionalDestructOptionalType::~OptionalDestructOptionalType();
		}
	}

	template <typename... ArgsType>
	OptionalType& Emplace(ArgsType&&... Args)
	{
		Reset();
		OptionalType* Result = new(&Value) OptionalType(Forward<ArgsType>(Args)...);
		bIsSet = true;
		return *Result;
	}

	friend bool operator==(const TOptional& lhs, const TOptional& rhs)
	{
		if (lhs.bIsSet != rhs.bIsSet)
		{
			return false;
		}
		if (!lhs.bIsSet) // both unset
		{
			return true;
		}
		return (*(OptionalType*)&lhs.Value) == (*(OptionalType*)&rhs.Value);
	}

	friend bool operator!=(const TOptional& lhs, const TOptional& rhs)
	{
		return !(lhs == rhs);
	}

	void Serialize(FArchive& Ar)
	{
		bool bOptionalIsSet = bIsSet;
		Ar << bOptionalIsSet;
		if (Ar.IsLoading())
		{
			if (bOptionalIsSet)
			{
				if (!bIsSet)
				{
					Emplace();
				}
				Ar << GetValue();
			}
			else
			{
				Reset();
			}
		}
		else
		{
			if (bOptionalIsSet)
			{
				Ar << GetValue();
			}
		}
	}

	/** @return true when the value is meaningful; false if calling GetValue() is undefined. */
	bool IsSet() const { return bIsSet; }
	FORCEINLINE explicit operator bool() const { return bIsSet; }

	/** @return The optional value; undefined when IsSet() returns false. */
	const OptionalType& GetValue() const { checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead.")); return *(OptionalType*)&Value; }
		  OptionalType& GetValue()		 { checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead.")); return *(OptionalType*)&Value; }

	const OptionalType* operator->() const { return &GetValue(); }
		  OptionalType* operator->()	   { return &GetValue(); }

	const OptionalType& operator*() const { return GetValue(); }
		  OptionalType& operator*()		  { return GetValue(); }

	/** @return The optional value when set; DefaultValue otherwise. */
	const OptionalType& Get(const OptionalType& DefaultValue) const { return IsSet() ? *(OptionalType*)&Value : DefaultValue; }

	/** @return A pointer to the optional value when set, nullptr otherwise. */
	OptionalType* GetPtrOrNull() { return IsSet() ? (OptionalType*)&Value : nullptr; }
	const OptionalType* GetPtrOrNull() const { return IsSet() ? (const OptionalType*)&Value : nullptr; }

private:
	TTypeCompatibleBytes<OptionalType> Value;
	bool bIsSet;
};

template<typename OptionalType>
FArchive& operator<<(FArchive& Ar, TOptional<OptionalType>& Optional)
{
	Optional.Serialize(Ar);
	return Ar;
}

/**
 * Trait which determines whether or not a type is a TOptional.
 */
template <typename T> static constexpr bool TIsTOptional_V                              = false;
template <typename T> static constexpr bool TIsTOptional_V<               TOptional<T>> = true;
template <typename T> static constexpr bool TIsTOptional_V<const          TOptional<T>> = true;
template <typename T> static constexpr bool TIsTOptional_V<      volatile TOptional<T>> = true;
template <typename T> static constexpr bool TIsTOptional_V<const volatile TOptional<T>> = true;
