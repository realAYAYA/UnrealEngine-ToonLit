// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeExtensionDataVariation.h"

#include "Misc/AssertionMacros.h"
#include "MuT/NodeExtensionDataVariationPrivate.h"

namespace mu
{
	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	FNodeType NodeExtensionDataVariation::Private::s_type =
		FNodeType("ExtensionDataVariation", NodeExtensionData::GetStaticType());

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	MUTABLE_IMPLEMENT_NODE(NodeExtensionDataVariation, EType::Variation, Node, Node::EType::ExtensionData);


	//---------------------------------------------------------------------------------------------
	// Own Interface
	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataVariation::SetDefaultValue(NodeExtensionDataPtr InValue)
	{
		m_pD->DefaultValue = InValue;
	}

	void NodeExtensionDataVariation::SetVariationCount(int InCount)
	{
		check(InCount >= 0);
		m_pD->Variations.SetNum(InCount);
	}

	int NodeExtensionDataVariation::GetVariationCount() const
	{
		return m_pD->Variations.Num();
	}

	void NodeExtensionDataVariation::SetVariationTag(int InIndex, const FString& Tag)
	{
		check(m_pD->Variations.IsValidIndex(InIndex));

		m_pD->Variations[InIndex].Tag = Tag;
	}
	//---------------------------------------------------------------------------------------------
	void NodeExtensionDataVariation::SetVariationValue(int InIndex, NodeExtensionDataPtr InValue)
	{
		check(m_pD->Variations.IsValidIndex(InIndex));

		m_pD->Variations[InIndex].Value = InValue;
	}

}
