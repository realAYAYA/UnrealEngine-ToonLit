// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeScalarSwitch.h"
#include "MuT/NodeScalarPrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/AST.h"


namespace mu
{


    class NodeScalarSwitch::Private : public NodeScalar::Private
	{
	public:

		static FNodeType s_type;

		NodeScalarPtr m_pParameter;
        TArray<NodeScalarPtr> m_options;

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
