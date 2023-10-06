// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeExtensionDataVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeExtensionDataVariationPrivate.h"

namespace mu
{
	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	NODE_TYPE NodeExtensionDataVariation::Private::s_type =
		NODE_TYPE("ExtensionDataVariation", NodeExtensionData::GetStaticType());

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	MUTABLE_IMPLEMENT_NODE(NodeExtensionDataVariation, EType::Variation, Node, Node::EType::ExtensionData);

	//---------------------------------------------------------------------------------------------
	// Node Interface
	//---------------------------------------------------------------------------------------------

	//---------------------------------------------------------------------------------------------
	int NodeExtensionDataVariation::GetInputCount() const
	{
		return 1 + m_pD->Variations.Num();
	}

	//---------------------------------------------------------------------------------------------
	Node* NodeExtensionDataVariation::GetInputNode(int i) const
	{
		check(i >= 0 && i < GetInputCount());

		if (i == 0)
		{
			return m_pD->DefaultValue.get();
		}

		i -= 1;

		if (m_pD->Variations.IsValidIndex(i))
		{
			return m_pD->Variations[i].Value.get();
		}

		return nullptr;
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataVariation::SetInputNode(int i, NodePtr pNode)
	{
		check(i >= 0 && i < GetInputCount());

		if (i == 0)
		{
			m_pD->DefaultValue = dynamic_cast<NodeExtensionData*>(pNode.get());
			return;
		}

		i -= 1;

		if (m_pD->Variations.IsValidIndex(i))
		{
			m_pD->Variations[i].Value = dynamic_cast<NodeExtensionData*>(pNode.get());
			return;
		}
	}

	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataVariation::SetDefaultValue(NodeExtensionDataPtr InValue)
	{
		m_pD->DefaultValue = InValue;
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataVariation::SetVariationCount(int InCount)
	{
		check(InCount >= 0);
		m_pD->Variations.SetNum(InCount);
	}

	//---------------------------------------------------------------------------------------------
	int NodeExtensionDataVariation::GetVariationCount() const
	{
		return m_pD->Variations.Num();
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataVariation::SetVariationTag(int InIndex, const char* InStrTag)
	{
		check(m_pD->Variations.IsValidIndex(InIndex));
		check(InStrTag);

		m_pD->Variations[InIndex].Tag = InStrTag;
	}

	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataVariation::SetVariationValue(int InIndex, NodeExtensionDataPtr InValue)
	{
		check(m_pD->Variations.IsValidIndex(InIndex));

		m_pD->Variations[InIndex].Value = InValue;
	}

}
