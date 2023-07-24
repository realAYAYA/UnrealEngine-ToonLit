// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeImageProject.h"

#include "MuT/NodeImagePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeColour.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodeProjector.h"
#include "MuR/MutableMath.h"

namespace mu
{

	class NodeImageProject::Private : public NodeImage::Private
	{
	public:

		MUTABLE_DEFINE_CONST_VISITABLE()

	public:

		static NODE_TYPE s_type;

		NodeProjectorPtr m_pProjector;
		NodeMeshPtr m_pMesh;
		NodeScalarPtr m_pAngleFadeStart;
		NodeScalarPtr m_pAngleFadeEnd;
		NodeImagePtr m_pImage;
		NodeImagePtr m_pMask;
		FUintVector2 m_imageSize;
		uint8 m_layout = 0;
		bool bIsRGBFadingEnabled = true;
		bool bIsAlphaFadingEnabled = true;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 3;
			arch << ver;

			arch << m_pProjector;
			arch << m_pMesh;
			arch << m_pAngleFadeStart;
			arch << m_pAngleFadeEnd;
			arch << m_pImage;
            arch << m_pMask;
            arch << m_layout;
			arch << m_imageSize;
			arch << bIsRGBFadingEnabled;
			arch << bIsAlphaFadingEnabled;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=2 && ver<=3);

			arch >> m_pProjector;
			arch >> m_pMesh;
			arch >> m_pAngleFadeStart;
			arch >> m_pAngleFadeEnd;
			arch >> m_pImage;
			arch >> m_pMask;
            arch >> m_layout;
			arch >> m_imageSize;
			if (ver >= 3)
			{
				arch >> bIsRGBFadingEnabled;
				arch >> bIsAlphaFadingEnabled;
			}
		}
	};


}
