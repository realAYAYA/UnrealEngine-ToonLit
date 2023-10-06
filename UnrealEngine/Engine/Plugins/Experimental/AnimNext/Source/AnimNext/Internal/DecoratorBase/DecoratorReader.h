// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArchiveProxy.h"

namespace UE::AnimNext
{
	/**
	  * FDecoratorReader
	  * 
	  * The decorator reader is used to read from a serialized binary blob that contains
	  * the anim graph data. An anim graph contains the following:
	  *     - A list of FNodeTemplates that the nodes use
	  *     - The graph shared data (FNodeDescription for every node)
	  */
	class ANIMNEXT_API FDecoratorReader : public FArchiveProxy
	{
	public:
		FDecoratorReader(FArchive& Ar);

		void ReadGraphSharedData(TArray<uint8>& GraphSharedData);
	};
}
