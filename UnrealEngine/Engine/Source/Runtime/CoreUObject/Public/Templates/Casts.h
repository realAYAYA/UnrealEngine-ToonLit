// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Templates/LosesQualifiersFromTo.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectHandle.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include <type_traits>

#define UE_USE_CAST_FLAGS (USTRUCT_FAST_ISCHILDOF_IMPL != USTRUCT_ISCHILDOF_STRUCTARRAY)

#ifndef UE_ENABLE_UNRELATED_CAST_WARNINGS
#define UE_ENABLE_UNRELATED_CAST_WARNINGS 1
#endif

class AActor;
class APawn;
class APlayerController;
class FField;
class FSoftClassProperty;
class UBlueprint;
class ULevel;
class UPrimitiveComponent;
class USceneComponent;
class USkeletalMeshComponent;
class USkinnedMeshComponent;
class UStaticMeshComponent;
/// @cond DOXYGEN_WARNINGS
template<class TClass> class TSubclassOf;
template <typename Type> struct TCastFlags;
/// @endcond

[[noreturn]] COREUOBJECT_API void CastLogError(const TCHAR* FromType, const TCHAR* ToType);

/**
 * Metafunction which detects whether or not a class is an IInterface.  Rules:
 *
 * 1. A UObject is not an IInterface.
 * 2. A type without a UClassType typedef member is not an IInterface.
 * 3. A type whose UClassType::StaticClassFlags does not have CLASS_Interface set is not an IInterface.
 *
 * Otherwise, assume it's an IInterface.
 */
template <typename T, bool bIsAUObject_IMPL = std::is_convertible_v<T*, const volatile UObject*>>
struct TIsIInterface
{
	enum { Value = false };
};

template <typename T>
struct TIsIInterface<T, false>
{
	template <typename U> static char (&Resolve(typename U::UClassType*))[(U::UClassType::StaticClassFlags & CLASS_Interface) ? 2 : 1];
	template <typename U> static char (&Resolve(...))[1];

	enum { Value = sizeof(Resolve<T>(0)) - 1 };
};

template <typename T>
FORCEINLINE FString GetTypeName()
{
	if constexpr (TIsIInterface<T>::Value)
	{
		return T::UClassType::StaticClass()->GetName();
	}
	else
	{
		return T::StaticClass()->GetName();
	}
}

template <typename Type>
struct TCastFlags
{
	static const EClassCastFlags Value = CASTCLASS_None;
};

// Dynamically cast an object type-safely.
template <typename To, typename From>
FORCEINLINE To* Cast(From* Src)
{
	static_assert(sizeof(From) > 0 && sizeof(To) > 0, "Attempting to cast between incomplete types");

	if (Src)
	{
		if constexpr (TIsIInterface<From>::Value)
		{
			if (UObject* Obj = Src->_getUObject())
			{
				if constexpr (TIsIInterface<To>::Value)
				{
					return (To*)Obj->GetInterfaceAddress(To::UClassType::StaticClass());
				}
				else
				{
					if constexpr (std::is_same_v<To, UObject>)
					{
						return Obj;
					}
					else
					{
						if (Obj->IsA<To>())
						{
							return (To*)Obj;
						}
					}
				}
			}
		}
		else if constexpr (UE_USE_CAST_FLAGS && TCastFlags<To>::Value != CASTCLASS_None)
		{
			if constexpr (std::is_base_of_v<To, From>)
			{
				return (To*)Src;
			}
			else
			{
#if UE_ENABLE_UNRELATED_CAST_WARNINGS
				UE_STATIC_ASSERT_WARN((std::is_base_of_v<From, To>), "Attempting to use Cast<> on types that are not related");
#endif
				if (((const UObject*)Src)->GetClass()->HasAnyCastFlag(TCastFlags<To>::Value))
				{
					return (To*)Src;
				}
			}
		}
		else
		{
			static_assert(std::is_base_of_v<UObject, From>, "Attempting to use Cast<> on a type that is not a UObject or an Interface");

			if constexpr (TIsIInterface<To>::Value)
			{
				return (To*)((UObject*)Src)->GetInterfaceAddress(To::UClassType::StaticClass());
			}
			else if constexpr (std::is_base_of_v<To, From>)
			{
				return Src;
			}
			else
			{
#if UE_ENABLE_UNRELATED_CAST_WARNINGS
				UE_STATIC_ASSERT_WARN((std::is_base_of_v<From, To>), "Attempting to use Cast<> on types that are not related");
#endif
				if (((const UObject*)Src)->IsA<To>())
				{
					return (To*)Src;
				}
			}
		}
	}

	return nullptr;
}

template< class T >
FORCEINLINE T* ExactCast( UObject* Src )
{
	return Src && (Src->GetClass() == T::StaticClass()) ? (T*)Src : nullptr;
}

#if DO_CHECK

	// Helper function to get the full name for UObjects and UInterfaces
	template <typename T>
	FString GetFullNameForCastLogError(T* InObjectOrInterface)
	{
		// A special version for FFields
		if constexpr (std::is_base_of_v<FField, T>)
		{
			return GetFullNameSafe(InObjectOrInterface);
		}
		else if constexpr (std::is_base_of_v<UObject, T>)
    	{
    		return InObjectOrInterface->GetFullName();;
    	}
		else
		{
			return Cast<UObject>(InObjectOrInterface)->GetFullName();
		}
	}

	template <typename To, typename From>
	FUNCTION_NON_NULL_RETURN_START
		To* CastChecked(From* Src)
	FUNCTION_NON_NULL_RETURN_END
	{
		static_assert(sizeof(From) > 0 && sizeof(To) > 0, "Attempting to cast between incomplete types");

		if (!Src)
		{
			CastLogError(TEXT("nullptr"), *GetTypeName<To>());
		}

		To* Result = Cast<To>(Src);
		if (!Result)
		{
			CastLogError(*GetFullNameForCastLogError(Src), *GetTypeName<To>());
		}

		return Result;
	}

	template <typename To, typename From>
	To* CastChecked(From* Src, ECastCheckedType::Type CheckType)
	{
		static_assert(sizeof(From) > 0 && sizeof(To) > 0, "Attempting to cast between incomplete types");

		if (Src)
		{
			To* Result = Cast<To>(Src);
			if (!Result)
			{
				CastLogError(*GetFullNameForCastLogError(Src), *GetTypeName<To>());
			}

			return Result;
		}

		if (CheckType == ECastCheckedType::NullChecked)
		{
			CastLogError(TEXT("nullptr"), *GetTypeName<To>());
		}

		return nullptr;
	}

#else

	template <typename To, typename From>
	FUNCTION_NON_NULL_RETURN_START
		FORCEINLINE To* CastChecked(From* Src)
	FUNCTION_NON_NULL_RETURN_END
	{
		static_assert(sizeof(From) > 0 && sizeof(To) > 0, "Attempting to cast between incomplete types");

		if (!Src)
		{
			return nullptr;
		}

		if constexpr (TIsIInterface<From>::Value)
		{
			UObject* Obj = Src->_getUObject();
			if constexpr (TIsIInterface<To>::Value)
			{
				return (To*)Obj->GetInterfaceAddress(To::UClassType::StaticClass());
			}
			else
			{
				return (To*)Obj;
			}
		}
		else if constexpr (TIsIInterface<To>::Value)
		{
			return (To*)((UObject*)Src)->GetInterfaceAddress(To::UClassType::StaticClass());
		}
		else
		{
			return (To*)Src;
		}
	}

	template <typename To, typename From>
	FORCEINLINE To* CastChecked(From* Src, ECastCheckedType::Type CheckType)
	{
		return CastChecked<To>(Src);
	}

#endif

// auto weak versions
template< class T, class U > FORCEINLINE T* Cast       ( const TWeakObjectPtr<U>& Src                                                                   ) { return Cast       <T>(Src.Get()); }
template< class T, class U > FORCEINLINE T* ExactCast  ( const TWeakObjectPtr<U>& Src                                                                   ) { return ExactCast  <T>(Src.Get()); }
template< class T, class U > FORCEINLINE T* CastChecked( const TWeakObjectPtr<U>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(Src.Get(), CheckType); }

// object ptr versions
template <typename To, typename From>
FORCEINLINE typename TCopyQualifiersFromTo<From, To>::Type* Cast(const TObjectPtr<From>& InSrc)
{
	static_assert(sizeof(To) > 0 && sizeof(From) > 0, "Attempting to cast between incomplete types");

	const FObjectPtr& Src = (const FObjectPtr&)InSrc;

	if constexpr (UE_USE_CAST_FLAGS && TCastFlags<To>::Value != CASTCLASS_None)
	{
		if (Src)
		{
			if constexpr (std::is_base_of_v<To, From>)
			{
				return (To*)Src.Get();
			}
			else
			{
	#if UE_ENABLE_UNRELATED_CAST_WARNINGS
				UE_STATIC_ASSERT_WARN((std::is_base_of_v<From, To>), "Attempting to use Cast<> on types that are not related");
	#endif
				if (Src.GetClass()->HasAnyCastFlag(TCastFlags<To>::Value))
				{
					return (To*)Src.Get();
				}
			}
		}
	}
	else if constexpr (TIsIInterface<To>::Value)
	{
		const UObject* SrcObj = UE::CoreUObject::Private::ResolveObjectHandleNoRead(Src.GetHandleRef());
		if (SrcObj)
		{
			UE::CoreUObject::Private::OnHandleRead(SrcObj);
			return (To*)Src.Get()->GetInterfaceAddress(To::UClassType::StaticClass());
		}
	}
	else if constexpr (std::is_base_of_v<To, From>)
	{
		if (Src)
		{
			return (To*)Src.Get();
		}
	}
	else
	{
#if UE_ENABLE_UNRELATED_CAST_WARNINGS
		UE_STATIC_ASSERT_WARN((std::is_base_of_v<From, To>), "Attempting to use Cast<> on types that are not related");
#endif
		if (Src && Src.IsA<To>())
		{
			return (To*)Src.Get();
		}
	}

	return nullptr;
}

template <typename To, typename From>
FORCEINLINE typename TCopyQualifiersFromTo<From, To>::Type* ExactCast(const TObjectPtr<From>& Src)
{
	static_assert(sizeof(To) > 0, "Attempting to cast to an incomplete type");

	UObject* SrcObj = UE::CoreUObject::Private::ResolveObjectHandleNoRead(((const FObjectPtr&)Src).GetHandleRef());
	if (SrcObj && (SrcObj->GetClass() == To::StaticClass()))
	{
		UE::CoreUObject::Private::OnHandleRead(SrcObj);
		return (To*)SrcObj;
	}
	return nullptr;
}

template <typename To, typename From>
FORCEINLINE typename TCopyQualifiersFromTo<From, To>::Type* CastChecked(const TObjectPtr<From>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked)
{
	static_assert(sizeof(From) > 0 && sizeof(To) > 0, "Attempting to cast between incomplete types");

#if DO_CHECK
	if (Src)
	{
		auto* Result = Cast<To>(Src);
		if (!Result)
		{
			CastLogError(*GetFullNameForCastLogError(Src.Get()), *GetTypeName<To>());
		}

		return Result;
	}

	if (CheckType == ECastCheckedType::NullChecked)
	{
		CastLogError(TEXT("nullptr"), *GetTypeName<To>());
	}

	return nullptr;
#else
	if constexpr (UE_USE_CAST_FLAGS && TCastFlags<To>::Value != CASTCLASS_None)
	{
		return (To*)((const FObjectPtr&)Src).Get();
	}
	else if constexpr (TIsIInterface<To>::Value)
	{
		UObject* SrcObj = UE::CoreUObject::Private::ResolveObjectHandleNoRead(((const FObjectPtr&)Src).GetHandleRef());
		UE::CoreUObject::Private::OnHandleRead(SrcObj);
		return (To*)((const FObjectPtr&)Src).Get()->GetInterfaceAddress(To::UClassType::StaticClass());
	}
	else
	{
		return (To*)((const FObjectPtr&)Src).Get();
	}
#endif
}

// TSubclassOf versions
template< class T, class U > FORCEINLINE T* Cast       ( const TSubclassOf<U>& Src                                                                   ) { return Cast       <T>(*Src); }
template< class T, class U > FORCEINLINE T* CastChecked( const TSubclassOf<U>& Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(*Src, CheckType); }

// Const versions of the casts
template< class T, class U > FORCEINLINE const T* Cast       ( const U      * Src                                                                   ) { return Cast       <T>(const_cast<U      *>(Src)); }
template< class T          > FORCEINLINE const T* ExactCast  ( const UObject* Src                                                                   ) { return ExactCast  <T>(const_cast<UObject*>(Src)); }
template< class T, class U > FORCEINLINE const T* CastChecked( const U      * Src, ECastCheckedType::Type CheckType = ECastCheckedType::NullChecked ) { return CastChecked<T>(const_cast<U      *>(Src), CheckType); }

#define DECLARE_CAST_BY_FLAG_FWD(ClassName) class ClassName;
#define DECLARE_CAST_BY_FLAG_CAST(ClassName) \
	template <> \
	struct TCastFlags<ClassName> \
	{ \
		static const EClassCastFlags Value = CASTCLASS_##ClassName; \
	}; \
	template <> \
	struct TCastFlags<const ClassName> \
	{ \
		static const EClassCastFlags Value = CASTCLASS_##ClassName; \
	};

#define DECLARE_CAST_BY_FLAG(ClassName) \
	DECLARE_CAST_BY_FLAG_FWD(ClassName) \
	DECLARE_CAST_BY_FLAG_CAST(ClassName)

#define FINISH_DECLARING_CAST_FLAGS // intentionally defined to do nothing.

// Define a macro that declares all the cast flags.
// This allows us to reuse these declarations elsewhere to define other properties for these classes.
// Note: When adding an item to this list, you must also add a CASTCLASS_ flag in ObjectBase.h and rebuild UnrealHeaderTool.
#define DECLARE_ALL_CAST_FLAGS \
DECLARE_CAST_BY_FLAG(UField)							\
DECLARE_CAST_BY_FLAG(UEnum)								\
DECLARE_CAST_BY_FLAG(UStruct)							\
DECLARE_CAST_BY_FLAG(UScriptStruct)						\
DECLARE_CAST_BY_FLAG(UClass)							\
DECLARE_CAST_BY_FLAG(FProperty)							\
DECLARE_CAST_BY_FLAG(FObjectPropertyBase)				\
DECLARE_CAST_BY_FLAG(FObjectProperty)					\
DECLARE_CAST_BY_FLAG(FWeakObjectProperty)				\
DECLARE_CAST_BY_FLAG(FLazyObjectProperty)				\
DECLARE_CAST_BY_FLAG(FSoftObjectProperty)				\
DECLARE_CAST_BY_FLAG(FSoftClassProperty)				\
DECLARE_CAST_BY_FLAG(FBoolProperty)						\
DECLARE_CAST_BY_FLAG(UFunction)							\
DECLARE_CAST_BY_FLAG(FStructProperty)					\
DECLARE_CAST_BY_FLAG(FByteProperty)						\
DECLARE_CAST_BY_FLAG(FIntProperty)						\
DECLARE_CAST_BY_FLAG(FFloatProperty)					\
DECLARE_CAST_BY_FLAG(FDoubleProperty)					\
DECLARE_CAST_BY_FLAG(FClassProperty)					\
DECLARE_CAST_BY_FLAG(FInterfaceProperty)				\
DECLARE_CAST_BY_FLAG(FNameProperty)						\
DECLARE_CAST_BY_FLAG(FStrProperty)						\
DECLARE_CAST_BY_FLAG(FTextProperty)						\
DECLARE_CAST_BY_FLAG(FArrayProperty)					\
DECLARE_CAST_BY_FLAG(FDelegateProperty)					\
DECLARE_CAST_BY_FLAG(FMulticastDelegateProperty)		\
DECLARE_CAST_BY_FLAG(UPackage)							\
DECLARE_CAST_BY_FLAG(ULevel)							\
DECLARE_CAST_BY_FLAG(AActor)							\
DECLARE_CAST_BY_FLAG(APlayerController)					\
DECLARE_CAST_BY_FLAG(APawn)								\
DECLARE_CAST_BY_FLAG(USceneComponent)					\
DECLARE_CAST_BY_FLAG(UPrimitiveComponent)				\
DECLARE_CAST_BY_FLAG(USkinnedMeshComponent)				\
DECLARE_CAST_BY_FLAG(USkeletalMeshComponent)			\
DECLARE_CAST_BY_FLAG(UBlueprint)						\
DECLARE_CAST_BY_FLAG(UDelegateFunction)					\
DECLARE_CAST_BY_FLAG(UStaticMeshComponent)				\
DECLARE_CAST_BY_FLAG(FEnumProperty)						\
DECLARE_CAST_BY_FLAG(FNumericProperty)					\
DECLARE_CAST_BY_FLAG(FInt8Property)						\
DECLARE_CAST_BY_FLAG(FInt16Property)					\
DECLARE_CAST_BY_FLAG(FInt64Property)					\
DECLARE_CAST_BY_FLAG(FUInt16Property)					\
DECLARE_CAST_BY_FLAG(FUInt32Property)					\
DECLARE_CAST_BY_FLAG(FUInt64Property)					\
DECLARE_CAST_BY_FLAG(FMapProperty)						\
DECLARE_CAST_BY_FLAG(FSetProperty)						\
DECLARE_CAST_BY_FLAG(USparseDelegateFunction)			\
DECLARE_CAST_BY_FLAG(FMulticastInlineDelegateProperty)	\
DECLARE_CAST_BY_FLAG(FMulticastSparseDelegateProperty)	\
DECLARE_CAST_BY_FLAG(FOptionalProperty)					\
DECLARE_CAST_BY_FLAG(FVerseValueProperty)				\
FINISH_DECLARING_CAST_FLAGS		// This is here to hopefully remind people to include the "\" in all declarations above, especially when copy/pasting the final line.

// Now actually declare the flags
DECLARE_ALL_CAST_FLAGS

#undef DECLARE_CAST_BY_FLAG
#undef DECLARE_CAST_BY_FLAG_CAST
#undef DECLARE_CAST_BY_FLAG_FWD

namespace UECasts_Private
{
	template <typename T>
	struct TIsCastable
	{
		// It's from-castable if it's an interface or a UObject-derived type
		enum { Value = TIsIInterface<T>::Value || std::is_convertible_v<T*, const volatile UObject*> };
	};

	template <typename To, typename From>
	FORCEINLINE To DynamicCast(From* Arg)
	{
		using ToValueType = std::remove_pointer_t<To>;

		if constexpr (!std::is_pointer_v<To> || !TIsCastable<From>::Value || !TIsCastable<ToValueType>::Value)
		{
			return dynamic_cast<To>(Arg);
		}
		else
		{
			// Casting away const/volatile
			static_assert(!TLosesQualifiersFromTo<From, ToValueType>::Value, "Conversion loses qualifiers");

			if constexpr (std::is_void_v<ToValueType>)
			{
				// When casting to void, cast to UObject instead and let it implicitly cast to void
				return Cast<UObject>(Arg);
			}
			else
			{
				return Cast<ToValueType>(Arg);
			}
		}
	}

	template <typename To, typename From>
	FORCEINLINE To DynamicCast(From&& Arg)
	{
		using FromValueType = std::remove_reference_t<From>;
		using ToValueType   = std::remove_reference_t<To>;

		if constexpr (!TIsCastable<FromValueType>::Value || !TIsCastable<ToValueType>::Value)
		{
			// This may fail when dynamic_casting rvalue references due to patchy compiler support
			return dynamic_cast<To>(Arg);
		}
		else
		{
			// Casting away const/volatile
			static_assert(!TLosesQualifiersFromTo<FromValueType, ToValueType>::Value, "Conversion loses qualifiers");

			// T&& can only be cast to U&&
			// http://en.cppreference.com/w/cpp/language/dynamic_cast
			static_assert(std::is_lvalue_reference_v<From> || std::is_rvalue_reference_v<To>, "Cannot dynamic_cast from an rvalue to a non-rvalue reference");

			return Forward<To>(*CastChecked<ToValueType>(&Arg));
		}
	}
}

#define dynamic_cast UECasts_Private::DynamicCast

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
