// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColourPrivate.h"
#include "MuT/NodeColourSampleImage.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeImage.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{


	class NodeColourSampleImage::Private : public NodeColour::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeImagePtr m_pImage;
		NodeScalarPtr m_pX;
		NodeScalarPtr m_pY;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pImage;
			arch << m_pX;
			arch << m_pY;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pImage;
			arch >> m_pX;
			arch >> m_pY;
		}
	};


}
