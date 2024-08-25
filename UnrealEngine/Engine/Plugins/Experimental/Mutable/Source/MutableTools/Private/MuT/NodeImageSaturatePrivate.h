// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageSaturate.h"
#include "MuT/NodeScalar.h"
#include "MuT/AST.h"


namespace mu
{

	class NodeImageSaturate::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeImagePtr m_pSource;
		NodeScalarPtr m_pFactor;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pSource;
			arch << m_pFactor;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pSource;
			arch >> m_pFactor;
		}
	};


}
