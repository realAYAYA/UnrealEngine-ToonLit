// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"

#include "Algo/AnyOf.h"
#include "MetasoundAssetManager.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendRegistries.h"


namespace Metasound::Frontend
{
	namespace ReroutePrivate
	{
		class FRerouteNodeTemplateTransform : public INodeTransform
		{
		public:
			FRerouteNodeTemplateTransform() = default;
			virtual ~FRerouteNodeTemplateTransform() = default;

			virtual bool Transform(const FGuid& InNodeID, FMetaSoundFrontendDocumentBuilder& OutBuilder) const override;
		};

		bool FRerouteNodeTemplateTransform::Transform(const FGuid& InNodeID, FMetaSoundFrontendDocumentBuilder& OutBuilder) const
		{
			FMetasoundFrontendEdge InputEdge;
			TArray<FMetasoundFrontendEdge> OutputEdges;

			const FMetasoundFrontendNode* Node = OutBuilder.FindNode(InNodeID);
			if (ensureMsgf(Node, TEXT("Failed to find node with ID '%s' when reroute template node transform was given a valid ID for builder '%s'."),
				*InNodeID.ToString(),
				*OutBuilder.GetDebugName()))
			{
				if (!ensureMsgf(Node->Interface.Inputs.Num() == 1, TEXT("Reroute nodes must only have one input")))
				{
					return false;
				}

				if (!ensureMsgf(Node->Interface.Outputs.Num() == 1, TEXT("Reroute nodes must only have one output")))
				{
					return false;
				}

				// Copy input edge to mutate from fields and avoid pointer going out of scope when template node is removed below
				{
					const FMetasoundFrontendVertex& InputVertex = Node->Interface.Inputs.Last();
					TArray<const FMetasoundFrontendEdge*> InputEdges = OutBuilder.FindEdges(Node->GetID(), InputVertex.VertexID);
					if (!InputEdges.IsEmpty())
					{
						InputEdge = *InputEdges.Last();
					}
				}

				// Copy output edges to mutate from fields and avoid pointer going out of scope when swapping below
				{
					const FMetasoundFrontendVertex& OutputVertex = Node->Interface.Outputs.Last();
					TArray<const FMetasoundFrontendEdge*> CurrentOutputEdges = OutBuilder.FindEdges(Node->GetID(), OutputVertex.VertexID);
					Algo::Transform(CurrentOutputEdges, OutputEdges, [](const FMetasoundFrontendEdge* CurrentEdge)
					{
						check(CurrentEdge);
						return *CurrentEdge;
					});
				}

				// Remove the template node
				OutBuilder.RemoveNode(Node->GetID());

				// Add new connections from reroute source node to reroute destination node. Either could be another reroute,
				// which is valid because said node will subsequently get processed.
				if (InputEdge.GetFromVertexHandle().IsSet())
				{
					bool bModified = !OutputEdges.IsEmpty();
					for (FMetasoundFrontendEdge& OutputEdge : OutputEdges)
					{
						OutputEdge.FromNodeID = InputEdge.FromNodeID;
						OutputEdge.FromVertexID = InputEdge.FromVertexID;
						OutBuilder.AddEdge(MoveTemp(OutputEdge));
					}

					return bModified;
				}
			}

			return false;
		}
	} // namespace ReroutePrivate

	const FMetasoundFrontendClassName FRerouteNodeTemplate::ClassName { "UE", "Reroute", "" };

	const FMetasoundFrontendVersionNumber FRerouteNodeTemplate::VersionNumber { 1, 0 };

	const FMetasoundFrontendClassName& FRerouteNodeTemplate::GetClassName() const
	{
		return ClassName;
	}

	TUniquePtr<INodeTransform> FRerouteNodeTemplate::GenerateNodeTransform(FMetasoundFrontendDocument& InPreprocessedDocument) const
	{
		return GenerateNodeTransform();
	}

	TUniquePtr<INodeTransform> FRerouteNodeTemplate::GenerateNodeTransform() const
	{
		using namespace ReroutePrivate;
		return TUniquePtr<INodeTransform>(new FRerouteNodeTemplateTransform());
	}

	const FMetasoundFrontendClass& FRerouteNodeTemplate::GetFrontendClass() const
	{
		auto CreateFrontendClass = []()
		{
			FMetasoundFrontendClass Class;
			Class.Metadata.SetClassName(ClassName);

#if WITH_EDITOR
			Class.Metadata.SetSerializeText(false);
			Class.Metadata.SetAuthor(Metasound::PluginAuthor);
			Class.Metadata.SetDescription(Metasound::PluginNodeMissingPrompt);

			FMetasoundFrontendClassStyleDisplay& StyleDisplay = Class.Style.Display;
			StyleDisplay.ImageName = "MetasoundEditor.Graph.Node.Class.Reroute";
			StyleDisplay.bShowInputNames = false;
			StyleDisplay.bShowOutputNames = false;
			StyleDisplay.bShowLiterals = false;
			StyleDisplay.bShowName = false;
#endif // WITH_EDITOR

			Class.Metadata.SetType(EMetasoundFrontendClassType::Template);
			Class.Metadata.SetVersion(VersionNumber);


			return Class;
		};

		static const FMetasoundFrontendClass FrontendClass = CreateFrontendClass();
		return FrontendClass;
	}

	FMetasoundFrontendNodeInterface FRerouteNodeTemplate::CreateNodeInterfaceFromDataType(FName InDataType)
	{
		auto CreateNewVertex = [&] { return FMetasoundFrontendVertex { "Value", InDataType, FGuid::NewGuid() }; };

		FMetasoundFrontendNodeInterface NewInterface;
		NewInterface.Inputs.Add(CreateNewVertex());
		NewInterface.Outputs.Add(CreateNewVertex());

		return NewInterface;
	}

	EMetasoundFrontendVertexAccessType FRerouteNodeTemplate::GetNodeInputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		// Recursive search up DAG for first connected non-reroute node's input access type
		if (const FMetasoundFrontendNode* Node = InBuilder.FindNode(InNodeID))
		{
			// Should only ever be one
			const FMetasoundFrontendVertex& RerouteOutput = Node->Interface.Outputs.Last();

			TArray<const FMetasoundFrontendNode*> ConnectedNodes;
			TArray<const FMetasoundFrontendVertex*> ConnectedInputs = InBuilder.FindNodeInputsConnectedToNodeOutput(InNodeID, InVertexID, &ConnectedNodes);
			for (int32 Index = 0; Index < ConnectedNodes.Num(); ++Index)
			{
				const FMetasoundFrontendNode* ConnectedNode = ConnectedNodes[Index];
				if (const FMetasoundFrontendClass* ConnectedNodeClass = InBuilder.FindDependency(ConnectedNode->ClassID))
				{
					const FMetasoundFrontendVertex* ConnectedInput = ConnectedInputs[Index];
					if (ConnectedNodeClass->Metadata.GetClassName() == ClassName)
					{
						return this->GetNodeInputAccessType(InBuilder, ConnectedNode->GetID(), ConnectedInput->VertexID);
					}

					return InBuilder.GetNodeInputAccessType(ConnectedNode->GetID(), ConnectedInput->VertexID);
				}
			}
		}

		return EMetasoundFrontendVertexAccessType::Unset;
	}

	EMetasoundFrontendVertexAccessType FRerouteNodeTemplate::GetNodeOutputAccessType(const FMetaSoundFrontendDocumentBuilder& InBuilder, const FGuid& InNodeID, const FGuid& InVertexID) const
	{
		// Depth-first recursive search for first connected non-reroute node's output access type
		if (const FMetasoundFrontendNode* Node = InBuilder.FindNode(InNodeID))
		{
			// Should only ever be one
			const FMetasoundFrontendVertex& RerouteInput = Node->Interface.Inputs.Last();

			const FMetasoundFrontendNode* ConnectedNode = nullptr;
			if (const FMetasoundFrontendVertex* ConnectedOutput = InBuilder.FindNodeOutputConnectedToNodeInput(InNodeID, RerouteInput.VertexID, &ConnectedNode))
			{
				if (const FMetasoundFrontendClass* ConnectedNodeClass = InBuilder.FindDependency(ConnectedNode->ClassID))
				{
					if (ConnectedNodeClass->Metadata.GetClassName() == ClassName)
					{
						return this->GetNodeOutputAccessType(InBuilder, ConnectedNode->GetID(), ConnectedOutput->VertexID);
					}

					return InBuilder.GetNodeOutputAccessType(ConnectedNode->GetID(), ConnectedOutput->VertexID);
				}
			}
		}

		return EMetasoundFrontendVertexAccessType::Unset;
	}

	const FNodeRegistryKey& FRerouteNodeTemplate::GetRegistryKey()
	{
		static const FNodeRegistryKey RegistryKey = FNodeRegistryKey(EMetasoundFrontendClassType::Template, ClassName, VersionNumber);
		return RegistryKey;
	}

	const FMetasoundFrontendVersionNumber& FRerouteNodeTemplate::GetVersionNumber() const
	{
		return VersionNumber;
	}

#if WITH_EDITOR
	bool FRerouteNodeTemplate::HasRequiredConnections(FConstNodeHandle InNodeHandle, FString* OutMessage) const
	{
		TArray<FConstOutputHandle> Outputs = InNodeHandle->GetConstOutputs();
		TArray<FConstInputHandle> Inputs = InNodeHandle->GetConstInputs();

		const bool bConnectedToNonRerouteOutputs = Algo::AnyOf(Outputs, [](const FConstOutputHandle& OutputHandle) { return Frontend::FindReroutedOutput(OutputHandle)->IsValid(); });
		const bool bConnectedToNonRerouteInputs = Algo::AnyOf(Inputs, [](const FConstInputHandle& InputHandle)
		{
			TArray<FConstInputHandle> Inputs;
			Frontend::FindReroutedInputs(InputHandle, Inputs);
			return !Inputs.IsEmpty();
		});

		const bool bHasRequiredConnections = bConnectedToNonRerouteOutputs || bConnectedToNonRerouteOutputs == bConnectedToNonRerouteInputs;
		if (!bHasRequiredConnections && OutMessage)
		{
			*OutMessage = TEXT("Reroute node(s) missing non-reroute input connection(s).");
		}

		return bHasRequiredConnections;
	}
#endif // WITH_EDITOR

	bool FRerouteNodeTemplate::IsInputAccessTypeDynamic() const
	{
		return true;
	}

	bool FRerouteNodeTemplate::IsOutputAccessTypeDynamic() const
	{
		return true;
	}

	bool FRerouteNodeTemplate::IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const
	{
		if (InNodeInterface.Inputs.Num() != 1)
		{
			return false;
		}
			
		if (InNodeInterface.Outputs.Num() != 1)
		{
			return false;
		}

		const FName DataType = InNodeInterface.Inputs.Last().TypeName;
		if (DataType != InNodeInterface.Outputs.Last().TypeName)
		{
			return false;
		}

		return IDataTypeRegistry::Get().IsRegistered(DataType);
	}
} // namespace Metasound::Frontend
