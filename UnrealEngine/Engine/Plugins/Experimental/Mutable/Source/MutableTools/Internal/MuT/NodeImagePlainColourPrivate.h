// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColour.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImagePlainColour.h"
#include "MuT/AST.h"


namespace mu
{


	class NodeImagePlainColour::Private : public NodeImage::Private
	{
	public:

		Private()
		{
            m_sizeX = 4;
            m_sizeY = 4;
		}

		static FNodeType s_type;

		Ptr<NodeColour> m_pColour;
		int32 m_sizeX, m_sizeY;
		EImageFormat Format = EImageFormat::IF_RGB_UBYTE;


		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 1;
			arch << ver;

			arch << m_pColour;
			arch << m_sizeX;
			arch << m_sizeY;
			arch << (uint32)Format;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver<=1);

			arch >> m_pColour;
			arch >> m_sizeX;
			arch >> m_sizeY;

			if (ver >= 1)
			{
				uint32 Temp = 0;
				arch >> Temp;
				Format = EImageFormat(Temp);
			}
			else
			{
				Format = EImageFormat::IF_RGB_UBYTE;
			}
		}
	};


}
