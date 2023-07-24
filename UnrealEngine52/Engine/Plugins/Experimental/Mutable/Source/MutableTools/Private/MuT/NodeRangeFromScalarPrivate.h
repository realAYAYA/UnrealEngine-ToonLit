// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeRangePrivate.h"
#include "MuT/NodeRangeFromScalar.h"
#include "MuT/NodeRange.h"
#include "MuT/NodeScalar.h"


namespace mu
{


    class NodeRangeFromScalar::Private : public NodeRange::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

        NodeScalarPtr m_pSize;
        string m_name;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

            arch << m_name;
            arch << m_pSize;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

            arch >> m_name;
            arch >> m_pSize;
		}
	};


}
