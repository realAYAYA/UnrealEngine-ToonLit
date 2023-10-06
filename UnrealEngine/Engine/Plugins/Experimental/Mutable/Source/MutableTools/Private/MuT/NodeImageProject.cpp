// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageProject.h"

#include "Math/IntVector.h"
#include "Misc/AssertionMacros.h"
#include "MuT/NodeImageProjectPrivate.h"
#include "MuT/NodeMesh.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeProjector.h"
#include "MuT/NodeScalar.h"



namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeImageProject::Private::s_type =
			NODE_TYPE( "ImageProject", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeImageProject, EType::Project, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeImageProject::GetInputCount() const
	{
		return 6;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeImageProject::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

		switch (i)
		{
		case 0:
			pResult = m_pD->m_pMesh.get();
			break;

		case 1:
			pResult = m_pD->m_pProjector.get();
			break;

		case 2:
			pResult = m_pD->m_pAngleFadeStart.get();
			break;

		case 3:
			pResult = m_pD->m_pAngleFadeEnd.get();
			break;

		case 4:
			pResult = m_pD->m_pMask.get();
			break;

		default:
			pResult = m_pD->m_pImage.get();
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetInputNode( int i, Ptr<Node> InNode)
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pMesh = dynamic_cast<NodeMesh*>(InNode.get());
			break;

		case 1:
			m_pD->m_pProjector = dynamic_cast<NodeProjector*>(InNode.get());
			break;

		case 2:
			m_pD->m_pAngleFadeStart = dynamic_cast<NodeScalar*>(InNode.get());
			break;

		case 3:
			m_pD->m_pAngleFadeEnd = dynamic_cast<NodeScalar*>(InNode.get());
			break;

		case 4:
			m_pD->m_pMask = dynamic_cast<NodeImage*>(InNode.get());
			break;

		default:
			m_pD->m_pImage = dynamic_cast<NodeImage*>(InNode.get());
			break;
		}
	}


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


