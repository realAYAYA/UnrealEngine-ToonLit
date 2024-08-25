// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/IntrusiveUnsetOptionalState.h"
#include "Misc/OptionalFwd.h"
#include "Templates/UnrealTemplate.h"
#include "Serialization/Archive.h"

inline constexpr FNullOpt NullOpt{0};

namespace UE::Core::Private
{
	struct CIntrusiveUnsettable
	{
		template <typename T>
		auto Requires(bool& b) -> decltype(
			b = std::decay_t<typename T::IntrusiveUnsetOptionalStateType>::bHasIntrusiveUnsetOptionalState
		);
	};

	template <size_t N>
	struct TNonIntrusiveOptionalStorage
	{
		uint8 Storage[N];
		bool bIsSet;
	};
}

template <typename T>
constexpr bool HasIntrusiveUnsetOptionalState()
{
	if constexpr (!TModels<UE::Core::Private::CIntrusiveUnsettable, T>::Value)
	{
		return false;
	}
	// Derived types are not guaranteed to have an intrusive state, so ensure IntrusiveUnsetOptionalStateType matches the type in the optional
	else if constexpr (!std::is_same_v<const typename T::IntrusiveUnsetOptionalStateType, const T>)
	{
		return false;
	}
	else
	{
		return T::bHasIntrusiveUnsetOptionalState;
	}
}

/**
 * When we have an optional value IsSet() returns true, and GetValue() is meaningful.
 * Otherwise GetValue() is not meaningful.
 */
template<typename OptionalType>
struct TOptional
{
private:
	static constexpr bool bUsingIntrusiveUnsetState = HasIntrusiveUnsetOptionalState<OptionalType>();

public:
	using ElementType = OptionalType;
	
	/** Construct an OptionalType with a valid value. */
	TOptional(const OptionalType& InValue)
		: TOptional(InPlace, InValue)
	{
	}
	TOptional(OptionalType&& InValue)
		: TOptional(InPlace, MoveTempIfPossible(InValue))
	{
	}
	template <typename... ArgTypes>
	explicit TOptional(EInPlace, ArgTypes&&... Args)
	{
		// If this fails to compile when trying to call TOptional(EInPlace, ...) with a non-public constructor,
		// do not make TOptional a friend.
		//
		// Instead, prefer this pattern:
		//
		//     class FMyType
		//     {
		//     private:
		//         struct FPrivateToken { explicit FPrivateToken() = default; };
		//
		//     public:
		//         // This has an equivalent access level to a private constructor,
		//         // as only friends of FMyType will have access to FPrivateToken,
		//         // but the TOptional constructor can legally call it since it's public.
		//         explicit FMyType(FPrivateToken, int32 Int, float Real, const TCHAR* String);
		//     };
		//
		//     // Won't compile if the caller doesn't have access to FMyType::FPrivateToken
		//     TOptional<FMyType> Opt(InPlace, FMyType::FPrivateToken{}, 5, 3.14f, TEXT("Banana"));
		//
		new(&Value) OptionalType(Forward<ArgTypes>(Args)...);

		if constexpr (!bUsingIntrusiveUnsetState)
		{
			Value.bIsSet = true;
		}
		else
		{
			// Ensure that a user doesn't emplace an unset state into the optional
			checkf(IsSet(), TEXT("TOptional::TOptional(EInPlace, ...) - optionals should not be unset by emplacement"));
		}
	}
	
	/** Construct an OptionalType with an invalid value. */
	TOptional(FNullOpt)
		: TOptional()
	{
	}

	/** Construct an OptionalType with no value; i.e. unset */
	TOptional()
	{
		if constexpr (bUsingIntrusiveUnsetState)
		{
			new (&Value) OptionalType(FIntrusiveUnsetOptionalState{});
		}
		else
		{
			Value.bIsSet = false;
		}
	}

	~TOptional()
	{
		Reset();
	}

	/** Copy/Move construction */
	TOptional(const TOptional& Other)
	{
		if constexpr (!bUsingIntrusiveUnsetState)
		{
			bool bLocalIsSet = Other.Value.bIsSet;
			Value.bIsSet = bLocalIsSet;
			if (!bLocalIsSet)
			{
				return;
			}
		}

		new(&Value) OptionalType(*(const OptionalType*)&Other.Value);
	}
	TOptional(TOptional&& Other)
	{
		if constexpr (!bUsingIntrusiveUnsetState)
		{
			bool bLocalIsSet = Other.Value.bIsSet;
			Value.bIsSet = bLocalIsSet;
			if (!bLocalIsSet)
			{
				return;
			}
		}

		new(&Value) OptionalType(MoveTempIfPossible(*(OptionalType*)&Other.Value));
	}

	TOptional& operator=(const TOptional& Other)
	{
		if (&Other != this)
		{
			if constexpr (bUsingIntrusiveUnsetState)
			{
				*(OptionalType*)&Value = *(const OptionalType*)&Other.Value;
			}
			else
			{
				Reset();
				if (Other.Value.bIsSet)
				{
					new(&Value) OptionalType(*(const OptionalType*)&Other.Value);
					Value.bIsSet = true;
				}
			}
		}
		return *this;
	}
	TOptional& operator=(TOptional&& Other)
	{
		if (&Other != this)
		{
			if constexpr (bUsingIntrusiveUnsetState)
			{
				*(OptionalType*)&Value = MoveTempIfPossible(*(OptionalType*)&Other.Value);
			}
			else
			{
				Reset();
				if (Other.Value.bIsSet)
				{
					new(&Value) OptionalType(MoveTempIfPossible(*(OptionalType*)&Other.Value));
					Value.bIsSet = true;
				}
			}
		}
		return *this;
	}

	TOptional& operator=(const OptionalType& InValue)
	{
		if (&InValue != (const OptionalType*)&Value)
		{
			Emplace(InValue);
		}
		return *this;
	}
	TOptional& operator=(OptionalType&& InValue)
	{
		if (&InValue != (const OptionalType*)&Value)
		{
			Emplace(MoveTempIfPossible(InValue));
		}
		return *this;
	}

	void Reset()
	{
		if constexpr (bUsingIntrusiveUnsetState)
		{
			*(OptionalType*)&Value = FIntrusiveUnsetOptionalState{};
		}
		else
		{
			if (Value.bIsSet)
			{
				Value.bIsSet = false;

				// We need a typedef here because VC won't compile the destructor call below if OptionalType itself has a member called OptionalType
				typedef OptionalType OptionalDestructOptionalType;
				((OptionalType*)&Value)->OptionalDestructOptionalType::~OptionalDestructOptionalType();
			}
		}
	}

	template <typename... ArgsType>
	OptionalType& Emplace(ArgsType&&... Args)
	{
		if constexpr (bUsingIntrusiveUnsetState)
		{
			// Destroy the member in-place before replacing it - a bit nasty, but it'll work since we don't support exceptions

			// We need a typedef here because VC won't compile the destructor call below if OptionalType itself has a member called OptionalType
			typedef OptionalType OptionalDestructOptionalType;
			((OptionalType*)&Value)->OptionalDestructOptionalType::~OptionalDestructOptionalType();
		}
		else
		{
			Reset();
		}

		// If this fails to compile when trying to call Emplace with a non-public constructor,
		// do not make TOptional a friend.
		//
		// Instead, prefer this pattern:
		//
		//     class FMyType
		//     {
		//     private:
		//         struct FPrivateToken { explicit FPrivateToken() = default; };
		//
		//     public:
		//         // This has an equivalent access level to a private constructor,
		//         // as only friends of FMyType will have access to FPrivateToken,
		//         // but Emplace can legally call it since it's public.
		//         explicit FMyType(FPrivateToken, int32 Int, float Real, const TCHAR* String);
		//     };
		//
		//     TOptional<FMyType> Opt:
		//
		//     // Won't compile if the caller doesn't have access to FMyType::FPrivateToken
		//     Opt.Emplace(FMyType::FPrivateToken{}, 5, 3.14f, TEXT("Banana"));
		//
		OptionalType* Result = new(&Value) OptionalType(Forward<ArgsType>(Args)...);

		if constexpr (!bUsingIntrusiveUnsetState)
		{
			Value.bIsSet = true;
		}
		else
		{
			// Ensure that a user doesn't emplace an unset state into the optional
			checkf(IsSet(), TEXT("TOptional::Emplace(...) - optionals should not be unset by an emplacement"));
		}

		return *Result;
	}

	friend bool operator==(const TOptional& Lhs, const TOptional& Rhs)
	{
		if constexpr (!bUsingIntrusiveUnsetState)
		{
			bool bIsLhsSet = Lhs.Value.bIsSet;
			bool bIsRhsSet = Rhs.Value.bIsSet;
			if (bIsLhsSet != bIsRhsSet)
			{
				return false;
			}
			if (!bIsLhsSet) // both unset
			{
				return true;
			}
		}

		return (*(const OptionalType*)&Lhs.Value) == (*(const OptionalType*)&Rhs.Value);
	}

	friend bool operator!=(const TOptional& Lhs, const TOptional& Rhs)
	{
		return !(Lhs == Rhs);
	}

	void Serialize(FArchive& Ar)
	{
		bool bOptionalIsSet = IsSet();
		if (Ar.IsLoading())
		{
			bool bOptionalWasSaved = false;
			Ar << bOptionalWasSaved;
			if (bOptionalWasSaved)
			{
				if (!bOptionalIsSet)
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
			Ar << bOptionalIsSet;
			if (bOptionalIsSet)
			{
				Ar << GetValue();
			}
		}
	}

	/** @return true when the value is meaningful; false if calling GetValue() is undefined. */
	bool IsSet() const
	{
		if constexpr (bUsingIntrusiveUnsetState)
		{
			return !(*(const OptionalType*)&Value == FIntrusiveUnsetOptionalState{});
		}
		else
		{
			return Value.bIsSet;
		}
	}
	FORCEINLINE explicit operator bool() const
	{
		return IsSet();
	}

	/** @return The optional value; undefined when IsSet() returns false. */
	OptionalType& GetValue()
	{
		checkf(IsSet(), TEXT("It is an error to call GetValue() on an unset TOptional. Please either check IsSet() or use Get(DefaultValue) instead."));
		return *(OptionalType*)&Value;
	}
	FORCEINLINE const OptionalType& GetValue() const
	{
		return const_cast<TOptional*>(this)->GetValue();
	}

	OptionalType* operator->()
	{
		return &GetValue();
	}
	FORCEINLINE const OptionalType* operator->() const
	{
		return const_cast<TOptional*>(this)->operator->();
	}

	OptionalType& operator*()
	{
		return GetValue();
	}
	FORCEINLINE const OptionalType& operator*() const
	{
		return const_cast<TOptional*>(this)->operator*();
	}

	/** @return The optional value when set; DefaultValue otherwise. */
	const OptionalType& Get(const OptionalType& DefaultValue UE_LIFETIMEBOUND) const UE_LIFETIMEBOUND
	{
		return IsSet() ? *(const OptionalType*)&Value : DefaultValue;
	}

	/** @return A pointer to the optional value when set, nullptr otherwise. */
	OptionalType* GetPtrOrNull()
	{
		return IsSet() ? (OptionalType*)&Value : nullptr;
	}
	FORCEINLINE const OptionalType* GetPtrOrNull() const
	{
		return const_cast<TOptional*>(this)->GetPtrOrNull();
	}

private:
	using ValueStorageType = std::conditional_t<bUsingIntrusiveUnsetState, uint8[sizeof(OptionalType)], UE::Core::Private::TNonIntrusiveOptionalStorage<sizeof(OptionalType)>>;
	alignas(OptionalType) ValueStorageType Value;
};

template<typename OptionalType>
FArchive& operator<<(FArchive& Ar, TOptional<OptionalType>& Optional)
{
	Optional.Serialize(Ar);
	return Ar;
}

template<typename OptionalType>
inline auto GetTypeHash(const TOptional<OptionalType>& Optional) -> decltype(GetTypeHash(*Optional))
{
	return Optional.IsSet() ? GetTypeHash(*Optional) : 0;
}

/**
 * Trait which determines whether or not a type is a TOptional.
 */
template <typename T> static constexpr bool TIsTOptional_V                              = false;
template <typename T> static constexpr bool TIsTOptional_V<               TOptional<T>> = true;
template <typename T> static constexpr bool TIsTOptional_V<const          TOptional<T>> = true;
template <typename T> static constexpr bool TIsTOptional_V<      volatile TOptional<T>> = true;
template <typename T> static constexpr bool TIsTOptional_V<const volatile TOptional<T>> = true;
