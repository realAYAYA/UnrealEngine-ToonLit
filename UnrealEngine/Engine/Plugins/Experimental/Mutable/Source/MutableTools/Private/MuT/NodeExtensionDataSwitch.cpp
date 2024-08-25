// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeExtensionDataSwitch.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeExtensionDataSwitchPrivate.h"
// #include "MuT/NodePrivate.h"
#include "MuT/NodeScalar.h"

namespace mu
{
	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeExtensionDataSwitch::Private::s_type =
		FNodeType("ExtensionDataSwitch", NodeExtensionData::GetStaticType());

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE(NodeExtensionDataSwitch, EType::Switch, Node, Node::EType::ExtensionData);

	NodeScalarPtr NodeExtensionDataSwitch::GetParameter() const
	{
		return m_pD->Parameter.get();
	}

	void NodeExtensionDataSwitch::SetParameter(NodeScalarPtr pNode)
	{
		m_pD->Parameter = pNode;
	}

	void NodeExtensionDataSwitch::SetOptionCount(int InNumOptions)
	{
		check(InNumOptions >= 0);
		m_pD->Options.SetNum(InNumOptions);
	}

	NodeExtensionDataPtr NodeExtensionDataSwitch::GetOption(int t) const
	{
		check(m_pD->Options.IsValidIndex(t));
		return m_pD->Options[t].get();
	}

	void NodeExtensionDataSwitch::SetOption(int t, NodeExtensionDataPtr pNode)
	{
		check(m_pD->Options.IsValidIndex(t));
		m_pD->Options[t] = pNode;
	}
}