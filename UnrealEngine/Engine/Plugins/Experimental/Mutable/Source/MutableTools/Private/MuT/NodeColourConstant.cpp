// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeColourConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuR/MutableMath.h"
#include "MuT/NodeColourConstantPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeColourConstant::Private::s_type =
			NODE_TYPE( "ColourConstant", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourConstant, EType::Constant, Node, Node::EType::Colour)

	NodeColourConstantPtr NodeColourConstant::OldStaticUnserialise(InputArchive& arch)
	{
		NodeColourConstantPtr pResult = new NodeColourConstant();
		vec3<float> Value;

		arch >> Value;
		pResult->GetPrivate()->m_value = FVector4f(Value[0], Value[1], Value[2], 1.0f);

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------
	int NodeColourConstant::GetInputCount() const
	{
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    Node* NodeColourConstant::GetInputNode( int ) const
	{
		check( false );
		return 0;
	}


	//---------------------------------------------------------------------------------------------
    void NodeColourConstant::SetInputNode( int, NodePtr )
	{
		check( false );
	}


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	FVector4f NodeColourConstant::GetValue() const
	{
		return m_pD->m_value;
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourConstant::SetValue(FVector4f Value)
	{
		m_pD->m_value = Value;
	}


}

