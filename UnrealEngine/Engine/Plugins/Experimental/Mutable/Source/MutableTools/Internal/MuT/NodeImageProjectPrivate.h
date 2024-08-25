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

		static FNodeType s_type;

		Ptr<NodeProjector> m_pProjector;
		Ptr<NodeMesh> m_pMesh;
		Ptr<NodeScalar> m_pAngleFadeStart;
		Ptr<NodeScalar> m_pAngleFadeEnd;
		Ptr<NodeImage> m_pImage;
		Ptr<NodeImage> m_pMask;
		FUintVector2 m_imageSize;
		uint8 m_layout = 0;
		bool bIsRGBFadingEnabled = true;
		bool bIsAlphaFadingEnabled = true;
		bool bEnableTextureSeamCorrection = true;
		ESamplingMethod SamplingMethod = ESamplingMethod::Point;
		EMinFilterMethod MinFilterMethod = EMinFilterMethod::None;

		//!
		void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 5;
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
			arch << bEnableTextureSeamCorrection;
			arch << SamplingMethod;
			arch << MinFilterMethod;
		}

		//!
		void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver>=2 && ver<=5);

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

			if (ver >= 5)
			{
				arch >> bEnableTextureSeamCorrection;
			}

			if (ver >= 4)
			{
				arch >> SamplingMethod;
				arch >> MinFilterMethod;
			}
		}
	};


}
