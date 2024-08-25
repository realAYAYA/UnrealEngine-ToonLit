// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourConstant.h"
#include "MuT/NodeColourPrivate.h"

#include "MuR/MutableMath.h"

namespace mu
{

	class NodeColourConstant::Private : public NodeColour::Private
	{
	public:

		static FNodeType s_type;

		FVector4f m_value;

		//!
		void Serialise( OutputArchive& arch ) const
		{
			const uint32 Version = 0;
			arch << Version;
			arch << m_value;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
			uint32 Version;
			arch >> Version;
			check(Version==0)

			arch >> m_value;
		}
	};

}
