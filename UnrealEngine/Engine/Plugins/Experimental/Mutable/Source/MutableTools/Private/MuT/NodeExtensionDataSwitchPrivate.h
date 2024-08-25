// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuT/NodeExtensionDataSwitch.h"
#include "MuT/NodePrivate.h"

namespace mu
{
	class NodeExtensionDataSwitch::Private : public Node::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr Parameter;
		TArray<NodeExtensionDataPtr> Options;

		void Serialise(OutputArchive& Archive) const
		{
			uint32_t Version = 0;
			Archive << Version;

			Archive << Parameter;
			Archive << Options;
		}

		void Unserialise(InputArchive& Archive)
		{
			uint32 Version;
			Archive >> Version;
			check(Version == 0);

			Archive >> Parameter;
			Archive >> Options;
		}
	};
}