// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeColour.h"
#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeImageGradient.h"
#include "MuT/AST.h"
#include "MuR/MutableMath.h"


namespace mu
{


	class NodeImageGradient::Private : public NodeImage::Private
	{
	public:

		static FNodeType s_type;

		NodeColourPtr m_pColour0;
		NodeColourPtr m_pColour1;
		FIntVector2 m_size = { 256,1 };		

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 0;
			arch << ver;

			arch << m_pColour0;
			arch << m_pColour1;
			arch << m_size;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
			check(ver==0);

			arch >> m_pColour0;
			arch >> m_pColour1;
			arch >> m_size;
		}
	};


}
