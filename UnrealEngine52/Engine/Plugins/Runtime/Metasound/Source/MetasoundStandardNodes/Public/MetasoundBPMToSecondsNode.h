// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundFacade.h"

namespace Metasound
{
	/** FBPMToSecondsNode
	 *
	 *  Calculates a beat time in seconds from the given BPM, beat multiplier and divisions of a whole note. 
	 */
	class METASOUNDSTANDARDNODES_API FBPMToSecondsNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		FBPMToSecondsNode(const FNodeInitData& InitData);
	};
} // namespace Metasound

