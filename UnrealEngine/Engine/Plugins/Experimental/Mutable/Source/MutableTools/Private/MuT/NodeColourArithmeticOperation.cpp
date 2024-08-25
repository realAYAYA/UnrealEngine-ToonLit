// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourArithmeticOperation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeColourArithmeticOperationPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{
	MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(NodeColourArithmeticOperation::OPERATION)

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeColourArithmeticOperation::Private::s_type =
			FNodeType( "ColourArithmenticOperation", NodeColour::GetStaticType() );

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
