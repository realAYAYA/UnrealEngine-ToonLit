// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ExtensionData.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuT/NodeExtensionData.h"

namespace mu
{
	// Forward declarations
	class NodeScalar;
	using NodeScalarPtr = Ptr<NodeScalar>;
	using NodeScalarPtrConst = Ptr<const NodeScalar>;

	class NodeExtensionDataSwitch;
	using NodeExtensionDataSwitchPtr = Ptr<NodeExtensionDataSwitch>;
	using NodeExtensionDataSwitchPtrConst = Ptr<const NodeExtensionDataSwitch>;

	class MUTABLETOOLS_API NodeExtensionDataSwitch : public NodeExtensionData
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------
		NodeExtensionDataSwitch();

		void SerialiseWrapper(OutputArchive& arch) const override;
		static void Serialise(const NodeExtensionDataSwitch* pNode, OutputArchive& arch);
		static NodeExtensionDataSwitchPtr StaticUnserialise(InputArchive& arch);

		//-----------------------------------------------------------------------------------------
		// Node Interface
		//-----------------------------------------------------------------------------------------
		const FNodeType* GetType() const override;
		static const FNodeType* GetStaticType();

		//-----------------------------------------------------------------------------------------
		// Own Interface
		//-----------------------------------------------------------------------------------------
		NodeScalarPtr GetParameter() const;
		void SetParameter(NodeScalarPtr InParameter);

		void SetOptionCount(int);

		NodeExtensionDataPtr GetOption(int t) const;
		void SetOption(int t, NodeExtensionDataPtr);

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;
		Private* GetPrivate() const;
		Node::Private* GetBasePrivate() const override;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~NodeExtensionDataSwitch();

	private:

		Private* m_pD;
	};
}