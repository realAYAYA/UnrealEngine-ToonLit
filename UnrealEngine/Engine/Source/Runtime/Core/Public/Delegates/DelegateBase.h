// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Math/UnrealMathUtility.h"
#include "UObject/NameTypes.h"
#include "Delegates/DelegateAccessHandler.h"
#include "Delegates/DelegateInstancesImplFwd.h"
#include "Delegates/DelegateSettings.h"
#include "Delegates/IDelegateInstance.h"

#if !defined(_WIN32) || defined(_WIN64) || (defined(ALLOW_DELEGATE_INLINE_ALLOCATORS_ON_WIN32) && ALLOW_DELEGATE_INLINE_ALLOCATORS_ON_WIN32)
	typedef TAlignedBytes<16, 16> FAlignedInlineDelegateType;
	#if !defined(NUM_DELEGATE_INLINE_BYTES) || NUM_DELEGATE_INLINE_BYTES == 0
		typedef FHeapAllocator FDelegateAllocatorType;
	#elif NUM_DELEGATE_INLINE_BYTES < 0 || (NUM_DELEGATE_INLINE_BYTES % 16) != 0
		#error NUM_DELEGATE_INLINE_BYTES must be a multiple of 16
	#else
		typedef TInlineAllocator<(NUM_DELEGATE_INLINE_BYTES / 16)> FDelegateAllocatorType;
	#endif
#else
	// ... except on Win32, because we can't pass 16-byte aligned types by value, as some delegates are
	// so we'll just keep it heap-allocated, which are always sufficiently aligned.
	typedef TAlignedBytes<16, 8> FAlignedInlineDelegateType;
	typedef FHeapAllocator FDelegateAllocatorType;
#endif

template <typename UserPolicy>
class TMulticastDelegateBase;

template <typename UserPolicy>
class TDelegateBase;

ALIAS_TEMPLATE_TYPE_LAYOUT(template<typename ElementType>, FDelegateAllocatorType::ForElementType<ElementType>, void*);

// not thread-safe version, with automatic race detection in dev builds
struct FDefaultDelegateUserPolicy
{
	// To extend delegates, you should implement a policy struct like this and pass it as the second template
	// argument to TDelegate and TMulticastDelegate.  This policy struct containing three classes called:
	// 
	// FDelegateInstanceExtras:
	//   - Must publicly inherit IDelegateInstance.
	//   - Should contain any extra data and functions injected into a binding (the object which holds and
	//     is able to invoke the binding passed to FMyDelegate::CreateSP, FMyDelegate::CreateLambda etc.).
	//   - This binding is not available through the public API of the delegate, but is accessible to FDelegateExtras.
	//
	// FDelegateExtras:
	//   - Must publicly inherit TDelegateBase<FThreadSafetyMode>.
	//   - Should contain any extra data and functions injected into a delegate (the object which holds an
	//     FDelegateInstance-derived object, above).
	//   - Public data members and member functions are accessible directly through the TDelegate object.
	//   - Typically member functions in this class will forward calls to the inner FDelegateInstanceExtras,
	//     by downcasting the result of a call to GetDelegateInstanceProtected().
	//
	// FMulticastDelegateExtras:
	//   - Must publicly inherit TMulticastDelegateBase<FYourUserPolicyStruct>.
	//   - Should contain any extra data and functions injected into a multicast delegate (the object which
	//     holds an array of FDelegateExtras-derived objects which is the invocation list).
	//   - Public data members and member functions are accessible directly through the TMulticastDelegate object.

	using FDelegateInstanceExtras = IDelegateInstance;
	using FThreadSafetyMode =
#if UE_DETECT_DELEGATES_RACE_CONDITIONS
		FNotThreadSafeDelegateMode;
#else
		FNotThreadSafeNotCheckedDelegateMode;
#endif
	using FDelegateExtras = TDelegateBase<FThreadSafetyMode>;
	using FMulticastDelegateExtras = TMulticastDelegateBase<FDefaultDelegateUserPolicy>;
};

// thread-safe version
struct FDefaultTSDelegateUserPolicy
{
	// see `FDefaultDelegateUserPolicy` for documentation
	using FDelegateInstanceExtras = IDelegateInstance;
	using FThreadSafetyMode = FThreadSafeDelegateMode;
	using FDelegateExtras = TDelegateBase<FThreadSafetyMode>;
	using FMulticastDelegateExtras = TMulticastDelegateBase<FDefaultTSDelegateUserPolicy>;
};

// not thread-safe version, no race detection. used primarily for deprecated unsafe delegates that must be kept running for backward compatibility
struct FNotThreadSafeNotCheckedDelegateUserPolicy
{
	using FDelegateInstanceExtras = IDelegateInstance;
	using FThreadSafetyMode = FNotThreadSafeNotCheckedDelegateMode;
	using FDelegateExtras = TDelegateBase<FThreadSafetyMode>;
	using FMulticastDelegateExtras = TMulticastDelegateBase<FNotThreadSafeNotCheckedDelegateUserPolicy>;
};

/**
 * Base class for unicast delegates.
 */
template<typename ThreadSafetyMode>
class TDelegateBase : public TDelegateAccessHandlerBase<ThreadSafetyMode>
{
private:
	using Super = TDelegateAccessHandlerBase<ThreadSafetyMode>;

	template <typename>
	friend class TMulticastDelegateBase;

	template <typename>
	friend class TDelegateBase;

	template <class, typename, typename, typename...>
	friend class TBaseUFunctionDelegateInstance;

	template <bool, class, ESPMode, typename, typename, typename...>
	friend class TBaseSPMethodDelegateInstance;

	template <typename, ESPMode, typename, typename, typename, typename...>
	friend class TBaseSPLambdaDelegateInstance;

	template <bool, class, typename, typename, typename...>
	friend class TBaseRawMethodDelegateInstance;

	template <bool, class, typename, typename, typename...>
	friend class TBaseUObjectMethodDelegateInstance;

	template <typename, typename, typename...>
	friend class TBaseStaticDelegateInstance;

	template <typename, typename, typename, typename...>
	friend class TBaseFunctorDelegateInstance;

	template <typename, typename, typename, typename, typename...>
	friend class TWeakBaseFunctorDelegateInstance;

protected:
	using typename Super::FReadAccessScope;
	using Super::GetReadAccessScope;
	using typename Super::FWriteAccessScope;
	using Super::GetWriteAccessScope;

	explicit TDelegateBase() = default;

public:
	~TDelegateBase()
	{
		Unbind();
	}

	TDelegateBase(TDelegateBase&& Other)
	{
		MoveConstruct(MoveTemp(Other));
	}

	TDelegateBase& operator=(TDelegateBase&& Other)
	{
		MoveAssign(MoveTemp(Other));
		return *this;
	}

	// support for moving from delegates with different thread-safety mode
	template<typename OtherThreadSafetyMode>
	explicit TDelegateBase(TDelegateBase<OtherThreadSafetyMode>&& Other)
	{
		MoveConstruct(MoveTemp(Other));
	}

	/**
	 * Unbinds this delegate
	 */
	FORCEINLINE void Unbind()
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		UnbindUnchecked();
	}

	/**
	 * Returns the amount of memory allocated by this delegate, not including sizeof(*this).
	 */
	SIZE_T GetAllocatedSize() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		return DelegateAllocator.GetAllocatedSize(DelegateSize, sizeof(FAlignedInlineDelegateType));
	}

#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME

	/**
	 * Tries to return the name of a bound function.  Returns NAME_None if the delegate is unbound or
	 * a binding name is unavailable.
	 *
	 * Note: Only intended to be used to aid debugging of delegates.
	 *
	 * @return The name of the bound function, NAME_None if no name was available.
	 */
	FName TryGetBoundFunctionName() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return DelegateInstance ? DelegateInstance->TryGetBoundFunctionName() : NAME_None;
	}

#endif

	/**
	 * If this is a UFunction or UObject delegate, return the UObject.
	 *
	 * @return The object associated with this delegate if there is one.
	 */
	FORCEINLINE class UObject* GetUObject( ) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return DelegateInstance ? DelegateInstance->GetUObject() : nullptr;
	}

	/**
	 * Checks to see if the user object bound to this delegate is still valid.
	 *
	 * @return True if the user object is still valid and it's safe to execute the function call.
	 */
	FORCEINLINE bool IsBound( ) const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return DelegateInstance && DelegateInstance->IsSafeToExecute();
	}

	/** 
	 * Returns a pointer to an object bound to this delegate, intended for quick lookup in the timer manager,
	 *
	 * @return A pointer to an object referenced by the delegate.
	 */
	FORCEINLINE const void* GetObjectForTimerManager() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return DelegateInstance ? DelegateInstance->GetObjectForTimerManager() : nullptr;
	}

	/**
	 * Returns the address of the method pointer which can be used to learn the address of the function that will be executed.
	 * Returns nullptr if this delegate type does not directly invoke a function pointer.
	 *
	 * Note: Only intended to be used to aid debugging of delegates.
	 *
	 * @return The address of the function pointer that would be executed by this delegate
	 */
	uint64 GetBoundProgramCounterForTimerManager() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return DelegateInstance ? DelegateInstance->GetBoundProgramCounterForTimerManager() : 0;
	}

	/** 
	 * Checks to see if this delegate is bound to the given user object.
	 *
	 * @return True if this delegate is bound to InUserObject, false otherwise.
	 */
	FORCEINLINE bool IsBoundToObject( void const* InUserObject ) const
	{
		if (!InUserObject)
		{
			return false;
		}

		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return DelegateInstance && DelegateInstance->HasSameObject(InUserObject);
	}

	/** 
	 * Checks to see if this delegate can ever become valid again - if not, it can
	 * be removed from broadcast lists or otherwise repurposed.
	 *
	 * @return True if the delegate is compatable, false otherwise.
	 */
	FORCEINLINE bool IsCompactable() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return !DelegateInstance || DelegateInstance->IsCompactable();
	}

	/**
	 * Gets a handle to the delegate.
	 *
	 * @return The delegate instance.
	 */
	FORCEINLINE FDelegateHandle GetHandle() const
	{
		FReadAccessScope ReadScope = GetReadAccessScope();

		const IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		return DelegateInstance ? DelegateInstance->GetHandle() : FDelegateHandle{};
	}

protected:
	/**
	 * "emplacement" of delegate instance of the given type
	 */
	template<typename DelegateInstanceType, typename... DelegateInstanceParams>
	void CreateDelegateInstance(DelegateInstanceParams&&... Params)
	{
		FWriteAccessScope WriteScope = GetWriteAccessScope();

		IDelegateInstance* DelegateInstance = GetDelegateInstanceProtected();
		if (DelegateInstance)
		{
			DelegateInstance->~IDelegateInstance();
		}

		new(Allocate(sizeof(DelegateInstanceType))) DelegateInstanceType(Forward<DelegateInstanceParams>(Params)...);
	}

	/**
	 * Gets the delegate instance.  Not intended for use by user code.
	 *
	 * @return The delegate instance.
	 * @see SetDelegateInstance
	 */
	FORCEINLINE IDelegateInstance* GetDelegateInstanceProtected()
	{
		return DelegateSize ? (IDelegateInstance*)DelegateAllocator.GetAllocation() : nullptr;
	}

	FORCEINLINE const IDelegateInstance* GetDelegateInstanceProtected() const
	{
		return DelegateSize ? (const IDelegateInstance*)DelegateAllocator.GetAllocation() : nullptr;
	}

private:
	void* Allocate(int32 Size)
	{
		int32 NewDelegateSize = FMath::DivideAndRoundUp(Size, (int32)sizeof(FAlignedInlineDelegateType));
		if (DelegateSize != NewDelegateSize)
		{
			DelegateAllocator.ResizeAllocation(0, NewDelegateSize, sizeof(FAlignedInlineDelegateType));
			DelegateSize = NewDelegateSize;
		}

		return DelegateAllocator.GetAllocation();
	}

	template<typename OtherThreadSafetyMode>
	void MoveConstruct(TDelegateBase<OtherThreadSafetyMode>&& Other)
	{
		typename TDelegateBase<OtherThreadSafetyMode>::FWriteAccessScope OtherWriteScope = Other.GetWriteAccessScope();

		DelegateAllocator.MoveToEmpty(Other.DelegateAllocator);
		DelegateSize = Other.DelegateSize;
		Other.DelegateSize = 0;
	}

	template<typename OtherThreadSafetyMode>
	void MoveAssign(TDelegateBase<OtherThreadSafetyMode>&& Other)
	{
		FDelegateAllocatorType::ForElementType<FAlignedInlineDelegateType> OtherDelegateAllocator;
		int32 OtherDelegateSize;
		{
			typename TDelegateBase<OtherThreadSafetyMode>::FWriteAccessScope OtherWriteScope = Other.GetWriteAccessScope();
			OtherDelegateAllocator.MoveToEmpty(Other.DelegateAllocator);
			OtherDelegateSize = Other.DelegateSize;
			Other.DelegateSize = 0;
		}

		{
			FWriteAccessScope WriteScope = GetWriteAccessScope();

			UnbindUnchecked();
			DelegateAllocator.MoveToEmpty(OtherDelegateAllocator);
			DelegateSize = OtherDelegateSize;
		}
	}

private:
	FORCEINLINE void UnbindUnchecked()
	{
		if (IDelegateInstance* Ptr = GetDelegateInstanceProtected())
		{
			Ptr->~IDelegateInstance();
			DelegateAllocator.ResizeAllocation(0, 0, sizeof(FAlignedInlineDelegateType));
			DelegateSize = 0;
		}
	}

	FDelegateAllocatorType::ForElementType<FAlignedInlineDelegateType> DelegateAllocator;
	int32 DelegateSize = 0;
};
