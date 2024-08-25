// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageLuminance.h"


namespace mu
{

	class NodeImageLuminance::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pSource;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pSource;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pSource;
		}
	};


}
