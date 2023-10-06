// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/Filtering/NetObjectFilterDefinitions.h"

TConstArrayView<FNetObjectFilterDefinition> UNetObjectFilterDefinitions::GetFilterDefinitions() const
{
	return MakeArrayView(NetObjectFilterDefinitions);
}
