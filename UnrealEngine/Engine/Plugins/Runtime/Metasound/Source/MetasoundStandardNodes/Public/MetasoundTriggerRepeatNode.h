// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundNode.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundTime.h"
#include "MetasoundTrigger.h"


namespace Metasound
{
	class METASOUNDSTANDARDNODES_API FTriggerRepeatNode : public FNodeFacade
	{
		public:
			FTriggerRepeatNode(const FNodeInitData& InInitData);

			virtual ~FTriggerRepeatNode() = default;

		private:
			float DefaultPeriod = 1.0f;
	};
}
