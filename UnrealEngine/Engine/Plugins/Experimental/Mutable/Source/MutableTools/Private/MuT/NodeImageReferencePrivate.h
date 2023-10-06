// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageReference.h"
#include "MuT/StreamsPrivate.h"


namespace mu
{

	class NodeImageReference::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE();

	public:

		static NODE_TYPE s_type;

		uint32 ImageReferenceID = 0;

		//!
		void Serialise(OutputArchive& arch) const
		{
			uint32 Ver = 0;
			arch << Ver;

			arch << ImageReferenceID;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
			uint32 Ver;
			arch >> Ver;
			check(Ver == 0);

			arch >> ImageReferenceID;
		}
	};

}
