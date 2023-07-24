// Copyright Epic Games, Inc. All Rights Reserved.


#include "MetasoundNode.h"
#include "CoreMinimal.h"

namespace Metasound
{
	FNode::FNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, const FNodeClassMetadata& InInfo)
	: InstanceName(InInstanceName)
	, InstanceID(InInstanceID)
	, Info(InInfo)
	{
	}

	/** Return the name of this specific instance of the node class. */
	const FVertexName& FNode::GetInstanceName() const
	{
		return InstanceName;
	}

	/** Return the ID of this specific instance of the node class. */
	const FGuid& FNode::GetInstanceID() const
	{
		return InstanceID;
	}

	/** Return the type name of this node. */
	const FNodeClassMetadata& FNode::GetMetadata() const
	{
		return Info;
	}
}
