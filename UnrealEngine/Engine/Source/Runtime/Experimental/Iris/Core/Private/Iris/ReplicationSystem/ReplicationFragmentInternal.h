// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/ReplicationFragment.h"

#pragma once

namespace UE::Net::Private
{
	struct FFragmentRegistrationContextPrivateAccessor
	{
		static const FReplicationFragments& GetReplicationFragments(const FFragmentRegistrationContext& Context) { return Context.Fragments; }
	};
}
