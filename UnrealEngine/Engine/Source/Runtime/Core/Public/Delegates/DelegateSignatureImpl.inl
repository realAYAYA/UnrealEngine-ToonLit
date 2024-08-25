// Copyright Epic Games, Inc. All Rights Reserved.

// Only designed to be included directly by Delegate.h
#if !defined( __Delegate_h__ ) || !defined( FUNC_INCLUDING_INLINE_IMPL )
	#error "This inline header must only be included by Delegate.h"
#endif

#pragma once

// HEADER_UNIT_SKIP - Not included directly

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Crc.h"
#include "UObject/NameTypes.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Templates/IsConst.h"
#include "Templates/RemoveReference.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include <type_traits>

class FDelegateHandle;
class IDelegateInstance;
struct FWeakObjectPtr;
template <typename FuncType, typename UserPolicy> struct IBaseDelegateInstance;
template <typename T> struct TObjectPtr;

template <typename T>
T* ToRawPtr(const TObjectPtr<T>& Ptr);

template <typename To, typename From>
To* Cast(From* Src);

template<typename UserPolicy> class TMulticastDelegateBase;

/**
 * Unicast delegate template class.
 */
template <typename DelegateSignature, typename UserPolicy = FDefaultDelegateUserPolicy>
class TDelegate
{
	static_assert(sizeof(UserPolicy) == 0, "Expected a function signature for the delegate template parameter");
};

template <typename InRetValType, typename... ParamTypes, typename UserPolicy>
class TDelegate<InRetValType(ParamTypes...), UserPolicy> : public UserPolicy::FDelegateExtras
{
	using Super                         = typename UserPolicy::FDelegateExtras;
	using FuncType                      = InRetValType (ParamTypes...);
	using DelegateInstanceInterfaceType = IBaseDelegateInstance<FuncType, UserPolicy>;

	static_assert(std::is_convertible_v<typename UserPolicy::FDelegateInstanceExtras*, IDelegateInstance*>, "UserPolicy::FDelegateInstanceExtras should publicly inherit IDelegateInstance");
	static_assert(std::is_convertible_v<typename UserPolicy::FMulticastDelegateExtras*, TMulticastDelegateBase<UserPolicy>*>, "UserPolicy::FMulticastDelegateExtras should publicly inherit TMulticastDelegateBase<UserPolicy>");

	template <typename, typename>
	friend class TDelegate;

	template <typename>
	friend class TMulticastDelegateBase;

	template <typename, typename>
	friend class TMulticastDelegate;

private:
	// Make sure FDelegateBase's protected functions are not accidentally exposed through the TDelegate API
	using typename Super::FReadAccessScope;
	using Super::GetReadAccessScope;
	using typename Super::FWriteAccessScope;
	using Super::GetWriteAccessScope;

public:
	/** Type definition for return value type. */
	typedef InRetValType RetValType;
	typedef InRetValType TFuncType(ParamTypes...);

	/* Helper typedefs for getting a member function pointer type for the delegate with a given payload */
	template <typename... VarTypes>                     using TFuncPtr        = RetValType(*)(ParamTypes..., VarTypes...);
	template <typename UserClass, typename... VarTypes> using TMethodPtr      = typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., VarTypes...)>::Type;
	template <typename UserClass, typename... VarTypes> using TConstMethodPtr = typename TMemFunPtrType<true,  UserClass, RetValType(ParamTypes..., VarTypes...)>::Type;

public:

	/**
	 * Static: Creates a raw C++ pointer global function delegate
	 */
	template <typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateStatic(typename TIdentity<RetValType (*)(ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseStaticDelegateInstance<FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename FunctorType, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateLambda(FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseFunctorDelegateInstance<FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a weak shared pointer C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename UserClass, ESPMode Mode, typename FunctorType, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSPLambda(const TSharedRef<UserClass, Mode>& InUserObjectRef, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPLambdaDelegateInstance<UserClass, Mode, FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(InUserObjectRef, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename FunctorType, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSPLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPLambdaDelegateInstance<UserClass, decltype(InUserObject->AsShared())::Mode, FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a weak object C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename UserClass, typename FunctorType, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TWeakBaseFunctorDelegateInstance<UserClass, FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(InUserObject, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a raw C++ pointer member function delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseRawMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, decltype(InUserObject->AsShared())::Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, decltype(InUserObject->AsShared())::Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<const UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based (thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 *
	 * Note: This function is redundant, but is retained for backwards compatibility.  CreateSP() works in both thread-safe and not-thread-safe modes and should be preferred.
	 */
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based (thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 *
	 * Note: This function is redundant, but is retained for backwards compatibility.  CreateSP() works in both thread-safe and not-thread-safe modes and should be preferred.
	 */
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<const UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunctionName, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UObjectTemplate, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUFunction(TObjectPtr<UObjectTemplate> InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(ToRawPtr(InUserObject), InFunctionName, Forward<VarTypes>(Vars)...);
		return Result;
	}

	/**
	 * Static: Creates a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(ToRawPtr(InUserObject), InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	[[nodiscard]] inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		Result.template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(ToRawPtr(InUserObject), InFunc, Forward<VarTypes>(Vars)...);
		return Result;
	}

public:

	TDelegate() = default;

	inline TDelegate(TYPE_OF_NULLPTR)
	{
	}

	inline TDelegate(const TDelegate& Other)
	{
		CopyFrom(Other);
	}

	TDelegate& operator=(const TDelegate& Other)
	{
		CopyFrom(Other);
		return *this;
	}

	TDelegate(TDelegate&& Other) = default;
	TDelegate& operator=(TDelegate&& Other) = default;

public:

	/**
	 * Binds a raw C++ pointer global function delegate
	 */
	template <typename... VarTypes>
	inline void BindStatic(typename TBaseStaticDelegateInstance<FuncType, UserPolicy, std::decay_t<VarTypes>...>::FFuncPtr InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseStaticDelegateInstance<FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InFunc, Forward<VarTypes>(Vars)...);
	}
	
	/**
	 * Static: Binds a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename FunctorType, typename... VarTypes>
	inline void BindLambda(FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseFunctorDelegateInstance<FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
	}

	/**
	 * Static: Binds a weak shared pointer C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename UserClass, ESPMode Mode, typename FunctorType, typename... VarTypes>
	inline void BindSPLambda(const TSharedRef<UserClass, Mode>& InUserObjectRef, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseSPLambdaDelegateInstance<const UserClass, Mode, FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(InUserObjectRef, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename FunctorType, typename... VarTypes>
	inline void BindSPLambda(const UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseSPLambdaDelegateInstance<const UserClass, decltype(InUserObject->AsShared())::Mode, FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(StaticCastSharedRef<const UserClass>(InUserObject->AsShared()), Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
	}

	/**
	 * Static: Binds a weak object C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename UserClass, typename FunctorType, typename... VarTypes>
	inline void BindWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TWeakBaseFunctorDelegateInstance<UserClass, FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, std::decay_t<VarTypes>...>>(InUserObject, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...);
	}

	/**
	 * Binds a raw C++ pointer delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		Super::template CreateDelegateInstance<TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseRawMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
	}

	/**
	 * Binds a shared pointer-based member function delegate.  Shared pointer delegates keep a weak reference to your object.  You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	inline void BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	inline void BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
	}

	/**
	 * Binds a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, decltype(InUserObject->AsShared())::Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, decltype(InUserObject->AsShared())::Mode, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<const UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
	}

	/**
	 * Binds a shared pointer-based (thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 *
	 * Note: This function is redundant, but is retained for backwards compatibility.  BindSP() works in both thread-safe and not-thread-safe modes and should be preferred.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...);
	}

	/**
	 * Binds a shared pointer-based (thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 *
	 * Note: This function is redundant, but is retained for backwards compatibility.  BindSP() works in both thread-safe and not-thread-safe modes and should be preferred.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseSPMethodDelegateInstance<true, const UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(StaticCastSharedRef<const UserClass>(InUserObject->AsShared()), InFunc, Forward<VarTypes>(Vars)...);
	}

	/**
	 * Binds a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	inline void BindUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunctionName, Forward<VarTypes>(Vars)...);
	}
	template <typename UObjectTemplate, typename... VarTypes>
	inline void BindUFunction(TObjectPtr<UObjectTemplate> InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(ToRawPtr(InUserObject), InFunctionName, Forward<VarTypes>(Vars)...);
	}

	/**
	 * Binds a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		Super::template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(InUserObject, InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		Super::template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(ToRawPtr(InUserObject), InFunc, Forward<VarTypes>(Vars)...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		Super::template CreateDelegateInstance<TBaseUObjectMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, std::decay_t<VarTypes>...>>(ToRawPtr(InUserObject), InFunc, Forward<VarTypes>(Vars)...);
	}

public:
	/**
	 * Execute the delegate.
	 *
	 * If the function pointer is not valid, an error will occur. Check IsBound() before
	 * calling this method or use ExecuteIfBound() instead.
	 *
	 * @see ExecuteIfBound
	 */
	FORCEINLINE RetValType Execute(ParamTypes... Params) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const DelegateInstanceInterfaceType* LocalDelegateInstance = GetDelegateInstanceProtected();

		// If this assert goes off, Execute() was called before a function was bound to the delegate.
		// Consider using ExecuteIfBound() instead.
		checkSlow(LocalDelegateInstance != nullptr);

		return LocalDelegateInstance->Execute(Forward<ParamTypes>(Params)...);
	}

	/**
	 * Execute the delegate, but only if the function pointer is still valid
	 *
	 * @return  Returns true if the function was executed
	 */
	 // NOTE: Currently only delegates with no return value support ExecuteIfBound()
	template <
		// This construct is intended to disable this function when RetValType != void.
		// DummyRetValType exists to create a substitution which can fail, to achieve SFINAE.
		typename DummyRetValType = RetValType,
		std::enable_if_t<std::is_void<DummyRetValType>::value>* = nullptr
	>
	inline bool ExecuteIfBound(ParamTypes... Params) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		if (const DelegateInstanceInterfaceType* Ptr = GetDelegateInstanceProtected())
		{
			return Ptr->ExecuteIfSafe(Forward<ParamTypes>(Params)...);
		}

		return false;
	}

protected:
	/**
	 * Returns a pointer to the correctly-typed delegate instance.
	 */
	FORCEINLINE DelegateInstanceInterfaceType* GetDelegateInstanceProtected()
	{
		return static_cast<DelegateInstanceInterfaceType*>(Super::GetDelegateInstanceProtected());
	}
	FORCEINLINE const DelegateInstanceInterfaceType* GetDelegateInstanceProtected() const
	{
		return static_cast<const DelegateInstanceInterfaceType*>(Super::GetDelegateInstanceProtected());
	}

private:
	template<typename OtherUserPolicy>
	void CopyFrom(const TDelegate<FuncType, OtherUserPolicy>& Other)
	{
		if ((void*)&Other == (void*)this)
		{
			return;
		}

		// to not hold both delegates locked, make a local copy of `Other` and then move it into this instance
		TDelegate LocalCopy;

		{
			typename TDelegate<FuncType, OtherUserPolicy>::FReadAccessScope OtherReadScope = Other.GetReadAccessScope();

			// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
			using OtherDelegateInstanceInterfaceType = IBaseDelegateInstance<FuncType, OtherUserPolicy>;
			const OtherDelegateInstanceInterfaceType* OtherDelegateInstance = Other.GetDelegateInstanceProtected();

			if (OtherDelegateInstance != nullptr)
			{
				OtherDelegateInstance->CreateCopy(LocalCopy);
			}
		}

		*this = MoveTemp(LocalCopy);
	}

	// copying from delegates with different user policy
	template<typename OtherUserPolicy>
	explicit TDelegate(const TDelegate<FuncType, OtherUserPolicy>& Other)
	{
		CopyFrom(Other);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename DelegateSignature>
using TTSDelegate = TDelegate<DelegateSignature, FDefaultTSDelegateUserPolicy>;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Multicast delegate template base class, used for both normal and event multicast delegates.
 *
 * This class implements the functionality of multicast delegates. It is templated to the function signature
 * that it is compatible with. Use the various DECLARE_MULTICAST_DELEGATE and DECLARE_EVENT macros to create
 * actual delegate types.
 *
 * Multicast delegates offer no guarantees for the calling order of bound functions. As bindings get added
 * and removed over time, the calling order may change. Only bindings without return values are supported.
 */
template <typename DelegateSignature, typename UserPolicy = FDefaultDelegateUserPolicy>
class TMulticastDelegate
{
	static_assert(sizeof(DelegateSignature) == 0, "Expected a function signature for the delegate template parameter");
};

template <typename RetValType, typename... ParamTypes, typename UserPolicy>
class TMulticastDelegate<RetValType(ParamTypes...), UserPolicy>
{
	static_assert(sizeof(RetValType) == 0, "The return type of a multicast delegate must be void");
};

template <typename... ParamTypes, typename UserPolicy>
class TMulticastDelegate<void(ParamTypes...), UserPolicy> : public UserPolicy::FMulticastDelegateExtras
{
	using Super                         = typename UserPolicy::FMulticastDelegateExtras;
	using InvocationListType            = typename Super::InvocationListType;
	using DelegateInstanceInterfaceType = IBaseDelegateInstance<void (ParamTypes...), UserPolicy>;

public:
	/** Type definition for unicast delegate classes whose delegate instances are compatible with this delegate. */
	using FDelegate = TDelegate<void(ParamTypes...), UserPolicy>;

public:
	// Make sure TMulticastDelegateBase's public functions are publicly exposed through the TMulticastDelegate API
	using Super::Clear;
	using Super::IsBound;
	using Super::IsBoundToObject;
	using Super::RemoveAll;
	using Super::GetAllocatedSize;

private:
	// Make sure TMulticastDelegateBase's protected functions are not accidentally exposed through the TMulticastDelegate API
	using Super::AddDelegateInstance;
	using Super::RemoveDelegateInstance;

public:
	TMulticastDelegate() = default;

	TMulticastDelegate(const TMulticastDelegate& Other)
	{
		*this = Other;
	}

	TMulticastDelegate& operator=(const TMulticastDelegate& Other)
	{
		if (&Other != this)
		{
			Super::template CopyFrom<DelegateInstanceInterfaceType>(Other);
		}
		return *this;
	}

	TMulticastDelegate(TMulticastDelegate&&) = default;
	TMulticastDelegate& operator=(TMulticastDelegate&&) = default;

public:

	/**
	 * Adds a delegate instance to this multicast delegate's invocation list.
	 *
	 * @param Delegate The delegate to add.
	 */
	FDelegateHandle Add(FDelegate&& InNewDelegate)
	{
		return Super::AddDelegateInstance(MoveTemp(InNewDelegate));
	}

	/**
	 * Adds a delegate instance to this multicast delegate's invocation list.
	 *
	 * @param Delegate The delegate to add.
	 */
	FDelegateHandle Add(const FDelegate& InNewDelegate)
	{
		return Super::AddDelegateInstance(CopyTemp(InNewDelegate));
	}

	/**
	 * Adds a raw C++ pointer global function delegate
	 *
	 * @param	InFunc	Function pointer
	 */
	template <typename... VarTypes>
	inline FDelegateHandle AddStatic(typename TBaseStaticDelegateInstance<void (ParamTypes...), UserPolicy, std::decay_t<VarTypes>...>::FFuncPtr InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateStatic(InFunc, Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 *
	 * @param	InFunctor	Functor (e.g. Lambda)
	 */
	template<typename FunctorType, typename... VarTypes>
	inline FDelegateHandle AddLambda(FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateLambda(Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a weak shared pointer C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 *
	 * @param	InUserObjectRef	User object to bind to
	 * @param	InFunctor		Functor (e.g. Lambda)
	 */
	template <typename UserClass, typename FunctorType, typename... VarTypes>
	inline FDelegateHandle AddSPLambda(const UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateSPLambda(InUserObject, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a weak object C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunctor		Functor (e.g. Lambda)
	 */
	template<typename UserClass, typename FunctorType, typename... VarTypes>
	inline FDelegateHandle AddWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateWeakLambda(InUserObject, Forward<FunctorType>(InFunctor), Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a raw C++ pointer delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Super::AddDelegateInstance(FDelegate::CreateRaw(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateRaw(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObjectRef	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	inline FDelegateHandle AddSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Super::AddDelegateInstance(FDelegate::CreateSP(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	inline FDelegateHandle AddSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateSP(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Super::AddDelegateInstance(FDelegate::CreateSP(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateSP(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a shared pointer-based (thread-safe) member function delegate.  Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObjectRef	User object to bind to
	 * @param	InFunc			Class method function address
	 *
	 * Note: This function is redundant, but is retained for backwards compatibility.  AddSP() works in both thread-safe and not-thread-safe modes and should be preferred.
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Super::AddDelegateInstance(FDelegate::CreateThreadSafeSP(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateThreadSafeSP(InUserObjectRef, InFunc, Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a shared pointer-based (thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 *
	 * Note: This function is redundant, but is retained for backwards compatibility.  AddSP() works in both thread-safe and not-thread-safe modes and should be preferred.
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Super::AddDelegateInstance(FDelegate::CreateThreadSafeSP(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateThreadSafeSP(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunctionName			Class method function address
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	inline FDelegateHandle AddUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateUFunction(InUserObject, InFunctionName, Forward<VarTypes>(Vars)...));
	}
	template <typename UObjectTemplate, typename... VarTypes>
	inline FDelegateHandle AddUFunction(TObjectPtr<UObjectTemplate> InUserObject, const FName& InFunctionName, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateUFunction(InUserObject, InFunctionName, Forward<VarTypes>(Vars)...));
	}

	/**
	 * Adds a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Super::AddDelegateInstance(FDelegate::CreateUObject(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateUObject(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Super::AddDelegateInstance(FDelegate::CreateUObject(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., std::decay_t<VarTypes>...)>::Type InFunc, VarTypes&&... Vars)
	{
		return Super::AddDelegateInstance(FDelegate::CreateUObject(InUserObject, InFunc, Forward<VarTypes>(Vars)...));
	}

public:

	/**
	 * Removes a delegate instance from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * Note that the order of the delegate instances may not be preserved!
	 *
	 * @param Handle The handle of the delegate instance to remove.
	 * @return  true if the delegate was successfully removed.
	 */
	bool Remove( FDelegateHandle Handle )
	{
		bool bResult = false;
		if (Handle.IsValid())
		{
			bResult = RemoveDelegateInstance(Handle);
		}
		return bResult;
	}

	/**
	 * Broadcasts this delegate to all bound objects, except to those that may have expired.
	 *
	 * The constness of this method is a lie, but it allows for broadcasting from const functions.
	 */
	void Broadcast(ParamTypes... Params) const
	{
		Super::template Broadcast<DelegateInstanceInterfaceType, ParamTypes...>(Params...);
	}
};

template <typename DelegateSignature>
using TTSMulticastDelegate = TMulticastDelegate<DelegateSignature, FDefaultTSDelegateUserPolicy>;


/**
 * Dynamic delegate template class (UObject-based, serializable).  You'll use the various DECLARE_DYNAMIC_DELEGATE
 * macros to create the actual delegate type, templated to the function signature the delegate is compatible with.
 * Then, you can create an instance of that class when you want to assign functions to the delegate.
 */
template <typename ThreadSafetyMode, typename RetValType, typename... ParamTypes>
class TBaseDynamicDelegate : public TScriptDelegate<ThreadSafetyMode>
{
public:
	/**
	 * Default constructor
	 */
	TBaseDynamicDelegate() { }

	/**
	 * Construction from an FScriptDelegate must be explicit.  This is really only used by UObject system internals.
	 *
	 * @param	InScriptDelegate	The delegate to construct from by copying
	 */
	explicit TBaseDynamicDelegate( const TScriptDelegate<ThreadSafetyMode>& InScriptDelegate )
		: TScriptDelegate<ThreadSafetyMode>( InScriptDelegate )
	{ }

	/**
	 * Templated helper class to define a typedef for user's method pointer, then used below
	 */
	template< class UserClass >
	class TMethodPtrResolver
	{
	public:
		typedef RetValType (UserClass::*FMethodPtr)(ParamTypes... Params);
	};

	/**
	 * Binds a UObject instance and a UObject method address to this delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call BindDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_BindDynamic( UserClass* InUserObject, typename TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		// NOTE: If you hit a compile error on the following line, it means you're trying to use a non-UObject type
		//       with this delegate, which is not supported
		this->Object = Cast<UObject>(InUserObject);

		// Store the function name.  The incoming function name was generated by a macro and includes the method's class name.
		this->FunctionName = InFunctionName;

		ensureMsgf(this->IsBound(), TEXT("Unable to bind delegate to '%s' (function might not be marked as a UFUNCTION or object may be pending kill)"), *InFunctionName.ToString());
	}
	template< class UserClass >
	void __Internal_BindDynamic( TObjectPtr<UserClass> InUserObject, typename TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		__Internal_BindDynamic(ToRawPtr(InUserObject), InMethodPtr, InFunctionName);
	}

	friend uint32 GetTypeHash(const TBaseDynamicDelegate& Key)
	{
		return FCrc::MemCrc_DEPRECATED(&Key,sizeof(Key));
	}

	// NOTE:  Execute() method must be defined in derived classes

	// NOTE:  ExecuteIfBound() method must be defined in derived classes
};


/**
 * Dynamic multi-cast delegate template class (UObject-based, serializable).  You'll use the various
 * DECLARE_DYNAMIC_MULTICAST_DELEGATE macros to create the actual delegate type, templated to the function
 * signature the delegate is compatible with.   Then, you can create an instance of that class when you
 * want to assign functions to the delegate.
 */
template <typename ThreadSafetyMode, typename RetValType, typename... ParamTypes>
class TBaseDynamicMulticastDelegate : public TMulticastScriptDelegate<ThreadSafetyMode>
{
public:
	/** The actual single-cast delegate class for this multi-cast delegate */
	typedef TBaseDynamicDelegate<ThreadSafetyMode, RetValType, ParamTypes...> FDelegate;

	/**
	 * Default constructor
	 */
	TBaseDynamicMulticastDelegate() { }

	/**
	 * Construction from an FMulticastScriptDelegate must be explicit.  This is really only used by UObject system internals.
	 *
	 * @param	InScriptDelegate	The delegate to construct from by copying
	 */
	explicit TBaseDynamicMulticastDelegate( const TMulticastScriptDelegate<ThreadSafetyMode>& InMulticastScriptDelegate )
		: TMulticastScriptDelegate<ThreadSafetyMode>( InMulticastScriptDelegate )
	{ }

	/**
	 * Tests if a UObject instance and a UObject method address pair are already bound to this multi-cast delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 * @return	True if the instance/method is already bound.
	 *
	 * NOTE:  Do not call this function directly.  Instead, call IsAlreadyBound() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	bool __Internal_IsAlreadyBound( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName ) const
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually using the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		return this->Contains( InUserObject, InFunctionName );
	}
	template< class UserClass >
	bool __Internal_IsAlreadyBound( TObjectPtr<UserClass> InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName ) const
	{
		return __Internal_IsAlreadyBound(ToRawPtr(InUserObject), InMethodPtr, InFunctionName);
	}

	/**
	 * Binds a UObject instance and a UObject method address to this multi-cast delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call AddDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_AddDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FDelegate NewDelegate;
		NewDelegate.__Internal_BindDynamic( InUserObject, InMethodPtr, InFunctionName );

		this->Add( NewDelegate );
	}
	template< class UserClass >
	void __Internal_AddDynamic( TObjectPtr<UserClass> InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		__Internal_AddDynamic(ToRawPtr(InUserObject), InMethodPtr, InFunctionName);
	}

	/**
	 * Binds a UObject instance and a UObject method address to this multi-cast delegate, but only if it hasn't been bound before.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call AddUniqueDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_AddUniqueDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FDelegate NewDelegate;
		NewDelegate.__Internal_BindDynamic( InUserObject, InMethodPtr, InFunctionName );

		this->AddUnique( NewDelegate );
	}
	template< class UserClass >
	void __Internal_AddUniqueDynamic( TObjectPtr<UserClass> InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		__Internal_AddUniqueDynamic(ToRawPtr(InUserObject), InMethodPtr, InFunctionName);
	}

	/**
	 * Unbinds a UObject instance and a UObject method address from this multi-cast delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call RemoveDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_RemoveDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		this->Remove( InUserObject, InFunctionName );
	}
	template< class UserClass >
	void __Internal_RemoveDynamic( TObjectPtr<UserClass> InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		__Internal_RemoveDynamic(ToRawPtr(InUserObject), InMethodPtr, InFunctionName);
	}

	// NOTE:  Broadcast() method must be defined in derived classes
};
