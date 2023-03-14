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
	void NodeColourConstant::GetValue( float* pR, float *pG, float* pB ) const
	{
		if (pR)
		{
			*pR = m_pD->m_value[0];
		}

		if (pG)
		{
			*pG = m_pD->m_value[1];
		}

		if (pB)
		{
			*pB = m_pD->m_value[2];
		}
	}


	//---------------------------------------------------------------------------------------------
	void NodeColourConstant::SetValue( float r, float g, float b )
	{
		m_pD->m_value = vec3<float>(r,g,b);
	}


}

