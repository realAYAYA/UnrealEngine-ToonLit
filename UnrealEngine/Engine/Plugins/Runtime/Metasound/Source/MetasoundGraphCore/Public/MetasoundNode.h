// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundVertex.h"


namespace Metasound
{
	class METASOUNDGRAPHCORE_API FNode : public INode
	{
		public:
			FNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, const FNodeClassMetadata& InInfo);

			virtual ~FNode() = default;

			/** Return the name of this specific instance of the node class. */
			const FVertexName& GetInstanceName() const override;

			/** Return the ID of this specific instance of the node class. */
			const FGuid& GetInstanceID() const override;

			/** Return metadata associated with this node. */
			const FNodeClassMetadata& GetMetadata() const override;

		private:

			FVertexName InstanceName;
			FGuid InstanceID;
			FNodeClassMetadata Info;
	};
}
