// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceTypes.h"
#include "IAnimNextInterface.h"

namespace UE::AnimNext::Interface
{

#define ANIM_NEXT_INTERFACE_TYPE(Type, Identifier) IMPLEMENT_ANIM_NEXT_INTERFACE_PARAM_TYPE(Type, Identifier)
#include "AnimNextInterfaceTypes.inl"
#undef ANIM_NEXT_INTERFACE_TYPE

}