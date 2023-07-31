// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/InternalNetSerializerDelegates.h"

namespace UE::Net
{

using namespace UE::Net::Private;

FNetSerializerRegistryDelegates::FNetSerializerRegistryDelegates()
: PreFreezeDelegate(FInternalNetSerializerDelegates::GetPreFreezeNetSerializerRegistryDelegate().AddRaw(this, &FNetSerializerRegistryDelegates::PreFreezeNetSerializerRegistry))
, PostFreezeDelegate(FInternalNetSerializerDelegates::GetPostFreezeNetSerializerRegistryDelegate().AddRaw(this, &FNetSerializerRegistryDelegates::PostFreezeNetSerializerRegistry))
{
}

FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
{
	if (PreFreezeDelegate.IsValid())
	{
		FInternalNetSerializerDelegates::GetPreFreezeNetSerializerRegistryDelegate().Remove(PreFreezeDelegate);
	}
	if (PostFreezeDelegate.IsValid())
	{
		FInternalNetSerializerDelegates::GetPostFreezeNetSerializerRegistryDelegate().Remove(PostFreezeDelegate);
	}
}

void FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
{
}

void FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
{
}

void FNetSerializerRegistryDelegates::PreFreezeNetSerializerRegistry()
{
	OnPreFreezeNetSerializerRegistry();
	FInternalNetSerializerDelegates::GetPreFreezeNetSerializerRegistryDelegate().Remove(PreFreezeDelegate);
	PreFreezeDelegate.Reset();
}

void FNetSerializerRegistryDelegates::PostFreezeNetSerializerRegistry()
{
	OnPostFreezeNetSerializerRegistry();
	FInternalNetSerializerDelegates::GetPostFreezeNetSerializerRegistryDelegate().Remove(PostFreezeDelegate);
	PostFreezeDelegate.Reset();
}

}
