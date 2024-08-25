// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuT/NodeExtensionDataConstant.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeExtensionDataConstantPrivate.h"
#include "MuT/NodePrivate.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeExtensionDataConstant::Private::s_type =
			FNodeType("ExtensionDataConstant", NodeExtensionData::GetStaticType());


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE(NodeExtensionDataConstant, EType::Constant, Node, Node::EType::ExtensionData);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	ExtensionDataPtrConst NodeExtensionDataConstant::GetValue() const
	{
		return m_pD->Value;
	}


	void NodeExtensionDataConstant::SetValue(ExtensionDataPtrConst Value)
	{
		m_pD->Value = Value;
    }
}
