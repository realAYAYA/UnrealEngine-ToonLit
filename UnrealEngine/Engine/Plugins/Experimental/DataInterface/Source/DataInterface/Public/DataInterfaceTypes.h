// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterfaceParam.h"
#include "UObject/ScriptInterface.h"

class IDataInterface;

#define DATA_INTERFACE_RETURN_TYPE(Typename) \
virtual FName GetReturnTypeNameImpl() const final override\
{\
	static FName ReturnTypeNameValue = #Typename;\
	return ReturnTypeNameValue;\
}\

#define DECLARE_DATA_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier) \
	template<> \
	struct UE::DataInterface::Private::TParamTypeImpl<ValueType> \
	{ \
		FORCEINLINE static const UE::DataInterface::FParamType& GetType() \
		{ \
			return Identifier##_DataInterfaceTypeInfo; \
		} \
	};

#define IMPLEMENT_DATA_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier) \
	UE::DataInterface::FParamType::FRegistrar Identifier##_Registrar([](){ UE::DataInterface::FParamType::FRegistrar::RegisterType(Identifier##_DataInterfaceTypeInfo, UE::DataInterface::Private::TParamTypeHelper<ValueType>::GetStruct(), FName(#Identifier), UE::DataInterface::Private::TParamTypeHelper<ValueType>::GetSize(), UE::DataInterface::Private::TParamTypeHelper<ValueType>::GetAlignment()); });

// Types that are passed between data interfaces (i.e. exist in in public headers) should declare via this mechanism
#define DECLARE_DATA_INTERFACE_PARAM_TYPE(APIType, ValueType, Identifier) \
	extern APIType UE::DataInterface::FParamType Identifier##_DataInterfaceTypeInfo; \
	DECLARE_DATA_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier)

// Types that are passed between data interfaces (i.e. exist in in public headers) should declare via this mechanism
#define DECLARE_BUILTIN_DATA_INTERFACE_PARAM_TYPE(ValueType, Identifier) \
	DECLARE_DATA_INTERFACE_PARAM_TYPE(DATAINTERFACE_API, ValueType, Identifier)

// Types that are passed between data interfaces should register via this mechanism
#define IMPLEMENT_DATA_INTERFACE_PARAM_TYPE(ValueType, Identifier) \
	UE::DataInterface::FParamType Identifier##_DataInterfaceTypeInfo; \
	IMPLEMENT_DATA_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier)

// Structs that are only used for internal state (in a cpp file) can use this simpler registration.
// If they are exposed in a public header please use the DECLARE_DATA_INTERFACE_PARAM_TYPE & IMPLEMENT_DATA_INTERFACE_PARAM_TYPE macros
#define IMPLEMENT_DATA_INTERFACE_STATE_TYPE(ValueType, Identifier) \
	static UE::DataInterface::FParamType Identifier##_DataInterfaceTypeInfo; \
	static UE::DataInterface::FParamType::FRegistrar Identifier##_Registrar([](){ UE::DataInterface::FParamType::FRegistrar::RegisterType(Identifier##_DataInterfaceTypeInfo, UE::DataInterface::Private::TParamTypeHelper<ValueType>::GetStruct(), FName(#Identifier), UE::DataInterface::Private::TParamTypeHelper<ValueType>::GetSize(), UE::DataInterface::Private::TParamTypeHelper<ValueType>::GetAlignment()); }); \
	DECLARE_DATA_INTERFACE_PARAM_TYPE_INTERNAL(ValueType, Identifier);

namespace UE::DataInterface
{

#define DATA_INTERFACE_TYPE(Type, Identifier) DECLARE_BUILTIN_DATA_INTERFACE_PARAM_TYPE(Type, Identifier)
#include "DataInterfaceTypes.inl"
#undef DATA_INTERFACE_TYPE

}