// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/Node.h"
#include "MuT/NodeObject.h"


namespace mu
{

// Forward definitions
class NodeExtensionData;
typedef Ptr<NodeExtensionData> NodeExtensionDataPtr;
typedef Ptr<const NodeExtensionData> NodeExtensionDataPtrConst;


//! Node that evaluates to an ExtensionData
//! \ingroup model
class MUTABLETOOLS_API NodeExtensionData : public Node
{
public:
	// Possible subclasses
	enum class EType : uint8
	{
		Constant = 0,
		Switch,
		Variation,
		None
	};

	//-----------------------------------------------------------------------------------------
	// Life cycle
	//-----------------------------------------------------------------------------------------

	static void Serialise(const NodeExtensionData* pNode, OutputArchive& arch);
	static NodeExtensionDataPtr StaticUnserialise(InputArchive& arch);

	//-----------------------------------------------------------------------------------------
	// Node Interface
	//-----------------------------------------------------------------------------------------
	const FNodeType* GetType() const override;
	static const FNodeType* GetStaticType();

	inline EType GetExtensionDataNodeType() const { return Type; }

	//-----------------------------------------------------------------------------------------
	// Own Interface
	//-----------------------------------------------------------------------------------------

	//-----------------------------------------------------------------------------------------
	// Interface pattern
	//-----------------------------------------------------------------------------------------
	class Private;

protected:

	//! Forbidden. Manage with the Ptr<> template.
	inline ~NodeExtensionData() {}

	EType Type = EType::None;
};



}
