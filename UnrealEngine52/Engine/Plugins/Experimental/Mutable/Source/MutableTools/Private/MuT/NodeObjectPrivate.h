// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodePrivate.h"

#include "MuT/NodeObject.h"
#include "MuT/NodeLayout.h"

namespace mu
{

	class NodeObject::Private : public Node::Private
	{
	public:

        virtual NodeLayoutPtr GetLayout( int lod, int component, int surface, int texture ) const = 0;

	};

}

