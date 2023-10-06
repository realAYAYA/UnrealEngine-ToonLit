// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Core/Misc/NetContext.h"
#include "Misc/AssertionMacros.h"

namespace UE::Net
{

bool FNetContext::bIsInRPCStack = false;

FScopedNetContextRPC::FScopedNetContextRPC()
{
	FNetContext::bIsInRPCStack = true;
}

FScopedNetContextRPC::~FScopedNetContextRPC()
{
	checkSlow(FNetContext::bIsInRPCStack); // Cannot have two remote rpc stacked on top of each other
	FNetContext::bIsInRPCStack = false;
}

} // end namespace UE::Net

