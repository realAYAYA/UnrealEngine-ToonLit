// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageBinarise.h"
#include "MuT/AST.h"
#include "MuT/NodeScalar.h"


namespace mu
{


	class NodeImageBinarise::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pBase;
		NodeScalarPtr m_pThreshold;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pBase;
			arch << m_pThreshold;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pBase;
			arch >> m_pThreshold;
		}
	};


}
