// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageProject.h"

#include "Math/IntVector.h"
#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageProjectPrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeScalar.h"
#include "MuR/Image.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeImageProject::Private::s_type =
			FNodeType( "ImageProject", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageProject, EType::Project, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetMesh( Ptr<NodeMesh> InNode)
	{
		m_pD->m_pMesh = InNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetProjector( Ptr<NodeProjector> InNode)
	{
		m_pD->m_pProjector = InNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetAngleFadeChannels(bool bFadeRGB, bool bFadeA)
	{
		m_pD->bIsRGBFadingEnabled = bFadeRGB;
		m_pD->bIsAlphaFadingEnabled = bFadeA;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetEnableSeamCorrection(bool bEnabled)
	{
		m_pD->bEnableTextureSeamCorrection = bEnabled;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetAngleFadeStart(Ptr<NodeScalar> InNode)
	{
		m_pD->m_pAngleFadeStart = InNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetAngleFadeEnd( Ptr<NodeScalar> InNode)
	{
		m_pD->m_pAngleFadeEnd = InNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetSamplingMethod( ESamplingMethod SamplingMethod )
	{
		m_pD->SamplingMethod = SamplingMethod;
	}

	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetMinFilterMethod( EMinFilterMethod MinFilterMethod )
	{
		m_pD->MinFilterMethod = MinFilterMethod;
	}



	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetTargetMask( Ptr<NodeImage> InNode)
	{
		m_pD->m_pMask = InNode;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetImage( Ptr<NodeImage> InNode )
	{
		m_pD->m_pImage = InNode;
	}


    //---------------------------------------------------------------------------------------------
    void NodeImageProject::SetLayout( uint8 LayoutIndex )
    {
        m_pD->m_layout = LayoutIndex;
    }


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetImageSize(const FUintVector2& Size)
	{
		m_pD->m_imageSize = Size;
	}

}


