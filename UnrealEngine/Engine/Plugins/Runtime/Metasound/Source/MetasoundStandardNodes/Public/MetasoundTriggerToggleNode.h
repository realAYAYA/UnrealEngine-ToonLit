// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundTime.h"
#include "MetasoundTrigger.h"


namespace Metasound
{
	class METASOUNDSTANDARDNODES_API FTriggerToggleNode : public FNodeFacade
	{
		public:
			FTriggerToggleNode(const FNodeInitData& InInitData);

			virtual ~FTriggerToggleNode() = default;
	};
}
