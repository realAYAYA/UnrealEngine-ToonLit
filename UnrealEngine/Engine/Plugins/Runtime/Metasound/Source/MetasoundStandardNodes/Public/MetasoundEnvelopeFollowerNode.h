// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundFacade.h"

namespace Metasound
{
	/** FEnvelopeFollowerNode
	 *
	 *  Delays an audio buffer by a specified amount.
	 */
	class METASOUNDSTANDARDNODES_API FEnvelopeFollowerNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		FEnvelopeFollowerNode(const FNodeInitData& InitData);
	};
} // namespace Metasound

