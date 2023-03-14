// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_WITH_IRIS
#include "Iris/ReplicationSystem/FastArrayReplicationFragment.h"

#define UE_NET_IMPLEMENT_FASTARRAY(FastArrayType) \
UE::Net::CreateAndRegisterReplicationFragmentFunc FastArrayType::GetFastArrayCreateReplicationFragmentFunction() \
{ \
	auto CreateFastArrayReplicationFragmentFunction = [](UObject* InLocalOwner, const UE::Net::FReplicationStateDescriptor* InLocalDescriptor, UE::Net::FFragmentRegistrationContext& InLocalContext) \
	{ \
		return UE::Net::Private::CreateAndRegisterFragment<FastArrayType>(InLocalOwner, InLocalDescriptor, InLocalContext); \
	}; \
	return CreateFastArrayReplicationFragmentFunction; \
}

#else
#define UE_NET_IMPLEMENT_FASTARRAY(...)
#endif
