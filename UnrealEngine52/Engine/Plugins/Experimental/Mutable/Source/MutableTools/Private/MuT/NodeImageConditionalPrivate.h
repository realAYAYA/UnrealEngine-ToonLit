// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodeImageConditional.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeBool.h"
#include "MuT/AST.h"


namespace mu
{


    class NodeImageConditional::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

        NodeBoolPtr m_parameter;
        NodeImagePtr m_true;
        NodeImagePtr m_false;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

            arch << m_parameter;
            arch << m_true;
            arch << m_false;
        }

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

            arch >> m_parameter;
            arch >> m_true;
            arch >> m_false;
        }
	};


}

