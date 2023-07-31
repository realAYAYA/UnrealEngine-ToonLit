// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceTypes.h"
#include "IDataInterface.h"

namespace UE::DataInterface
{

#define DATA_INTERFACE_TYPE(Type, Identifier) IMPLEMENT_DATA_INTERFACE_PARAM_TYPE(Type, Identifier)
#include "DataInterfaceTypes.inl"
#undef DATA_INTERFACE_TYPE

}