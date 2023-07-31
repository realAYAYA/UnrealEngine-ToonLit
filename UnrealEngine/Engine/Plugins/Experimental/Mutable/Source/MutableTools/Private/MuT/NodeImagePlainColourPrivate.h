// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/AST.h"

#include "MuR/MemoryPrivate.h"


namespace mu
{


	class NodeImagePlainColour::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		Private()
		{
            m_sizeX = 4;
            m_sizeY = 4;
		}

		static NODE_TYPE s_type;

		NodeColourPtr m_pColour;
		int m_sizeX, m_sizeY;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32_t ver = 0;
			arch << ver;

			arch << m_pColour;
			arch << m_sizeX;
			arch << m_sizeY;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32_t ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pColour;
			arch >> m_sizeX;
			arch >> m_sizeY;
		}
	};


}
