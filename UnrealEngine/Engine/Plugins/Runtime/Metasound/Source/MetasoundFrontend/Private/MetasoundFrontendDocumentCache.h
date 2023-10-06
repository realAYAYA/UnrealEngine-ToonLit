// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HAL/Platform.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentCacheInterface.h"
#include "Containers/SortedMap.h"

// Forward Declarations
struct FMetaSoundFrontendDocumentBuilder;


namespace Metasound::Frontend
{
	// Forward Declarations
	class FDocumentCache;

	class FDocumentGraphEdgeCache : public IDocumentGraphEdgeCache
	{
	public:
		FDocumentGraphEdgeCache() = default;
		FDocumentGraphEdgeCache(TSharedRef<IDocumentCache> ParentCache);
		virtual ~FDocumentGraphEdgeCache() = default;

		// IDocumentGraphEdgeCache implementation
		virtual bool ContainsEdge(const FMetasoundFrontendEdge& InEdge) const override;

		virtual bool IsNodeInputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual bool IsNodeOutputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const override;

		virtual TArray<const FMetasoundFrontendEdge*> FindEdges(const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual const int32* FindEdgeIndexToNodeInput(const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual const TArray<int32>* FindEdgeIndicesFromNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID) const override;

		void Init(FEdgeModifyDelegates& OutDelegates);

	private:
		void OnEdgeAdded(int32 InNewIndex);
		void OnRemoveSwappingEdge(int32 SwapIndex, int32 LastIndex);

		// Cache of outputs NodeId/VertexId pairs to associated edge indices
		TMap<FMetasoundFrontendVertexHandle, TArray<int32>> OutputToEdgeIndices;

		// Cache of Input NodeId/VertexId pairs to associated edge indices
		TMap<FMetasoundFrontendVertexHandle, int32> InputToEdgeIndex;

		TSharedPtr<IDocumentCache> Parent;

		FDelegateHandle OnAddedHandle;
		FDelegateHandle OnRemoveSwappingHandle;
	};


	class FDocumentGraphNodeCache : public IDocumentGraphNodeCache
	{
	public:
		FDocumentGraphNodeCache() = default;
		FDocumentGraphNodeCache(TSharedRef<IDocumentCache> ParentCache);
		virtual ~FDocumentGraphNodeCache() = default;

		// IDocumentGraphNodeCache implementation
		virtual bool ContainsNode(const FGuid& InNodeID) const override;
		virtual bool ContainsNodesOfClassID(const FGuid& InClassID) const override;

		virtual const int32* FindNodeIndex(const FGuid& InNodeID) const override;
		virtual TArray<const FMetasoundFrontendNode*> FindNodesOfClassID(const FGuid& InClassID) const override;
		virtual const FMetasoundFrontendNode* FindNode(const FGuid& InNodeID) const override;

		virtual TArray<const FMetasoundFrontendVertex*> FindNodeInputs(const FGuid& InNodeID, FName TypeName = FName()) const override;
		virtual TArray<const FMetasoundFrontendVertex*> FindNodeOutputs(const FGuid& InNodeID, FName TypeName = FName()) const override;

		virtual const FMetasoundFrontendVertex* FindInputVertex(const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual const FMetasoundFrontendVertex* FindInputVertex(const FGuid& InNodeID, FName InVertexName) const override;
		virtual const FMetasoundFrontendVertex* FindOutputVertex(const FGuid& InNodeID, const FGuid& InVertexID) const override;
		virtual const FMetasoundFrontendVertex* FindOutputVertex(const FGuid& InNodeID, FName InVertexName) const override;

		void Init(FNodeModifyDelegates& OutDelegates);

	private:
		void OnNodeAdded(int32 NewIndex);
		void OnRemoveSwappingNode(int32 IndexBeingRemoved, int32 LastIndex);

		// Cache of NodeId to array index of node
		TSortedMap<FGuid, int32> IDToIndex;

		// Cache of ClassID to referencing node indices
		TSortedMap<FGuid, TArray<int32>> ClassIDToNodeIndices;

		TSharedPtr<IDocumentCache> Parent;

		FDelegateHandle OnAddedHandle;
		FDelegateHandle OnRemoveSwappingHandle;
	};


	class FDocumentGraphInterfaceCache : public IDocumentGraphInterfaceCache
	{
	public:
		FDocumentGraphInterfaceCache() = default;
		FDocumentGraphInterfaceCache(TSharedRef<IDocumentCache> ParentCache);
		virtual ~FDocumentGraphInterfaceCache() = default;

		// IDocumentGraphInterfaceCache implementation
		virtual const FMetasoundFrontendClassInput* FindInput(FName InputName) const override;
		virtual const FMetasoundFrontendClassOutput* FindOutput(FName OutputName) const override;

		void Init(FInterfaceModifyDelegates& OutDelegates);

	private:
		void OnInputAdded(int32 NewIndex);
		void OnOutputAdded(int32 NewIndex);
		void OnRemovingInput(int32 IndexBeingRemoved);
		void OnRemovingOutput(int32 IndexBeingRemoved);

		// Cache of Input name to array index of input
		TMap<FName, int32> InputNameToIndex;

		// Cache of Output name to array index of output
		TMap<FName, int32> OutputNameToIndex;

		TSharedPtr<IDocumentCache> Parent;

		FDelegateHandle OnInputAddedHandle;
		FDelegateHandle OnOutputAddedHandle;
		FDelegateHandle OnRemovingInputHandle;
		FDelegateHandle OnRemovingOutputHandle;
	};


	class FDocumentCache : public IDocumentCache
	{
	public:
		FDocumentCache() = default;
		FDocumentCache(const FMetasoundFrontendDocument& InDocument);
		virtual ~FDocumentCache() = default;

		virtual bool ContainsDependency(const FNodeRegistryKey& InClassKey) const override;

		virtual const FMetasoundFrontendClass* FindDependency(const FNodeRegistryKey& InClassKey) const override;
		virtual const FMetasoundFrontendClass* FindDependency(const FGuid& InClassID) const override;
		virtual const int32* FindDependencyIndex(const Metasound::Frontend::FNodeRegistryKey& InClassKey) const override;
		virtual const int32* FindDependencyIndex(const FGuid& InClassID) const override;

		virtual const FMetasoundFrontendDocument& GetDocument() const override;
		virtual const IDocumentGraphEdgeCache& GetEdgeCache() const override;
		virtual const IDocumentGraphNodeCache& GetNodeCache() const override;
		virtual const IDocumentGraphInterfaceCache& GetInterfaceCache() const override;

		void Init(FDocumentModifyDelegates& OutDelegates);

	private:
		void OnDependencyAdded(int32 InNewIndex);
		void OnRemoveSwappingDependency(int32 SwapIndex, int32 LastIndex);
		void OnRenamingDependencyClass(const int32 IndexBeingRenamed, const FMetasoundFrontendClassName& NewName);

		// Cache of dependency (Class) ID to corresponding class dependency index
		TSortedMap<FGuid, int32> IDToIndex;

		// Cache of version data to corresponding class dependency index
		TSortedMap<FNodeRegistryKey, int32> KeyToIndex;

		TSharedPtr<FDocumentGraphEdgeCache> EdgeCache;
		TSharedPtr<FDocumentGraphNodeCache> NodeCache;
		TSharedPtr<FDocumentGraphInterfaceCache> InterfaceCache;

		FDelegateHandle OnAddedHandle;
		FDelegateHandle OnRemoveSwappingHandle;
		FDelegateHandle OnRenamingDependencyClassHandle;

		const FMetasoundFrontendDocument* Document = nullptr;
	};
} // namespace Metasound::Frontend
