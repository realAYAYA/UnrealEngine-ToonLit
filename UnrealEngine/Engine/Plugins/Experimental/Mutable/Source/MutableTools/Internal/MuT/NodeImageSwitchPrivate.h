// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImageSwitch.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeScalar.h"


namespace mu
{


	class NodeImageSwitch::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr m_pParameter;
		TArray<NodeImagePtr> m_options;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pParameter;
			arch << m_options;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pParameter;
			arch >> m_options;
		}
	};


}
