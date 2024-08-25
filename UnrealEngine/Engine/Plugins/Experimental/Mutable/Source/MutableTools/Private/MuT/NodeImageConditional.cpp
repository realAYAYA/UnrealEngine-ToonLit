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
    FNodeType NodeImageConditional::Private::s_type =
            FNodeType( "ImageConditional", NodeImage::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeImageConditional, EType::Conditional, Node, Node::EType::Image)


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


