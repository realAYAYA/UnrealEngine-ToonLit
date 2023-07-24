// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MVVM/ViewModelTypeID.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Invoke.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"

#include <type_traits>

namespace UE
{
namespace Sequencer
{

struct FCastableBoilerplate;

class SEQUENCERCORE_API ICastable
{
public:

	virtual ~ICastable(){}

	template<typename RetType, typename InterfaceType, typename ...ArgTypes, typename ...ParamTypes>
	RetType CastedCall(RetType(InterfaceType::*MemberFunc)(ArgTypes...), ParamTypes&&... InArgs)
	{
		return Invoke(MemberFunc, this->CastThisChecked<InterfaceType>(), Forward<ParamTypes>(InArgs)...);
	}

	template<typename RetType, typename InterfaceType, typename ...ArgTypes, typename ...ParamTypes>
	RetType CastedCall(RetType(InterfaceType::*MemberFunc)(ArgTypes...), ParamTypes&&... InArgs) const
	{
		return Invoke(MemberFunc, this->CastThisChecked<InterfaceType>(), Forward<ParamTypes>(InArgs)...);
	}

	template<typename T>
	static T* CastWeakPtr(const TWeakPtr<ICastable>& InWeakPtr)
	{
		if (TSharedPtr<ICastable> Pinned = InWeakPtr.Pin())
		{
			return Pinned->CastThis<T>();
		}
		return nullptr;
	}

	template<typename T>
	static TSharedPtr<T> CastWeakPtrShared(const TWeakPtr<ICastable>& InWeakPtr)
	{
		if (TSharedPtr<ICastable> Pinned = InWeakPtr.Pin())
		{
			return TSharedPtr<T>(Pinned, Pinned->CastThis<T>());
		}
		return nullptr;
	}

	bool IsA(FViewModelTypeID InType) const
	{
		return CastRaw(InType) != nullptr;
	}

	template<typename T>
	bool IsA() const
	{
		return CastThis<T>() != nullptr;
	}

	template<typename T>
	bool IsAny() const;

	void* CastRaw(FViewModelTypeID InType);
	const void* CastRaw(FViewModelTypeID InType) const;

	template<typename T>
	T* CastThis()
	{
		return static_cast<T*>(const_cast<void*>(CastRaw(T::ID)));
	}

	template<typename T>
	const T* CastThis() const
	{
		return static_cast<const T*>(CastRaw(T::ID));
	}

	template<typename T>
	T* CastThisChecked()
	{
		T* Result = CastThis<T>();
		check(Result);
		return Result;
	}

	template<typename T>
	const T* CastThisChecked() const
	{
		const T* Result = CastThis<const T>();
		check(Result);
		return Result;
	}

protected:

	// Usually implemented through UE_SEQUENCER_DECLARE_CASTABLE
	virtual void CastImpl(FViewModelTypeID Type, const void*& OutPtr) const = 0;
};

template<typename ...T>
struct TImplements;

#define UE_SEQUENCER_DECLARE_CASTABLE(ThisType, ...)                                             \
	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(ThisType);                                           \
	using Implements = TImplements<__VA_ARGS__>;                                                 \
	virtual void CastImpl(FViewModelTypeID ToType, const void*& OutPtr) const override  \
	{                                                                                            \
		FCastableBoilerplate::CastImplementation(this, ToType, OutPtr);                          \
	}

#define UE_SEQUENCER_DEFINE_CASTABLE(ThisType)                                                   \
	UE_SEQUENCER_DEFINE_VIEW_MODEL_TYPE_ID(ThisType);


template<typename ImplementsType>
struct TCompositeCast;

struct FCastableBoilerplate
{
	template<typename T>
	struct THasImplements
	{
		template<typename U, typename V = typename U::Implements>
		static uint8 HasImplements(const U* Input);
		static uint16 HasImplements(const void* Input);

		static constexpr bool Value = sizeof(HasImplements((const T*)0)) == 1;
	};

	template<typename T>
	FORCENOINLINE static void CastImplementation(const T* This, FViewModelTypeID ToType, const void*& OutResult)
	{
		if (const void* Result = DirectCast(This, ToType))
		{
			OutResult = Result;
		}
		else
		{
			CompositeCast(This, ToType, OutResult);
		}
	}

	template<typename T>
	FORCENOINLINE static bool IsAnyImplementation(const ICastable* This)
	{
		const void* Result = This->CastRaw(T::ID);
		if (!Result)
		{
			return CompositeIsAny<T>(This);
		}
	}

	template<typename T, typename U = decltype(T::ID)>
	FORCENOINLINE static const void* DirectCast(const T* Input, FViewModelTypeID ToType)
	{
		if (ToType == T::ID)
		{
			return Input;
		}
		return nullptr;
	}

	FORCENOINLINE static const void* DirectCast(const void* Input, FViewModelTypeID ToType)
	{
		return nullptr;
	}

	template<typename T>
	FORCENOINLINE static std::enable_if_t<THasImplements<T>::Value> CompositeCast(const T* Input, FViewModelTypeID ToType, const void*& OutResult)
	{
		TCompositeCast<typename T::Implements>::Apply(Input, ToType, OutResult);
	}

	FORCENOINLINE static void CompositeCast(const void* Input, FViewModelTypeID ToType, const void*& OutResult)
	{
	}

	template<typename T>
	FORCENOINLINE static std::enable_if_t<THasImplements<T>::Value, bool> CompositeIsAny(const ICastable* This)
	{
		return TCompositeCast<typename T::Implements>::IsAny(This);
	}

	template<typename T>
	FORCENOINLINE static std::enable_if_t<!THasImplements<T>::Value, bool> CompositeIsAny(const ICastable* This)
	{
	}
};

template<typename ...CastableTo>
struct TCompositeCast<TImplements<CastableTo...>>
{
	template<typename T>
	FORCENOINLINE static void Apply(const T* This, FViewModelTypeID ToType, const void*& OutResult)
	{
		(FCastableBoilerplate::CastImplementation(static_cast<const CastableTo*>(This), ToType, OutResult), ...);
	}
	template<typename T>
	FORCENOINLINE static bool IsAny(const ICastable* This)
	{
		bool bIsAny = false;
		(( bIsAny |= FCastableBoilerplate::IsAnyImplementation<CastableTo>(This) ), ...);
		return bIsAny;
	}
};

template<typename ...T>
struct TImplements
{
};


template<typename T>
bool ICastable::IsAny() const
{
	return FCastableBoilerplate::template IsAnyImplementation<T>(this);
}

} // namespace Sequencer
} // namespace UE

