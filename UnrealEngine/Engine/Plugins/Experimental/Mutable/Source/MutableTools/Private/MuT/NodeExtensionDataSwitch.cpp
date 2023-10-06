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
	NODE_TYPE NodeExtensionDataSwitch::Private::s_type =
		NODE_TYPE("ExtensionDataSwitch", NodeExtensionData::GetStaticType());

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	MUTABLE_IMPLEMENT_NODE(NodeExtensionDataSwitch, EType::Switch, Node, Node::EType::ExtensionData);

	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------

	//---------------------------------------------------------------------------------------------
	int NodeExtensionDataSwitch::GetInputCount() const
	{
		return 1 + m_pD->Options.Num();
	}

	//---------------------------------------------------------------------------------------------
	Node* NodeExtensionDataSwitch::GetInputNode(int i) const
	{
		check(i >=0 && i < GetInputCount());


		Node* Result = 0;
		switch (i)
		{
			case 0:
				Result = m_pD->Parameter.get();
				break;

			default:
				Result = m_pD->Options[i - 1].get();
				break;
		}

		return Result;
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataSwitch::SetInputNode(int i, NodePtr pNode)
	{
		check(i >= 0 && i < GetInputCount());

		switch (i)
		{
			case 0:
				m_pD->Parameter = dynamic_cast<NodeScalar*>(pNode.get());
				break;

			default:
				m_pD->Options[i - 1] = dynamic_cast<NodeExtensionData*>(pNode.get());
				break;
		}
	}

	//---------------------------------------------------------------------------------------------
	NodeScalarPtr NodeExtensionDataSwitch::GetParameter() const
	{
		return m_pD->Parameter.get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataSwitch::SetParameter(NodeScalarPtr pNode)
	{
		m_pD->Parameter = pNode;
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataSwitch::SetOptionCount(int InNumOptions)
	{
		check(InNumOptions >= 0);
		m_pD->Options.SetNum(InNumOptions);
	}

	//---------------------------------------------------------------------------------------------
	NodeExtensionDataPtr NodeExtensionDataSwitch::GetOption(int t) const
	{
		check(m_pD->Options.IsValidIndex(t));
		return m_pD->Options[t].get();
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataSwitch::SetOption(int t, NodeExtensionDataPtr pNode)
	{
		check(m_pD->Options.IsValidIndex(t));
		m_pD->Options[t] = pNode;
	}
}