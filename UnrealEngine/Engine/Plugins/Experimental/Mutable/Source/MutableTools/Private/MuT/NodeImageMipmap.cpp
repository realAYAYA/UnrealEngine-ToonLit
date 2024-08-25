// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageMipmap.h"

#include "Misc/AssertionMacros.h"
#include "MuR/ImagePrivate.h"
#include "MuT/NodeImageMipmapPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeImageMipmap::Private::s_type =
            FNodeType( "ImageMipmap", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageMipmap, EType::Mipmap, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeImagePtr NodeImageMipmap::GetSource() const
	{
		return m_pD->m_pSource.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageMipmap::SetSource( NodeImagePtr pNode )
	{
		m_pD->m_pSource = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageMipmap::SetMipmapGenerationSettings( EMipmapFilterType filterType,
													   EAddressMode addressMode,
													   float sharpenFactor,
													   bool mipDitherAlpha )
	{
		m_pD->m_settings = FMipmapGenerationSettings{ sharpenFactor, filterType, addressMode, mipDitherAlpha };
	}

}
