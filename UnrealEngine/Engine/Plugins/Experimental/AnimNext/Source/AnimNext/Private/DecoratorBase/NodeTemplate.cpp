// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/NodeTemplate.h"

#include "Serialization/Archive.h"

namespace UE::AnimNext
{
	void FNodeTemplate::Serialize(FArchive& Ar)
	{
		Ar << UID;
		Ar << InstanceSize;
		Ar << NumDecorators;

		FDecoratorTemplate* DecoratorTemplates = GetDecorators();
		for (uint32 DecoratorIndex = 0; DecoratorIndex < GetNumDecorators(); ++DecoratorIndex)
		{
			DecoratorTemplates[DecoratorIndex].Serialize(Ar);
		}
	}
}
