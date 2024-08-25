// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuT/NodeExtensionDataConstant.h"
#include "MuT/NodeLayout.h"
#include "MuT/NodePrivate.h"
#include "MuT/AST.h"


namespace mu
{

class NodeExtensionDataConstant::Private : public Node::Private
{
public:

	static FNodeType s_type;

	ExtensionDataPtrConst Value;
	
	//!
	void Serialise(OutputArchive& Archive) const
	{
		uint32_t Version = 0;
		Archive << Version;

		Archive << Value;
	}

	//!
	void Unserialise(InputArchive& Archive)
	{
		uint32_t Version;
		Archive >> Version;
		check(Version == 0);

		Archive >> Value;
	}
};

}
