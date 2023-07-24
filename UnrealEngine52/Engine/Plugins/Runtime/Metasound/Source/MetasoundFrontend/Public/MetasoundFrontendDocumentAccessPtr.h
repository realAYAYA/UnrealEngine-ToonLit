// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundAccessPtr.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendRegistries.h"

/** Metasound Frontend Document Access Pointers provide convenience methods for
 * getting access pointers of FMetasoundFrontendDocument sub-elements. For instance,
 * to get an input vertex subobject on the root graph of the document, one can call:
 *
 * FDocumentAccessPtr DocumentAccessPtr = MakeAccessPtr<FDocumentAccessPtr>(AccessPoint, Document);
 *
 * FClassAccessPtr ClassAccessPtr = DocumentAccessPtr.GetRootGraph().GetNodeWithNodeID(NodeID).GetInputWithVertexID(VertexID);
 *
 */

namespace Metasound
{
	namespace Frontend
	{
		using FVertexAccessPtr = TAccessPtr<FMetasoundFrontendVertex>;
		using FConstVertexAccessPtr = TAccessPtr<const FMetasoundFrontendVertex>;
		using FClassInputAccessPtr = TAccessPtr<FMetasoundFrontendClassInput>;
		using FConstClassInputAccessPtr = TAccessPtr<const FMetasoundFrontendClassInput>;
		using FClassOutputAccessPtr = TAccessPtr<FMetasoundFrontendClassOutput>;
		using FConstClassOutputAccessPtr = TAccessPtr<const FMetasoundFrontendClassOutput>;
		using FVariableAccessPtr = TAccessPtr<FMetasoundFrontendVariable>;
		using FConstVariableAccessPtr = TAccessPtr<const FMetasoundFrontendVariable>;

		class METASOUNDFRONTEND_API FNodeAccessPtr : public TAccessPtr<FMetasoundFrontendNode>
		{
		public:
			using Super = TAccessPtr<FMetasoundFrontendNode>;

			// Inherit constructors from base class
			using Super::Super;

			FVertexAccessPtr GetInputWithName(const FVertexName& InName);
			FVertexAccessPtr GetInputWithVertexID(const FGuid& InID);
			FVertexAccessPtr GetOutputWithName(const FVertexName& InName);
			FVertexAccessPtr GetOutputWithVertexID(const FGuid& InID);

			FConstVertexAccessPtr GetInputWithName(const FVertexName& InName) const;
			FConstVertexAccessPtr GetInputWithVertexID(const FGuid& InID) const;
			FConstVertexAccessPtr GetOutputWithName(const FVertexName& InName) const;
			FConstVertexAccessPtr GetOutputWithVertexID(const FGuid& InID) const;
		};

		class METASOUNDFRONTEND_API FConstNodeAccessPtr : public TAccessPtr<const FMetasoundFrontendNode>
		{
		public:
			using Super = TAccessPtr<const FMetasoundFrontendNode>;

			// Inherit constructors from base class
			using Super::Super;

			FConstVertexAccessPtr GetInputWithName(const FVertexName& InName) const;
			FConstVertexAccessPtr GetInputWithVertexID(const FGuid& InID) const;
			FConstVertexAccessPtr GetOutputWithName(const FVertexName& InName) const;
			FConstVertexAccessPtr GetOutputWithVertexID(const FGuid& InID) const;
		};

		using FGraphAccessPtr = TAccessPtr<FMetasoundFrontendGraph>;
		using FConstGraphAccessPtr = TAccessPtr<const FMetasoundFrontendGraph>;

		class METASOUNDFRONTEND_API FClassAccessPtr : public TAccessPtr<FMetasoundFrontendClass>
		{
		public:
			using Super = TAccessPtr<FMetasoundFrontendClass>;

			// Inherit constructors from base class
			using Super::Super;

			FClassInputAccessPtr GetInputWithName(const FVertexName& InName);
			FClassOutputAccessPtr GetOutputWithName(const FVertexName& InName);
			FConstClassInputAccessPtr GetInputWithName(const FVertexName& InName) const;
			FConstClassOutputAccessPtr GetOutputWithName(const FVertexName& InName) const;
		};

		class METASOUNDFRONTEND_API FConstClassAccessPtr : public TAccessPtr<const FMetasoundFrontendClass>
		{
		public:
			using Super = TAccessPtr<const FMetasoundFrontendClass>;

			// Inherit constructors from base class
			using Super::Super;

			FConstClassInputAccessPtr GetInputWithName(const FVertexName& InName) const;
			FConstClassOutputAccessPtr GetOutputWithName(const FVertexName& InName) const;
		};

		class METASOUNDFRONTEND_API FGraphClassAccessPtr : public TAccessPtr<FMetasoundFrontendGraphClass>
		{
		public:
			using Super = TAccessPtr<FMetasoundFrontendGraphClass>;

			// Inherit constructors from base class
			using Super::Super;

			FClassInputAccessPtr GetInputWithName(const FVertexName& InName);
			FClassInputAccessPtr GetInputWithNodeID(const FGuid& InNodeID);
			FClassOutputAccessPtr GetOutputWithName(const FVertexName& InName);
			FClassOutputAccessPtr GetOutputWithNodeID(const FGuid& InNodeID);

			FVariableAccessPtr GetVariableWithID(const FGuid& InID);
			FNodeAccessPtr GetNodeWithNodeID(const FGuid& InNodeID);
			FGraphAccessPtr GetGraph();

			FConstClassInputAccessPtr GetInputWithName(const FVertexName& InName) const;
			FConstClassInputAccessPtr GetInputWithNodeID(const FGuid& InNodeID) const;
			FConstClassOutputAccessPtr GetOutputWithName(const FVertexName& InName) const;
			FConstClassOutputAccessPtr GetOutputWithNodeID(const FGuid& InNodeID) const;

			FConstVariableAccessPtr GetVariableWithID(const FGuid& InID) const;
			FConstNodeAccessPtr GetNodeWithNodeID(const FGuid& InNodeID) const;
			FConstGraphAccessPtr GetGraph() const;
		};

		class METASOUNDFRONTEND_API FConstGraphClassAccessPtr : public TAccessPtr<const FMetasoundFrontendGraphClass>
		{
			using Super = TAccessPtr<const FMetasoundFrontendGraphClass>;

			// Inherit constructors from base class
			using Super::Super;

			FConstClassInputAccessPtr GetInputWithName(const FVertexName& InName) const;
			FConstClassInputAccessPtr GetInputWithNodeID(const FGuid& InNodeID) const;
			FConstClassOutputAccessPtr GetOutputWithName(const FVertexName& InName) const;
			FConstClassOutputAccessPtr GetOutputWithNodeID(const FGuid& InNodeID) const;
			FConstVariableAccessPtr GetVariableWithID(const FGuid& InID) const;
			FConstNodeAccessPtr GetNodeWithNodeID(const FGuid& InNodeID) const;
			FConstGraphAccessPtr GetGraph() const;
		};

		class METASOUNDFRONTEND_API FDocumentAccessPtr : public TAccessPtr<FMetasoundFrontendDocument>
		{
		public:
			using Super = TAccessPtr<FMetasoundFrontendDocument>;

			// Inherit constructors from base class
			using Super::Super;

			FGraphClassAccessPtr GetRootGraph();
			FGraphClassAccessPtr GetSubgraphWithID(const FGuid& InID);
			FClassAccessPtr GetDependencyWithID(const FGuid& InID);
			FClassAccessPtr GetClassWithID(const FGuid& InID);
			FClassAccessPtr GetClassWithMetadata(const FMetasoundFrontendClassMetadata& InMetadata);
			FClassAccessPtr GetClassWithInfo(const FNodeClassInfo& InInfo);
			FClassAccessPtr GetClassWithRegistryKey(const FNodeRegistryKey& InKey);

			FConstGraphClassAccessPtr GetRootGraph() const;
			FConstGraphClassAccessPtr GetSubgraphWithID(const FGuid& InID) const;
			FConstClassAccessPtr GetDependencyWithID(const FGuid& InID) const;
			FConstClassAccessPtr GetClassWithID(const FGuid& InID) const;
			FConstClassAccessPtr GetClassWithMetadata(const FMetasoundFrontendClassMetadata& InMetadata) const;
			FConstClassAccessPtr GetClassWithInfo(const FNodeClassInfo& InInfo) const;
			FConstClassAccessPtr GetClassWithRegistryKey(const FNodeRegistryKey& InKey) const;
		};

		class METASOUNDFRONTEND_API FConstDocumentAccessPtr : public TAccessPtr<const FMetasoundFrontendDocument>
		{
		public:
			using Super = TAccessPtr<const FMetasoundFrontendDocument>;

			// Inherit constructors from base class
			using Super::Super;

			FConstGraphClassAccessPtr GetRootGraph() const;
			FConstGraphClassAccessPtr GetSubgraphWithID(const FGuid& InID) const;
			FConstClassAccessPtr GetDependencyWithID(const FGuid& InID) const;
			FConstClassAccessPtr GetClassWithID(const FGuid& InID) const;
			FConstClassAccessPtr GetClassWithMetadata(const FMetasoundFrontendClassMetadata& InMetadata) const;
			FConstClassAccessPtr GetClassWithInfo(const FNodeClassInfo& InInfo) const;
			FConstClassAccessPtr GetClassWithRegistryKey(const FNodeRegistryKey& InKey) const;
		};
	}
}
