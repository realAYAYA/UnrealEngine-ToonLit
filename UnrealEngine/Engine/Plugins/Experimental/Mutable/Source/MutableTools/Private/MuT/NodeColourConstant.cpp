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
	FNodeType NodeColourConstant::Private::s_type =
			FNodeType( "ColourConstant", NodeColour::GetStaticType() );


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE( NodeColourConstant, EType::Constant, Node, Node::EType::Colour)

	NodeColourConstantPtr NodeColourConstant::OldStaticUnserialise(InputArchive& arch)
	{
		NodeColourConstantPtr pResult = new NodeColourConstant();
		FVector3f Value;

		arch >> Value;
		pResult->GetPrivate()->m_value = FVector4f(Value[0], Value[1], Value[2], 1.0f);

		return pResult;
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

