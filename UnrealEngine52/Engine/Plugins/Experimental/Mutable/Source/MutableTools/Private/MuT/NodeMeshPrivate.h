// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"

#include "MuT/NodeMesh.h"
#include "MuT/NodeLayout.h"


namespace mu
{


	class NodeMesh::Private : public Node::Private
	{
	public:

		virtual NodeLayoutPtr GetLayout( int index ) const = 0;

	};

}
