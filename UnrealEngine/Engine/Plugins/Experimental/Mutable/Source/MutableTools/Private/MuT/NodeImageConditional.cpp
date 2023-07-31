// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeImageConditional.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeBool.h"
#include "MuT/NodeImageConditionalPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    NODE_TYPE NodeImageConditional::Private::s_type =
            NODE_TYPE( "ImageConditional", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageConditional, EType::Conditional, Node, Node::EType::Image)


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
    int NodeImageConditional::GetInputCount() const
	{
        return 3;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeImageConditional::GetInputNode( int i ) const
	{
		check( i>=0 && i<GetInputCount() );

		Node* pResult = 0;

        switch (i)
		{
        case 0:
            pResult = m_pD->m_parameter.get();
            break;

        case 1:
            pResult = m_pD->m_true.get();
            break;

        case 2:
            pResult = m_pD->m_false.get();
            break;

        default:
			break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageConditional::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i<GetInputCount() );

        switch (i)
		{
        case 0:
            m_pD->m_parameter = dynamic_cast<NodeBool*>(pNode.get());
            break;

        case 1:
            m_pD->m_true = dynamic_cast<NodeImage*>(pNode.get());
            break;

        case 2:
            m_pD->m_false = dynamic_cast<NodeImage*>(pNode.get());
            break;

        default:
			break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeBool* NodeImageConditional::GetParameter() const
	{
        return m_pD->m_parameter.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeImageConditional::SetParameter( NodeBool* pNode )
	{
        m_pD->m_parameter = pNode;
	}


    //---------------------------------------------------------------------------------------------
    NodeImage* NodeImageConditional::GetOptionTrue() const
    {
        return m_pD->m_true.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageConditional::SetOptionTrue( NodeImage* pNode )
    {
        m_pD->m_true = pNode;
    }


    //---------------------------------------------------------------------------------------------
    NodeImage* NodeImageConditional::GetOptionFalse() const
    {
        return m_pD->m_false.get();
    }


    //---------------------------------------------------------------------------------------------
    void NodeImageConditional::SetOptionFalse( NodeImage* pNode )
    {
        m_pD->m_false = pNode;
    }



}


