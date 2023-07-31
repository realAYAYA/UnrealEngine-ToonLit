// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeStringPrivate.h"

#include "MuT/NodeStringConstant.h"
#include "MuT/NodeImage.h"
#include "MuT/AST.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/ParametersPrivate.h"


namespace mu
{


	class NodeStringConstant::Private : public NodeString::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		string m_value;

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
