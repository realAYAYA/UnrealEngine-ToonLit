// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundBuilderSubsystem.h"

#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/MetasoundOutputFormatInterfaces.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Metasound.h"
#include "MetasoundAssetSubsystem.h"
#include "MetasoundDataReference.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundLog.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundSource.h"
#include "MetasoundTrace.h"
#include "MetasoundUObjectRegistry.h"
#include "MetasoundVertex.h"
#include "PerPlatformProperties.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundBuilderSubsystem)


namespace Metasound::Engine
{
	namespace BuilderSubsystemPrivate
	{
		int32 TransactionBasedRegistrationEnabled = 1;

		template <typename TLiteralType>
		FMetasoundFrontendLiteral CreatePODMetaSoundLiteral(const TLiteralType& Value, FName& OutDataType)
		{
			OutDataType = GetMetasoundDataTypeName<TLiteralType>();

			FMetasoundFrontendLiteral Literal;
			Literal.Set(Value);
			return Literal;
		}

		TUniquePtr<INode> CreateDynamicNodeFromFrontendLiteral(const FName DataType, const FMetasoundFrontendLiteral& InLiteral)
		{
			using namespace Frontend;
			FLiteral ValueLiteral = InLiteral.ToLiteral(DataType);

			// TODO: Node name "Literal" is always the same.  Consolidate and deprecate providing unique node name to avoid unnecessary FName table bloat.
			FLiteralNodeConstructorParams Params { "Literal", FGuid::NewGuid(), MoveTemp(ValueLiteral) };
			return IDataTypeRegistry::Get().CreateLiteralNode(DataType, MoveTemp(Params));
		}
	} // namespace BuilderSubsystemPrivate

	FAutoConsoleVariableRef CVarMetaSoundBuilderForceAllFrontendRegistration(
		TEXT("au.MetaSound.Builder.TransactionBasedRegistrationEnabled"),
		BuilderSubsystemPrivate::TransactionBasedRegistrationEnabled,
		TEXT("Forces all builder calls to register MetaSound objects with the Frontend.\n")
		TEXT("Enabled (Default): !0, Disabled: 0"),
		ECVF_Default);
} // namespace Metasound::Engine


void UMetaSoundBuilderBase::BeginDestroy()
{
	if (UMetaSoundBuilderSubsystem* Subsystem = UMetaSoundBuilderSubsystem::Get())
	{
		const FMetasoundFrontendDocument& Document = static_cast<const FMetaSoundFrontendDocumentBuilder&>(Builder).GetDocument();
		const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();
		Subsystem->TransientBuilders.Remove(ClassName);
	}

	// Need to detach before destroying UPROPERTYs as the Builder 
	// often holds a TScriptInterface<IMetaSoundDocumentInterface> of
	// a UPROPERTY that lives on this or derived objects. 
	Builder.FinishBuilding();

	Super::BeginDestroy();
}

FMetaSoundBuilderNodeOutputHandle UMetaSoundBuilderBase::AddGraphInputNode(FName Name, FName DataType, FMetasoundFrontendLiteral DefaultValue, EMetaSoundBuilderResult& OutResult, bool bIsConstructorInput)
{
	using namespace Metasound::Frontend;

	FMetaSoundBuilderNodeOutputHandle NewHandle;

	if (IDataTypeRegistry::Get().FindDataTypeRegistryEntry(DataType) == nullptr)
	{
		UE_LOG(LogMetaSound, Error, TEXT("AddGraphInputNode Failed on builder '%s' when attempting to add '%s': '%s' is not a registered DataType"), *GetName(), *Name.ToString(), *DataType.ToString());
	}
	else
	{
		const FMetasoundFrontendNode* Node = Builder.FindGraphInputNode(Name);
		if (Node)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("AddGraphInputNode Failed: Input Node already exists with name '%s'; returning handle to existing node which may or may not match requested DataType '%s'"), *Name.ToString(), *DataType.ToString());
		}
		else
		{
			FDocumentIDGenerator& IDGenerator = FDocumentIDGenerator::Get();
			const FMetasoundFrontendDocument& Doc = GetConstBuilder().GetDocument();

			FMetasoundFrontendClassInput Description;
			Description.Name = Name;
			Description.TypeName = DataType;
			Description.NodeID = IDGenerator.CreateNodeID(Doc);
			Description.VertexID = IDGenerator.CreateVertexID(Doc);
			Description.DefaultLiteral = static_cast<FMetasoundFrontendLiteral>(DefaultValue);
			Description.AccessType = bIsConstructorInput ? EMetasoundFrontendVertexAccessType::Value : EMetasoundFrontendVertexAccessType::Reference;
			Node = Builder.AddGraphInput(Description);
		}

		if (Node)
		{
			const TArray<FMetasoundFrontendVertex>& Outputs = Node->Interface.Outputs;
			checkf(!Outputs.IsEmpty(), TEXT("Node should be initialized and have one output."));

			NewHandle.NodeID = Node->GetID();
			NewHandle.VertexID = Outputs.Last().VertexID;
		}
	}

	OutResult = NewHandle.IsSet() ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
	return NewHandle;
}

FMetaSoundBuilderNodeInputHandle UMetaSoundBuilderBase::AddGraphOutputNode(FName Name, FName DataType, FMetasoundFrontendLiteral DefaultValue, EMetaSoundBuilderResult& OutResult, bool bIsConstructorOutput)
{
	using namespace Metasound::Frontend;

	FMetaSoundBuilderNodeInputHandle NewHandle;

	if (IDataTypeRegistry::Get().FindDataTypeRegistryEntry(DataType) == nullptr)
	{
		UE_LOG(LogMetaSound, Error, TEXT("AddGraphOutputNode Failed on builder '%s' when attempting to add '%s': '%s' is not a registered DataType"), *GetName(), *Name.ToString(), *DataType.ToString());
	}
	else
	{
		const FMetasoundFrontendNode* Node = Builder.FindGraphOutputNode(Name);
		if (Node)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("AddGraphOutputNode Failed: Output Node already exists with name '%s'; returning handle to existing node which may or may not match requested DataType '%s'"), *Name.ToString(), *DataType.ToString());
		}
		else
		{
			FDocumentIDGenerator& IDGenerator = FDocumentIDGenerator::Get();
			const FMetasoundFrontendDocument& Doc = GetConstBuilder().GetDocument();

			FMetasoundFrontendClassOutput Description;
			Description.Name = Name;
			Description.TypeName = DataType;
			Description.NodeID = IDGenerator.CreateNodeID(Doc);
			Description.VertexID = IDGenerator.CreateVertexID(Doc);
			Description.AccessType = bIsConstructorOutput ? EMetasoundFrontendVertexAccessType::Value : EMetasoundFrontendVertexAccessType::Reference;
			Node = Builder.AddGraphOutput(Description);
		}

		if (Node)
		{
			const TArray<FMetasoundFrontendVertex>& Inputs = Node->Interface.Inputs;
			checkf(!Inputs.IsEmpty(), TEXT("Node should be initialized and have one input."));

			const FGuid& VertexID = Inputs.Last().VertexID;
			if (Builder.SetNodeInputDefault(Node->GetID(), VertexID, DefaultValue))
			{
				NewHandle.NodeID = Node->GetID();
				NewHandle.VertexID = VertexID;
			}
		}
	}

	OutResult = NewHandle.IsSet() ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
	return NewHandle;
}

void UMetaSoundBuilderBase::AddInterface(FName InterfaceName, EMetaSoundBuilderResult& OutResult)
{
	const bool bInterfaceAdded = Builder.AddInterface(InterfaceName);
	OutResult = bInterfaceAdded ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

FMetaSoundNodeHandle UMetaSoundBuilderBase::AddNode(const TScriptInterface<IMetaSoundDocumentInterface>& NodeClass, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FMetaSoundNodeHandle NewHandle;

	if (NodeClass)
	{
		UObject* NodeClassObject = NodeClass.GetObject();
		check(NodeClassObject);

#if WITH_EDITOR
		// Assets that may undergo serialization cannot reference transient objects
		const bool bIsInvalidReference = !NodeClassObject->IsAsset() && Builder.CastDocumentObjectChecked<UObject>().IsAsset();
#else
		constexpr bool bIsInvalidReference = false;
#endif // WITH_EDITOR

		if (bIsInvalidReference)
		{
			UObject& ThisBuildersObject = Builder.CastDocumentObjectChecked<UObject>();
			UE_LOG(LogMetaSound, Warning,
				TEXT("Failed to add node of transient asset '%s' to serialized asset '%s': "
				"Transient object node class cannot be referenced from asset node class."),
				*NodeClassObject->GetPathName(),
				*ThisBuildersObject.GetPathName());
		}
		else
		{
			RegisterGraphIfOutstandingTransactions(*NodeClassObject);

			const FMetasoundFrontendDocument& NodeClassDoc = NodeClass->GetConstDocument();
			const FMetasoundFrontendGraphClass& NodeClassGraph = NodeClassDoc.RootGraph;
			if (const FMetasoundFrontendNode* NewNode = Builder.AddGraphNode(NodeClassGraph))
			{
				NewHandle.NodeID = NewNode->GetID();
			}
		}
	}

	OutResult = NewHandle.IsSet() ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
	return NewHandle;
}

FMetaSoundNodeHandle UMetaSoundBuilderBase::AddNodeByClassName(const FMetasoundFrontendClassName& ClassName, int32 MajorVersion, EMetaSoundBuilderResult& OutResult)
{
	return AddNodeByClassName(ClassName, OutResult, MajorVersion);
}

FMetaSoundNodeHandle UMetaSoundBuilderBase::AddNodeByClassName(const FMetasoundFrontendClassName& ClassName, EMetaSoundBuilderResult& OutResult, int32 MajorVersion)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FMetaSoundNodeHandle NewHandle;
	if (const FMetasoundFrontendNode* NewNode = Builder.AddNodeByClassName(ClassName, MajorVersion))
	{
		NewHandle.NodeID = NewNode->GetID();
	}

	OutResult = NewHandle.IsSet() ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
	return NewHandle;
}

bool UMetaSoundBuilderBase::ContainsNode(const FMetaSoundNodeHandle& NodeHandle) const
{
	return Builder.ContainsNode(NodeHandle.NodeID);
}

bool UMetaSoundBuilderBase::ContainsNodeInput(const FMetaSoundBuilderNodeInputHandle& InputHandle) const
{
	return Builder.FindNodeInput(InputHandle.NodeID, InputHandle.VertexID) != nullptr;
}

bool UMetaSoundBuilderBase::ContainsNodeOutput(const FMetaSoundBuilderNodeOutputHandle& OutputHandle) const
{
	return Builder.FindNodeOutput(OutputHandle.NodeID, OutputHandle.VertexID) != nullptr;
}

void UMetaSoundBuilderBase::ConnectNodes(const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Frontend;

	OutResult = EMetaSoundBuilderResult::Failed;

	FMetasoundFrontendEdge NewEdge { NodeOutputHandle.NodeID, NodeOutputHandle.VertexID, NodeInputHandle.NodeID, NodeInputHandle.VertexID };
	const EInvalidEdgeReason InvalidEdgeReason = Builder.IsValidEdge(NewEdge);

	if (InvalidEdgeReason == Metasound::Frontend::EInvalidEdgeReason::None)
	{
#if !NO_LOGGING
		const FMetasoundFrontendNode* OldOutputNode = nullptr;
		const FMetasoundFrontendVertex* OldOutputVertex = nullptr;
		if (Builder.IsNodeInputConnected(NodeInputHandle.NodeID, NodeInputHandle.VertexID))
		{
			OldOutputVertex = Builder.FindNodeOutputConnectedToNodeInput(NodeInputHandle.NodeID, NodeInputHandle.VertexID, &OldOutputNode);
		}
#endif // !NO_LOGGING

		const bool bRemovedEdge = Builder.RemoveEdgeToNodeInput(NodeInputHandle.NodeID, NodeInputHandle.VertexID);
		Builder.AddEdge(MoveTemp(NewEdge));

#if !NO_LOGGING
		if (bRemovedEdge)
		{
			checkf(OldOutputNode, TEXT("MetaSound edge was removed from output but output node not found."));
			checkf(OldOutputVertex, TEXT("MetaSound edge was removed from output but output vertex not found."));

			const FMetasoundFrontendNode* InputNode = Builder.FindNode(NodeInputHandle.NodeID);
			checkf(InputNode, TEXT("Edge was deemed valid but input parent node is missing"));

			const FMetasoundFrontendVertex* InputVertex = Builder.FindNodeInput(NodeInputHandle.NodeID, NodeInputHandle.VertexID);
			checkf(InputVertex, TEXT("Edge was deemed valid but input is missing"));

			const FMetasoundFrontendNode* OutputNode = Builder.FindNode(NodeOutputHandle.NodeID);
			checkf(OutputNode, TEXT("Edge was deemed valid but output parent node is missing"));

			const FMetasoundFrontendVertex* OutputVertex = Builder.FindNodeOutput(NodeOutputHandle.NodeID, NodeOutputHandle.VertexID);
			checkf(OutputVertex, TEXT("Edge was deemed valid but output is missing"));

			UE_LOG(LogMetaSound, Verbose, TEXT("Removed connection from node output '%s:%s' to node '%s:%s' in order to connect to node output '%s:%s'"),
				*OldOutputNode->Name.ToString(),
				*OldOutputVertex->Name.ToString(),
				*InputNode->Name.ToString(),
				*InputVertex->Name.ToString(),
				*OutputNode->Name.ToString(),
				*OutputVertex->Name.ToString());
		}
#endif // !NO_LOGGING

		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Builder '%s' 'ConnectNodes' failed: '%s'"), *GetName(), *LexToString(InvalidEdgeReason));
	}
}

void UMetaSoundBuilderBase::ConnectNodesByInterfaceBindings(const FMetaSoundNodeHandle& FromNodeHandle, const FMetaSoundNodeHandle& ToNodeHandle, EMetaSoundBuilderResult& OutResult)
{
	const bool bEdgesAdded = Builder.AddEdgesByNodeClassInterfaceBindings(FromNodeHandle.NodeID, ToNodeHandle.NodeID);
	OutResult = bEdgesAdded ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

TArray<FMetaSoundBuilderNodeInputHandle> UMetaSoundBuilderBase::ConnectNodeOutputsToMatchingGraphInterfaceOutputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult)
{
	TArray<const FMetasoundFrontendEdge*> NewEdges;
	const bool bEdgesAdded = Builder.AddEdgesFromMatchingInterfaceNodeOutputsToGraphOutputs(NodeHandle.NodeID, NewEdges);
	OutResult = bEdgesAdded ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;

	TArray<FMetaSoundBuilderNodeInputHandle> ConnectedVertices;
	Algo::Transform(NewEdges, ConnectedVertices, [this](const FMetasoundFrontendEdge* NewEdge)
	{
		const FMetasoundFrontendVertex* Vertex = Builder.FindNodeInput(NewEdge->ToNodeID, NewEdge->ToVertexID);
		checkf(Vertex, TEXT("Edge connection reported success but vertex not found."));
		return FMetaSoundBuilderNodeInputHandle(NewEdge->ToNodeID, Vertex->VertexID);
	});

	return ConnectedVertices;
}

TArray<FMetaSoundBuilderNodeOutputHandle> UMetaSoundBuilderBase::ConnectNodeInputsToMatchingGraphInterfaceInputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult)
{
	TArray<const FMetasoundFrontendEdge*> NewEdges;
	const bool bEdgesAdded = Builder.AddEdgesFromMatchingInterfaceNodeInputsToGraphInputs(NodeHandle.NodeID, NewEdges);
	OutResult = bEdgesAdded ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;

	TArray<FMetaSoundBuilderNodeOutputHandle> ConnectedVertices;
	Algo::Transform(NewEdges, ConnectedVertices, [this](const FMetasoundFrontendEdge* NewEdge)
	{
		const FMetasoundFrontendVertex* Vertex = Builder.FindNodeOutput(NewEdge->FromNodeID, NewEdge->FromVertexID);
		checkf(Vertex, TEXT("Edge connection reported success but vertex not found."));
		return FMetaSoundBuilderNodeOutputHandle(NewEdge->ToNodeID, Vertex->VertexID);
	});

	return ConnectedVertices;
}

void UMetaSoundBuilderBase::ConnectNodeOutputToGraphOutput(FName GraphOutputName, const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Frontend;

	OutResult = EMetaSoundBuilderResult::Failed;

	if (const FMetasoundFrontendNode* GraphOutputNode = Builder.FindGraphOutputNode(GraphOutputName))
	{
		const FMetasoundFrontendVertex& InputVertex = GraphOutputNode->Interface.Inputs.Last();
		FMetasoundFrontendEdge NewEdge { NodeOutputHandle.NodeID, NodeOutputHandle.VertexID, GraphOutputNode->GetID(), InputVertex.VertexID };
		const EInvalidEdgeReason InvalidEdgeReason = Builder.IsValidEdge(NewEdge);
		if (InvalidEdgeReason == EInvalidEdgeReason::None)
		{
			Builder.RemoveEdgeToNodeInput(GraphOutputNode->GetID(), InputVertex.VertexID);
			Builder.AddEdge(MoveTemp(NewEdge));
			OutResult = EMetaSoundBuilderResult::Succeeded;
		}
		else
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Builder '%s' 'ConnectNodeOutputToGraphOutput' failed: '%s'"), *GetName(), *LexToString(InvalidEdgeReason));
		}
	}
}

void UMetaSoundBuilderBase::ConnectNodeInputToGraphInput(FName GraphInputName, const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Frontend;

	OutResult = EMetaSoundBuilderResult::Failed;

	if (const FMetasoundFrontendNode* GraphInputNode = Builder.FindGraphInputNode(GraphInputName))
	{
		const FMetasoundFrontendVertex& OutputVertex = GraphInputNode->Interface.Outputs.Last();
		FMetasoundFrontendEdge NewEdge { GraphInputNode->GetID(), OutputVertex.VertexID, NodeInputHandle.NodeID, NodeInputHandle.VertexID };
		const EInvalidEdgeReason InvalidEdgeReason = Builder.IsValidEdge(NewEdge);
		if (InvalidEdgeReason == EInvalidEdgeReason::None)
		{
			Builder.RemoveEdgeToNodeInput(NodeInputHandle.NodeID, NodeInputHandle.VertexID);
			Builder.AddEdge(MoveTemp(NewEdge));
			OutResult = EMetaSoundBuilderResult::Succeeded;
		}
		else
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Builder '%s' 'ConnectNodeInputToGraphInput' failed: '%s'"), *GetName(), *LexToString(InvalidEdgeReason));
		}
	}
}

void UMetaSoundBuilderBase::ConvertFromPreset(EMetaSoundBuilderResult& OutResult)
{
	const bool bSuccess = Builder.ConvertFromPreset();
	OutResult = bSuccess ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::ConvertToPreset(const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedNodeClass, EMetaSoundBuilderResult& OutResult)
{
	const IMetaSoundDocumentInterface* ReferencedInterface = ReferencedNodeClass.GetInterface();
	if (!ReferencedInterface)
	{
		OutResult = EMetaSoundBuilderResult::Failed;
		return;
	}
	
	// Ensure the referenced node class isn't transient 
	if (Cast<UMetaSoundBuilderDocument>(ReferencedInterface))
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Transient document builders cannot be referenced when converting builder '%s' to a preset. Build the referenced node class an asset first or use an existing asset instead"), *GetName());
		OutResult = EMetaSoundBuilderResult::Failed;
		return;
	}

	// Ensure the referenced node class is a matching object type 
	const UClass& BaseMetaSoundClass = ReferencedInterface->GetBaseMetaSoundUClass();
	UObject* ReferencedObject = ReferencedNodeClass.GetObject();
	if (!ReferencedObject || (ReferencedObject && !ReferencedObject->IsA(&BaseMetaSoundClass)))
	{
		UE_LOG(LogMetaSound, Warning, TEXT("The referenced node type must match the base MetaSound class when converting builder '%s' to a preset (ex. source preset must reference another source)"), *GetName());
		OutResult = EMetaSoundBuilderResult::Failed;
		return;
	}

	// Ensure the referenced node is registered
	if (FMetasoundAssetBase* ReferencedMetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(ReferencedObject))
	{
		ReferencedMetaSoundAsset->RegisterGraphWithFrontend();
	}

	const FMetasoundFrontendDocument& ReferencedDocument = ReferencedInterface->GetConstDocument();
	if (Builder.ConvertToPreset(ReferencedDocument))
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		OutResult = EMetaSoundBuilderResult::Failed;
	}
}

void UMetaSoundBuilderBase::DisconnectNodes(const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult)
{
	const bool bEdgeRemoved = Builder.RemoveEdge(FMetasoundFrontendEdge
	{
		NodeOutputHandle.NodeID,
		NodeOutputHandle.VertexID,
		NodeInputHandle.NodeID,
		NodeInputHandle.VertexID,
	});
	OutResult = bEdgeRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::DisconnectNodeInput(const FMetaSoundBuilderNodeInputHandle& NodeInputHandle, EMetaSoundBuilderResult& OutResult)
{
	const bool bEdgeRemoved = Builder.RemoveEdgeToNodeInput(NodeInputHandle.NodeID, NodeInputHandle.VertexID);
	OutResult = bEdgeRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::DisconnectNodeOutput(const FMetaSoundBuilderNodeOutputHandle& NodeOutputHandle, EMetaSoundBuilderResult& OutResult)
{
	const bool bEdgeRemoved = Builder.RemoveEdgesFromNodeOutput(NodeOutputHandle.NodeID, NodeOutputHandle.VertexID);
	OutResult = bEdgeRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::DisconnectNodesByInterfaceBindings(const FMetaSoundNodeHandle& FromNodeHandle, const FMetaSoundNodeHandle& ToNodeHandle, EMetaSoundBuilderResult& OutResult)
{
	const bool bEdgesRemoved = Builder.RemoveEdgesByNodeClassInterfaceBindings(FromNodeHandle.NodeID, ToNodeHandle.NodeID);
	OutResult = bEdgesRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

FMetaSoundBuilderNodeInputHandle UMetaSoundBuilderBase::FindNodeInputByName(const FMetaSoundNodeHandle& NodeHandle, FName InputName, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendNode* Node = Builder.FindNode(NodeHandle.NodeID))
	{
		const TArray<FMetasoundFrontendVertex>& InputVertices = Node->Interface.Inputs;

		auto FindByNamePredicate = [&InputName](const FMetasoundFrontendVertex& Vertex) { return Vertex.Name == InputName; };
		if (const FMetasoundFrontendVertex* Input = InputVertices.FindByPredicate(FindByNamePredicate))
		{
			OutResult = EMetaSoundBuilderResult::Succeeded;
			return FMetaSoundBuilderNodeInputHandle(Node->GetID(), Input->VertexID);
		}

		FString NodeClassName = TEXT("N/A");
		if (const FMetasoundFrontendClass* Class = Builder.FindDependency(Node->ClassID))
		{
			NodeClassName = Class->Metadata.GetClassName().ToString();
		}

		UE_LOG(LogMetaSound, Display, TEXT("Builder '%s' failed to find node input '%s': Node class '%s' contains no such input"), *GetName(), *InputName.ToString(), *NodeClassName);
	}
	else
	{
		UE_LOG(LogMetaSound, Display, TEXT("Builder '%s' failed to find node input '%s': Node with ID '%s' not found"), *GetName(), *InputName.ToString(), *NodeHandle.NodeID.ToString());
	}


	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

TArray<FMetaSoundBuilderNodeInputHandle> UMetaSoundBuilderBase::FindNodeInputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult)
{
	return FindNodeInputsByDataType(NodeHandle, OutResult, { });
}

TArray<FMetaSoundBuilderNodeInputHandle> UMetaSoundBuilderBase::FindNodeInputsByDataType(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult, FName DataType)
{
	TArray<FMetaSoundBuilderNodeInputHandle> FoundVertices;
	if (Builder.ContainsNode(NodeHandle.NodeID))
	{
		TArray<const FMetasoundFrontendVertex*> Vertices = Builder.FindNodeInputs(NodeHandle.NodeID, DataType);
		Algo::Transform(Vertices, FoundVertices, [&NodeHandle](const FMetasoundFrontendVertex* Vertex)
		{
			return FMetaSoundBuilderNodeInputHandle(NodeHandle.NodeID, Vertex->VertexID);
		});
		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		UE_LOG(LogMetaSound, Display, TEXT("Failed to find node inputs by data type with builder '%s'. Node of with ID '%s' not found"), *GetName(), *NodeHandle.NodeID.ToString());
		OutResult = EMetaSoundBuilderResult::Failed;
	}

	return FoundVertices;
}

FMetaSoundBuilderNodeOutputHandle UMetaSoundBuilderBase::FindNodeOutputByName(const FMetaSoundNodeHandle& NodeHandle, FName OutputName, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendNode* Node = Builder.FindNode(NodeHandle.NodeID))
	{
		const TArray<FMetasoundFrontendVertex>& OutputVertices = Node->Interface.Outputs;

		auto FindByNamePredicate = [&OutputName](const FMetasoundFrontendVertex& Vertex) { return Vertex.Name == OutputName; };
		if (const FMetasoundFrontendVertex* Output = OutputVertices.FindByPredicate(FindByNamePredicate))
		{
			OutResult = EMetaSoundBuilderResult::Succeeded;
			return FMetaSoundBuilderNodeOutputHandle(Node->GetID(), Output->VertexID);
		}

		FString NodeClassName = TEXT("N/A");
		if (const FMetasoundFrontendClass* Class = Builder.FindDependency(Node->ClassID))
		{
			NodeClassName = Class->Metadata.GetClassName().ToString();
		}

		UE_LOG(LogMetaSound, Display, TEXT("Builder '%s' failed to find node output '%s': Node class '%s' contains no such output"), *GetName(), *OutputName.ToString(), *NodeClassName);
	}
	else
	{
		UE_LOG(LogMetaSound, Display, TEXT("Builder '%s' failed to find node output '%s': Node with ID '%s' not found"), *GetName(), *OutputName.ToString(), *NodeHandle.NodeID.ToString());
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

TArray<FMetaSoundBuilderNodeOutputHandle> UMetaSoundBuilderBase::FindNodeOutputs(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult)
{
	return FindNodeOutputsByDataType(NodeHandle, OutResult, { });
}

TArray<FMetaSoundBuilderNodeOutputHandle> UMetaSoundBuilderBase::FindNodeOutputsByDataType(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult, FName DataType)
{
	TArray<FMetaSoundBuilderNodeOutputHandle> FoundVertices;
	if (Builder.ContainsNode(NodeHandle.NodeID))
	{
		TArray<const FMetasoundFrontendVertex*> Vertices = Builder.FindNodeOutputs(NodeHandle.NodeID, DataType);
		Algo::Transform(Vertices, FoundVertices, [&NodeHandle](const FMetasoundFrontendVertex* Vertex)
		{
			return FMetaSoundBuilderNodeOutputHandle(NodeHandle.NodeID, Vertex->VertexID);
		});
		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		UE_LOG(LogMetaSound, Display, TEXT("Failed to find node outputs by data type with builder '%s'. Node of with ID '%s' not found"), *GetName(), *NodeHandle.NodeID.ToString());
		OutResult = EMetaSoundBuilderResult::Failed;
	}

	return FoundVertices;
}

TArray<FMetaSoundNodeHandle> UMetaSoundBuilderBase::FindInterfaceInputNodes(FName InterfaceName, EMetaSoundBuilderResult& OutResult)
{
	TArray<FMetaSoundNodeHandle> NodeHandles;

	TArray<const FMetasoundFrontendNode*> Nodes;
	if (Builder.FindInterfaceInputNodes(InterfaceName, Nodes))
	{
		Algo::Transform(Nodes, NodeHandles, [this](const FMetasoundFrontendNode* Node)
		{
			check(Node);
			return FMetaSoundNodeHandle { Node->GetID() };
		});
		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		UE_LOG(LogMetaSound, Display, TEXT("'%s' interface not found on builder '%s'. No input nodes returned"), *InterfaceName.ToString(), *GetName());
		OutResult = EMetaSoundBuilderResult::Failed;
	}

	return NodeHandles;
}

TArray<FMetaSoundNodeHandle> UMetaSoundBuilderBase::FindInterfaceOutputNodes(FName InterfaceName, EMetaSoundBuilderResult& OutResult)
{
	TArray<FMetaSoundNodeHandle> NodeHandles;

	TArray<const FMetasoundFrontendNode*> Nodes;
	if (Builder.FindInterfaceOutputNodes(InterfaceName, Nodes))
	{
		Algo::Transform(Nodes, NodeHandles, [this](const FMetasoundFrontendNode* Node)
		{
			check(Node);
			return FMetaSoundNodeHandle { Node->GetID() };
		});
		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		OutResult = EMetaSoundBuilderResult::Failed;
	}

	return NodeHandles;
}

FMetaSoundNodeHandle UMetaSoundBuilderBase::FindGraphInputNode(FName InputName, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendNode* GraphInputNode = Builder.FindGraphInputNode(InputName))
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
		return FMetaSoundNodeHandle { GraphInputNode->GetID() };
	}

	UE_LOG(LogMetaSound, Display, TEXT("Failed to find graph input by name '%s' with builder '%s'"), *InputName.ToString(), *GetName());
	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

FMetaSoundNodeHandle UMetaSoundBuilderBase::FindGraphOutputNode(FName OutputName, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendNode* GraphOutputNode = Builder.FindGraphOutputNode(OutputName))
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
		return FMetaSoundNodeHandle{ GraphOutputNode->GetID() };
	}

	UE_LOG(LogMetaSound, Display, TEXT("Failed to find graph output by name '%s' with builder '%s'"), *OutputName.ToString(), *GetName());
	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

UMetaSoundBuilderDocument* UMetaSoundBuilderBase::CreateTransientDocumentObject() const
{
	return &UMetaSoundBuilderDocument::Create(GetBuilderUClass());
}

const FMetaSoundFrontendDocumentBuilder& UMetaSoundBuilderBase::GetConstBuilder() const
{
	return Builder;
}

UObject* UMetaSoundBuilderBase::GetReferencedPresetAsset() const
{
	using namespace Metasound::Frontend;
	if (!IsPreset())
	{
		return nullptr;
	}

	// Find the single external node which is the referenced preset asset, 
	// and find the asset with its registry key 
	auto FindExternalNode = [this](const FMetasoundFrontendNode& Node) 
	{
		const FMetasoundFrontendClass* Class = Builder.FindDependency(Node.ClassID);
		return Class->Metadata.GetType() == EMetasoundFrontendClassType::External;
	};
	const FMetasoundFrontendNode* Node = Builder.GetDocument().RootGraph.Graph.Nodes.FindByPredicate(FindExternalNode);
	if (Node != nullptr)
	{
		const FMetasoundFrontendClass* NodeClass = Builder.FindDependency(Node->ClassID);
		const FNodeRegistryKey NodeClassRegistryKey = FNodeRegistryKey(NodeClass->Metadata);
		if (FMetasoundAssetBase* Asset = IMetaSoundAssetManager::GetChecked().TryLoadAssetFromKey(NodeClassRegistryKey))
		{
			return Asset->GetOwningAsset();
		}
	}

	return nullptr;
}

void UMetaSoundBuilderBase::InitFrontendBuilder()
{
	UMetaSoundBuilderDocument& DocObject = UMetaSoundBuilderDocument::Create(GetBuilderUClass());
	Builder = FMetaSoundFrontendDocumentBuilder(&DocObject);
	Builder.InitDocument();
}

void UMetaSoundBuilderBase::InitNodeLocations()
{
	Builder.InitNodeLocations();
}

bool UMetaSoundBuilderBase::InterfaceIsDeclared(FName InterfaceName) const
{
	return Builder.IsInterfaceDeclared(InterfaceName);
}

void UMetaSoundBuilderBase::InvalidateCache()
{
	Builder.InvalidateCache();
}

bool UMetaSoundBuilderBase::IsPreset() const
{
	return Builder.IsPreset();
}

bool UMetaSoundBuilderBase::NodesAreConnected(const FMetaSoundBuilderNodeOutputHandle& OutputHandle, const FMetaSoundBuilderNodeInputHandle& InputHandle) const
{
	const FMetasoundFrontendEdge Edge = { OutputHandle.NodeID, OutputHandle.VertexID, InputHandle.NodeID, InputHandle.VertexID };
	return Builder.ContainsEdge(Edge);
}

bool UMetaSoundBuilderBase::NodeInputIsConnected(const FMetaSoundBuilderNodeInputHandle& InputHandle) const
{
	return Builder.IsNodeInputConnected(InputHandle.NodeID, InputHandle.VertexID);
}

bool UMetaSoundBuilderBase::NodeOutputIsConnected(const FMetaSoundBuilderNodeOutputHandle& OutputHandle) const
{
	return Builder.IsNodeOutputConnected(OutputHandle.NodeID, OutputHandle.VertexID);
}

void UMetaSoundBuilderBase::RegisterGraphIfOutstandingTransactions(UObject& InMetaSound)
{
	using namespace Metasound;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine::BuilderSubsystemPrivate;

	IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();
	FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
	check(MetaSoundAsset);

	FMetaSoundAssetRegistrationOptions Options;
	if (TransactionBasedRegistrationEnabled)
	{
		Options.bForceReregister = false;
		Options.bRegisterDependencies = false; // Function handles registration via own recursive functionality below

		TArray<FMetasoundAssetBase*> References = MetaSoundAsset->GetReferencedAssets();
		for (FMetasoundAssetBase* Reference : References)
		{
			UObject* RefMetaSound = Reference->GetOwningAsset();
			check(RefMetaSound);
			AssetManager.AddOrUpdateAsset(*RefMetaSound);
			RegisterGraphIfOutstandingTransactions(*RefMetaSound);
		}

		if (UMetaSoundBuilderBase* Builder = UMetaSoundBuilderSubsystem::GetChecked().FindBuilderOfDocument(&InMetaSound))
		{
			const int32 TransactionCount = Builder->Builder.GetTransactionCount();

			// Force registration if transactions occurred since now and the last time the builder registered the asset.
			Options.bForceReregister = Builder->LastTransactionRegistered != TransactionCount;
			Builder->LastTransactionRegistered = TransactionCount;
		}
	}

	MetaSoundAsset->RegisterGraphWithFrontend(Options);
}

void UMetaSoundBuilderBase::ReloadCache(bool bPrimeCache)
{
	Builder.ReloadCache();
}

void UMetaSoundBuilderBase::RemoveGraphInput(FName Name, EMetaSoundBuilderResult& OutResult)
{
	const bool bRemoved = Builder.RemoveGraphInput(Name);
	OutResult = bRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::RemoveGraphOutput(FName Name, EMetaSoundBuilderResult& OutResult)
{
	const bool bRemoved = Builder.RemoveGraphOutput(Name);
	OutResult = bRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::RemoveInterface(FName InterfaceName, EMetaSoundBuilderResult& OutResult)
{
	const bool bInterfaceRemoved = Builder.RemoveInterface(InterfaceName);
	OutResult = bInterfaceRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::RemoveNode(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult)
{
	const bool bNodeRemoved = Builder.RemoveNode(NodeHandle.NodeID);
	OutResult = bNodeRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::RemoveNodeInputDefault(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult)
{
	const bool bInputDefaultRemoved = Builder.RemoveNodeInputDefault(InputHandle.NodeID, InputHandle.VertexID);
	OutResult = bInputDefaultRemoved ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::RenameRootGraphClass(const FMetasoundFrontendClassName& InName)
{
	Builder.RenameRootGraphClass(InName);
}

#if WITH_EDITOR
void UMetaSoundBuilderBase::SetAuthor(const FString& InAuthor)
{
	Builder.SetAuthor(InAuthor);
}
#endif // WITH_EDITOR

void UMetaSoundBuilderBase::SetGraphInputDefault(FName InputName, const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	const bool bInputDefaultSet = Builder.SetGraphInputDefault(InputName, Literal);
	OutResult = bInputDefaultSet ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

void UMetaSoundBuilderBase::SetNodeInputDefault(const FMetaSoundBuilderNodeInputHandle& InputHandle, const FMetasoundFrontendLiteral& Literal, EMetaSoundBuilderResult& OutResult)
{
	const bool bInputDefaultSet = Builder.SetNodeInputDefault(InputHandle.NodeID, InputHandle.VertexID, Literal);
	OutResult = bInputDefaultSet ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

#if WITH_EDITOR
void UMetaSoundBuilderBase::SetNodeLocation(const FMetaSoundNodeHandle& InNodeHandle, const FVector2D& InLocation, EMetaSoundBuilderResult& OutResult)
{
	const bool bLocationSet = Builder.SetNodeLocation(InNodeHandle.NodeID, InLocation);
	OutResult = bLocationSet ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}
#endif // WITH_EDITOR

FMetaSoundNodeHandle UMetaSoundBuilderBase::FindNodeInputParent(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult)
{
	if (Builder.ContainsNode(InputHandle.NodeID))
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
		return FMetaSoundNodeHandle { InputHandle.NodeID };
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

FMetaSoundNodeHandle UMetaSoundBuilderBase::FindNodeOutputParent(const FMetaSoundBuilderNodeOutputHandle& OutputHandle, EMetaSoundBuilderResult& OutResult)
{
	if (Builder.ContainsNode(OutputHandle.NodeID))
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
		return FMetaSoundNodeHandle{ OutputHandle.NodeID };
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

FMetasoundFrontendVersion UMetaSoundBuilderBase::FindNodeClassVersion(const FMetaSoundNodeHandle& NodeHandle, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendNode* Node = Builder.FindNode(NodeHandle.NodeID))
	{
		if (const FMetasoundFrontendClass* Class = Builder.FindDependency(Node->ClassID))
		{
			OutResult = EMetaSoundBuilderResult::Succeeded;
			return FMetasoundFrontendVersion { Class->Metadata.GetClassName().GetFullName(), Class->Metadata.GetVersion() };
		}
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return FMetasoundFrontendVersion::GetInvalid();
}

FMetasoundFrontendClassName UMetaSoundBuilderBase::GetRootGraphClassName() const
{
	return Builder.GetDocument().RootGraph.Metadata.GetClassName();
}

void UMetaSoundBuilderBase::GetNodeInputData(const FMetaSoundBuilderNodeInputHandle& InputHandle, FName& Name, FName& DataType, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendVertex* Vertex = Builder.FindNodeInput(InputHandle.NodeID, InputHandle.VertexID))
	{
		Name = Vertex->Name;
		DataType = Vertex->TypeName;
		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		Name = { };
		DataType = { };
		OutResult = EMetaSoundBuilderResult::Failed;
	}
}

FMetasoundFrontendLiteral UMetaSoundBuilderBase::GetNodeInputDefault(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendLiteral* Default = Builder.GetNodeInputDefault(InputHandle.NodeID, InputHandle.VertexID))
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
		return *Default;
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

FMetasoundFrontendLiteral UMetaSoundBuilderBase::GetNodeInputClassDefault(const FMetaSoundBuilderNodeInputHandle& InputHandle, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendLiteral* Default = Builder.GetNodeInputClassDefault(InputHandle.NodeID, InputHandle.VertexID))
	{
		OutResult = EMetaSoundBuilderResult::Succeeded;
		return *Default;
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return { };
}

bool UMetaSoundBuilderBase::GetNodeInputIsConstructorPin(const FMetaSoundBuilderNodeInputHandle& InputHandle) const
{
	const EMetasoundFrontendVertexAccessType AccessType = Builder.GetNodeInputAccessType(InputHandle.NodeID, InputHandle.VertexID);
	return AccessType == EMetasoundFrontendVertexAccessType::Value;
}

void UMetaSoundBuilderBase::GetNodeOutputData(const FMetaSoundBuilderNodeOutputHandle& OutputHandle, FName& Name, FName& DataType, EMetaSoundBuilderResult& OutResult)
{
	if (const FMetasoundFrontendVertex* Vertex = Builder.FindNodeOutput(OutputHandle.NodeID, OutputHandle.VertexID))
	{
		Name = Vertex->Name;
		DataType = Vertex->TypeName;
		OutResult = EMetaSoundBuilderResult::Succeeded;
	}
	else
	{
		Name = { };
		DataType = { };
		OutResult = EMetaSoundBuilderResult::Failed;
	}
}

bool UMetaSoundBuilderBase::GetNodeOutputIsConstructorPin(const FMetaSoundBuilderNodeOutputHandle& OutputHandle) const
{
	const EMetasoundFrontendVertexAccessType AccessType = Builder.GetNodeOutputAccessType(OutputHandle.NodeID, OutputHandle.VertexID);
	return AccessType == EMetasoundFrontendVertexAccessType::Value;
}

void UMetaSoundBuilderBase::UpdateDependencyClassNames(const TMap<FMetasoundFrontendClassName, FMetasoundFrontendClassName>& OldToNewReferencedClassNames)
{
	Builder.UpdateDependencyClassNames(OldToNewReferencedClassNames);
}

void UMetaSoundPatchBuilder::CreateTransientBuilder()
{
	using namespace Metasound::Frontend;

	UMetaSoundPatch* Patch = NewObject<UMetaSoundPatch>();
	check(Patch);

	Builder = FMetaSoundFrontendDocumentBuilder(Patch);
	Builder.InitDocument();
}

TScriptInterface<IMetaSoundDocumentInterface> UMetaSoundPatchBuilder::Build(UObject* Parent, const FMetaSoundBuilderOptions& InBuilderOptions) const
{
	return &BuildInternal<UMetaSoundPatch>(Parent, InBuilderOptions);
}

const UClass& UMetaSoundPatchBuilder::GetBuilderUClass() const
{
	return *UMetaSoundPatch::StaticClass();
}

UMetaSoundBuilderBase& UMetaSoundBuilderSubsystem::AttachBuilderToAssetChecked(UObject& InObject) const
{
	const UClass* BaseClass = InObject.GetClass();
	if (BaseClass == UMetaSoundSource::StaticClass())
	{
		UMetaSoundSourceBuilder* NewBuilder = AttachSourceBuilderToAsset(CastChecked<UMetaSoundSource>(&InObject));
		return *NewBuilder;
	}
	else if (BaseClass == UMetaSoundPatch::StaticClass())
	{
		UMetaSoundPatchBuilder* NewBuilder = AttachPatchBuilderToAsset(CastChecked<UMetaSoundPatch>(&InObject));
		return *NewBuilder;
	}
	else
	{
		checkf(false, TEXT("UClass '%s' is not a base MetaSound that supports attachment via the MetaSoundBuilderSubsystem"), *BaseClass->GetFullName());
		return *NewObject<UMetaSoundPatchBuilder>();
	}
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::AttachPatchBuilderToAsset(UMetaSoundPatch* InPatch) const
{
	if (InPatch)
	{
		return &AttachBuilderToAssetCheckedPrivate<UMetaSoundPatchBuilder>(InPatch);
	}

	return nullptr;
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::AttachSourceBuilderToAsset(UMetaSoundSource* InSource) const
{
	if (InSource)
	{
		UMetaSoundSourceBuilder& SourceBuilder = AttachBuilderToAssetCheckedPrivate<UMetaSoundSourceBuilder>(InSource);
		return &SourceBuilder;
	}

	return nullptr;
}

void UMetaSoundSourceBuilder::Audition(UObject* Parent, UAudioComponent* AudioComponent, FOnCreateAuditionGeneratorHandleDelegate CreateGenerator, bool bLiveUpdatesEnabled)
{
	using namespace Metasound;
	using namespace Metasound::DynamicGraph;
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSourceBuilder::Audition);

	if (!AudioComponent)
	{
		UE_LOG(LogMetaSound, Error, TEXT("Failed to audition MetaSoundBuilder '%s': No AudioComponent supplied"), *GetFullName());
		return;
	}

	UMetaSoundSource& MetaSoundSource = GetMetaSoundSource();
	RegisterGraphIfOutstandingTransactions(MetaSoundSource);

	// Must be called post register as register ensures cached runtime data passed to transactor is up-to-date
	MetaSoundSource.SetDynamicGeneratorEnabled(MetaSoundSource.GetAssetPathChecked(), bLiveUpdatesEnabled);
	MetaSoundSource.ConformObjectDataToInterfaces();

	AudioComponent->SetSound(&MetaSoundSource);

	if (CreateGenerator.IsBound())
	{
		UMetasoundGeneratorHandle* NewHandle = UMetasoundGeneratorHandle::CreateMetaSoundGeneratorHandle(AudioComponent);
		checkf(NewHandle, TEXT("BindToGeneratorDelegate Failed when attempting to audition MetaSoundSource builder '%s'"), *GetName());
		CreateGenerator.Execute(NewHandle);
	}

	if (bLiveUpdatesEnabled)
	{
		LiveComponentIDs.Add(AudioComponent->GetAudioComponentID());
		LiveComponentHandle = AudioComponent->OnAudioFinishedNative.AddUObject(this, &UMetaSoundSourceBuilder::OnLiveComponentFinished);
	}

	AudioComponent->Play();
}

void UMetaSoundSourceBuilder::OnLiveComponentFinished(UAudioComponent* AudioComponent)
{
	LiveComponentIDs.RemoveSwap(AudioComponent->GetAudioComponentID(), EAllowShrinking::No);
	if (LiveComponentIDs.IsEmpty())
	{
		AudioComponent->OnAudioFinishedNative.Remove(LiveComponentHandle);
	}
}

bool UMetaSoundSourceBuilder::ExecuteAuditionableTransaction(FAuditionableTransaction Transaction) const
{
	using namespace Metasound::Engine::BuilderSubsystemPrivate;
	using namespace Metasound::DynamicGraph;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(UMetaSoundSourceBuilder::ExecuteAuditionableTransaction);

	TSharedPtr<FDynamicOperatorTransactor> Transactor = GetMetaSoundSource().GetDynamicGeneratorTransactor();
	if (Transactor.IsValid())
	{
		return Transaction(*Transactor);
	}

	return false;
}

TScriptInterface<IMetaSoundDocumentInterface> UMetaSoundSourceBuilder::Build(UObject* Parent, const FMetaSoundBuilderOptions& InBuilderOptions) const
{
	return &BuildInternal<UMetaSoundSource>(Parent, InBuilderOptions);
}

void UMetaSoundSourceBuilder::CreateTransientBuilder()
{
	using namespace Metasound::Frontend;

	TSharedRef<FDocumentModifyDelegates> DocumentDelegates = MakeShared<FDocumentModifyDelegates>();
	InitDelegates(*DocumentDelegates);

	UMetaSoundSource* Source = NewObject<UMetaSoundSource>();
	check(Source);

	Builder = FMetaSoundFrontendDocumentBuilder(Source, DocumentDelegates);
	Builder.InitDocument();
}

const Metasound::Engine::FOutputAudioFormatInfoPair* UMetaSoundSourceBuilder::FindOutputAudioFormatInfo() const
{
	using namespace Metasound::Engine;

	const FOutputAudioFormatInfoMap& FormatInfo = GetOutputAudioFormatInfo();

	auto Predicate = [this](const FOutputAudioFormatInfoPair& Pair)
	{
		const FMetasoundFrontendDocument& Document = Builder.GetDocument();
		return Document.Interfaces.Contains(Pair.Value.InterfaceVersion);
	};

	return Algo::FindByPredicate(FormatInfo, Predicate);
}

const UClass& UMetaSoundSourceBuilder::GetBuilderUClass() const
{
	return *UMetaSoundSource::StaticClass();
}

bool UMetaSoundSourceBuilder::GetLiveUpdatesEnabled() const
{
	return GetMetaSoundSource().GetDynamicGeneratorTransactor().IsValid();
}

const UMetaSoundSource& UMetaSoundSourceBuilder::GetMetaSoundSource() const
{
	return GetConstBuilder().CastDocumentObjectChecked<UMetaSoundSource>();
}

UMetaSoundSource& UMetaSoundSourceBuilder::GetMetaSoundSource()
{
	return Builder.CastDocumentObjectChecked<UMetaSoundSource>();
}

void UMetaSoundSourceBuilder::InitDelegates(Metasound::Frontend::FDocumentModifyDelegates& OutDocumentDelegates)
{
	OutDocumentDelegates.EdgeDelegates.OnEdgeAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnEdgeAdded);
	OutDocumentDelegates.EdgeDelegates.OnRemoveSwappingEdge.AddUObject(this, &UMetaSoundSourceBuilder::OnRemoveSwappingEdge);

	OutDocumentDelegates.InterfaceDelegates.OnInputAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnInputAdded);
	OutDocumentDelegates.InterfaceDelegates.OnOutputAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnOutputAdded);
	OutDocumentDelegates.InterfaceDelegates.OnRemovingInput.AddUObject(this, &UMetaSoundSourceBuilder::OnRemovingInput);
	OutDocumentDelegates.InterfaceDelegates.OnRemovingOutput.AddUObject(this, &UMetaSoundSourceBuilder::OnRemovingOutput);

	OutDocumentDelegates.NodeDelegates.OnNodeAdded.AddUObject(this, &UMetaSoundSourceBuilder::OnNodeAdded);
	OutDocumentDelegates.NodeDelegates.OnNodeInputLiteralSet.AddUObject(this, &UMetaSoundSourceBuilder::OnNodeInputLiteralSet);
	OutDocumentDelegates.NodeDelegates.OnRemoveSwappingNode.AddUObject(this, &UMetaSoundSourceBuilder::OnRemoveSwappingNode);
	OutDocumentDelegates.NodeDelegates.OnRemovingNodeInputLiteral.AddUObject(this, &UMetaSoundSourceBuilder::OnRemovingNodeInputLiteral);
}

void UMetaSoundSourceBuilder::OnEdgeAdded(int32 EdgeIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendDocument& Doc = Builder.GetDocument();
	const FMetasoundFrontendEdge& NewEdge = Doc.RootGraph.Graph.Edges[EdgeIndex];
	ExecuteAuditionableTransaction([this, &NewEdge](Metasound::DynamicGraph::FDynamicOperatorTransactor& Transactor)
	{
		const FMetaSoundFrontendDocumentBuilder& Builder = GetConstBuilder();
		const FMetasoundFrontendVertex* FromNodeOutput = Builder.FindNodeOutput(NewEdge.FromNodeID, NewEdge.FromVertexID);
		const FMetasoundFrontendVertex* ToNodeInput = Builder.FindNodeInput(NewEdge.ToNodeID, NewEdge.ToVertexID);
		if (FromNodeOutput && ToNodeInput)
		{
			Transactor.AddDataEdge(NewEdge.FromNodeID, FromNodeOutput->Name, NewEdge.ToNodeID, ToNodeInput->Name);
			return true;
		}

		return false;
	});
}

TOptional<Metasound::FAnyDataReference> UMetaSoundSourceBuilder::CreateDataReference(
	const Metasound::FOperatorSettings& InOperatorSettings,
	FName DataType,
	const Metasound::FLiteral& InLiteral,
	Metasound::EDataReferenceAccessType AccessType)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	return IDataTypeRegistry::Get().CreateDataReference(DataType, AccessType, InLiteral, InOperatorSettings);
};

void UMetaSoundSourceBuilder::OnInputAdded(int32 InputIndex)
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, InputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder;
		const FMetasoundFrontendDocument& Doc = ConstBuilder.GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassInput& NewInput = GraphClass.Interface.Inputs[InputIndex];

		constexpr bool bCreateUObjectProxies = true;
		UMetaSoundSource& Source = GetMetaSoundSource();
		Source.RuntimeInputData.InputMap.Add(NewInput.Name, UMetaSoundSource::CreateRuntimeInput(IDataTypeRegistry::Get(), NewInput, bCreateUObjectProxies));

		for (uint64 AudioComponentID : LiveComponentIDs)
		{
			if (UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(AudioComponentID))
			{
				if (FAudioDevice* AudioDevice = AudioComponent->GetAudioDevice())
				{
					AudioDevice->SendCommandToActiveSounds(AudioComponentID, [NewInputName = NewInput.Name](FActiveSound& ActiveSound)
					{
						static_cast<FMetaSoundParameterTransmitter*>(ActiveSound.GetTransmitter())->AddAvailableParameter(NewInputName);
					});
				}
			}
		}

		const FLiteral NewInputLiteral = NewInput.DefaultLiteral.ToLiteral(NewInput.TypeName);
		Transactor.AddInputDataDestination(NewInput.NodeID, NewInput.Name, NewInputLiteral, &UMetaSoundSourceBuilder::CreateDataReference);
		return true;
	});
}

void UMetaSoundSourceBuilder::OnNodeAdded(int32 NodeIndex) const
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, NodeIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendNode& AddedNode = GraphClass.Graph.Nodes[NodeIndex];

		const FMetasoundFrontendClass* NodeClass = Builder.FindDependency(AddedNode.ClassID);
		checkf(NodeClass, TEXT("Node successfully added to graph but document is missing associated dependency"));

		const FNodeRegistryKey& ClassKey = FNodeRegistryKey(NodeClass->Metadata);
		FMetasoundFrontendRegistryContainer& NodeRegistry = *FMetasoundFrontendRegistryContainer::Get();
		IDataTypeRegistry& DataTypeRegistry = IDataTypeRegistry::Get();

		TUniquePtr<INode> NewNode;

		switch (NodeClass->Metadata.GetType())
		{
			case EMetasoundFrontendClassType::VariableDeferredAccessor:
			case EMetasoundFrontendClassType::VariableAccessor:
			case EMetasoundFrontendClassType::VariableMutator:
			case EMetasoundFrontendClassType::External:
			case EMetasoundFrontendClassType::Graph:
			{
				const Metasound::FNodeInitData InitData { AddedNode.Name, AddedNode.GetID() };
				NewNode = NodeRegistry.CreateNode(ClassKey, InitData);
			}
			break;

			case EMetasoundFrontendClassType::Input:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				const FMetasoundFrontendVertex& InputVertex = AddedNode.Interface.Inputs.Last();

				const FMetasoundFrontendLiteral* DefaultLiteral = nullptr;
				auto HasEqualVertexID = [&VertexID = InputVertex.VertexID](const FMetasoundFrontendVertexLiteral& InVertexLiteral)
				{
					return InVertexLiteral.VertexID == VertexID;
				};

				// Check for default literal on Node
				if (const FMetasoundFrontendVertexLiteral* VertexLiteralOnNode = AddedNode.InputLiterals.FindByPredicate(HasEqualVertexID))
				{
					DefaultLiteral = &(VertexLiteralOnNode->Value);
				}
				// Check for default literal on class input
				else if (const FMetasoundFrontendClassInput* GraphInput = Builder.FindGraphInput(InputVertex.Name))
				{
					DefaultLiteral = &(GraphInput->DefaultLiteral);
				}
				else
				{
					// As a last resort, get default literal on node class 
					DefaultLiteral = &(NodeClass->Interface.Inputs.Last().DefaultLiteral);
				}

				FInputNodeConstructorParams InitData
				{
					AddedNode.Name,
					AddedNode.GetID(),
					InputVertex.Name,
					DefaultLiteral->ToLiteral(DataTypeName)
				};

				NewNode = DataTypeRegistry.CreateInputNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Variable:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				FDefaultLiteralNodeConstructorParams InitData { AddedNode.Name, AddedNode.GetID(), DataTypeRegistry.CreateDefaultLiteral(DataTypeName)};
				NewNode = DataTypeRegistry.CreateVariableNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Literal:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				FDefaultLiteralNodeConstructorParams InitData { AddedNode.Name, AddedNode.GetID(), DataTypeRegistry.CreateDefaultLiteral(DataTypeName)};
				NewNode = DataTypeRegistry.CreateLiteralNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Output:
			{
				const FName& DataTypeName = NodeClass->Metadata.GetClassName().Name;
				const FMetasoundFrontendVertex& OutputVertex = AddedNode.Interface.Outputs.Last();
				FDefaultNamedVertexNodeConstructorParams InitData { AddedNode.Name, AddedNode.GetID(), OutputVertex.Name };
				NewNode = DataTypeRegistry.CreateOutputNode(DataTypeName, MoveTemp(InitData));
			}
			break;

			case EMetasoundFrontendClassType::Template:
			default:
			static_assert(static_cast<int32>(EMetasoundFrontendClassType::Invalid) == 10, "Possible missed EMetasoundFrontendClassType case coverage");
		};

		if (!NewNode.IsValid())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' failed to create and forward added node '%s' to live update transactor."), *GetName(), *AddedNode.Name.ToString());
			return false;
		}

		Transactor.AddNode(AddedNode.GetID(), MoveTemp(NewNode));
		return true;
	});
}

void UMetaSoundSourceBuilder::OnNodeInputLiteralSet(int32 NodeIndex, int32 VertexIndex, int32 LiteralIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendGraphClass& GraphClass = Builder.GetDocument().RootGraph;
	const FMetasoundFrontendNode& Node = GraphClass.Graph.Nodes[NodeIndex];
	const FMetasoundFrontendVertex& Input = Node.Interface.Inputs[VertexIndex];

	// Only send the literal down if not connected, as the graph core layer
	// will disconnect if a new literal is sent and edge already exists.
	if (!Builder.IsNodeInputConnected(Node.GetID(), Input.VertexID))
	{
		ExecuteAuditionableTransaction([this, &Node, &Input, &LiteralIndex](FDynamicOperatorTransactor& Transactor)
		{
			using namespace Metasound;
			using namespace Metasound::Engine;

			const FMetasoundFrontendLiteral& InputDefault = Node.InputLiterals[LiteralIndex].Value;
			TUniquePtr<INode> LiteralNode = BuilderSubsystemPrivate::CreateDynamicNodeFromFrontendLiteral(Input.TypeName, InputDefault);
			Transactor.SetValue(Node.GetID(), Input.Name, MoveTemp(LiteralNode));
			return true;
		});
	}
}

void UMetaSoundSourceBuilder::OnOutputAdded(int32 OutputIndex) const
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, OutputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassOutput& NewOutput = GraphClass.Interface.Outputs[OutputIndex];

		Transactor.AddOutputDataSource(NewOutput.NodeID, NewOutput.Name);
		return true;
	});
}

void UMetaSoundSourceBuilder::OnRemoveSwappingEdge(int32 SwapIndex, int32 LastIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendDocument& Doc = Builder.GetDocument();
	const FMetasoundFrontendEdge& EdgeBeingRemoved = Doc.RootGraph.Graph.Edges[SwapIndex];
	ExecuteAuditionableTransaction([this, EdgeBeingRemoved](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Engine;

		const FMetaSoundFrontendDocumentBuilder& Builder = GetConstBuilder();
		const FMetasoundFrontendVertex* FromNodeOutput = Builder.FindNodeOutput(EdgeBeingRemoved.FromNodeID, EdgeBeingRemoved.FromVertexID);
		const FMetasoundFrontendVertex* ToNodeInput = Builder.FindNodeInput(EdgeBeingRemoved.ToNodeID, EdgeBeingRemoved.ToVertexID);
		if (FromNodeOutput && ToNodeInput)
		{
			const FMetasoundFrontendLiteral* InputDefault = Builder.GetNodeInputDefault(EdgeBeingRemoved.ToNodeID, EdgeBeingRemoved.ToVertexID);
			if (!InputDefault)
			{
				InputDefault = Builder.GetNodeInputClassDefault(EdgeBeingRemoved.ToNodeID, EdgeBeingRemoved.ToVertexID);
			}

			if (ensureAlwaysMsgf(InputDefault, TEXT("Could not dynamically assign default literal upon removing edge: literal should be assigned by either the frontend document's input or the class definition")))
			{
				TUniquePtr<INode> LiteralNode = BuilderSubsystemPrivate::CreateDynamicNodeFromFrontendLiteral(ToNodeInput->TypeName, *InputDefault);
				Transactor.RemoveDataEdge(EdgeBeingRemoved.FromNodeID, FromNodeOutput->Name, EdgeBeingRemoved.ToNodeID, ToNodeInput->Name, MoveTemp(LiteralNode));
				return true;
			}
		}

		return false;
	});
}

void UMetaSoundSourceBuilder::OnRemovingInput(int32 InputIndex)
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, InputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound;
		using namespace Metasound::Frontend;

		const FMetaSoundFrontendDocumentBuilder& ConstBuilder = Builder;
		const FMetasoundFrontendDocument& Doc = ConstBuilder.GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassInput& InputBeingRemoved = GraphClass.Interface.Inputs[InputIndex];

		UMetaSoundSource& Source = GetMetaSoundSource();
		Source.RuntimeInputData.InputMap.Remove(InputBeingRemoved.Name);

		Transactor.RemoveInputDataDestination(InputBeingRemoved.Name);

		for (uint64 AudioComponentID : LiveComponentIDs)
		{
			if (UAudioComponent* AudioComponent = UAudioComponent::GetAudioComponentFromID(AudioComponentID))
			{
				if (FAudioDevice* AudioDevice = AudioComponent->GetAudioDevice())
				{
					AudioDevice->SendCommandToActiveSounds(AudioComponentID, [InputRemoved = InputBeingRemoved.Name](FActiveSound& ActiveSound)
						{
							static_cast<FMetaSoundParameterTransmitter*>(ActiveSound.GetTransmitter())->RemoveAvailableParameter(InputRemoved);
						});
				}
			}
		}

		return true;
	});
}

void UMetaSoundSourceBuilder::OnRemoveSwappingNode(int32 SwapIndex, int32 LastIndex) const
{
	using namespace Metasound::DynamicGraph;

	// Last index will just be re-added, so this aspect of the swap is ignored by transactor
	// (i.e. no sense removing and re-adding the node that is swapped from the end as this
	// would potentially disconnect that node in the runtime graph model).
	ExecuteAuditionableTransaction([this, SwapIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendNode& NodeBeingRemoved = GraphClass.Graph.Nodes[SwapIndex];
		const FGuid& NodeID = NodeBeingRemoved.GetID();
		Transactor.RemoveNode(NodeID);
		return true;
	});
}

void UMetaSoundSourceBuilder::OnRemovingNodeInputLiteral(int32 NodeIndex, int32 VertexIndex, int32 LiteralIndex) const
{
	using namespace Metasound::DynamicGraph;

	const FMetasoundFrontendGraphClass& GraphClass = Builder.GetDocument().RootGraph;
	const FMetasoundFrontendNode& Node = GraphClass.Graph.Nodes[NodeIndex];
	const FMetasoundFrontendVertex& Input = Node.Interface.Inputs[VertexIndex];

	// Only send the literal down if not connected, as the graph core layer will disconnect.
	if (!Builder.IsNodeInputConnected(Node.GetID(), Input.VertexID))
	{
		ExecuteAuditionableTransaction([this, &NodeIndex, &VertexIndex, &LiteralIndex](FDynamicOperatorTransactor& Transactor)
		{
			using namespace Metasound;
			using namespace Metasound::Engine;
			using namespace Metasound::Frontend;

			const TArray<FMetasoundFrontendNode>& Nodes = Builder.GetDocument().RootGraph.Graph.Nodes;
			const FMetasoundFrontendNode& Node = Nodes[NodeIndex];
			const FMetasoundFrontendVertex& Input = Node.Interface.Inputs[VertexIndex];

			const FMetasoundFrontendLiteral* InputDefault = Builder.GetNodeInputClassDefault(Node.GetID(), Input.VertexID);
			if (ensureAlwaysMsgf(InputDefault, TEXT("Could not dynamically assign default literal from class definition upon removing input '%s' literal: document's dependency entry invalid and has no default assigned"), *Input.Name.ToString()))
			{
				TUniquePtr<INode> LiteralNode = BuilderSubsystemPrivate::CreateDynamicNodeFromFrontendLiteral(Input.TypeName, *InputDefault);
				Transactor.SetValue(Node.GetID(), Input.Name, MoveTemp(LiteralNode));
				return true;
			}

			return false;
		});
	}
}

void UMetaSoundSourceBuilder::OnRemovingOutput(int32 OutputIndex) const
{
	using namespace Metasound::DynamicGraph;

	ExecuteAuditionableTransaction([this, OutputIndex](FDynamicOperatorTransactor& Transactor)
	{
		using namespace Metasound::Frontend;

		const FMetasoundFrontendDocument& Doc = Builder.GetDocument();
		const FMetasoundFrontendGraphClass& GraphClass = Doc.RootGraph;
		const FMetasoundFrontendClassOutput& OutputBeingRemoved = GraphClass.Interface.Outputs[OutputIndex];

		Transactor.RemoveOutputDataSource(OutputBeingRemoved.Name);
		return true;
	});
}

void UMetaSoundSourceBuilder::SetBlockRateOverride(float BlockRate)
{
#if WITH_EDITORONLY_DATA
	GetMetaSoundSource().BlockRateOverride.Default = BlockRate;
#endif //WITH_EDITORONLY_DATA
}

void UMetaSoundSourceBuilder::SetFormat(EMetaSoundOutputAudioFormat OutputFormat, EMetaSoundBuilderResult& OutResult)
{
	using namespace Metasound::Engine;
	using namespace Metasound::Frontend;

	// Convert to non-preset MetaSoundSource since interface data is being altered
	Builder.ConvertFromPreset();

	const FOutputAudioFormatInfoMap& FormatMap = GetOutputAudioFormatInfo();

	// Determine which interfaces to add and remove from the document due to the
	// output format being changed.
	TArray<FMetasoundFrontendVersion> OutputFormatsToAdd;
	if (const FOutputAudioFormatInfo* FormatInfo = FormatMap.Find(OutputFormat))
	{
		OutputFormatsToAdd.Add(FormatInfo->InterfaceVersion);
	}

	TArray<FMetasoundFrontendVersion> OutputFormatsToRemove;

	const FMetasoundFrontendDocument& Document = GetConstBuilder().GetDocument();
	for (const FOutputAudioFormatInfoPair& Pair : FormatMap)
	{
		const FMetasoundFrontendVersion& FormatVersion = Pair.Value.InterfaceVersion;
		if (Document.Interfaces.Contains(FormatVersion))
		{
			if (!OutputFormatsToAdd.Contains(FormatVersion))
			{
				OutputFormatsToRemove.Add(FormatVersion);
			}
		}
	}

	FModifyInterfaceOptions Options(OutputFormatsToRemove, OutputFormatsToAdd);

#if WITH_EDITORONLY_DATA
	Options.bSetDefaultNodeLocations = true;
#endif // WITH_EDITORONLY_DATA

	const bool bSuccess = Builder.ModifyInterfaces(MoveTemp(Options));
	OutResult = bSuccess ? EMetaSoundBuilderResult::Succeeded : EMetaSoundBuilderResult::Failed;
}

#if WITH_EDITORONLY_DATA
void UMetaSoundSourceBuilder::SetPlatformBlockRateOverride(const FPerPlatformFloat& PlatformBlockRate)
{
	GetMetaSoundSource().BlockRateOverride = PlatformBlockRate;
}

void UMetaSoundSourceBuilder::SetPlatformSampleRateOverride(const FPerPlatformInt& PlatformSampleRate)
{
	GetMetaSoundSource().SampleRateOverride = PlatformSampleRate;
}
#endif // WITH_EDITORONLY_DATA

void UMetaSoundSourceBuilder::SetQuality(FName Quality)
{
#if WITH_EDITORONLY_DATA
	GetMetaSoundSource().QualitySetting = Quality;
#endif //WITH_EDITORONLY_DATA	
}

void UMetaSoundSourceBuilder::SetSampleRateOverride(int32 SampleRate)
{
#if WITH_EDITORONLY_DATA
	GetMetaSoundSource().SampleRateOverride.Default = SampleRate;
#endif //WITH_EDITORONLY_DATA	
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::CreatePatchBuilder(FName BuilderName, EMetaSoundBuilderResult& OutResult)
{
	OutResult = EMetaSoundBuilderResult::Succeeded;
	return &CreateTransientBuilder<UMetaSoundPatchBuilder>(BuilderName);
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::CreateSourceBuilder(
	FName BuilderName,
	FMetaSoundBuilderNodeOutputHandle& OnPlayNodeOutput,
	FMetaSoundBuilderNodeInputHandle& OnFinishedNodeInput,
	TArray<FMetaSoundBuilderNodeInputHandle>& AudioOutNodeInputs,
	EMetaSoundBuilderResult& OutResult,
	EMetaSoundOutputAudioFormat OutputFormat,
	bool bIsOneShot)
{
	using namespace Metasound::Frontend;
	using namespace Metasound::Engine::BuilderSubsystemPrivate;

	OnPlayNodeOutput = { };
	OnFinishedNodeInput = { };
	AudioOutNodeInputs.Reset();

	UMetaSoundSourceBuilder& NewBuilder = CreateTransientBuilder<UMetaSoundSourceBuilder>(BuilderName);
	OutResult = EMetaSoundBuilderResult::Succeeded;
	if (OutputFormat != EMetaSoundOutputAudioFormat::Mono)
	{
		NewBuilder.SetFormat(OutputFormat, OutResult);
	}
	
	if (OutResult == EMetaSoundBuilderResult::Succeeded)
	{
		TArray<FMetaSoundNodeHandle> AudioOutputNodes;
		if (const Metasound::Engine::FOutputAudioFormatInfoPair* FormatInfo = NewBuilder.FindOutputAudioFormatInfo())
		{
			AudioOutputNodes = NewBuilder.FindInterfaceOutputNodes(FormatInfo->Value.InterfaceVersion.Name, OutResult);
		}
		else
		{
			OutResult = EMetaSoundBuilderResult::Failed;
		}

		if (OutResult == EMetaSoundBuilderResult::Succeeded)
		{
			Algo::Transform(AudioOutputNodes, AudioOutNodeInputs, [&NewBuilder, &BuilderName](const FMetaSoundNodeHandle& AudioOutputNode) -> FMetaSoundBuilderNodeInputHandle
			{
				EMetaSoundBuilderResult Result;
				TArray<FMetaSoundBuilderNodeInputHandle> Inputs = NewBuilder.FindNodeInputs(AudioOutputNode, Result);
				if (!Inputs.IsEmpty())
				{
					return Inputs.Last();
				}

				UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find expected audio output node input vertex. Returned vertices set may be incomplete."), *BuilderName.ToString());
				return { };
			});
		}
		else
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find expected audio output format and/or associated output nodes."), *BuilderName.ToString());
			return nullptr;
		}
	}
	else
	{
		UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to set output format when initializing."), *BuilderName.ToString());
		return nullptr;
	}

	{
		FMetaSoundNodeHandle OnPlayNode = NewBuilder.FindGraphInputNode(SourceInterface::Inputs::OnPlay, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to add required interface '%s' when attempting to create MetaSound Source Builder"), *BuilderName.ToString(), * SourceInterface::GetVersion().ToString());
			return nullptr;
		}

		TArray<FMetaSoundBuilderNodeOutputHandle> Outputs = NewBuilder.FindNodeOutputs(OnPlayNode, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find output vertex for 'OnPlay' input node when attempting to create MetaSound Source Builder"), *BuilderName.ToString());
			return nullptr;
		}

		check(!Outputs.IsEmpty());
		OnPlayNodeOutput = Outputs.Last();
	}

	if (bIsOneShot)
	{
		FMetaSoundNodeHandle OnFinishedNode = NewBuilder.FindGraphOutputNode(SourceOneShotInterface::Outputs::OnFinished, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to add '%s' interface; interface definition may not be registered."), *BuilderName.ToString(), *SourceOneShotInterface::GetVersion().ToString());
		}

		TArray<FMetaSoundBuilderNodeInputHandle> Inputs = NewBuilder.FindNodeInputs(OnFinishedNode, OutResult);
		if (OutResult == EMetaSoundBuilderResult::Failed)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Builder '%s' Creation Error: Failed to find input vertex for 'OnFinished' output node when attempting to create MetaSound Source Builder"), *BuilderName.ToString());
			return nullptr;
		}

		check(!Inputs.IsEmpty());
		OnFinishedNodeInput = Inputs.Last();
	}
	else
	{
		NewBuilder.RemoveInterface(SourceOneShotInterface::GetVersion().Name, OutResult);
	}

	return &NewBuilder;
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::CreatePatchPresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedNodeClass, EMetaSoundBuilderResult& OutResult)
{
	if (ReferencedNodeClass)
	{
		UMetaSoundPatchBuilder& Builder = CreateTransientBuilder<UMetaSoundPatchBuilder>(BuilderName);
		Builder.ConvertToPreset(ReferencedNodeClass, OutResult);
		return &Builder;
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return nullptr;
}

UMetaSoundBuilderBase& UMetaSoundBuilderSubsystem::CreatePresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedPatchClass, EMetaSoundBuilderResult& OutResult)
{
	const UClass& Class = ReferencedPatchClass->GetBaseMetaSoundUClass();
	if (&Class == UMetaSoundSource::StaticClass())
	{
		return *CreateSourcePresetBuilder(BuilderName, ReferencedPatchClass, OutResult);
	}
	else if (&Class == UMetaSoundPatch::StaticClass())
	{
		return *CreatePatchPresetBuilder(BuilderName, ReferencedPatchClass, OutResult);
	}
	else
	{
		checkf(false, TEXT("UClass '%s' cannot be built to a MetaSound preset"), *Class.GetFullName());
		return *NewObject<UMetaSoundPatchBuilder>();
	}
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::CreateSourcePresetBuilder(FName BuilderName, const TScriptInterface<IMetaSoundDocumentInterface>& ReferencedNodeClass, EMetaSoundBuilderResult& OutResult)
{
	if (ReferencedNodeClass)
	{
		UMetaSoundSourceBuilder& Builder = CreateTransientBuilder<UMetaSoundSourceBuilder>();
		Builder.ConvertToPreset(ReferencedNodeClass, OutResult);
		return &Builder;
	}

	OutResult = EMetaSoundBuilderResult::Failed;
	return nullptr;
}

UMetaSoundBuilderSubsystem* UMetaSoundBuilderSubsystem::Get()
{
	if (GEngine)
	{
		if (UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>())
		{
			return BuilderSubsystem;
		}
	}

	return nullptr;
}

UMetaSoundBuilderSubsystem& UMetaSoundBuilderSubsystem::GetChecked()
{
	checkf(GEngine, TEXT("Cannot access UMetaSoundBuilderSubsystem without engine loaded"));
	UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>();
	checkf(BuilderSubsystem, TEXT("Failed to find initialized 'UMetaSoundBuilderSubsystem"));
	return *BuilderSubsystem;
}

const UMetaSoundBuilderSubsystem* UMetaSoundBuilderSubsystem::GetConst()
{
	if (GEngine)
	{
		if (const UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<const UMetaSoundBuilderSubsystem>())
		{
			return BuilderSubsystem;
		}
	}

	return nullptr;
}

const UMetaSoundBuilderSubsystem& UMetaSoundBuilderSubsystem::GetConstChecked()
{
	checkf(GEngine, TEXT("Cannot access UMetaSoundBuilderSubsystem without engine loaded"));
	UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>();
	checkf(BuilderSubsystem, TEXT("Failed to find initialized 'UMetaSoundBuilderSubsystem"));
	return *BuilderSubsystem;
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateBoolMetaSoundLiteral(bool Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateBoolArrayMetaSoundLiteral(const TArray<bool>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateFloatMetaSoundLiteral(float Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateFloatArrayMetaSoundLiteral(const TArray<float>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateIntMetaSoundLiteral(int32 Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateIntArrayMetaSoundLiteral(const TArray<int32>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateStringMetaSoundLiteral(const FString& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateStringArrayMetaSoundLiteral(const TArray<FString>& Value, FName& OutDataType)
{
	return Metasound::Engine::BuilderSubsystemPrivate::CreatePODMetaSoundLiteral(Value, OutDataType);
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateObjectMetaSoundLiteral(UObject* Value)
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Value);
	return Literal;
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateObjectArrayMetaSoundLiteral(const TArray<UObject*>& Value)
{
	FMetasoundFrontendLiteral Literal;
	Literal.Set(Value);
	return Literal;
}

FMetasoundFrontendLiteral UMetaSoundBuilderSubsystem::CreateMetaSoundLiteralFromParam(const FAudioParameter& Param)
{
	return FMetasoundFrontendLiteral { Param };
}

bool UMetaSoundBuilderSubsystem::DetachBuilderFromAsset(const FMetasoundFrontendClassName& InClassName) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	TWeakObjectPtr<UMetaSoundBuilderBase> Builder = AssetBuilders.FindRef(InClassName);
	if (Builder.IsValid())
	{
		// If the builder has applied transactions to its document object that are not mirrored in the frontend registry,
		// unregister version in registry. This will ensure that future requests for the builder's associated asset will
		// register a fresh version from the object as the transaction history is intrinsically lost once this builder
		// is destroyed.
		if (Builder->LastTransactionRegistered != Builder->Builder.GetTransactionCount())
		{
			UObject& MetaSound = Builder->Builder.CastDocumentObjectChecked<UObject>();
			if (FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound))
			{
				MetaSoundAsset->UnregisterGraphWithFrontend();
			}
		}

		ensureAlways(AssetBuilders.Remove(InClassName));
		return true;
	}

	return false;
}

void UMetaSoundBuilderSubsystem::InvalidateDocumentCache(const FMetasoundFrontendClassName& InClassName) const
{
	using namespace Metasound::Frontend;

	TWeakObjectPtr<UMetaSoundBuilderBase> BuilderPtr = AssetBuilders.FindRef(InClassName);
	if (BuilderPtr.IsValid())
	{
		BuilderPtr->InvalidateCache();
	}
}

UMetaSoundBuilderBase* UMetaSoundBuilderSubsystem::FindBuilder(FName BuilderName)
{
	return NamedBuilders.FindRef(BuilderName);
}

UMetaSoundBuilderBase* UMetaSoundBuilderSubsystem::FindBuilderOfDocument(TScriptInterface<const IMetaSoundDocumentInterface> InMetaSound) const
{
	TWeakObjectPtr<UMetaSoundBuilderBase> Builder;
	if (const UObject* MetaSoundObject = InMetaSound.GetObject())
	{
		const FMetasoundFrontendDocument& Document = InMetaSound->GetConstDocument();
		const FMetasoundFrontendClassName& ClassName = Document.RootGraph.Metadata.GetClassName();

		Builder = AssetBuilders.FindRef(ClassName);
		if (Builder.IsValid())
		{
			ensureAlwaysMsgf(MetaSoundObject->IsAsset(), TEXT("MetaSound is asset but Builder was registered with Subsystem as transient"));
		}
		else
		{
			Builder = TransientBuilders.FindRef(ClassName);
			if (Builder.IsValid())
			{
				ensureAlwaysMsgf(!MetaSoundObject->IsAsset(), TEXT("MetaSound is transient object but Builder was registered with Subsystem as asset"));
			}
		}

	}

	return Builder.Get();
}

UMetaSoundPatchBuilder* UMetaSoundBuilderSubsystem::FindPatchBuilder(FName BuilderName)
{
	if (UMetaSoundBuilderBase* Builder = FindBuilder(BuilderName))
	{
		return Cast<UMetaSoundPatchBuilder>(Builder);
	}

	return nullptr;
}

UMetaSoundSourceBuilder* UMetaSoundBuilderSubsystem::FindSourceBuilder(FName BuilderName)
{
	if (UMetaSoundBuilderBase* Builder = FindBuilder(BuilderName))
	{
		return Cast<UMetaSoundSourceBuilder>(Builder);
	}

	return nullptr;
}

void UMetaSoundBuilderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	using namespace Metasound::Frontend;

	IDocumentBuilderRegistry::Set([]() -> IDocumentBuilderRegistry&
	{
		check(GEngine);
		UMetaSoundBuilderSubsystem* BuilderSubsystem = GEngine->GetEngineSubsystem<UMetaSoundBuilderSubsystem>();
		check(BuilderSubsystem);
		return static_cast<IDocumentBuilderRegistry&>(*BuilderSubsystem);
	});
}

bool UMetaSoundBuilderSubsystem::IsInterfaceRegistered(FName InInterfaceName) const
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendInterface Interface;
	return ISearchEngine::Get().FindInterfaceWithHighestVersion(InInterfaceName, Interface);
}

#if WITH_EDITOR
void UMetaSoundBuilderSubsystem::PostBuilderAssetTransaction(const FMetasoundFrontendClassName& InClassName)
{
	TWeakObjectPtr<UMetaSoundBuilderBase> BuilderPtr = AssetBuilders.FindRef(InClassName);
	if (UMetaSoundBuilderBase* Builder = BuilderPtr.Get())
	{
		Builder->ReloadCache();
	}
}
#endif // WITH_EDITOR

void UMetaSoundBuilderSubsystem::RegisterBuilder(FName BuilderName, UMetaSoundBuilderBase* Builder)
{
	if (Builder)
	{
		NamedBuilders.FindOrAdd(BuilderName) = Builder;
	}
}

void UMetaSoundBuilderSubsystem::RegisterPatchBuilder(FName BuilderName, UMetaSoundPatchBuilder* Builder)
{
	if (Builder)
	{
		NamedBuilders.FindOrAdd(BuilderName) = Builder;
	}
}

void UMetaSoundBuilderSubsystem::RegisterSourceBuilder(FName BuilderName, UMetaSoundSourceBuilder* Builder)
{
	if (Builder)
	{
		NamedBuilders.FindOrAdd(BuilderName) = Builder;
	}
}

bool UMetaSoundBuilderSubsystem::UnregisterBuilder(FName BuilderName)
{
	return NamedBuilders.Remove(BuilderName) > 0;
}

bool UMetaSoundBuilderSubsystem::UnregisterPatchBuilder(FName BuilderName)
{
	return NamedBuilders.Remove(BuilderName) > 0;
}

bool UMetaSoundBuilderSubsystem::UnregisterSourceBuilder(FName BuilderName)
{
	return NamedBuilders.Remove(BuilderName) > 0;
}
