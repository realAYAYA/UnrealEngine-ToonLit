// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTemplates/MetasoundFrontendDocumentTemplatePreprocessor.h"

#include "Containers/UnrealString.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundTrace.h"


namespace Metasound
{
	namespace Frontend
	{
		bool FDocumentTemplatePreprocessTransform::Transform(FDocumentHandle InDocument) const
		{
			unimplemented();
			return false;
		}

		bool FDocumentTemplatePreprocessTransform::Transform(FMetasoundFrontendDocument& InOutDoc) const
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Frontend::AssetBase::Preprocessor);

			bool bModified = false;

			FMetasoundFrontendGraph& Graph = InOutDoc.RootGraph.Graph;
			TArray<FMetasoundFrontendClass>& Dependencies = InOutDoc.Dependencies;
			const TArray<FMetasoundFrontendEdge>& GraphEdges = Graph.Edges;

			struct FTemplateParams
			{
				FGuid ClassID;
				const INodeTemplate* Template = nullptr;
			};

			// 1. Find template dependencies to build
			TArray<FTemplateParams> TemplateParams;
			Algo::TransformIf(Dependencies, TemplateParams,
				[](const FMetasoundFrontendClass& Class) { return Class.Metadata.GetType() == EMetasoundFrontendClassType::Template; },
				[](const FMetasoundFrontendClass& Class)
				{
					const FNodeRegistryKey Key = NodeRegistryKey::CreateKey(Class.Metadata);
					FGuid ClassID = Class.ID;
					const INodeTemplate* Template = INodeTemplateRegistry::Get().FindTemplate(Key);
					ensureMsgf(Template, TEXT("Template not found for template class reference '%s'"), *Class.Metadata.GetClassName().ToString());
					return FTemplateParams { ClassID, Template };
				}
			);

			for (FTemplateParams& Params : TemplateParams)
			{
				if (!Params.Template)
				{
					continue;
				}

				TUniquePtr<INodeTransform> NodeTransform = Params.Template->GenerateNodeTransform(InOutDoc);
				check(NodeTransform.IsValid());

				TSet<const FMetasoundFrontendNode*> NodesToRemove;
				TSet<TPair<FGuid, FGuid>> VerticesRemoved;

				// 1a. Preprocess template nodes
				for (FMetasoundFrontendNode& Node : Graph.Nodes)
				{
					if (Params.ClassID == Node.ClassID)
					{
						NodeTransform->Transform(Node);
						NodesToRemove.Add(&Node);

						auto GetNodeVertexGuidPair = [NodeID = Node.GetID()](const FMetasoundFrontendVertex& Vertex) { return TPair<FGuid, FGuid> { NodeID, Vertex.VertexID }; };
						Algo::Transform(Node.Interface.Inputs, VerticesRemoved, GetNodeVertexGuidPair);
						Algo::Transform(Node.Interface.Outputs, VerticesRemoved, GetNodeVertexGuidPair);

						bModified = true;
					}
				}

				// 1b. Remove template node from graph
				constexpr bool bAllowShrinking = false;
				for (int32 i = Graph.Nodes.Num() - 1; i >= 0; --i)
				{
					if (NodesToRemove.Contains(&Graph.Nodes[i]))
					{
						Graph.Nodes.RemoveAtSwap(i, 1, bAllowShrinking);
					}
				}
				Graph.Nodes.Shrink();

				// 1c. Remove edges connecting template node from graph
				for (int32 i = Graph.Edges.Num() - 1; i >= 0; --i)
				{
					const TPair<FGuid, FGuid> FromNodeVertexPair { Graph.Edges[i].FromNodeID, Graph.Edges[i].FromVertexID };
					if (VerticesRemoved.Contains(FromNodeVertexPair))
					{
						Graph.Edges.RemoveAtSwap(i, 1, bAllowShrinking);
					}
					else
					{
						const TPair<FGuid, FGuid> ToNodeVertexPair { Graph.Edges[i].ToNodeID, Graph.Edges[i].ToVertexID };
						if (VerticesRemoved.Contains(ToNodeVertexPair))
						{
							Graph.Edges.RemoveAtSwap(i, 1, bAllowShrinking);
						}
					}
				}
				Graph.Edges.Shrink();
			}

			// 2. Finally, Remove template classes from dependency list
			{
				TSet<FString> TemplateKeys;
				Algo::Transform(TemplateParams, TemplateKeys, [](const FTemplateParams& Params)
				{
					if (Params.Template)
					{
						return NodeRegistryKey::CreateKey(Params.Template->GetFrontendClass().Metadata);
					}

					return FString();
				});
				constexpr bool bAllowShrinking = false;
				Dependencies.RemoveAllSwap([&TemplateKeys](const FMetasoundFrontendClass& Class)
				{
					return TemplateKeys.Contains(NodeRegistryKey::CreateKey(Class.Metadata));
				}, bAllowShrinking);
			}

			return bModified;
		}
	} // namespace Frontend
} // namespace Metasound
