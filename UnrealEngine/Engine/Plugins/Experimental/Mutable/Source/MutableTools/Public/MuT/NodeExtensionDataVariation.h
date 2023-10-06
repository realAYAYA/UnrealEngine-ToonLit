// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/NodeExtensionData.h"

namespace mu
{
	// Forward declarations
	class NodeExtensionDataVariation;
	using NodeExtensionDataVariationPtr = Ptr<NodeExtensionDataVariation>;
	using NodeExtensionDataVariationPtrConst = Ptr<const NodeExtensionDataVariation>;

	class MUTABLETOOLS_API NodeExtensionDataVariation : public NodeExtensionData
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------
		NodeExtensionDataVariation();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise(const NodeExtensionDataVariation* pNode, OutputArchive& arch);
		static NodeExtensionDataVariationPtr StaticUnserialise(InputArchive& arch);

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------
		const NODE_TYPE* GetType() const override;
		static const NODE_TYPE* GetStaticType();

		int GetInputCount() const override;
		Node* GetInputNode(int i) const override;
		void SetInputNode(int i, NodePtr pNode) override;

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------
		void SetDefaultValue(NodeExtensionDataPtr InValue);

		void SetVariationCount(int InCount);
		int GetVariationCount() const;

		void SetVariationTag(int InIndex, const char* InStrTag);

		void SetVariationValue(int InIndex, NodeExtensionDataPtr InValue);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
		Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeExtensionDataVariation();

	private:

		Private* m_pD;
	};
}