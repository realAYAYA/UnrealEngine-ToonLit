// Copyright Epic Games, Inc. All Rights Reserved.

// Only designed to be included directly by Delegate.h
#if !defined( __Delegate_h__ ) || !defined( FUNC_INCLUDING_INLINE_IMPL )
	#error "This inline header must only be included by Delegate.h"
#endif

#pragma once
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

class FDelegateBase;
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
template<typename UserPolicy> class TTSMulticastDelegateBase;

/**
 * Unicast delegate template class.
 *
 * Use the various DECLARE_DELEGATE macros to create the actual delegate type, templated to
 * the function signature the delegate is compatible with. Then, you can create an instance
 * of that class when you want to bind a function to the delegate.
 */
template <typename DelegateSignature, typename UserPolicy = FDefaultDelegateUserPolicy>
class TDelegate
{
	static_assert(sizeof(UserPolicy) == 0, "Expected a function signature for the delegate template parameter");
};

template <typename InRetValType, typename... ParamTypes, typename UserPolicy>
class TDelegate<InRetValType(ParamTypes...), UserPolicy> : public TDelegateBase<UserPolicy>
{
	using Super                         = TDelegateBase<UserPolicy>;
	using FuncType                      = InRetValType (ParamTypes...);
	using DelegateInstanceInterfaceType = IBaseDelegateInstance<FuncType, UserPolicy>;

	using DelegateType          = typename UserPolicy::FDelegateExtras;
	using DelegateInstanceType  = typename UserPolicy::FDelegateInstanceExtras;
	using MulticastDelegateType = typename UserPolicy::FMulticastDelegateExtras;

	static_assert(std::is_convertible_v<DelegateType*, FDelegateBase*>, "UserPolicy::FDelegateExtras should publicly inherit FDelegateBase");
	static_assert(std::is_convertible_v<DelegateInstanceType*, IDelegateInstance*>, "UserPolicy::FDelegateInstanceExtras should publicly inherit IDelegateInstance");
	static_assert(std::is_convertible_v<MulticastDelegateType*, TMulticastDelegateBase<UserPolicy>*> || std::is_convertible_v<MulticastDelegateType*, TTSMulticastDelegateBase<UserPolicy>*>, "UserPolicy::FMulticastDelegateExtras should publicly inherit TMulticastDelegateBase<UserPolicy> or TTSMulticastDelegateBase<UserPolicy>");

	template <typename>
	friend class TMulticastDelegateBase;

	template <typename>
	friend class TTSMulticastDelegateBase;

public:
	// Make sure FDelegateBase's public functions are publicly exposed through the TDelegate API
	using Super::Unbind;

private:
	// Make sure FDelegateBase's protected functions are not accidentally exposed through the TDelegate API
	using Super::GetDelegateInstanceProtected;

public:
	/** Type definition for return value type. */
	typedef InRetValType RetValType;
	typedef InRetValType TFuncType(ParamTypes...);

	/* Helper typedefs for getting a member function pointer type for the delegate with a given payload */
	template <typename... VarTypes>                     using TFuncPtr        = RetValType(*)(ParamTypes..., VarTypes...);
	template <typename UserClass, typename... VarTypes> using TMethodPtr      = typename TMemFunPtrType<false, UserClass, RetValType(ParamTypes..., VarTypes...)>::Type;
	template <typename UserClass, typename... VarTypes> using TConstMethodPtr = typename TMemFunPtrType<true,  UserClass, RetValType(ParamTypes..., VarTypes...)>::Type;

	/** Declare the user's shared pointer-based delegate instance types. */
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")                      TSPMethodDelegate                 : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > Super; TSPMethodDelegate                (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")           TSPMethodDelegate_Const           : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > Super; TSPMethodDelegate_Const          (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_OneVar<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")               TSPMethodDelegate_OneVar          : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > Super; TSPMethodDelegate_OneVar         (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_OneVar_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")    TSPMethodDelegate_OneVar_Const    : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > Super; TSPMethodDelegate_OneVar_Const   (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_TwoVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")              TSPMethodDelegate_TwoVars         : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TSPMethodDelegate_TwoVars        (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_TwoVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")   TSPMethodDelegate_TwoVars_Const   : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TSPMethodDelegate_TwoVars_Const  (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_ThreeVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")            TSPMethodDelegate_ThreeVars       : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TSPMethodDelegate_ThreeVars      (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_ThreeVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead") TSPMethodDelegate_ThreeVars_Const : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TSPMethodDelegate_ThreeVars_Const(const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_FourVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")             TSPMethodDelegate_FourVars        : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TSPMethodDelegate_FourVars       (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TSPMethodDelegate_FourVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")  TSPMethodDelegate_FourVars_Const  : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TSPMethodDelegate_FourVars_Const (const TSharedRef< UserClass, ESPMode::ThreadSafe >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };

	/** Declare the user's shared pointer-based delegate instance types. */
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")                      TThreadSafeSPMethodDelegate                 : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > Super; TThreadSafeSPMethodDelegate                (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")           TThreadSafeSPMethodDelegate_Const           : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy                                        > Super; TThreadSafeSPMethodDelegate_Const          (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_OneVar<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")               TThreadSafeSPMethodDelegate_OneVar          : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > Super; TThreadSafeSPMethodDelegate_OneVar         (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_OneVar_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")    TThreadSafeSPMethodDelegate_OneVar_Const    : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type                              > Super; TThreadSafeSPMethodDelegate_OneVar_Const   (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_TwoVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")              TThreadSafeSPMethodDelegate_TwoVars         : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TThreadSafeSPMethodDelegate_TwoVars        (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_TwoVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")   TThreadSafeSPMethodDelegate_TwoVars_Const   : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TThreadSafeSPMethodDelegate_TwoVars_Const  (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_ThreeVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")            TThreadSafeSPMethodDelegate_ThreeVars       : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TThreadSafeSPMethodDelegate_ThreeVars      (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_ThreeVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead") TThreadSafeSPMethodDelegate_ThreeVars_Const : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TThreadSafeSPMethodDelegate_ThreeVars_Const(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_FourVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")             TThreadSafeSPMethodDelegate_FourVars        : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TThreadSafeSPMethodDelegate_FourVars       (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TThreadSafeSPMethodDelegate_FourVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")  TThreadSafeSPMethodDelegate_FourVars_Const  : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TThreadSafeSPMethodDelegate_FourVars_Const (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };

	/** Declare the user's C++ pointer-based delegate instance types. */
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")                      TRawMethodDelegate                 : TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy                                        > { typedef TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy                                        > Super; TRawMethodDelegate                (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")           TRawMethodDelegate_Const           : TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy                                        > { typedef TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy                                        > Super; TRawMethodDelegate_Const          (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_OneVar<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")               TRawMethodDelegate_OneVar          : TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type                              > { typedef TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type                              > Super; TRawMethodDelegate_OneVar         (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_OneVar_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")    TRawMethodDelegate_OneVar_Const    : TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type                              > { typedef TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type                              > Super; TRawMethodDelegate_OneVar_Const   (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_TwoVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")              TRawMethodDelegate_TwoVars         : TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TRawMethodDelegate_TwoVars        (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_TwoVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")   TRawMethodDelegate_TwoVars_Const   : TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TRawMethodDelegate_TwoVars_Const  (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_ThreeVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")            TRawMethodDelegate_ThreeVars       : TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TRawMethodDelegate_ThreeVars      (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_ThreeVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead") TRawMethodDelegate_ThreeVars_Const : TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TRawMethodDelegate_ThreeVars_Const(UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_FourVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")             TRawMethodDelegate_FourVars        : TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TRawMethodDelegate_FourVars       (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TRawMethodDelegate_FourVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")  TRawMethodDelegate_FourVars_Const  : TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseRawMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TRawMethodDelegate_FourVars_Const (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	
	/** Declare the user's UFunction-based delegate instance types. */
	template <typename UObjectTemplate                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TUFunctionDelegateBinding has been deprecated")           TUFunctionDelegateBinding           : TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy                                        > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy                                        > Super; TUFunctionDelegateBinding          (UObjectTemplate* InUserObject, const FName& InFunctionName                                                            ) : Super(InUserObject, InFunctionName                        ) {} };
	template <typename UObjectTemplate, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TUFunctionDelegateBinding_OneVar has been deprecated")    TUFunctionDelegateBinding_OneVar    : TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type                              > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type                              > Super; TUFunctionDelegateBinding_OneVar   (UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1                                             ) : Super(InUserObject, InFunctionName, Var1                  ) {} };
	template <typename UObjectTemplate, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TUFunctionDelegateBinding_TwoVars has been deprecated")   TUFunctionDelegateBinding_TwoVars   : TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TUFunctionDelegateBinding_TwoVars  (UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InFunctionName, Var1, Var2            ) {} };
	template <typename UObjectTemplate, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TUFunctionDelegateBinding_ThreeVars has been deprecated") TUFunctionDelegateBinding_ThreeVars : TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TUFunctionDelegateBinding_ThreeVars(UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InFunctionName, Var1, Var2, Var3      ) {} };
	template <typename UObjectTemplate, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TUFunctionDelegateBinding_FourVars has been deprecated")  TUFunctionDelegateBinding_FourVars  : TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TUFunctionDelegateBinding_FourVars (UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InFunctionName, Var1, Var2, Var3, Var4) {} };

	/** Declare the user's UObject-based delegate instance types. */
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")                      TUObjectMethodDelegate                 : TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy                                        > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy                                        > Super; TUObjectMethodDelegate                (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")           TUObjectMethodDelegate_Const           : TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy                                        > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy                                        > Super; TUObjectMethodDelegate_Const          (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_OneVar<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")               TUObjectMethodDelegate_OneVar          : TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type                              > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type                              > Super; TUObjectMethodDelegate_OneVar         (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_OneVar_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")    TUObjectMethodDelegate_OneVar_Const    : TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type                              > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type                              > Super; TUObjectMethodDelegate_OneVar_Const   (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_TwoVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")              TUObjectMethodDelegate_TwoVars         : TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TUObjectMethodDelegate_TwoVars        (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_TwoVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")   TUObjectMethodDelegate_TwoVars_Const   : TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type                    > Super; TUObjectMethodDelegate_TwoVars_Const  (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_ThreeVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")            TUObjectMethodDelegate_ThreeVars       : TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TUObjectMethodDelegate_ThreeVars      (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_ThreeVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead") TUObjectMethodDelegate_ThreeVars_Const : TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type          > Super; TUObjectMethodDelegate_ThreeVars_Const(UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_FourVars<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TMethodPtr<UserClass, ...> instead")             TUObjectMethodDelegate_FourVars        : TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TUObjectMethodDelegate_FourVars       (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct UE_DEPRECATED(5.1, "DelegateType::TUObjectMethodDelegate_FourVars_Const<UserClass, ...>::FMethodPtr has been deprecated - use DelegateType::TConstMethodPtr<UserClass, ...> instead")  TUObjectMethodDelegate_FourVars_Const  : TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, FuncType, UserPolicy, Var1Type, Var2Type, Var3Type, Var4Type> Super; TUObjectMethodDelegate_FourVars_Const (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	
	/** Declare the user's static function pointer delegate instance types. */
	template <typename... VarTypes > struct UE_DEPRECATED(5.1, "DelegateType::FStaticDelegate<...>::FFuncPtr has been deprecated - use DelegateType::TFuncPtr<...> instead") FStaticDelegate : TBaseStaticDelegateInstance<FuncType, UserPolicy, VarTypes... > { typedef TBaseStaticDelegateInstance<FuncType, UserPolicy, VarTypes... > Super; FStaticDelegate ( typename Super::FFuncPtr InFuncPtr, VarTypes... Vars ) : Super(InFuncPtr, Vars... ) {} };

public:

	/**
	 * Static: Creates a raw C++ pointer global function delegate
	 */
	template <typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateStatic(typename TIdentity<RetValType (*)(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseStaticDelegateInstance<FuncType, UserPolicy, VarTypes...>::Create(Result, InFunc, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename FunctorType, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseFunctorDelegateInstance<FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, VarTypes...>::Create(Result, Forward<FunctorType>(InFunctor), Vars...);
		return Result;
	}

	/**
	 * Static: Creates a weak object C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename UserClass, typename FunctorType, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TWeakBaseFunctorDelegateInstance<UserClass, FuncType, UserPolicy, typename TRemoveReference<FunctorType>::Type, VarTypes...>::Create(Result, InUserObject, Forward<FunctorType>(InFunctor), Vars...);
		return Result;
	}

	/**
	 * Static: Creates a raw C++ pointer member function delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseRawMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseRawMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseSPMethodDelegateInstance<false, UserClass, Mode, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseSPMethodDelegateInstance<true, const UserClass, Mode, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return CreateSP(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return CreateSP(StaticCastSharedRef<const UserClass>(InUserObject->AsShared()), InFunc, Vars...);
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
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseSPMethodDelegateInstance<true, const UserClass, ESPMode::ThreadSafe, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
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
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return CreateThreadSafeSP(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateThreadSafeSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return CreateThreadSafeSP(StaticCastSharedRef<const UserClass>(InUserObject->AsShared()), InFunc, Vars...);
	}

	/**
	 * Static: Creates a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObject, InFunctionName, Vars...);
		return Result;
	}
	template <typename UObjectTemplate, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUFunction(TObjectPtr<UObjectTemplate> InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseUFunctionDelegateInstance<UObjectTemplate, FuncType, UserPolicy, VarTypes...>::Create(Result, ToRawPtr(InUserObject), InFunctionName, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseUObjectMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseUObjectMethodDelegateInstance<false, UserClass, FuncType, UserPolicy, VarTypes...>::Create(Result, ToRawPtr(InUserObject), InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	UE_NODISCARD inline static TDelegate<RetValType(ParamTypes...), UserPolicy> CreateUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		TDelegate<RetValType(ParamTypes...), UserPolicy> Result;
		TBaseUObjectMethodDelegateInstance<true, const UserClass, FuncType, UserPolicy, VarTypes...>::Create(Result, ToRawPtr(InUserObject), InFunc, Vars...);
		return Result;
	}

public:

	/**
	 * Default constructor
	 */
	inline TDelegate()
	{
	}

	/**
	 * 'Null' constructor
	 */
	inline TDelegate(TYPE_OF_NULLPTR)
	{
	}

	/**
	 * Destructor.
	 */
	inline ~TDelegate()
	{
		Unbind();
	}

	/**
	 * Move constructor.
	 *
	 * @param Other The delegate object to move from.
	 */
	TDelegate(TDelegate&& Other) = default;

	/**
	 * Creates and initializes a new instance from an existing delegate object.
	 *
	 * @param Other The delegate object to copy from.
	 */
	inline TDelegate(const TDelegate& Other)
	{
		*this = Other;
	}

	/**
	 * Move assignment operator.
	 *
	 * @param	OtherDelegate	Delegate object to copy from
	 */
	TDelegate& operator=(TDelegate&& Other) = default;

	/**
	 * Assignment operator.
	 *
	 * @param	OtherDelegate	Delegate object to copy from
	 */
	inline TDelegate& operator=(const TDelegate& Other)
	{
		if (&Other != this)
		{
			// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
			DelegateInstanceInterfaceType* OtherInstance = Other.GetDelegateInstanceProtected();

			if (OtherInstance != nullptr)
			{
				OtherInstance->CreateCopy(*this);
			}
			else
			{
				Unbind();
			}
		}

		return *this;
	}

public:

	/**
	 * Binds a raw C++ pointer global function delegate
	 */
	template <typename... VarTypes>
	inline void BindStatic(typename TBaseStaticDelegateInstance<FuncType, UserPolicy, VarTypes...>::FFuncPtr InFunc, VarTypes... Vars)
	{
		*this = CreateStatic(InFunc, Vars...);
	}
	
	/**
	 * Static: Binds a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename FunctorType, typename... VarTypes>
	inline void BindLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		*this = CreateLambda(Forward<FunctorType>(InFunctor), Vars...);
	}

	/**
	 * Static: Binds a weak object C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename UserClass, typename FunctorType, typename... VarTypes>
	inline void BindWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes... Vars)
	{
		*this = CreateWeakLambda(InUserObject, Forward<FunctorType>(InFunctor), Vars...);
	}

	/**
	 * Binds a raw C++ pointer delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		*this = CreateRaw(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateRaw(InUserObject, InFunc, Vars...);
	}

	/**
	 * Binds a shared pointer-based member function delegate.  Shared pointer delegates keep a weak reference to your object.  You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	inline void BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		*this = CreateSP(InUserObjectRef, InFunc, Vars...);
	}
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	inline void BindSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateSP(InUserObjectRef, InFunc, Vars...);
	}

	/**
	 * Binds a shared pointer-based member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		*this = CreateSP(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateSP(InUserObject, InFunc, Vars...);
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
	inline void BindThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		*this = CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...);
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
	inline void BindThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		*this = CreateThreadSafeSP(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateThreadSafeSP(InUserObject, InFunc, Vars...);
	}

	/**
	 * Binds a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	inline void BindUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		*this = CreateUFunction(InUserObject, InFunctionName, Vars...);
	}
	template <typename UObjectTemplate, typename... VarTypes>
	inline void BindUFunction(TObjectPtr<UObjectTemplate> InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		*this = CreateUFunction(InUserObject, InFunctionName, Vars...);
	}

	/**
	 * Binds a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		*this = CreateUObject(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateUObject(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		*this = CreateUObject(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateUObject(InUserObject, InFunc, Vars...);
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
		DelegateInstanceInterfaceType* LocalDelegateInstance = GetDelegateInstanceProtected();

		// If this assert goes off, Execute() was called before a function was bound to the delegate.
		// Consider using ExecuteIfSafe() instead.
		checkSlow(LocalDelegateInstance != nullptr);

		return LocalDelegateInstance->Execute(Params...);
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
		if (DelegateInstanceInterfaceType* Ptr = GetDelegateInstanceProtected())
		{
			return Ptr->ExecuteIfSafe(Params...);
		}

		return false;
	}

protected:
	/**
	 * Returns a pointer to the correctly-typed delegate instance.
	 */
	FORCEINLINE DelegateInstanceInterfaceType* GetDelegateInstanceProtected() const
	{
		return (DelegateInstanceInterfaceType*)Super::GetDelegateInstanceProtected();
	}
};

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

private:
	// Make sure TMulticastDelegateBase's protected functions are not accidentally exposed through the TMulticastDelegate API
	using Super::AddDelegateInstance;
	using Super::RemoveDelegateInstance;
	using Super::GetDelegateInstanceProtectedHelper;

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
	inline FDelegateHandle AddStatic(typename TBaseStaticDelegateInstance<void (ParamTypes...), UserPolicy, VarTypes...>::FFuncPtr InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateStatic(InFunc, Vars...));
	}

	/**
	 * Adds a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 *
	 * @param	InFunctor	Functor (e.g. Lambda)
	 */
	template<typename FunctorType, typename... VarTypes>
	inline FDelegateHandle AddLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		return Add(FDelegate::CreateLambda(Forward<FunctorType>(InFunctor), Vars...));
	}

	/**
	 * Adds a weak object C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunctor		Functor (e.g. Lambda)
	 */
	template<typename UserClass, typename FunctorType, typename... VarTypes>
	inline FDelegateHandle AddWeakLambda(UserClass* InUserObject, FunctorType&& InFunctor, VarTypes... Vars)
	{
		return Add(FDelegate::CreateWeakLambda(InUserObject, Forward<FunctorType>(InFunctor), Vars...));
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
	inline FDelegateHandle AddRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Add(FDelegate::CreateRaw(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddRaw(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateRaw(InUserObject, InFunc, Vars...));
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
	inline FDelegateHandle AddSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Add(FDelegate::CreateSP(InUserObjectRef, InFunc, Vars...));
	}
	template <typename UserClass, ESPMode Mode, typename... VarTypes>
	inline FDelegateHandle AddSP(const TSharedRef<UserClass, Mode>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObjectRef, InFunc, Vars...));
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
	inline FDelegateHandle AddSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Add(FDelegate::CreateSP(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObject, InFunc, Vars...));
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
	inline FDelegateHandle AddThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Add(FDelegate::CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...));
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
	inline FDelegateHandle AddThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Add(FDelegate::CreateThreadSafeSP(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateThreadSafeSP(InUserObject, InFunc, Vars...));
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
	inline FDelegateHandle AddUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		return Add(FDelegate::CreateUFunction(InUserObject, InFunctionName, Vars...));
	}
	template <typename UObjectTemplate, typename... VarTypes>
	inline FDelegateHandle AddUFunction(TObjectPtr<UObjectTemplate> InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		return Add(FDelegate::CreateUFunction(InUserObject, InFunctionName, Vars...));
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
	inline FDelegateHandle AddUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Add(FDelegate::CreateUObject(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(const UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateUObject(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		static_assert(!TIsConst<UserClass>::Value, "Attempting to bind a delegate with a const object pointer and non-const member function.");

		return Add(FDelegate::CreateUObject(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(TObjectPtr<UserClass> InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateUObject(InUserObject, InFunc, Vars...));
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
	 * Hidden default constructor.
	 */
	inline TMulticastDelegate( ) { }

	/**
	 * Hidden copy constructor (for proper deep copies).
	 *
	 * @param Other The multicast delegate to copy from.
	 */
	TMulticastDelegate( const TMulticastDelegate& Other )
	{
		*this = Other;
	}

	/**
	 * Hidden assignment operator (for proper deep copies).
	 *
	 * @param Other The delegate to assign from.
	 * @return This instance.
	 */
	TMulticastDelegate& operator=( const TMulticastDelegate& Other )
	{
		if (&Other != this)
		{
			Super::template CopyFrom<DelegateInstanceInterfaceType, FDelegate>(Other);
		}
		return *this;
	}

	/**
	 * Broadcasts this delegate to all bound objects, except to those that may have expired.
	 *
	 * The constness of this method is a lie, but it allows for broadcasting from const functions.
	 */
	void Broadcast(ParamTypes... Params) const
	{
		Super::template Broadcast<DelegateInstanceInterfaceType, FDelegate, ParamTypes...>(Params...);
	}
};


/**
 * Dynamic delegate template class (UObject-based, serializable).  You'll use the various DECLARE_DYNAMIC_DELEGATE
 * macros to create the actual delegate type, templated to the function signature the delegate is compatible with.
 * Then, you can create an instance of that class when you want to assign functions to the delegate.
 */
template <typename TWeakPtr, typename RetValType, typename... ParamTypes>
class TBaseDynamicDelegate : public TScriptDelegate<TWeakPtr>
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
	explicit TBaseDynamicDelegate( const TScriptDelegate<TWeakPtr>& InScriptDelegate )
		: TScriptDelegate<TWeakPtr>( InScriptDelegate )
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
template <typename TWeakPtr, typename RetValType, typename... ParamTypes>
class TBaseDynamicMulticastDelegate : public TMulticastScriptDelegate<TWeakPtr>
{
public:
	/** The actual single-cast delegate class for this multi-cast delegate */
	typedef TBaseDynamicDelegate<FWeakObjectPtr, RetValType, ParamTypes...> FDelegate;

	/**
	 * Default constructor
	 */
	TBaseDynamicMulticastDelegate() { }

	/**
	 * Construction from an FMulticastScriptDelegate must be explicit.  This is really only used by UObject system internals.
	 *
	 * @param	InScriptDelegate	The delegate to construct from by copying
	 */
	explicit TBaseDynamicMulticastDelegate( const TMulticastScriptDelegate<TWeakPtr>& InMulticastScriptDelegate )
		: TMulticastScriptDelegate<TWeakPtr>( InMulticastScriptDelegate )
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
