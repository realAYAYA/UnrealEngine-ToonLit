// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourSwitch.h"
#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/AST.h"


namespace mu
{


    class NodeColourSwitch::Private : public NodeColour::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeScalarPtr m_pParameter;
        TArray<NodeColourPtr> m_options;

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

