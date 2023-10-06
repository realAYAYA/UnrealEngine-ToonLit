// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "Templates/SharedPointer.h"
#include "UObject/Interface.h"


// Forward Declarations
struct FMetasoundFrontendClass;
struct FMetasoundFrontendClassMetadata;
struct FMetasoundFrontendDocument;
struct FMetaSoundFrontendDocumentBuilder;
struct FMetasoundFrontendEdge;
struct FMetasoundFrontendGraph;
struct FMetasoundFrontendNode;
struct FMetasoundFrontendVertex;


namespace Metasound::Frontend
{
	using FNodeRegistryKey = FString;

	/** Interface for querying cached document nodes. */
	class IDocumentGraphNodeCache : public TSharedFromThis<IDocumentGraphNodeCache>
	{
	public:
		virtual ~IDocumentGraphNodeCache() = default;

		virtual bool ContainsNode(const FGuid& InNodeID) const = 0;
		virtual bool ContainsNodesOfClassID(const FGuid& InClassID) const = 0;

		virtual const int32* FindNodeIndex(const FGuid& InNodeID) const = 0;
		virtual TArray<const FMetasoundFrontendNode*> FindNodesOfClassID(const FGuid& InClassID) const = 0;

		virtual const FMetasoundFrontendNode* FindNode(const FGuid& InNodeID) const = 0;

		virtual TArray<const FMetasoundFrontendVertex*> FindNodeInputs(const FGuid& InNodeID, FName TypeName = FName()) const = 0;
		virtual TArray<const FMetasoundFrontendVertex*> FindNodeOutputs(const FGuid& InNodeID, FName TypeName = FName()) const = 0;

		virtual const FMetasoundFrontendVertex* FindInputVertex(const FGuid& InNodeID, const FGuid& InVertexID) const = 0;
		virtual const FMetasoundFrontendVertex* FindInputVertex(const FGuid& InNodeID, FName InVertexName) const = 0;
		virtual const FMetasoundFrontendVertex* FindOutputVertex(const FGuid& InNodeID, const FGuid& InVertexID) const = 0;
		virtual const FMetasoundFrontendVertex* FindOutputVertex(const FGuid& InNodeID, FName InVertexName) const = 0;
	};

	/** Interface for querying cached document edges. */
	class IDocumentGraphEdgeCache : public TSharedFromThis<IDocumentGraphEdgeCache>
	{
	public:
		virtual ~IDocumentGraphEdgeCache() = default;

		virtual bool ContainsEdge(const FMetasoundFrontendEdge& InEdge) const = 0;

		virtual bool IsNodeInputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const = 0;
		virtual bool IsNodeOutputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const = 0;

		virtual TArray<const FMetasoundFrontendEdge*> FindEdges(const FGuid& InNodeID, const FGuid& InVertexID) const = 0;
		virtual const int32* FindEdgeIndexToNodeInput(const FGuid& InNodeID, const FGuid& InVertexID) const = 0;
		virtual const TArray<int32>* FindEdgeIndicesFromNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID) const = 0;
	};

	/** Interface for querying cached document interface. */
	class IDocumentGraphInterfaceCache : public TSharedFromThis<IDocumentGraphInterfaceCache>
	{
	public:
		virtual ~IDocumentGraphInterfaceCache() = default;

		virtual const FMetasoundFrontendClassInput* FindInput(FName InputName) const = 0;
		virtual const FMetasoundFrontendClassOutput* FindOutput(FName OutputName) const = 0;
	};

	/** Interface for querying cached document dependencies. */
	class IDocumentCache : public TSharedFromThis<IDocumentCache>
	{
	public:
		virtual ~IDocumentCache() = default;

		virtual bool ContainsDependency(const FNodeRegistryKey& InClassKey) const = 0;

		virtual const FMetasoundFrontendClass* FindDependency(const Metasound::Frontend::FNodeRegistryKey& InClassKey) const = 0;
		virtual const FMetasoundFrontendClass* FindDependency(const FGuid& InClassID) const = 0;
		virtual const int32* FindDependencyIndex(const Metasound::Frontend::FNodeRegistryKey& InClassKey) const = 0;
		virtual const int32* FindDependencyIndex(const FGuid& InClassID) const = 0;

		virtual const FMetasoundFrontendDocument& GetDocument() const = 0;

		virtual const IDocumentGraphNodeCache& GetNodeCache() const = 0;
		virtual const IDocumentGraphEdgeCache& GetEdgeCache() const = 0;
		virtual const IDocumentGraphInterfaceCache& GetInterfaceCache() const = 0;
	};

} // namespace Metasound::Frontend
