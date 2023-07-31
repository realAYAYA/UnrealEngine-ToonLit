// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MetasoundFacade.h"
#include "MetasoundVertex.h"


namespace Metasound
{
	class METASOUNDSTANDARDNODES_API FNoiseNode : public FNodeFacade
	{
	public:
		FNoiseNode(const FVertexName& InName, const FGuid& InInstanceID, int32 InDefaultSeed);
		FNoiseNode(const FNodeInitData& InInitData);

		int32 GetDefaultSeed() const { return DefaultSeed; }
	private:
		int32 DefaultSeed;
	};
}
