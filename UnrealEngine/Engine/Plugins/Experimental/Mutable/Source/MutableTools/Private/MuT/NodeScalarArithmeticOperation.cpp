// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeScalarArithmeticOperation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"
#include "MuT/NodeScalarArithmeticOperationPrivate.h"
#include "MuR/Serialisation.h"


namespace mu
{
    MUTABLE_IMPLEMENT_ENUM_SERIALISABLE(NodeScalarArithmeticOperation::OPERATION)

	
	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
    FNodeType NodeScalarArithmeticOperation::Private::s_type =
            FNodeType( "ScalarArithmenticOperation", NodeScalar::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

    MUTABLE_IMPLEMENT_NODE( NodeScalarArithmeticOperation, EType::ArithmeticOperation, Node, Node::EType::Scalar)


    //---------------------------------------------------------------------------------------------
    const char* NodeScalarArithmeticOperation::s_opTypeName[] =
    {
        "Add",
        "Subtract",
        "Multiply",
        "Divide"
    };


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
    NodeScalarArithmeticOperation::OPERATION NodeScalarArithmeticOperation::GetOperation() const
	{
		return m_pD->m_operation;
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarArithmeticOperation::SetOperation(NodeScalarArithmeticOperation::OPERATION o)
	{
		m_pD->m_operation = o;
	}


    NodeScalarPtr NodeScalarArithmeticOperation::GetA() const
	{
		return m_pD->m_pA.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarArithmeticOperation::SetA(NodeScalarPtr pNode)
	{
		m_pD->m_pA = pNode;
	}


	//---------------------------------------------------------------------------------------------
    NodeScalarPtr NodeScalarArithmeticOperation::GetB() const
	{
		return m_pD->m_pB.get();
	}


	//---------------------------------------------------------------------------------------------
    void NodeScalarArithmeticOperation::SetB( NodeScalarPtr pNode )
	{
		m_pD->m_pB = pNode;
	}

}

