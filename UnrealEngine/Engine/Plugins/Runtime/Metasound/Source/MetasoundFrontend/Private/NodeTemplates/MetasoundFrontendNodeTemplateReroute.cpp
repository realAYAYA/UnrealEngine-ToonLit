// Copyright Epic Games, Inc. All Rights Reserved.

#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"

#include "Algo/AnyOf.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendRegistries.h"
#include "NodeTemplates/MetasoundFrontendDocumentTemplatePreprocessor.h"


namespace Metasound
{
	namespace Frontend
	{
		namespace ReroutePrivate
		{
			class FRerouteNodeTemplatePreprocessTransform : public FNodeTemplatePreprocessTransformBase
			{
			public:
				FRerouteNodeTemplatePreprocessTransform(FMetasoundFrontendDocument& InDocument)
					: FNodeTemplatePreprocessTransformBase(InDocument)
				{
					TArray<FMetasoundFrontendEdge>& Edges = Graph.Edges;
					for (int32 i = 0; i < Edges.Num(); ++i)
					{
						FMetasoundFrontendEdge& Edge = Edges[i];
						InputEdgeMap.Add({ Edge.ToNodeID, Edge.ToVertexID }, &Edge);
						OutputEdgeMap.FindOrAdd({ Edge.FromNodeID, Edge.FromVertexID }).Add(&Edge);
					}
				}

				virtual ~FRerouteNodeTemplatePreprocessTransform() = default;

				virtual bool Transform(FMetasoundFrontendNode& InOutNode) const override;

			private:
				using FNodeVertexGuidPair = TPair<FGuid, FGuid>;

				mutable TMap<FNodeVertexGuidPair, FMetasoundFrontendEdge*> InputEdgeMap;
				mutable TMap<FNodeVertexGuidPair, TArray<FMetasoundFrontendEdge*>> OutputEdgeMap;
			};

			bool FRerouteNodeTemplatePreprocessTransform::Transform(FMetasoundFrontendNode& InOutNode) const
			{
				using namespace ReroutePrivate;

				const FMetasoundFrontendEdge* InputEdge = nullptr;
				{
					if (!ensure(InOutNode.Interface.Inputs.Num() == 1))
					{
						return false;
					}

					const FMetasoundFrontendVertex& InputVertex = InOutNode.Interface.Inputs.Last();
					FNodeVertexGuidPair InputNodeVertexPair{ InOutNode.GetID(), InputVertex.VertexID };
					InputEdge = InputEdgeMap.FindRef(InputNodeVertexPair);

					// This can happen if the reroute node isn't provided an input, so its perfectly
					// acceptable to just ignore this node as it ultimately provides no sourced input.
					if (!InputEdge)
					{
						return false;
					}
				}

				TArray<FMetasoundFrontendEdge*>* OutputEdges = nullptr;
				{
					if (!ensure(InOutNode.Interface.Outputs.Num() == 1))
					{
						return false;
					}

					const FMetasoundFrontendVertex& OutputVertex = InOutNode.Interface.Outputs.Last();
					FNodeVertexGuidPair OutputNodeVertexPair{ InOutNode.GetID(), OutputVertex.VertexID };
					OutputEdges = OutputEdgeMap.Find(OutputNodeVertexPair);

					// This can happen if the reroute node isn't provided any outputs to connect to, so its
					// perfectly acceptable to just ignore this node as it ultimately provides no sourced input.
					if (!OutputEdges)
					{
						return false;
					}
				}

				for (FMetasoundFrontendEdge* OutputEdge : *OutputEdges)
				{
					OutputEdge->FromNodeID = InputEdge->FromNodeID;
					OutputEdge->FromVertexID = InputEdge->FromVertexID;
				}

				return true;
			}
		}

		const FMetasoundFrontendClassName FRerouteNodeTemplate::ClassName { "UE", "Reroute", "" };

		const FMetasoundFrontendVersion FRerouteNodeTemplate::Version { ClassName.GetFullName(), { 1, 0 } };

		TUniquePtr<INodeTransform> FRerouteNodeTemplate::GenerateNodeTransform(FMetasoundFrontendDocument& InPreprocessedDocument) const
		{
			using namespace ReroutePrivate;
			return TUniquePtr<INodeTransform>(new FRerouteNodeTemplatePreprocessTransform(InPreprocessedDocument));
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
				Class.Metadata.SetVersion(Version.Number);


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

		const FNodeRegistryKey& FRerouteNodeTemplate::GetRegistryKey()
		{
			static const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(
				EMetasoundFrontendClassType::Template,
				ClassName.ToString(), 
				Version.Number.Major, 
				Version.Number.Minor);

			return RegistryKey;
		}

		const FMetasoundFrontendVersion& FRerouteNodeTemplate::GetVersion() const
		{
			return Version;
		}

#if WITH_EDITOR
		bool FRerouteNodeTemplate::HasRequiredConnections(FConstNodeHandle InNodeHandle) const
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

			return bConnectedToNonRerouteOutputs || bConnectedToNonRerouteOutputs == bConnectedToNonRerouteInputs;
		}
#endif // WITH_EDITOR

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
	} // namespace Frontend
} // namespace Metasound