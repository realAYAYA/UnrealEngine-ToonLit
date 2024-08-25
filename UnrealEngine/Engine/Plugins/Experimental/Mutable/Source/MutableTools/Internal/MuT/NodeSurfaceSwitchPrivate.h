// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeSurfaceSwitch.h"

#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{


	class NodeSurfaceSwitch::Private : public NodeSurface::Private
	{
	public:

		static FNodeType s_type;

		Ptr<NodeScalar> Parameter;
		TArray<Ptr<NodeSurface>> Options;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << Parameter;
			arch << Options;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> Parameter;
			arch >> Options;
		}

	};


}
