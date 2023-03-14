// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourArithmeticOperation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourArithmeticOperationPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeColourArithmeticOperation::Private::s_type =
			NODE_TYPE( "ColourArithmenticOperation", NodeColour::GetStaticType() );

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourArithmeticOperation, EType::ArithmeticOperation, Node, Node::EType::Colour)


    //---------------------------------------------------------------------------------------------
    const char* NodeColourArithmeticOperation::s_opTypeName[] =
    {
        "Add",
        "Subtract",
        "Multiply",
        "Divide"
    };


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeColourArithmeticOperation::GetInputCount() const
	{
		return 2;
	}


	//---------------------------------------------------------------------------------------------
	Node* NodeColourArithmeticOperation::GetInputNode( int i ) const
	{
		check( i>=0 && i< GetInputCount());

		Node* pResult = 0;

		switch (i)
		{
		case 0: pResult = m_pD->m_pA.get(); break;
		case 1: pResult = m_pD->m_pB.get(); break;
		}

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourArithmeticOperation::SetInputNode( int i, NodePtr pNode )
	{
		check( i>=0 && i< GetInputCount());

		switch (i)
		{
		case 0: m_pD->m_pA = dynamic_cast<NodeColour*>(pNode.get()); break;
		case 1: m_pD->m_pB = dynamic_cast<NodeColour*>(pNode.get()); break;
		}
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	NodeColourArithmeticOperation::OPERATION NodeColourArithmeticOperation::GetOperation() const
	{
		return m_pD->m_operation;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourArithmeticOperation::SetOperation(NodeColourArithmeticOperation::OPERATION o)
	{
		m_pD->m_operation = o;
	}


	NodeColourPtr NodeColourArithmeticOperation::GetA() const
	{
		return m_pD->m_pA.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourArithmeticOperation::SetA(NodeColourPtr pNode)
	{
		m_pD->m_pA = pNode;
	}


	//---------------------------------------------------------------------------------------------
	NodeColourPtr NodeColourArithmeticOperation::GetB() const
	{
		return m_pD->m_pB.get();
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourArithmeticOperation::SetB( NodeColourPtr pNode )
	{
		m_pD->m_pB = pNode;
	}

}
