// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorHandle.h"
#include "DecoratorBase/DecoratorReader.h"
#include "Serialization/Archive.h"

bool FAnimNextDecoratorHandle::Serialize(FArchive& Ar)
{
	Ar << PackedDecoratorIndexAndNodeHandle;

	if (Ar.IsLoading() && IsValid())
	{
		// On load, we hold a node ID that needs fix-up
		check(GetNodeHandle().IsNodeID());

		UE::AnimNext::FDecoratorReader& DecoratorReader = static_cast<UE::AnimNext::FDecoratorReader&>(Ar);
		*this = DecoratorReader.ResolveDecoratorHandle(*this);
		check(GetNodeHandle().IsSharedOffset());
	}

	return true;
}
