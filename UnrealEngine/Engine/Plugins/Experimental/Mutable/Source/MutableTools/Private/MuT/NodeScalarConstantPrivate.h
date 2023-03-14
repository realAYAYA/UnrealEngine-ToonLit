// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeScalarPrivate.h"

#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeImage.h"
#include "MuT/AST.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeScalarConstant::Private : public NodeScalar::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		float m_value;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_value;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_value;
		}
	};


}
