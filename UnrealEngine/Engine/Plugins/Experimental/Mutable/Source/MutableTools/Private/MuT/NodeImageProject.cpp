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
	void NodeImageProject::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

		switch (i)
		{
		case 0:
			m_pD->m_pMesh = dynamic_cast<NodeMesh*>(pNode.get());
			break;

		case 1:
			m_pD->m_pProjector = dynamic_cast<NodeProjector*>(pNode.get());
			break;

		case 2:
			m_pD->m_pAngleFadeStart = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		case 3:
			m_pD->m_pAngleFadeEnd = dynamic_cast<NodeScalar*>(pNode.get());
			break;

		case 4:
			m_pD->m_pMask = dynamic_cast<NodeImage*>(pNode.get());
			break;

		default:
			m_pD->m_pImage = dynamic_cast<NodeImage*>(pNode.get());
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeMeshPtr NodeImageProject::GetMesh() const
	{
		return m_pD->m_pMesh.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetMesh( NodeMeshPtr pNode )
	{
		m_pD->m_pMesh = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeProjectorPtr NodeImageProject::GetProjector() const
	{
		return m_pD->m_pProjector.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetProjector( NodeProjectorPtr pNode )
	{
		m_pD->m_pProjector = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageProject::GetAngleFadeStart() const
	{
		return m_pD->m_pAngleFadeStart.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetAngleFadeStart( NodeScalarPtr pNode )
	{
		m_pD->m_pAngleFadeStart = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeImageProject::GetAngleFadeEnd() const
	{
		return m_pD->m_pAngleFadeEnd.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetAngleFadeEnd( NodeScalarPtr pNode )
	{
		m_pD->m_pAngleFadeEnd = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageProject::GetTargetMask() const
	{
		return m_pD->m_pMask;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetTargetMask( NodeImagePtr pNode )
	{
		m_pD->m_pMask = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeImagePtr NodeImageProject::GetImage() const
	{
		return m_pD->m_pImage;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetImage( NodeImagePtr pNode )
	{
		m_pD->m_pImage = pNode;
	}


    //---------------------------------------------------------------------------------------------
    uint8_t NodeImageProject::GetLayout() const
    {
        return m_pD->m_layout;
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageProject::SetLayout( uint8_t l )
    {
        m_pD->m_layout = l;
    }


	//---------------------------------------------------------------------------------------------
	const FUintVector2& NodeImageProject::GetImageSize() const
	{
		return m_pD->m_imageSize;
	}


	//---------------------------------------------------------------------------------------------
	void NodeImageProject::SetImageSize(const FUintVector2& size)
	{
		m_pD->m_imageSize = size;
	}

}


