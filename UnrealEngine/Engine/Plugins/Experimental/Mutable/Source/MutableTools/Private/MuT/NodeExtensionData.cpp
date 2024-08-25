// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/NodeExtensionData.h"

#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuT/NodeExtensionDataConstant.h"
#include "MuT/NodeExtensionDataSwitch.h"
#include "MuT/NodeExtensionDataVariation.h"

namespace mu
{


	//---------------------------------------------------------------------------------------------
	// Static initialisation
	//---------------------------------------------------------------------------------------------
	static FNodeType s_type = FNodeType("NodeExtensionData", Node::GetStaticType());

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------

	//---------------------------------------------------------------------------------------------
	void NodeExtensionData::Serialise(const NodeExtensionData* Node, OutputArchive& Archive)
	{
		uint32_t Ver = 0;
		Archive << Ver;

		Archive << uint32_t(Node->Type);
		Node->SerialiseWrapper(Archive);
	}


	//---------------------------------------------------------------------------------------------
	NodeExtensionDataPtr NodeExtensionData::StaticUnserialise(InputArchive& Archive)
	{
		uint32_t Ver;
		Archive >> Ver;
		check(Ver == 0);

		uint32_t Id;
		Archive >> Id;

		switch (EType(Id))
		{
			case EType::Constant:		return NodeExtensionDataConstant::StaticUnserialise(Archive); break;
			case EType::Switch:			return NodeExtensionDataSwitch::StaticUnserialise(Archive); break;
			case EType::Variation:		return NodeExtensionDataVariation::StaticUnserialise(Archive); break;
			default: check(false);
		}

		return 0;
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeExtensionData::GetType() const
	{
		return GetStaticType();
	}


	//---------------------------------------------------------------------------------------------
	const FNodeType* NodeExtensionData::GetStaticType()
	{
		return &s_type;
	}
}


