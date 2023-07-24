// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextInterfaceParam.h"
#include "UObject/ScriptInterface.h"

class IAnimNextInterface;

#define ANIM_NEXT_INTERFACE_RETURN_TYPE(Typename) \
virtual FName GetReturnTypeNameImpl() const final override\
{\
	static FName ReturnTypeNameValue = #Typename;\
	return ReturnTypeNameValue;\
}\

#define DECLARE_ANIM_NEXT_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier) \
	template<> \
	struct UE::AnimNext::Interface::Private::TParamTypeImpl<ValueType> \
	{ \
		FORCEINLINE static const UE::AnimNext::Interface::FParamType& GetType() \
		{ \
			return Identifier##_AnimNextInterfaceTypeInfo; \
		} \
	};

#define IMPLEMENT_ANIM_NEXT_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier) \
	UE::AnimNext::Interface::FParamType::FRegistrar Identifier##_Registrar([](){ UE::AnimNext::Interface::FParamType::FRegistrar::RegisterType( \
		Identifier##_AnimNextInterfaceTypeInfo, \
		UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetStruct(), \
		FName(#Identifier), UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetSize(), \
		UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetAlignment(), \
		UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetParamCloneFunction()); });

// Types that are passed between anim interfaces (i.e. exist in in public headers) should declare via this mechanism
#define DECLARE_ANIM_NEXT_INTERFACE_PARAM_TYPE(APIType, ValueType, Identifier) \
	extern APIType UE::AnimNext::Interface::FParamType Identifier##_AnimNextInterfaceTypeInfo; \
	DECLARE_ANIM_NEXT_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier)

// Types that are passed between anim interfaces (i.e. exist in in public headers) should declare via this mechanism
#define DECLARE_BUILTIN_ANIM_NEXT_INTERFACE_PARAM_TYPE(ValueType, Identifier) \
	DECLARE_ANIM_NEXT_INTERFACE_PARAM_TYPE(ANIMNEXTINTERFACE_API, ValueType, Identifier)

// Types that are passed between anim interfaces should register via this mechanism
#define IMPLEMENT_ANIM_NEXT_INTERFACE_PARAM_TYPE(ValueType, Identifier) \
	UE::AnimNext::Interface::FParamType Identifier##_AnimNextInterfaceTypeInfo; \
	IMPLEMENT_ANIM_NEXT_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier)

// Structs that are only used for internal state (in a cpp file) can use this simpler registration.
// If they are exposed in a public header please use the DECLARE_ANIM_NEXT_INTERFACE_PARAM_TYPE & IMPLEMENT_ANIM_NEXT_INTERFACE_PARAM_TYPE macros
#define IMPLEMENT_ANIM_NEXT_INTERFACE_STATE_TYPE(ValueType, Identifier) \
	static UE::AnimNext::Interface::FParamType Identifier##_AnimNextInterfaceTypeInfo; \
	static UE::AnimNext::Interface::FParamType::FRegistrar Identifier##_Registrar([](){ UE::AnimNext::Interface::FParamType::FRegistrar::RegisterType( \
		Identifier##_AnimNextInterfaceTypeInfo, \
		UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetStruct(), \
		FName(#Identifier), UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetSize(), \
		UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetAlignment(), \
		UE::AnimNext::Interface::Private::TParamTypeHelper<ValueType>::GetParamCloneFunction()); }); \
	DECLARE_ANIM_NEXT_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier);

namespace UE::AnimNext::Interface
{

#define ANIM_NEXT_INTERFACE_TYPE(Type, Identifier) DECLARE_BUILTIN_ANIM_NEXT_INTERFACE_PARAM_TYPE(Type, Identifier)
#include "AnimNextInterfaceTypes.inl"
#undef ANIM_NEXT_INTERFACE_TYPE

}