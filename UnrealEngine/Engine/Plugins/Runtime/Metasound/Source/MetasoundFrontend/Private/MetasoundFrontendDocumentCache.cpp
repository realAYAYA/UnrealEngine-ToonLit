// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendDocumentCache.h"

#include "Algo/ForEach.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundTrace.h"


namespace Metasound::Frontend
{
	namespace DocumentCachePrivate
	{
		// Modifies cache map of an attempted array index swap removal, where the
		// given cache map is a map of keys to indices.
		template <typename TMapType, typename TKeyType>
		void RemoveSwapMapIndexChecked(const TKeyType& InSwapKey, const TKeyType& InLastKey, TMapType& OutMap)
		{
			// Remove swap item
			int32 SwapIndex = INDEX_NONE;
			const bool bRemoved = OutMap.RemoveAndCopyValue(InSwapKey, SwapIndex);
			checkf(bRemoved, TEXT("Failed to remove/swap expected MetaSound Document Cache key/value pair"));

			// Swap last item index if not the same entry
			if (InLastKey != InSwapKey)
			{
				OutMap.FindChecked(InLastKey) = SwapIndex;
			}
		}

		// Modifies cache map of an attempted array index swap removal, where the
		// given cache map is a map of keys to arrays of indices.
		template <typename TMapArrayType, typename TKeyType>
		void RemoveSwapMapArrayIndexChecked(
			const TKeyType& InSwapKey,
			int32 SwapIndex,
			const TKeyType& InLastKey,
			int32 LastIndex,
			TMapArrayType& OutMap
		)
		{
			constexpr bool bAllowShrinking = false;

			// Remove swap item
			{
				TArray<int32>& Indices = OutMap.FindChecked(InSwapKey);
				Indices.RemoveAllSwap([&SwapIndex](const int32& Index)
				{
					return Index == SwapIndex;
				}, bAllowShrinking);
				if (Indices.IsEmpty())
				{
					OutMap.Remove(InSwapKey);
				}
			}

			// Swap last item index if not the same entry
			if (InSwapKey != InLastKey && SwapIndex != LastIndex)
			{
				TArray<int32>& Indices = OutMap.FindChecked(InLastKey);
				Algo::ForEachIf(Indices,
					[&LastIndex](const int32& Index) { return Index == LastIndex; },
					[&SwapIndex](int32& Index) { Index = SwapIndex; }
				);
			}
		}
	}

	FDocumentCache::FDocumentCache(const FMetasoundFrontendDocument& InDocument)
		: Document(&InDocument)
	{
	}

	void FDocumentCache::Init(FDocumentModifyDelegates& OutDelegates)
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FDocumentCache::Init);

		check(Document);

		IDToIndex.Reset();
		KeyToIndex.Reset();

		for (int32 Index = 0; Index < Document->Dependencies.Num(); ++Index)
		{
			const FMetasoundFrontendClass& Class = Document->Dependencies[Index];
			const FMetasoundFrontendClassMetadata& Metadata = Class.Metadata;
			FNodeRegistryKey Key = NodeRegistryKey::CreateKey(Metadata);
			KeyToIndex.Add(MoveTemp(Key), Index);
			IDToIndex.Add(Class.ID, Index);
		}

		OutDelegates.OnDependencyAdded.Remove(OnAddedHandle);
		OutDelegates.OnRemoveSwappingDependency.Remove(OnRemoveSwappingHandle);
		OutDelegates.OnRenamingDependencyClass.Remove(OnRenamingDependencyClassHandle);

		OnAddedHandle = OutDelegates.OnDependencyAdded.AddSP(this, &FDocumentCache::OnDependencyAdded);
		OnRemoveSwappingHandle = OutDelegates.OnRemoveSwappingDependency.AddSP(this, &FDocumentCache::OnRemoveSwappingDependency);
		OnRenamingDependencyClassHandle = OutDelegates.OnRenamingDependencyClass.AddSP(this, &FDocumentCache::OnRenamingDependencyClass);

		if (!EdgeCache.IsValid())
		{
			EdgeCache = MakeShared<FDocumentGraphEdgeCache>(AsShared());
		}
		EdgeCache->Init(OutDelegates.EdgeDelegates);

		if (!NodeCache.IsValid())
		{
			NodeCache = MakeShared<FDocumentGraphNodeCache>(AsShared());
		}
		NodeCache->Init(OutDelegates.NodeDelegates);

		if (!InterfaceCache.IsValid())
		{
			InterfaceCache = MakeShared<FDocumentGraphInterfaceCache>(AsShared());
		}
		InterfaceCache->Init(OutDelegates.InterfaceDelegates);
	}

	const FMetasoundFrontendDocument& FDocumentCache::GetDocument() const
	{
		check(Document);
		return *Document;
	}

	bool FDocumentCache::ContainsDependency(const FNodeRegistryKey& InClassKey) const
	{
		return KeyToIndex.Contains(InClassKey);
	}

	const FMetasoundFrontendClass* FDocumentCache::FindDependency(const FNodeRegistryKey& InClassKey) const
	{
		if (const int32* Index = KeyToIndex.Find(InClassKey))
		{
			return &GetDocument().Dependencies[*Index];
		}

		return nullptr;
	}

	const FMetasoundFrontendClass* FDocumentCache::FindDependency(const FGuid& InClassID) const
	{
		if (const int32* Index = IDToIndex.Find(InClassID))
		{
			return &GetDocument().Dependencies[*Index];
		}

		return nullptr;
	}

	const int32* FDocumentCache::FindDependencyIndex(const Metasound::Frontend::FNodeRegistryKey& InClassKey) const
	{
		return KeyToIndex.Find(InClassKey);
	}

	const int32* FDocumentCache::FindDependencyIndex(const FGuid& InClassID) const
	{
		return IDToIndex.Find(InClassID);
	}

	const IDocumentGraphEdgeCache& FDocumentCache::GetEdgeCache() const
	{
		return *EdgeCache;
	}

	const IDocumentGraphNodeCache& FDocumentCache::GetNodeCache() const
	{
		return *NodeCache;
	}

	const IDocumentGraphInterfaceCache& FDocumentCache::GetInterfaceCache() const
	{
		return *InterfaceCache;
	}

	void FDocumentCache::OnDependencyAdded(int32 NewIndex)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendClass& Dependency = GetDocument().Dependencies[NewIndex];
		const FNodeRegistryKey Key = NodeRegistryKey::CreateKey(Dependency.Metadata);
		KeyToIndex.Add(Key, NewIndex);
		IDToIndex.Add(Dependency.ID, NewIndex);
	}

	void FDocumentCache::OnRemoveSwappingDependency(int32 SwapIndex, int32 LastIndex)
	{
		using namespace Metasound::Frontend;

		const TArray<FMetasoundFrontendClass>& Dependencies = GetDocument().Dependencies;
		const FMetasoundFrontendClass& SwapDependency = Dependencies[SwapIndex];
		const FMetasoundFrontendClass& LastDependency = Dependencies[LastIndex];

		const FNodeRegistryKey SwapKey = NodeRegistryKey::CreateKey(SwapDependency.Metadata);
		const FNodeRegistryKey LastKey = NodeRegistryKey::CreateKey(LastDependency.Metadata);
		DocumentCachePrivate::RemoveSwapMapIndexChecked(SwapKey, LastKey, KeyToIndex);
		DocumentCachePrivate::RemoveSwapMapIndexChecked(SwapDependency.ID, LastDependency.ID, IDToIndex);
	}

	void FDocumentCache::OnRenamingDependencyClass(const int32 IndexBeingRenamed, const FMetasoundFrontendClassName& NewName)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendClass& DependencyBeingRemoved = GetDocument().Dependencies[IndexBeingRenamed];
		const FNodeRegistryKey OldKey = NodeRegistryKey::CreateKey(DependencyBeingRemoved.Metadata);
		FMetasoundFrontendClassMetadata NewMetadata = DependencyBeingRemoved.Metadata;
		NewMetadata.SetClassName(NewName);
		const FNodeRegistryKey NewKey = NodeRegistryKey::CreateKey(NewMetadata);

		KeyToIndex.Add(NewKey, IndexBeingRenamed);
		KeyToIndex.Remove(OldKey);
	}

	FDocumentGraphInterfaceCache::FDocumentGraphInterfaceCache(TSharedRef<IDocumentCache> ParentCache)
		: Parent(ParentCache)
	{
	}

	const FMetasoundFrontendClassInput* FDocumentGraphInterfaceCache::FindInput(FName InputName) const
	{
		if (const int32* Index = InputNameToIndex.Find(InputName))
		{
			const FMetasoundFrontendDocument& Doc = Parent->GetDocument();
			return &Doc.RootGraph.Interface.Inputs[*Index];
		}

		return nullptr;
	}

	const FMetasoundFrontendClassOutput* FDocumentGraphInterfaceCache::FindOutput(FName OutputName) const
	{
		if (const int32* Index = OutputNameToIndex.Find(OutputName))
		{
			const FMetasoundFrontendDocument& Doc = Parent->GetDocument();
			return &Doc.RootGraph.Interface.Outputs[*Index];
		}

		return nullptr;
	}

	void FDocumentGraphInterfaceCache::Init(FInterfaceModifyDelegates& OutDelegates)
	{
		InputNameToIndex.Reset();
		OutputNameToIndex.Reset();

		const FMetasoundFrontendGraphClass& GraphClass = Parent->GetDocument().RootGraph;
		const TArray<FMetasoundFrontendClassInput>& Inputs = GraphClass.Interface.Inputs;
		for (int32 Index = 0; Index < Inputs.Num(); ++Index)
		{
			InputNameToIndex.Add(Inputs[Index].Name, Index);
		};

		const TArray<FMetasoundFrontendClassOutput>& Outputs = GraphClass.Interface.Outputs;
		for (int32 Index = 0; Index < Outputs.Num(); ++Index)
		{
			OutputNameToIndex.Add(Outputs[Index].Name, Index);
		};

		OutDelegates.OnInputAdded.Remove(OnInputAddedHandle);
		OutDelegates.OnOutputAdded.Remove(OnOutputAddedHandle);
		OutDelegates.OnRemovingInput.Remove(OnRemovingInputHandle);
		OutDelegates.OnRemovingOutput.Remove(OnRemovingOutputHandle);

		OnInputAddedHandle = OutDelegates.OnInputAdded.AddSP(this, &FDocumentGraphInterfaceCache::OnInputAdded);
		OnOutputAddedHandle = OutDelegates.OnOutputAdded.AddSP(this, &FDocumentGraphInterfaceCache::OnOutputAdded);
		OnRemovingInputHandle = OutDelegates.OnRemovingInput.AddSP(this, &FDocumentGraphInterfaceCache::OnRemovingInput);
		OnRemovingOutputHandle = OutDelegates.OnRemovingOutput.AddSP(this, &FDocumentGraphInterfaceCache::OnRemovingOutput);
	}

	void FDocumentGraphInterfaceCache::OnInputAdded(int32 NewIndex)
	{
		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Document.RootGraph;
		const FMetasoundFrontendClassInput& Input = GraphClass.Interface.Inputs[NewIndex];
		InputNameToIndex.Add(Input.Name, NewIndex);
	}

	void FDocumentGraphInterfaceCache::OnOutputAdded(int32 NewIndex)
	{
		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Document.RootGraph;
		const FMetasoundFrontendClassOutput& Output = GraphClass.Interface.Outputs[NewIndex];
		OutputNameToIndex.Add(Output.Name, NewIndex);
	}

	void FDocumentGraphInterfaceCache::OnRemovingInput(int32 IndexBeingRemoved)
	{
		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Document.RootGraph;
		const FMetasoundFrontendClassInput& Input = GraphClass.Interface.Inputs[IndexBeingRemoved];
		InputNameToIndex.Remove(Input.Name);
	}

	void FDocumentGraphInterfaceCache::OnRemovingOutput(int32 IndexBeingRemoved)
	{
		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Document.RootGraph;
		const FMetasoundFrontendClassOutput& Output = GraphClass.Interface.Outputs[IndexBeingRemoved];
		OutputNameToIndex.Remove(Output.Name);
	}

	FDocumentGraphNodeCache::FDocumentGraphNodeCache(TSharedRef<IDocumentCache> ParentCache)
		: Parent(ParentCache)
	{
	}

	void FDocumentGraphNodeCache::Init(FNodeModifyDelegates& OutDelegates)
	{
		IDToIndex.Reset();
		ClassIDToNodeIndices.Reset();

		const FMetasoundFrontendGraph& Graph = Parent->GetDocument().RootGraph.Graph;
		const TArray<FMetasoundFrontendNode>& Nodes = Graph.Nodes;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			const FMetasoundFrontendNode& Node = Nodes[Index];
			const FMetasoundFrontendClass* Class = Parent->FindDependency(Node.ClassID);
			check(Class);

			const FGuid& NodeID = Node.GetID();
			IDToIndex.Add(NodeID, Index);
			ClassIDToNodeIndices.FindOrAdd(Node.ClassID).Add(Index);
		}

		OutDelegates.OnNodeAdded.Remove(OnAddedHandle);
		OnAddedHandle = OutDelegates.OnNodeAdded.AddSP(this, &FDocumentGraphNodeCache::OnNodeAdded);

		OutDelegates.OnRemoveSwappingNode.Remove(OnRemoveSwappingHandle);
		OnRemoveSwappingHandle = OutDelegates.OnRemoveSwappingNode.AddSP(this, &FDocumentGraphNodeCache::OnRemoveSwappingNode);
	}

	bool FDocumentGraphNodeCache::ContainsNode(const FGuid& InNodeID) const
	{
		return IDToIndex.Contains(InNodeID);
	}

	const int32* FDocumentGraphNodeCache::FindNodeIndex(const FGuid& InNodeID) const
	{
		return IDToIndex.Find(InNodeID);
	}

	TArray<const FMetasoundFrontendNode*> FDocumentGraphNodeCache::FindNodesOfClassID(const FGuid& InClassID) const
	{
		TArray<const FMetasoundFrontendNode*> Nodes;
		if (const TArray<int32>* NodeIndices = ClassIDToNodeIndices.Find(InClassID))
		{
			const FMetasoundFrontendDocument& Document = Parent->GetDocument();
			const FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
			Algo::Transform(*NodeIndices, Nodes, [&Graph](const int32& Index) { return &Graph.Nodes[Index]; });
		}
		return Nodes;
	}

	const FMetasoundFrontendNode* FDocumentGraphNodeCache::FindNode(const FGuid& InNodeID) const
	{
		if (const int32* NodeIndex = IDToIndex.Find(InNodeID))
		{
			const FMetasoundFrontendDocument& Document = Parent->GetDocument();
			const FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
			return &Graph.Nodes[*NodeIndex];
		}

		return nullptr;
	}

	bool FDocumentGraphNodeCache::ContainsNodesOfClassID(const FGuid& InClassID) const
	{
		if (const TArray<int32>* NodeIndices = ClassIDToNodeIndices.Find(InClassID))
		{
			return !NodeIndices->IsEmpty();
		}

		return false;
	}

	TArray<const FMetasoundFrontendVertex*> FDocumentGraphNodeCache::FindNodeInputs(const FGuid& InNodeID, FName TypeName) const
	{
		TArray<const FMetasoundFrontendVertex*> FoundVertices;
		if (const FMetasoundFrontendNode* Node = FindNode(InNodeID))
		{
			auto IsVertexOfType = [&TypeName](const FMetasoundFrontendVertex& Vertex) { return TypeName.IsNone() || Vertex.TypeName == TypeName; };
			auto GetVertexPtr = [](const FMetasoundFrontendVertex& Vertex) { return &Vertex; };
			Algo::TransformIf(Node->Interface.Inputs, FoundVertices, IsVertexOfType, GetVertexPtr);
		}

		return FoundVertices;
	}

	TArray<const FMetasoundFrontendVertex*> FDocumentGraphNodeCache::FindNodeOutputs(const FGuid& InNodeID, FName TypeName) const
	{
		TArray<const FMetasoundFrontendVertex*> FoundVertices;
		if (const FMetasoundFrontendNode* Node = FindNode(InNodeID))
		{
			auto IsVertexOfType = [&TypeName](const FMetasoundFrontendVertex& Vertex) { return TypeName.IsNone() || Vertex.TypeName == TypeName; };
			auto GetVertexPtr = [](const FMetasoundFrontendVertex& Vertex) { return &Vertex; };
			Algo::TransformIf(Node->Interface.Outputs, FoundVertices, IsVertexOfType, GetVertexPtr);
		}

		return FoundVertices;
	}

	const FMetasoundFrontendVertex* FDocumentGraphNodeCache::FindInputVertex(const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		if (const FMetasoundFrontendNode* Node = FindNode(InNodeID))
		{
			auto VertexMatchesPredicate = [&InVertexID](const FMetasoundFrontendVertex& Vertex)
			{
				return Vertex.VertexID == InVertexID;
			};
			return Node->Interface.Inputs.FindByPredicate(VertexMatchesPredicate);
		}

		return nullptr;
	}

	const FMetasoundFrontendVertex* FDocumentGraphNodeCache::FindInputVertex(const FGuid& InNodeID, FName InVertexName) const
	{
		if (const FMetasoundFrontendNode* Node = FindNode(InNodeID))
		{
			auto VertexMatchesPredicate = [&InVertexName](const FMetasoundFrontendVertex& Vertex)
			{
				return Vertex.Name == InVertexName;
			};
			return Node->Interface.Inputs.FindByPredicate(VertexMatchesPredicate);
		}

		return nullptr;
	}

	const FMetasoundFrontendVertex* FDocumentGraphNodeCache::FindOutputVertex(const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		if (const FMetasoundFrontendNode* Node = FindNode(InNodeID))
		{
			auto VertexMatchesPredicate = [&InVertexID](const FMetasoundFrontendVertex& Vertex)
			{
				return Vertex.VertexID == InVertexID;
			};
			return Node->Interface.Outputs.FindByPredicate(VertexMatchesPredicate);
		}

		return nullptr;
	}

	const FMetasoundFrontendVertex* FDocumentGraphNodeCache::FindOutputVertex(const FGuid& InNodeID, FName InVertexName) const
	{
		if (const FMetasoundFrontendNode* Node = FindNode(InNodeID))
		{
			auto VertexMatchesPredicate = [&InVertexName](const FMetasoundFrontendVertex& Vertex)
			{
				return Vertex.Name == InVertexName;
			};
			return Node->Interface.Outputs.FindByPredicate(VertexMatchesPredicate);
		}

		return nullptr;
	}

	void FDocumentGraphNodeCache::OnNodeAdded(int32 InNewIndex)
	{
		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
		const FMetasoundFrontendNode& Node = Graph.Nodes[InNewIndex];
		IDToIndex.Add(Node.GetID(), InNewIndex);
		ClassIDToNodeIndices.FindOrAdd(Node.ClassID).Add(InNewIndex);
	}

	void FDocumentGraphNodeCache::OnRemoveSwappingNode(int32 SwapIndex, int32 LastIndex)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const TArray<FMetasoundFrontendNode>& Nodes = Document.RootGraph.Graph.Nodes;
		const FMetasoundFrontendNode& SwapNode = Nodes[SwapIndex];
		const FMetasoundFrontendNode& LastNode = Nodes[LastIndex];

		DocumentCachePrivate::RemoveSwapMapArrayIndexChecked(SwapNode.ClassID, SwapIndex, LastNode.ClassID, LastIndex, ClassIDToNodeIndices);
		DocumentCachePrivate::RemoveSwapMapIndexChecked(SwapNode.GetID(), LastNode.GetID(), IDToIndex);
	}

	FDocumentGraphEdgeCache::FDocumentGraphEdgeCache(TSharedRef<IDocumentCache> ParentCache)
		: Parent(ParentCache)
	{
	}

	void FDocumentGraphEdgeCache::Init(FEdgeModifyDelegates& OutDelegates)
	{
		OutputToEdgeIndices.Reset();
		InputToEdgeIndex.Reset();

		const TArray<FMetasoundFrontendEdge>& Edges = Parent->GetDocument().RootGraph.Graph.Edges;
		for (int32 Index = 0; Index < Edges.Num(); ++Index)
		{
			const FMetasoundFrontendEdge& Edge = Edges[Index];
			OutputToEdgeIndices.FindOrAdd(Edge.GetFromVertexHandle()).Add(Index);
			InputToEdgeIndex.Add(Edge.GetToVertexHandle()) = Index;
		}

		OutDelegates.OnEdgeAdded.Remove(OnAddedHandle);
		OnAddedHandle = OutDelegates.OnEdgeAdded.AddSP(this, &FDocumentGraphEdgeCache::OnEdgeAdded);

		OutDelegates.OnRemoveSwappingEdge.Remove(OnRemoveSwappingHandle);
		OnRemoveSwappingHandle = OutDelegates.OnRemoveSwappingEdge.AddSP(this, &FDocumentGraphEdgeCache::OnRemoveSwappingEdge);
	}

	bool FDocumentGraphEdgeCache::ContainsEdge(const FMetasoundFrontendEdge& InEdge) const
	{
		const FMetasoundFrontendVertexHandle InputPair = { InEdge.ToNodeID, InEdge.ToVertexID };
		if (const int32* Index = InputToEdgeIndex.Find(InputPair))
		{
			const FMetasoundFrontendDocument& Document = Parent->GetDocument();
			const FMetasoundFrontendEdge& Edge = Document.RootGraph.Graph.Edges[*Index];
			return Edge.FromNodeID == InEdge.FromNodeID && Edge.FromVertexID == InEdge.FromVertexID;
		}

		return false;
	}

	TArray<const FMetasoundFrontendEdge*> FDocumentGraphEdgeCache::FindEdges(const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		TArray<const FMetasoundFrontendEdge*> Edges;

		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;

		const FMetasoundFrontendVertexHandle Handle { InNodeID, InVertexID };
		if (const int32* Index = InputToEdgeIndex.Find(Handle))
		{
			Edges.Add(&Graph.Edges[*Index]);
		}

		if (const TArray<int32>* Indices = OutputToEdgeIndices.Find(Handle))
		{
			Algo::Transform(*Indices, Edges, [&Graph](const int32& Index) { return &Graph.Edges[Index]; });
		}

		return Edges;
	}

	const int32* FDocumentGraphEdgeCache::FindEdgeIndexToNodeInput(const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		return InputToEdgeIndex.Find({ InNodeID, InVertexID });
	}

	const TArray<int32>* FDocumentGraphEdgeCache::FindEdgeIndicesFromNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		return OutputToEdgeIndices.Find({ InNodeID, InVertexID });
	}

	bool FDocumentGraphEdgeCache::IsNodeInputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		return InputToEdgeIndex.Contains(FMetasoundFrontendVertexHandle { InNodeID, InVertexID });
	}

	bool FDocumentGraphEdgeCache::IsNodeOutputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		return OutputToEdgeIndices.Contains(FMetasoundFrontendVertexHandle { InNodeID, InVertexID });
	}

	void FDocumentGraphEdgeCache::OnEdgeAdded(int32 InNewIndex)
	{
		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
		const FMetasoundFrontendEdge& NewEdge = Graph.Edges[InNewIndex];
		InputToEdgeIndex.Add(NewEdge.GetToVertexHandle(), InNewIndex);
		OutputToEdgeIndices.FindOrAdd(NewEdge.GetFromVertexHandle()).Add(InNewIndex);
	}

	void FDocumentGraphEdgeCache::OnRemoveSwappingEdge(int32 SwapIndex, int32 LastIndex)
	{
		const FMetasoundFrontendDocument& Document = Parent->GetDocument();
		const TArray<FMetasoundFrontendEdge>& Edges = Document.RootGraph.Graph.Edges;

		const FMetasoundFrontendEdge& SwapEdge = Edges[SwapIndex];
		const FMetasoundFrontendEdge& LastEdge = Edges[LastIndex];
		DocumentCachePrivate::RemoveSwapMapIndexChecked(SwapEdge.GetToVertexHandle(), LastEdge.GetToVertexHandle(), InputToEdgeIndex);
		DocumentCachePrivate::RemoveSwapMapArrayIndexChecked(SwapEdge.GetFromVertexHandle(), SwapIndex, LastEdge.GetFromVertexHandle(), LastIndex, OutputToEdgeIndices);
	}
} // namespace Metasound::Frontend
