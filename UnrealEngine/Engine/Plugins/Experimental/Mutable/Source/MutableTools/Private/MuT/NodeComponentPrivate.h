// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodePrivate.h"
#include "MuT/NodeComponent.h"
#include "MuT/NodeComponentNew.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{
	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class NodeComponent::Private : public Node::Private
	{
	public:

		//! Look for the parent component that this node is editing
		virtual const NodeComponentNew::Private* GetParentComponentNew() const = 0;
	};


}

