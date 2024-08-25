// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/NodeExtensionData.h"


namespace mu
{

// Forward definitions
class NodeExtensionDataConstant;
typedef Ptr<NodeExtensionDataConstant> NodeExtensionDataConstantPtr;
typedef Ptr<const NodeExtensionDataConstant> NodeExtensionDataConstantPtrConst;

//! Node that outputs a constant ExtensionData
//! \ingroup model
class MUTABLETOOLS_API NodeExtensionDataConstant : public NodeExtensionData
{
public:

	//-----------------------------------------------------------------------------------------
	// Life cycle
	//-----------------------------------------------------------------------------------------
	NodeExtensionDataConstant();

	void SerialiseWrapper(OutputArchive& arch) const override;
	static void Serialise(const NodeExtensionDataConstant* pNode, OutputArchive& arch);
	static NodeExtensionDataConstantPtr StaticUnserialise(InputArchive& arch);


	//-----------------------------------------------------------------------------------------
	// Node Interface
	//-----------------------------------------------------------------------------------------
	const FNodeType* GetType() const override;
	static const FNodeType* GetStaticType();

	//-----------------------------------------------------------------------------------------
	// Own Interface
	//-----------------------------------------------------------------------------------------

	//! Get the constant ExtensionData that will be returned.
	ExtensionDataPtrConst GetValue() const;

	//! Set the constant ExtensionData that will be returned.
	void SetValue(ExtensionDataPtrConst Value);

	//-----------------------------------------------------------------------------------------
	// Interface pattern
	//-----------------------------------------------------------------------------------------
	class Private;
	Private* GetPrivate() const;
	Node::Private* GetBasePrivate() const override;

protected:
	//! Forbidden. Manage with the Ptr<> template.
	~NodeExtensionDataConstant();

private:
	Private* m_pD;

};


}
