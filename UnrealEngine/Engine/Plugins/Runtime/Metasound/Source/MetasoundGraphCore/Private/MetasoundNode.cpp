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

	const FVertexInterface& FNode::GetVertexInterface() const
	{
		return Info.DefaultInterface;
	}

	bool FNode::SetVertexInterface(const FVertexInterface& InInterface)
	{
		// This code looks a bit counter-intuitive. Here's the explanation: 
		// 1. This node doesn't support manipulating the interface. The default 
		//    interface is the only interface that works with this node.
		// 2. The only valid argument to this function is so set the interface 
		//    to it's default state. 
		// 3. If the input vertex interface is exactly the same as the default 
		//    interface, then we can say that this function executed 
		//    successfully.
		// 4. Return "true" if the supplied interface is the same as the 
		//    default interface, false otherwise. 
		return GetVertexInterface() == InInterface;
	}

	bool FNode::IsVertexInterfaceSupported(const FVertexInterface& InInterface) const
	{
		// This node doesn't support manipulating the interface. The default 
		// interface is the only interface that works with this node.
		return GetVertexInterface() == InInterface;
	}
}
