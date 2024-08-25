// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundFrontendDocumentBuilder.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/ForEach.h"
#include "Algo/NoneOf.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "Interfaces/MetasoundFrontendInterfaceBindingRegistry.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocumentCache.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendDocumentModifyDelegates.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendRegistryKey.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundFrontendDocumentBuilder)


namespace Metasound::Frontend
{
	namespace DocumentBuilderPrivate
	{
		bool TryGetInterfaceBoundEdges(
			const FGuid& InFromNodeID,
			const TSet<FMetasoundFrontendVersion>& InFromNodeInterfaces,
			const FGuid& InToNodeID,
			const TSet<FMetasoundFrontendVersion>& InToNodeInterfaces,
			TSet<FNamedEdge>& OutNamedEdges)
		{
			OutNamedEdges.Reset();
			TSet<FName> InputNames;
			for (const FMetasoundFrontendVersion& InputInterfaceVersion : InToNodeInterfaces)
			{
				TArray<const FInterfaceBindingRegistryEntry*> BindingEntries;
				if (IInterfaceBindingRegistry::Get().FindInterfaceBindingEntries(InputInterfaceVersion, BindingEntries))
				{
					Algo::Sort(BindingEntries, [](const FInterfaceBindingRegistryEntry* A, const FInterfaceBindingRegistryEntry* B)
					{
						check(A);
						check(B);
						return A->GetBindingPriority() < B->GetBindingPriority();
					});

					// Bindings are sorted in registry with earlier entries being higher priority to apply connections,
					// so earlier listed connections are selected over potential collisions with later entries.
					for (const FInterfaceBindingRegistryEntry* BindingEntry : BindingEntries)
					{
						check(BindingEntry);
						if (InFromNodeInterfaces.Contains(BindingEntry->GetOutputInterfaceVersion()))
						{
							for (const FMetasoundFrontendInterfaceVertexBinding& VertexBinding : BindingEntry->GetVertexBindings())
							{
								if (!InputNames.Contains(VertexBinding.InputName))
								{
									InputNames.Add(VertexBinding.InputName);
									OutNamedEdges.Add(FNamedEdge { InFromNodeID, VertexBinding.OutputName, InToNodeID, VertexBinding.InputName });
								}
							}
						}
					}
				}
			};

			return true;
		}

		void SetNodeAndVertexNames(FMetasoundFrontendNode& InOutNode, const FMetasoundFrontendClassVertex& InVertex)
		{
			InOutNode.Name = InVertex.Name;
			// Set name on related vertices of input node
			auto IsVertexWithTypeName = [&InVertex](const FMetasoundFrontendVertex& Vertex) { return Vertex.TypeName == InVertex.TypeName; };
			if (FMetasoundFrontendVertex* InputVertex = InOutNode.Interface.Inputs.FindByPredicate(IsVertexWithTypeName))
			{
				InputVertex->Name = InVertex.Name;
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("Node associated with graph vertex of type '%s' does not contain input vertex of matching type."), *InVertex.TypeName.ToString());
			}

			if (FMetasoundFrontendVertex* OutputVertex = InOutNode.Interface.Outputs.FindByPredicate(IsVertexWithTypeName))
			{
				OutputVertex->Name = InVertex.Name;
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("Node associated with graph vertex of type '%s' does not contain output vertex of matching type."), *InVertex.TypeName.ToString());
			}
		}

		void SetDefaultLiteralOnInputNode(FMetasoundFrontendNode& InOutNode, const FMetasoundFrontendClassInput& InClassInput)
		{
			// Set the default literal on the nodes inputs so that it gets passed to the instantiated TInputNode on a live
			// auditioned MetaSound
			auto IsVertexWithName = [&Name = InClassInput.Name](const FMetasoundFrontendVertex& InVertex)
			{
				return InVertex.Name == Name;
			};

			if (const FMetasoundFrontendVertex* InputVertex = InOutNode.Interface.Inputs.FindByPredicate(IsVertexWithName))
			{
				auto IsVertexLiteralWithVertexID = [&VertexID = InputVertex->VertexID](const FMetasoundFrontendVertexLiteral& VertexLiteral)
				{
					return VertexLiteral.VertexID == VertexID;
				};
				if (FMetasoundFrontendVertexLiteral* VertexLiteral = InOutNode.InputLiterals.FindByPredicate(IsVertexLiteralWithVertexID))
				{
					// Update existing literal default value with value from class input.
					VertexLiteral->Value = InClassInput.DefaultLiteral;
				}
				else
				{
					// Add literal default value with value from class input.
					InOutNode.InputLiterals.Add(FMetasoundFrontendVertexLiteral{InputVertex->VertexID, InClassInput.DefaultLiteral});
				}
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("Input node associated with graph input vertex of name '%s' does not contain input vertex with matching name."), *InClassInput.Name.ToString());
			}
		}

		const FString GetDebugName(const IMetaSoundDocumentInterface& DocumentInterface)
		{
			const FMetasoundFrontendClassMetadata& Metadata = DocumentInterface.GetConstDocument().RootGraph.Metadata;
			const FMetasoundFrontendVersionNumber& Version = Metadata.GetVersion();
			const FNodeRegistryKey RegKey(EMetasoundFrontendClassType::External, Metadata.GetClassName(), Version.Major, Version.Minor);
			if (const FMetasoundAssetBase* Asset = IMetaSoundAssetManager::GetChecked().TryLoadAssetFromKey(RegKey))
			{
				return Asset->GetOwningAssetName();
			}

			return Metadata.GetClassName().ToString();
		}

		class FModifyInterfacesImpl
		{
		public:
			FModifyInterfacesImpl(FMetasoundFrontendDocument& InDocument, FModifyInterfaceOptions&& InOptions)
				: Options(MoveTemp(InOptions))
				, Document(InDocument)
			{
				for (const FMetasoundFrontendInterface& FromInterface : Options.InterfacesToRemove)
				{
					InputsToRemove.Append(FromInterface.Inputs);
					OutputsToRemove.Append(FromInterface.Outputs);
				}

				for (const FMetasoundFrontendInterface& ToInterface : Options.InterfacesToAdd)
				{
					Algo::Transform(ToInterface.Inputs, InputsToAdd, [this, &ToInterface](const FMetasoundFrontendClassInput& Input)
					{
						FMetasoundFrontendClassInput NewInput = Input;
						NewInput.NodeID = FDocumentIDGenerator::Get().CreateNodeID(Document);
						NewInput.VertexID = FDocumentIDGenerator::Get().CreateVertexID(Document);
						return FInputInterfacePair { MoveTemp(NewInput), &ToInterface };
					});

					Algo::Transform(ToInterface.Outputs, OutputsToAdd, [this, &ToInterface](const FMetasoundFrontendClassOutput& Output)
					{
						FMetasoundFrontendClassOutput NewOutput = Output;
						NewOutput.NodeID = FDocumentIDGenerator::Get().CreateNodeID(Document);
						NewOutput.VertexID = FDocumentIDGenerator::Get().CreateVertexID(Document);
						return FOutputInterfacePair { MoveTemp(NewOutput), &ToInterface };
					});
				}

				// Iterate in reverse to allow removal from `InputsToAdd`
				for (int32 AddIndex = InputsToAdd.Num() - 1; AddIndex >= 0; AddIndex--)
				{
					const FMetasoundFrontendClassVertex& VertexToAdd = InputsToAdd[AddIndex].Key;

					const int32 RemoveIndex = InputsToRemove.IndexOfByPredicate([&](const FMetasoundFrontendClassVertex& VertexToRemove)
						{
							if (VertexToAdd.TypeName != VertexToRemove.TypeName)
							{
								return false;
							}

							if (Options.NamePairingFunction)
							{
								return Options.NamePairingFunction(VertexToAdd.Name, VertexToRemove.Name);
							}

							FName ParamA;
							FName ParamB;
							FName Namespace;
							VertexToAdd.SplitName(Namespace, ParamA);
							VertexToRemove.SplitName(Namespace, ParamB);

							return ParamA == ParamB;
						});

					if (INDEX_NONE != RemoveIndex)
					{
						PairedInputs.Add(FVertexPair { InputsToRemove[RemoveIndex], InputsToAdd[AddIndex].Key });
						InputsToRemove.RemoveAtSwap(RemoveIndex, 1, EAllowShrinking::No);
						InputsToAdd.RemoveAtSwap(AddIndex, 1, EAllowShrinking::No);
					}
				}

				// Iterate in reverse to allow removal from `OutputsToAdd`
				for (int32 AddIndex = OutputsToAdd.Num() - 1; AddIndex >= 0; AddIndex--)
				{
					const FMetasoundFrontendClassVertex& VertexToAdd = OutputsToAdd[AddIndex].Key;

					const int32 RemoveIndex = OutputsToRemove.IndexOfByPredicate([&](const FMetasoundFrontendClassVertex& VertexToRemove)
						{
							if (VertexToAdd.TypeName != VertexToRemove.TypeName)
							{
								return false;
							}

							if (Options.NamePairingFunction)
							{
								return Options.NamePairingFunction(VertexToAdd.Name, VertexToRemove.Name);
							}

							FName ParamA;
							FName ParamB;
							FName Namespace;
							VertexToAdd.SplitName(Namespace, ParamA);
							VertexToRemove.SplitName(Namespace, ParamB);

							return ParamA == ParamB;
						});

					if (INDEX_NONE != RemoveIndex)
					{
						PairedOutputs.Add(FVertexPair{ OutputsToRemove[RemoveIndex], OutputsToAdd[AddIndex].Key });
						OutputsToRemove.RemoveAtSwap(RemoveIndex);
						OutputsToAdd.RemoveAtSwap(AddIndex);
					}
				}
			}

		private:
			bool AddMissingVertices(FMetaSoundFrontendDocumentBuilder& OutBuilder) const
			{
				if (!InputsToAdd.IsEmpty() || !OutputsToAdd.IsEmpty())
				{
					for (const FInputInterfacePair& Pair: InputsToAdd)
					{
						OutBuilder.AddGraphInput(Pair.Key);
					}

					for (const FOutputInterfacePair& Pair : OutputsToAdd)
					{
						OutBuilder.AddGraphOutput(Pair.Key);
					}

					return true;
				}

				return false;
			}

			bool RemoveUnsupportedVertices(FMetaSoundFrontendDocumentBuilder& OutBuilder) const
			{
				bool bDidEdit = false;

				for (const TPair<FMetasoundFrontendClassInput, const FMetasoundFrontendInterface*>& Pair : InputsToAdd)
				{
					if (OutBuilder.RemoveGraphInput(Pair.Key.Name))
					{
						UE_LOG(LogMetaSound, Warning, TEXT("Removed existing targeted input '%s' to avoid name collision/member data descrepancies while modifying interface(s). Desired edges may have been removed as a result."), *Pair.Key.Name.ToString());
						bDidEdit = true;
					}
				}

				for (const TPair<FMetasoundFrontendClassOutput, const FMetasoundFrontendInterface*>& Pair : OutputsToAdd)
				{
					if (OutBuilder.RemoveGraphOutput(Pair.Key.Name))
					{
						UE_LOG(LogMetaSound, Warning, TEXT("Removed existing targeted output '%s' to avoid name collision/member data descrepancies while modifying interface(s). Desired edges may have been removed as a result."), *Pair.Key.Name.ToString());
						bDidEdit = true;
					}
				}

				if (!InputsToRemove.IsEmpty() || !OutputsToRemove.IsEmpty())
				{
					// Remove unsupported inputs
					for (const FMetasoundFrontendClassVertex& InputToRemove : InputsToRemove)
					{
						if (OutBuilder.RemoveGraphInput(InputToRemove.Name))
						{
							bDidEdit = true;
						}
						else
						{
							UE_LOG(LogMetaSound, Warning, TEXT("Failed to remove existing input '%s', which was an expected member of a removed interface."), *InputToRemove.Name.ToString());
						}
					}

					// Remove unsupported outputs
					for (const FMetasoundFrontendClassVertex& OutputToRemove : OutputsToRemove)
					{
						if (OutBuilder.RemoveGraphOutput(OutputToRemove.Name))
						{
							bDidEdit = true;
						}
						else
						{
							UE_LOG(LogMetaSound, Warning, TEXT("Failed to remove existing output '%s', which was an expected member of a removed interface."), *OutputToRemove.Name.ToString());
						}
					}

					return true;
				}

				return false;
			}

			bool SwapPairedVertices(FMetaSoundFrontendDocumentBuilder& OutBuilder) const
			{
				bool bDidEdit = false;
				for (const FVertexPair& PairedInput : PairedInputs)
				{
					const bool bSwapped = OutBuilder.SwapGraphInput(PairedInput.Get<0>(), PairedInput.Get<1>());
					bDidEdit |= bSwapped;
				}

				for (const FVertexPair& PairedOutput : PairedOutputs)
				{
					const bool bSwapped = OutBuilder.SwapGraphOutput(PairedOutput.Get<0>(), PairedOutput.Get<1>());
					bDidEdit |= bSwapped;
				}

				return bDidEdit;
			}

#if WITH_EDITORONLY_DATA
			void UpdateAddedVertexNodePositions(
				EMetasoundFrontendClassType ClassType,
				const FMetaSoundFrontendDocumentBuilder& InBuilder,
				TSet<FName>& AddedNames,
				TFunctionRef<int32(const FVertexName&)> InGetSortOrder,
				TArrayView<FMetasoundFrontendNode> OutNodes)
			{
				// Add graph member nodes by sort order
				TSortedMap<int32, FMetasoundFrontendNode*> SortOrderToNode;
				for (FMetasoundFrontendNode& Node : OutNodes)
				{
					if (const FMetasoundFrontendClass* Class = InBuilder.FindDependency(Node.ClassID))
					{
						if (Class->Metadata.GetType() == ClassType)
						{
							const int32 Index = InGetSortOrder(Node.Name);
							SortOrderToNode.Add(Index, &Node);
						}
					}
				}

				// Prime the first location as an offset prior to an existing location (as provided by a swapped member)
				//  to avoid placing away from user's active area if possible.
				FVector2D NextLocation = { 0.0f, 0.0f };
				{
					int32 NumBeforeDefined = 1;
					for (const TPair<int32, FMetasoundFrontendNode*>& Pair : SortOrderToNode)
					{
						const FMetasoundFrontendNode* Node = Pair.Value;
						const FName NodeName = Node->Name;
						if (AddedNames.Contains(NodeName))
						{
							NumBeforeDefined++;
						}
						else
						{
							const TMap<FGuid, FVector2D>& Locations = Node->Style.Display.Locations;
							if (!Locations.IsEmpty())
							{
								for (const TPair<FGuid, FVector2D>& Location : Locations)
								{
									NextLocation = Location.Value - (NumBeforeDefined * DisplayStyle::NodeLayout::DefaultOffsetY);
									break;
								}
								break;
							}
						}
					}
				}

				// Iterate through sorted map in sequence, slotting in new locations after existing swapped nodes with predefined locations.
				for (TPair<int32, FMetasoundFrontendNode*>& Pair : SortOrderToNode)
				{
					FMetasoundFrontendNode* Node = Pair.Value;
					const FName NodeName = Node->Name;
					if (AddedNames.Contains(NodeName))
					{
						Node->Style.Display.Locations.Add(FGuid(), NextLocation);
						NextLocation += DisplayStyle::NodeLayout::DefaultOffsetY;
					}
					else
					{
						for (const TPair<FGuid, FVector2D>& Location : Node->Style.Display.Locations)
						{
							NextLocation = Location.Value + DisplayStyle::NodeLayout::DefaultOffsetY;
						}
					}
				}
			}
#endif // WITH_EDITORONLY_DATA

		public:
			bool Execute(FMetaSoundFrontendDocumentBuilder& OutBuilder, FDocumentModifyDelegates& OutDelegates)
			{
				bool bDidEdit = false;

				for (const FMetasoundFrontendInterface& Interface : Options.InterfacesToRemove)
				{
					if (Document.Interfaces.Contains(Interface.Version))
					{
						OutDelegates.InterfaceDelegates.OnRemovingInterface.Broadcast(Interface);
						bDidEdit = true;
#if WITH_EDITORONLY_DATA
						Document.Metadata.ModifyContext.AddInterfaceModified(Interface.Version.Name);
#endif // WITH_EDITORONLY_DATA
						Document.Interfaces.Remove(Interface.Version);
					}
				}

				for (const FMetasoundFrontendInterface& Interface : Options.InterfacesToAdd)
				{
					bool bAlreadyInSet = false;
					Document.Interfaces.Add(Interface.Version, &bAlreadyInSet);
					if (!bAlreadyInSet)
					{
						OutDelegates.InterfaceDelegates.OnInterfaceAdded.Broadcast(Interface);
						bDidEdit = true;
#if WITH_EDITORONLY_DATA
						Document.Metadata.ModifyContext.AddInterfaceModified(Interface.Version.Name);
#endif // WITH_EDITORONLY_DATA
					}
				}

				bDidEdit |= RemoveUnsupportedVertices(OutBuilder);
				bDidEdit |= SwapPairedVertices(OutBuilder);
				const bool bAddedVertices = AddMissingVertices(OutBuilder);
				bDidEdit |= bAddedVertices;

				if (bDidEdit)
				{
					OutBuilder.RemoveUnusedDependencies();
				}

#if WITH_EDITORONLY_DATA
				if (bAddedVertices && Options.bSetDefaultNodeLocations)
				{
					TArray<FMetasoundFrontendNode>& Nodes = Document.RootGraph.Graph.Nodes;
					// Sort/Place Inputs
					{
						TSet<FName> NamesToSort;
						Algo::Transform(InputsToAdd, NamesToSort, [](const FInputInterfacePair& Pair) { return Pair.Key.Name; });
						auto GetInputSortOrder = [&OutBuilder](const FVertexName& InVertexName)
						{
							const FMetasoundFrontendClassInput* Input = OutBuilder.FindGraphInput(InVertexName);
							checkf(Input, TEXT("Input must exist by this point of modifying the document's interfaces and respective members"));
							return Input->Metadata.SortOrderIndex;
						};
						UpdateAddedVertexNodePositions(EMetasoundFrontendClassType::Input, OutBuilder, NamesToSort, GetInputSortOrder, Nodes);
					}

					// Sort/Place Outputs
					{
						TSet<FName> NamesToSort;
						Algo::Transform(OutputsToAdd, NamesToSort, [](const FOutputInterfacePair& OutputInterfacePair) { return OutputInterfacePair.Key.Name; });
						auto GetOutputSortOrder = [&OutBuilder](const FVertexName& InVertexName)
						{
							const FMetasoundFrontendClassOutput* Output = OutBuilder.FindGraphOutput(InVertexName);
							checkf(Output, TEXT("Output must exist by this point of modifying the document's interfaces and respective members"));
							return Output->Metadata.SortOrderIndex;
						};
						UpdateAddedVertexNodePositions(EMetasoundFrontendClassType::Output, OutBuilder, NamesToSort, GetOutputSortOrder, Nodes);
					}
				}
#endif // WITH_EDITORONLY_DATA

				return bDidEdit;
			}

			const FModifyInterfaceOptions Options;

		private:
			FMetasoundFrontendDocument& Document;

			using FVertexPair = TTuple<FMetasoundFrontendClassVertex, FMetasoundFrontendClassVertex>;
			TArray<FVertexPair> PairedInputs;
			TArray<FVertexPair> PairedOutputs;

			using FInputInterfacePair = TPair<FMetasoundFrontendClassInput, const FMetasoundFrontendInterface*>;
			using FOutputInterfacePair = TPair<FMetasoundFrontendClassOutput, const FMetasoundFrontendInterface*>;
			TArray<FInputInterfacePair> InputsToAdd;
			TArray<FOutputInterfacePair> OutputsToAdd;

			TArray<FMetasoundFrontendClassInput> InputsToRemove;
			TArray<FMetasoundFrontendClassOutput> OutputsToRemove;
		};
	} // namespace DocumentBuilderPrivate

	FString LexToString(const EInvalidEdgeReason& InReason)
	{
		switch (InReason)
		{
			case EInvalidEdgeReason::None:
				return TEXT("No reason");

			case EInvalidEdgeReason::MismatchedAccessType:
				return TEXT("Mismatched Access Type");

			case EInvalidEdgeReason::MismatchedDataType:
				return TEXT("Mismatched DataType");

			case EInvalidEdgeReason::MissingInput:
				return TEXT("Missing Input");

			case EInvalidEdgeReason::MissingOutput:
				return TEXT("Missing Output");

			default:
				return TEXT("COUNT");
		}

		static_assert(static_cast<uint32>(EInvalidEdgeReason::COUNT) == 5, "Potential missing case coverage for EInvalidEdgeReason");
	}

	FModifyInterfaceOptions::FModifyInterfaceOptions(const TArray<FMetasoundFrontendInterface>& InInterfacesToRemove, const TArray<FMetasoundFrontendInterface>& InInterfacesToAdd)
		: InterfacesToRemove(InInterfacesToRemove)
		, InterfacesToAdd(InInterfacesToAdd)
	{
	}

	FModifyInterfaceOptions::FModifyInterfaceOptions(TArray<FMetasoundFrontendInterface>&& InInterfacesToRemove, TArray<FMetasoundFrontendInterface>&& InInterfacesToAdd)
		: InterfacesToRemove(MoveTemp(InInterfacesToRemove))
		, InterfacesToAdd(MoveTemp(InInterfacesToAdd))
	{
	}

	FModifyInterfaceOptions::FModifyInterfaceOptions(const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToRemove, const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToAdd)
	{
		Algo::Transform(InInterfaceVersionsToRemove, InterfacesToRemove, [](const FMetasoundFrontendVersion& Version)
		{
			FMetasoundFrontendInterface Interface;
			const bool bFromInterfaceFound = IInterfaceRegistry::Get().FindInterface(GetInterfaceRegistryKey(Version), Interface);
			if (!ensureAlways(bFromInterfaceFound))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to find interface '%s' to remove"), *Version.ToString());
			}
			return Interface;
		});

		Algo::Transform(InInterfaceVersionsToAdd, InterfacesToAdd, [](const FMetasoundFrontendVersion& Version)
		{
			FMetasoundFrontendInterface Interface;
			const bool bToInterfaceFound = IInterfaceRegistry::Get().FindInterface(GetInterfaceRegistryKey(Version), Interface);
			if (!ensureAlways(bToInterfaceFound))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to find interface '%s' to add"), *Version.ToString());
			}
			return Interface;
		});
	}
} // namespace Metasound::Frontend


UMetaSoundBuilderDocument& UMetaSoundBuilderDocument::Create(const UClass& InBuilderClass)
{
	UMetaSoundBuilderDocument* DocObject = NewObject<UMetaSoundBuilderDocument>();
	DocObject->MetaSoundUClass = InBuilderClass;
	return *DocObject;
}

UMetaSoundBuilderDocument& UMetaSoundBuilderDocument::Create(const IMetaSoundDocumentInterface& InDocToCopy)
{
	UMetaSoundBuilderDocument* DocObject = NewObject<UMetaSoundBuilderDocument>();
	DocObject->Document = InDocToCopy.GetConstDocument();
	DocObject->MetaSoundUClass = InDocToCopy.GetBaseMetaSoundUClass();
	return *DocObject;
}

FTopLevelAssetPath UMetaSoundBuilderDocument::GetAssetPathChecked() const
{
	FTopLevelAssetPath Path;
	ensureAlwaysMsgf(Path.TrySetPath(this), TEXT("Failed to set TopLevelAssetPath from transient MetaSound '%s'. MetaSound must be highest level object in package."), *GetPathName());
	ensureAlwaysMsgf(Path.IsValid(), TEXT("Failed to set TopLevelAssetPath from MetaSound '%s'. This may be caused by calling this function when the asset is being destroyed."), *GetPathName());
	return Path;
}

const FMetasoundFrontendDocument& UMetaSoundBuilderDocument::GetConstDocument() const
{
	return Document;
}

const UClass& UMetaSoundBuilderDocument::GetBaseMetaSoundUClass() const
{
	checkf(MetaSoundUClass, TEXT("BaseMetaSoundUClass must be set upon creation of UMetaSoundBuilderDocument instance"));
	return *MetaSoundUClass;
}

FMetasoundFrontendDocument& UMetaSoundBuilderDocument::GetDocument()
{
	return Document;
}

void UMetaSoundBuilderDocument::OnBeginActiveBuilder()
{
	// Nothing to do here. UMetaSoundBuilderDocuments are always being used by builders
}

void UMetaSoundBuilderDocument::OnFinishActiveBuilder()
{
	// Nothing to do here. UMetaSoundBuilderDocuments are always being used by builders
}

FMetaSoundFrontendDocumentBuilder::FMetaSoundFrontendDocumentBuilder()
	: DocumentDelegates(MakeShared<Metasound::Frontend::FDocumentModifyDelegates>())
{
}

FMetaSoundFrontendDocumentBuilder::FMetaSoundFrontendDocumentBuilder(TScriptInterface<IMetaSoundDocumentInterface> InDocumentInterface)
	: DocumentInterface(InDocumentInterface)
	, DocumentDelegates(MakeShared<Metasound::Frontend::FDocumentModifyDelegates>())
{
	if (DocumentInterface)
	{
		BeginBuilding();
		InitCacheInternal();
	}
}

FMetaSoundFrontendDocumentBuilder::FMetaSoundFrontendDocumentBuilder(TScriptInterface<IMetaSoundDocumentInterface> InDocumentInterface, TSharedRef<Metasound::Frontend::FDocumentModifyDelegates> InDocumentDelegates)
	: DocumentInterface(InDocumentInterface)
	, DocumentDelegates(InDocumentDelegates)
{
	if (DocumentInterface)
	{
		BeginBuilding();
		InitCacheInternal();
	}
}
FMetaSoundFrontendDocumentBuilder::~FMetaSoundFrontendDocumentBuilder()
{
	FinishBuilding();
}

const FMetasoundFrontendClass* FMetaSoundFrontendDocumentBuilder::AddDependency(const FMetasoundFrontendClass& InClass)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Document = GetDocument();
	const FMetasoundFrontendClass* Dependency = nullptr;

	FMetasoundFrontendClass NewDependency = InClass;

	// All 'Graph' dependencies are listed as 'External' from the perspective of the owning document.
	// This makes them implementation agnostic to accommodate nativization of assets.
	if (NewDependency.Metadata.GetType() == EMetasoundFrontendClassType::Graph)
	{
		NewDependency.Metadata.SetType(EMetasoundFrontendClassType::External);
	}

	NewDependency.ID = FDocumentIDGenerator::Get().CreateClassID(Document);
	Dependency = &Document.Dependencies.Emplace_GetRef(MoveTemp(NewDependency));

	const int32 NewIndex = Document.Dependencies.Num() - 1;
	DocumentDelegates->OnDependencyAdded.Broadcast(NewIndex);

	return Dependency;
}

void FMetaSoundFrontendDocumentBuilder::AddEdge(FMetasoundFrontendEdge&& InNewEdge)
{
	using namespace Metasound::Frontend;

	checkf(CanAddEdge(InNewEdge), TEXT("Attempted call to AddEdge in MetaSound Builder where not valid."));

	FMetasoundFrontendDocument& Document = GetDocument();
	FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
	Graph.Edges.Add(MoveTemp(InNewEdge));
	const int32 NewIndex = Graph.Edges.Num() - 1;
	DocumentDelegates->EdgeDelegates.OnEdgeAdded.Broadcast(NewIndex);
}

bool FMetaSoundFrontendDocumentBuilder::AddNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& EdgesToMake, TArray<const FMetasoundFrontendEdge*>* OutNewEdges, bool bReplaceExistingConnections)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Document = GetDocument();
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();

	if (OutNewEdges)
	{
		OutNewEdges->Reset();
	}

	bool bSuccess = true;

	struct FNewEdgeData
	{
		FMetasoundFrontendEdge NewEdge;
		const FMetasoundFrontendVertex* OutputVertex = nullptr;
		const FMetasoundFrontendVertex* InputVertex = nullptr;
	};

	TArray<FNewEdgeData> EdgesToAdd;
	for (const FNamedEdge& Edge : EdgesToMake)
	{
		const FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
		const FMetasoundFrontendVertex* OutputVertex = NodeCache.FindOutputVertex(Edge.OutputNodeID, Edge.OutputName);
		const FMetasoundFrontendVertex* InputVertex = NodeCache.FindInputVertex(Edge.InputNodeID, Edge.InputName);

		if (OutputVertex && InputVertex)
		{
			FMetasoundFrontendEdge NewEdge = { Edge.OutputNodeID, OutputVertex->VertexID, Edge.InputNodeID, InputVertex->VertexID };
			const EInvalidEdgeReason InvalidEdgeReason = IsValidEdge(NewEdge);
			if (InvalidEdgeReason == EInvalidEdgeReason::None)
			{
				EdgesToAdd.Add(FNewEdgeData { MoveTemp(NewEdge), OutputVertex, InputVertex });
			}
			else
			{
				bSuccess = false;
				UE_LOG(LogMetaSound, Error, TEXT("Failed to add connections between MetaSound output '%s' and input '%s': '%s'."), *Edge.OutputName.ToString(), *Edge.InputName.ToString(), *LexToString(InvalidEdgeReason));
			}
		}
	}

	const TArray<FMetasoundFrontendEdge>& Edges = GetDocument().RootGraph.Graph.Edges;
	const int32 LastIndex = Edges.Num() - 1;
	for (FNewEdgeData& EdgeToAdd : EdgesToAdd)
	{
		if (bReplaceExistingConnections)
		{
#if !NO_LOGGING
			const FMetasoundFrontendNode* OldOutputNode = nullptr;
			const FMetasoundFrontendVertex* OldOutputVertex = FindNodeOutputConnectedToNodeInput(EdgeToAdd.NewEdge.ToNodeID, EdgeToAdd.NewEdge.ToVertexID, &OldOutputNode);
#endif // !NO_LOGGING

			const bool bRemovedEdge = RemoveEdgeToNodeInput(EdgeToAdd.NewEdge.ToNodeID, EdgeToAdd.NewEdge.ToVertexID);

#if !NO_LOGGING
			if (bRemovedEdge)
			{
				checkf(OldOutputNode, TEXT("MetaSound edge was removed from output but output node not found."));
				checkf(OldOutputVertex, TEXT("MetaSound edge was removed from output but output vertex not found."));

				const FMetasoundFrontendNode* InputNode = FindNode(EdgeToAdd.NewEdge.ToNodeID);
				checkf(InputNode, TEXT("Edge was deemed valid but input parent node is missing"));

				const FMetasoundFrontendNode* OutputNode = FindNode(EdgeToAdd.NewEdge.FromNodeID);
				checkf(OutputNode, TEXT("Edge was deemed valid but output parent node is missing"));

				UE_LOG(LogMetaSound, Verbose, TEXT("Removed connection from node output '%s:%s' to node '%s:%s' in order to connect to node output '%s:%s'"),
					*OldOutputNode->Name.ToString(),
					*OldOutputVertex->Name.ToString(),
					*InputNode->Name.ToString(),
					*EdgeToAdd.InputVertex->Name.ToString(),
					*OutputNode->Name.ToString(),
					*EdgeToAdd.OutputVertex->Name.ToString());
			}
#endif // !NO_LOGGING

			AddEdge(MoveTemp(EdgeToAdd.NewEdge));
		}
		else if (!IsNodeInputConnected(EdgeToAdd.NewEdge.ToNodeID, EdgeToAdd.NewEdge.ToVertexID))
		{
			AddEdge(MoveTemp(EdgeToAdd.NewEdge));
		}
		else
		{
			bSuccess = false;

#if !NO_LOGGING
			FMetasoundFrontendEdge EdgeToRemove;
			if (const int32* EdgeIndex = DocumentCache->GetEdgeCache().FindEdgeIndexToNodeInput(EdgeToAdd.NewEdge.ToNodeID, EdgeToAdd.NewEdge.ToVertexID))
			{
				EdgeToRemove = Document.RootGraph.Graph.Edges[*EdgeIndex];
			}

			const FMetasoundFrontendVertex* Input = FindNodeInput(EdgeToAdd.NewEdge.ToNodeID, EdgeToAdd.NewEdge.ToVertexID);
			checkf(Input, TEXT("Prior loop to check edge validity should protect against missing input vertex"));

			const FMetasoundFrontendVertex* Output = FindNodeOutput(EdgeToAdd.NewEdge.FromNodeID, EdgeToAdd.NewEdge.FromVertexID);
			checkf(Input, TEXT("Prior loop to check edge validity should protect against missing output vertex"));

			UE_LOG(LogMetaSound, Warning, TEXT("Connection between MetaSound output '%s' and input '%s' not added: Input already connected to '%s'."), *Output->Name.ToString(), *Input->Name.ToString(), *Output->Name.ToString());
#endif // !NO_LOGGING
		}
	}

	if (OutNewEdges)
	{
		for (int32 Index = LastIndex + 1; Index < Edges.Num(); ++Index)
		{
			OutNewEdges->Add(&Edges[Index]);
		}
	}

	return bSuccess;
}

bool FMetaSoundFrontendDocumentBuilder::AddEdgesByNodeClassInterfaceBindings(const FGuid& InFromNodeID, const FGuid& InToNodeID, bool bReplaceExistingConnections)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	TSet<FMetasoundFrontendVersion> FromInterfaceVersions;
	TSet<FMetasoundFrontendVersion> ToInterfaceVersions;
	if (FindNodeClassInterfaces(InFromNodeID, FromInterfaceVersions) && FindNodeClassInterfaces(InToNodeID, ToInterfaceVersions))
	{
		TSet<FNamedEdge> NamedEdges;
		if (DocumentBuilderPrivate::TryGetInterfaceBoundEdges(InFromNodeID, FromInterfaceVersions, InToNodeID, ToInterfaceVersions, NamedEdges))
		{
			return AddNamedEdges(NamedEdges, nullptr, bReplaceExistingConnections);
		}
	}

	return false;

}

bool FMetaSoundFrontendDocumentBuilder::AddEdgesFromMatchingInterfaceNodeOutputsToGraphOutputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetaSoundFrontendDocumentBuilder::AddEdgesFromMatchingInterfaceNodeOutputsToGraphOutputs);

	using namespace Metasound::Frontend;

	OutEdgesCreated.Reset();

	TSet<FMetasoundFrontendVersion> NodeInterfaces;
	if (!FindNodeClassInterfaces(InNodeID, NodeInterfaces))
	{
		// Did not find any node interfaces
		return false;
	}

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	const IDocumentGraphInterfaceCache& InterfaceCache = DocumentCache->GetInterfaceCache();
	const TSet<FMetasoundFrontendVersion> CommonInterfaces = NodeInterfaces.Intersect(GetDocument().Interfaces);

	TSet<FNamedEdge> EdgesToMake;
	for (const FMetasoundFrontendVersion& Version : CommonInterfaces)
	{
		const FInterfaceRegistryKey InterfaceKey = GetInterfaceRegistryKey(Version);
		if (const IInterfaceRegistryEntry* RegistryEntry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(InterfaceKey))
		{
			Algo::Transform(RegistryEntry->GetInterface().Outputs, EdgesToMake, [this, &NodeCache, &InterfaceCache, InNodeID](const FMetasoundFrontendClassOutput& Output)
			{
				const FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
				const FMetasoundFrontendVertex* NodeVertex = NodeCache.FindOutputVertex(InNodeID, Output.Name);
				check(NodeVertex);
				const FMetasoundFrontendClassOutput* OutputClass = InterfaceCache.FindOutput(Output.Name);
				check(OutputClass);
				const FMetasoundFrontendNode* OutputNode = NodeCache.FindNode(OutputClass->NodeID);
				check(OutputNode);
				const TArray<FMetasoundFrontendVertex>& Inputs = OutputNode->Interface.Inputs;
				check(!Inputs.IsEmpty());
				return FNamedEdge { InNodeID, NodeVertex->Name, OutputNode->GetID(), Inputs.Last().Name };
			});
		}
	}

	return AddNamedEdges(EdgesToMake, &OutEdgesCreated, bReplaceExistingConnections);
}

bool FMetaSoundFrontendDocumentBuilder::AddEdgesFromMatchingInterfaceNodeInputsToGraphInputs(const FGuid& InNodeID, TArray<const FMetasoundFrontendEdge*>& OutEdgesCreated, bool bReplaceExistingConnections)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetaSoundFrontendDocumentBuilder::AddEdgesFromMatchingInterfaceNodeInputsToGraphInputs);

	using namespace Metasound::Frontend;

	OutEdgesCreated.Reset();

	TSet<FMetasoundFrontendVersion> NodeInterfaces;
	if (!FindNodeClassInterfaces(InNodeID, NodeInterfaces))
	{
		// Did not find any node interfaces
		return false;
	}

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	const IDocumentGraphInterfaceCache& InterfaceCache = DocumentCache->GetInterfaceCache();
	const TSet<FMetasoundFrontendVersion> CommonInterfaces = NodeInterfaces.Intersect(GetDocument().Interfaces);

	TSet<FNamedEdge> EdgesToMake;
	for (const FMetasoundFrontendVersion& Version : CommonInterfaces)
	{
		const FInterfaceRegistryKey InterfaceKey = GetInterfaceRegistryKey(Version);
		if (const IInterfaceRegistryEntry* RegistryEntry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(InterfaceKey))
		{
			Algo::Transform(RegistryEntry->GetInterface().Inputs, EdgesToMake, [this, &NodeCache, &InterfaceCache, InNodeID](const FMetasoundFrontendClassInput& Input)
			{
				const FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
				const FMetasoundFrontendVertex* NodeVertex = NodeCache.FindInputVertex(InNodeID, Input.Name);
				check(NodeVertex);
				const FMetasoundFrontendClassInput* InputClass = InterfaceCache.FindInput(Input.Name);
				check(InputClass);
				const FMetasoundFrontendNode* InputNode = NodeCache.FindNode(InputClass->NodeID);
				check(InputNode);
				const TArray<FMetasoundFrontendVertex>& Outputs = InputNode->Interface.Outputs;
				check(!Outputs.IsEmpty());
				return FNamedEdge { InputNode->GetID(), Outputs.Last().Name, InNodeID, NodeVertex->Name };
			});
		}
	}

	return AddNamedEdges(EdgesToMake, &OutEdgesCreated, bReplaceExistingConnections);
}

const FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::AddGraphInput(const FMetasoundFrontendClassInput& InClassInput)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Document = GetDocument();
	FMetasoundFrontendGraphClass& RootGraph = Document.RootGraph;

	checkf(InClassInput.NodeID.IsValid(), TEXT("Unassigned NodeID when adding graph input"));
	checkf(InClassInput.VertexID.IsValid(), TEXT("Unassigned VertexID when adding graph input"));

	if (InClassInput.TypeName.IsNone())
	{
		UE_LOG(LogMetaSound, Error, TEXT("TypeName unset when attempting to add class input '%s'"), *InClassInput.Name.ToString());
		return nullptr;
	}
	else if (const FMetasoundFrontendClassInput* Input = DocumentCache->GetInterfaceCache().FindInput(InClassInput.Name))
	{
		UE_LOG(LogMetaSound, Error, TEXT("Attempting to add MetaSound graph input '%s' when input with name already exists"), *InClassInput.Name.ToString());
		const FMetasoundFrontendNode* OutputNode = DocumentCache->GetNodeCache().FindNode(Input->NodeID);
		check(OutputNode);
		return OutputNode;
	}
	else if (!IDataTypeRegistry::Get().IsRegistered(InClassInput.TypeName))
	{
		UE_LOG(LogMetaSound, Error, TEXT("Cannot add MetaSound graph input '%s' with unregistered TypeName '%s'"), *InClassInput.Name.ToString(), *InClassInput.TypeName.ToString());
		return nullptr;
	}

	auto FindRegistryClass = [&InClassInput](FMetasoundFrontendClass& OutClass) -> bool
	{
		switch (InClassInput.AccessType)
		{
		case EMetasoundFrontendVertexAccessType::Value:
		{
			return IDataTypeRegistry::Get().GetFrontendConstructorInputClass(InClassInput.TypeName, OutClass);
		}
		break;

		case EMetasoundFrontendVertexAccessType::Reference:
		{
			return IDataTypeRegistry::Get().GetFrontendInputClass(InClassInput.TypeName, OutClass);
		}
		break;

		case EMetasoundFrontendVertexAccessType::Unset:
		default:
		{
			checkNoEntry();
		}
		break;
		}

		return false;
	};

	FMetasoundFrontendClass Class;
	if (FindRegistryClass(Class))
	{
		if(!FindDependency(Class.Metadata))
		{
			AddDependency(Class);
		}

		auto FinalizeNode = [&InClassInput](FMetasoundFrontendNode& InOutNode, const Metasound::Frontend::FNodeRegistryKey&)
		{
			// Sets the name of the node an vertices on the node to match the class vertex name
			DocumentBuilderPrivate::SetNodeAndVertexNames(InOutNode, InClassInput);

			// Set the default literal on the nodes inputs so that it gets passed to the instantiated TInputNode on a live
			// auditioned MetaSound.
			DocumentBuilderPrivate::SetDefaultLiteralOnInputNode(InOutNode, InClassInput);
		};
		if (FMetasoundFrontendNode* NewNode = AddNodeInternal(Class.Metadata, FinalizeNode, InClassInput.NodeID))
		{
			// Remove the default literal on the node added during the "FinalizeNode" call. This matches how 
			// nodes are serialized in editor. The default literals are only stored on the FMetasoundFrontendClassInputs.
			NewNode->InputLiterals.Reset();

			const int32 NewIndex = RootGraph.Interface.Inputs.Num();
			FMetasoundFrontendClassInput& NewInput = RootGraph.Interface.Inputs.Add_GetRef(InClassInput);
			if (!NewInput.VertexID.IsValid())
			{
				NewInput.VertexID = FDocumentIDGenerator::Get().CreateVertexID(Document);
			}

			DocumentDelegates->InterfaceDelegates.OnInputAdded.Broadcast(NewIndex);
#if WITH_EDITORONLY_DATA
			Document.Metadata.ModifyContext.AddMemberIDModified(InClassInput.NodeID);
#endif // WITH_EDITORONLY_DATA

			return NewNode;
		}
	}

	return nullptr;
}

const FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::AddGraphOutput(const FMetasoundFrontendClassOutput& InClassOutput)
{
	using namespace Metasound::Frontend;

	checkf(InClassOutput.NodeID.IsValid(), TEXT("Unassigned NodeID when adding graph output"));
	checkf(InClassOutput.VertexID.IsValid(), TEXT("Unassigned VertexID when adding graph output"));

	FMetasoundFrontendDocument& Document = GetDocument();
	FMetasoundFrontendGraphClass& RootGraph = Document.RootGraph;

	if (InClassOutput.TypeName.IsNone())
	{
		UE_LOG(LogMetaSound, Error, TEXT("TypeName unset when attempting to add class output '%s'"), *InClassOutput.Name.ToString());
		return nullptr;
	}
	else if (const FMetasoundFrontendClassOutput* Output = DocumentCache->GetInterfaceCache().FindOutput(InClassOutput.Name))
	{
		UE_LOG(LogMetaSound, Error, TEXT("Attempting to add MetaSound graph output '%s' when output with name already exists"), *InClassOutput.Name.ToString());
		return DocumentCache->GetNodeCache().FindNode(Output->NodeID);
	}
	else if (!IDataTypeRegistry::Get().IsRegistered(InClassOutput.TypeName))
	{
		UE_LOG(LogMetaSound, Error, TEXT("Cannot add MetaSound graph output '%s' with unregistered TypeName '%s'"), *InClassOutput.Name.ToString(), *InClassOutput.TypeName.ToString());
		return nullptr;
	}

	auto FindRegistryClass = [&InClassOutput](FMetasoundFrontendClass& OutClass) -> bool
	{
		switch (InClassOutput.AccessType)
		{
		case EMetasoundFrontendVertexAccessType::Value:
		{
			return IDataTypeRegistry::Get().GetFrontendConstructorOutputClass(InClassOutput.TypeName, OutClass);
		}
		break;

		case EMetasoundFrontendVertexAccessType::Reference:
		{
			return IDataTypeRegistry::Get().GetFrontendOutputClass(InClassOutput.TypeName, OutClass);
		}
		break;

		case EMetasoundFrontendVertexAccessType::Unset:
		default:
		{
			checkNoEntry();
		}
		break;
		}

		return false;
	};

	FMetasoundFrontendClass Class;
	if (FindRegistryClass(Class))
	{
		if (!FindDependency(Class.Metadata))
		{
			AddDependency(Class);
		}

		auto FinalizeNode = [&InClassOutput](FMetasoundFrontendNode& InOutNode, const Metasound::Frontend::FNodeRegistryKey&)
		{
			DocumentBuilderPrivate::SetNodeAndVertexNames(InOutNode, InClassOutput);
		};
		if (FMetasoundFrontendNode* NewNode = AddNodeInternal(Class.Metadata, FinalizeNode, InClassOutput.NodeID))
		{
			const int32 NewIndex = RootGraph.Interface.Outputs.Num();
			FMetasoundFrontendClassOutput& NewOutput = RootGraph.Interface.Outputs.Add_GetRef(InClassOutput);
			if (!NewOutput.VertexID.IsValid())
			{
				NewOutput.VertexID = FDocumentIDGenerator::Get().CreateVertexID(Document);;
			}

			DocumentDelegates->InterfaceDelegates.OnOutputAdded.Broadcast(NewIndex);
#if WITH_EDITORONLY_DATA
			Document.Metadata.ModifyContext.AddMemberIDModified(InClassOutput.NodeID);
#endif // WITH_EDITORONLY_DATA

			return NewNode;
		}
	}

	return nullptr;
}

bool FMetaSoundFrontendDocumentBuilder::AddInterface(FName InterfaceName)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendInterface Interface;
	if (ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceName, Interface))
	{
		if (GetDocument().Interfaces.Contains(Interface.Version))
		{
			UE_LOG(LogMetaSound, VeryVerbose, TEXT("MetaSound interface '%s' already found on document. MetaSoundBuilder skipping add request."), *InterfaceName.ToString());
			return true;
		}

		const FTopLevelAssetPath BuilderClassPath = GetBuilderClassPath();
		const FInterfaceRegistryKey Key = GetInterfaceRegistryKey(Interface.Version);
		if (const IInterfaceRegistryEntry* Entry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(Key))
		{
			const FMetasoundFrontendInterfaceUClassOptions* ClassOptions = Entry->GetInterface().FindClassOptions(BuilderClassPath);
			if (ClassOptions && !ClassOptions->bIsModifiable)
			{
				UE_LOG(LogMetaSound, Error, TEXT("DocumentBuilder failed to add MetaSound Interface '%s' to document: is not set to be modifiable for given UClass '%s'"), *InterfaceName.ToString(), *BuilderClassPath.ToString());
				return false;
			}

			TArray<FMetasoundFrontendInterface> InterfacesToAdd;
			InterfacesToAdd.Add(Entry->GetInterface());
			FModifyInterfaceOptions Options({ }, MoveTemp(InterfacesToAdd));
			return ModifyInterfaces(MoveTemp(Options));
		}
	}

	return false;
}

const FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::AddGraphNode(const FMetasoundFrontendGraphClass& InGraphClass, FGuid InNodeID)
{
	auto FinalizeNode = [](FMetasoundFrontendNode& InOutNode, const Metasound::Frontend::FNodeRegistryKey& ClassKey)
	{
#if WITH_EDITOR
		using namespace Metasound::Frontend;

		// Cache the asset name on the node if it node is reference to asset-defined graph.
		if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
		{
			if (const FSoftObjectPath* Path = AssetManager->FindObjectPathFromKey(ClassKey))
			{
				InOutNode.Name = Path->GetAssetFName();
				return;
			}
		}

		InOutNode.Name = ClassKey.ClassName.GetFullName();
#endif // WITH_EDITOR
	};

	// Dependency is considered "External" when looked up or added on another graph
	FMetasoundFrontendClassMetadata NewClassMetadata = InGraphClass.Metadata;
	NewClassMetadata.SetType(EMetasoundFrontendClassType::External);

	if (!FindDependency(NewClassMetadata))
	{
		AddDependency(InGraphClass);
	}

	return AddNodeInternal(NewClassMetadata, FinalizeNode, InNodeID);
}

const FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::AddNodeByClassName(const FMetasoundFrontendClassName& InClassName, int32 InMajorVersion, FGuid InNodeID)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FMetasoundFrontendClass RegisteredClass;
	if (ISearchEngine::Get().FindClassWithHighestMinorVersion(InClassName, InMajorVersion, RegisteredClass))
	{
		const EMetasoundFrontendClassType ClassType = RegisteredClass.Metadata.GetType();
		if (ClassType != EMetasoundFrontendClassType::External && ClassType != EMetasoundFrontendClassType::Graph)
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Failed to add new node by class name '%s': Class is restricted type '%s' that cannot be added via this function."),
				*InClassName.ToString(),
				LexToString(ClassType));
			return nullptr;
		}

		// Dependency is considered "External" when looked up or added as a dependency to a graph
		RegisteredClass.Metadata.SetType(EMetasoundFrontendClassType::External);

		const FMetasoundFrontendClass* Dependency = FindDependency(RegisteredClass.Metadata);
		if (!Dependency)
		{
			Dependency = AddDependency(RegisteredClass);
		}

		if (Dependency)
		{
			return AddNodeInternal(Dependency->Metadata, [](const FMetasoundFrontendNode& Node, const Metasound::Frontend::FNodeRegistryKey& ClassKey) { return Node.Name; }, InNodeID);
		}
	}

	UE_LOG(LogMetaSound, Warning, TEXT("Failed to add new node by class name '%s' and major version '%d': Class not found"), *InClassName.ToString(), InMajorVersion);
	return nullptr;
}

FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::AddNodeInternal(const FMetasoundFrontendClassMetadata& InClassMetadata, FFinalizeNodeFunctionRef FinalizeNode, FGuid InNodeID)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetaSoundFrontendDocumentBuilder::AddNodeInternal);

	using namespace Metasound::Frontend;

	const FNodeRegistryKey ClassKey = FNodeRegistryKey(InClassMetadata);
	if (const FMetasoundFrontendClass* Dependency = DocumentCache->FindDependency(ClassKey))
	{
		FMetasoundFrontendDocument& Document = GetDocument();
		FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
		TArray<FMetasoundFrontendNode>& Nodes = Graph.Nodes;
		FMetasoundFrontendNode& Node = Nodes.Emplace_GetRef(*Dependency);
		Node.UpdateID(InNodeID);
		FinalizeNode(Node, ClassKey);

		const int32 NewIndex = Nodes.Num() - 1;
		const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
		DocumentDelegates->NodeDelegates.OnNodeAdded.Broadcast(NewIndex);

#if WITH_EDITORONLY_DATA
		Document.Metadata.ModifyContext.AddNodeIDModified(InNodeID);
#endif // WITH_EDITORONLY_DATA
		return &Node;
	}

	return nullptr;
}

bool FMetaSoundFrontendDocumentBuilder::CanAddEdge(const FMetasoundFrontendEdge& InEdge) const
{
	using namespace Metasound::Frontend;

	const FMetasoundFrontendDocument& Document = GetDocument();
	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();

	if (!EdgeCache.IsNodeInputConnected(InEdge.ToNodeID, InEdge.ToVertexID))
	{
		return IsValidEdge(InEdge) == EInvalidEdgeReason::None;
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveUnusedDependencies()
{
	bool bDidEdit = false;

	TArray<FMetasoundFrontendClass>& Dependencies = GetDocument().Dependencies;
	for (int32 i = Dependencies.Num() - 1; i >= 0; --i)
	{
		const FGuid& ClassID = Dependencies[i].ID;
		if (!DocumentCache->GetNodeCache().ContainsNodesOfClassID(ClassID))
		{
			checkf(RemoveDependency(ClassID), TEXT("Failed to remove dependency that was found on document and was not referenced by nodes"));
			bDidEdit = true;
		}
	}

	return bDidEdit;
}

void FMetaSoundFrontendDocumentBuilder::ClearGraph()
{
	FMetasoundFrontendGraphClass& GraphClass = GetDocument().RootGraph;
	GraphClass.Graph.Nodes.Reset();
	GraphClass.Graph.Edges.Reset();
	GraphClass.Interface.Inputs.Reset();
	GraphClass.Interface.Outputs.Reset();
	GraphClass.PresetOptions.InputsInheritingDefault.Reset();
	GetDocument().Interfaces.Reset();
	RemoveUnusedDependencies();
	InitCacheInternal();
}

bool FMetaSoundFrontendDocumentBuilder::ContainsDependencyOfType(EMetasoundFrontendClassType ClassType) const
{
	return DocumentCache->ContainsDependencyOfType(ClassType);
}

bool FMetaSoundFrontendDocumentBuilder::ContainsEdge(const FMetasoundFrontendEdge& InEdge) const
{
	using namespace Metasound::Frontend;
	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();
	return EdgeCache.ContainsEdge(InEdge);
}

bool FMetaSoundFrontendDocumentBuilder::ContainsNode(const FGuid& InNodeID) const
{
	using namespace Metasound::Frontend;
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	return NodeCache.ContainsNode(InNodeID);
}

bool FMetaSoundFrontendDocumentBuilder::ConvertFromPreset()
{
	using namespace Metasound::Frontend;

	if (IsPreset())
	{
		FMetasoundFrontendDocument& Document = GetDocument();
		FMetasoundFrontendGraphClass& RootGraphClass = Document.RootGraph;
		FMetasoundFrontendGraphClassPresetOptions& PresetOptions = RootGraphClass.PresetOptions;
		PresetOptions.bIsPreset = false;

#if WITH_EDITOR
		FMetasoundFrontendGraphStyle& Style = Document.RootGraph.Graph.Style;
		Style.bIsGraphEditable = true;
#endif // WITH_EDITOR

		return true;
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::ConvertToPreset(const FMetasoundFrontendDocument& InReferencedDocument)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	ClearGraph();

	FMetasoundFrontendGraphClass& PresetAssetRootGraph = GetDocument().RootGraph;
	FMetasoundFrontendGraph& PresetAssetGraph = PresetAssetRootGraph.Graph;
	// Mark preset as auto-update and non-editable
#if WITH_EDITORONLY_DATA
	PresetAssetGraph.Style.bIsGraphEditable = false;
#endif // WITH_EDITORONLY_DATA

	// Mark all inputs as inherited by default
	TArray<FName> InputsInheritingDefault;
	Algo::Transform(PresetAssetRootGraph.Interface.Inputs, InputsInheritingDefault, [](const FMetasoundFrontendClassInput& Input)
	{
		return Input.Name;
	});

	PresetAssetRootGraph.PresetOptions.bIsPreset = true;
	PresetAssetRootGraph.PresetOptions.InputsInheritingDefault = TSet<FName>(InputsInheritingDefault);

	// Apply root graph transform 
	FRebuildPresetRootGraph RebuildPresetRootGraph(InReferencedDocument);
	if (RebuildPresetRootGraph.Transform(GetDocument()))
	{
		InitCacheInternal();
		return true;
	}
	return false;
}

bool FMetaSoundFrontendDocumentBuilder::FindDeclaredInterfaces(TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces) const
{
	return FindDeclaredInterfaces(GetDocument(), OutInterfaces);
}

bool FMetaSoundFrontendDocumentBuilder::FindDeclaredInterfaces(const FMetasoundFrontendDocument& InDocument, TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	bool bInterfacesFound = true;

	Algo::Transform(InDocument.Interfaces, OutInterfaces, [&bInterfacesFound](const FMetasoundFrontendVersion& Version)
	{
		const FInterfaceRegistryKey InterfaceKey = GetInterfaceRegistryKey(Version);
		const IInterfaceRegistryEntry* RegistryEntry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(InterfaceKey);
		if (!RegistryEntry)
		{
			bInterfacesFound = false;
			UE_LOG(LogMetaSound, Warning, TEXT("No registered interface matching interface version on document [InterfaceVersion:%s]"), *Version.ToString());
		}

		return RegistryEntry;
	});

	return bInterfacesFound;
}

const FMetasoundFrontendClass* FMetaSoundFrontendDocumentBuilder::FindDependency(const FGuid& InClassID) const
{
	return DocumentCache->FindDependency(InClassID);
}

const FMetasoundFrontendClass* FMetaSoundFrontendDocumentBuilder::FindDependency(const FMetasoundFrontendClassMetadata& InMetadata) const
{
	using namespace Metasound::Frontend;

	checkf(InMetadata.GetType() != EMetasoundFrontendClassType::Graph,
		TEXT("Dependencies are never listed as 'Graph' types. Graphs are considered 'External' from the perspective of the parent document to allow for nativization."));
	const FNodeRegistryKey RegistryKey = FNodeRegistryKey(InMetadata);
	return DocumentCache->FindDependency(RegistryKey);
}

TArray<const FMetasoundFrontendEdge*> FMetaSoundFrontendDocumentBuilder::FindEdges(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();
	return EdgeCache.FindEdges(InNodeID, InVertexID);
}

bool FMetaSoundFrontendDocumentBuilder::FindInterfaceInputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutInputs) const
{
	using namespace Metasound::Frontend;

	OutInputs.Reset();

	FMetasoundFrontendInterface Interface;
	if (ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceName, Interface))
	{
		if (GetDocument().Interfaces.Contains(Interface.Version))
		{
			const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
			const IDocumentGraphInterfaceCache& InterfaceCache = DocumentCache->GetInterfaceCache();

			TArray<const FMetasoundFrontendNode*> InterfaceInputs;
			for (const FMetasoundFrontendClassInput& Input : Interface.Inputs)
			{
				const FMetasoundFrontendClassInput* ClassInput = InterfaceCache.FindInput(Input.Name);
				if (!ClassInput)
				{
					return false;
				}

				if (const FMetasoundFrontendNode* Node = NodeCache.FindNode(ClassInput->NodeID))
				{
					InterfaceInputs.Add(Node);
				}
				else
				{
					return false;
				}
			}

			OutInputs = MoveTemp(InterfaceInputs);
			return true;
		}
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::FindInterfaceOutputNodes(FName InterfaceName, TArray<const FMetasoundFrontendNode*>& OutOutputs) const
{
	using namespace Metasound::Frontend;

	OutOutputs.Reset();

	FMetasoundFrontendInterface Interface;
	if (ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceName, Interface))
	{
		if (GetDocument().Interfaces.Contains(Interface.Version))
		{
			const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
			const IDocumentGraphInterfaceCache& InterfaceCache = DocumentCache->GetInterfaceCache();

			TArray<const FMetasoundFrontendNode*> InterfaceOutputs;
			for (const FMetasoundFrontendClassOutput& Output : Interface.Outputs)
			{
				const FMetasoundFrontendClassOutput* ClassOutput = InterfaceCache.FindOutput(Output.Name);
				if (!ClassOutput)
				{
					return false;
				}

				if (const FMetasoundFrontendNode* Node = NodeCache.FindNode(ClassOutput->NodeID))
				{
					InterfaceOutputs.Add(Node);
				}
				else
				{
					return false;
				}
			}

			OutOutputs = MoveTemp(InterfaceOutputs);
			return true;
		}
	}

	return false;
}

const FMetasoundFrontendClassInput* FMetaSoundFrontendDocumentBuilder::FindGraphInput(FName InputName) const
{
	return DocumentCache->GetInterfaceCache().FindInput(InputName);
}

const FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::FindGraphInputNode(FName InputName) const
{
	using namespace Metasound::Frontend;

	if (const FMetasoundFrontendClassInput* InputClass = FindGraphInput(InputName))
	{
		const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
		return NodeCache.FindNode(InputClass->NodeID);
	}

	return nullptr;
}

const FMetasoundFrontendClassOutput* FMetaSoundFrontendDocumentBuilder::FindGraphOutput(FName OutputName) const
{
	return DocumentCache->GetInterfaceCache().FindOutput(OutputName);
}

const FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::FindGraphOutputNode(FName OutputName) const
{
	using namespace Metasound::Frontend;

	if (const FMetasoundFrontendClassOutput* OutputClass = FindGraphOutput(OutputName))
	{
		const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
		return NodeCache.FindNode(OutputClass->NodeID);
	}

	return nullptr;
}

const FMetasoundFrontendNode* FMetaSoundFrontendDocumentBuilder::FindNode(const FGuid& InNodeID) const
{
	using namespace Metasound::Frontend;
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	return NodeCache.FindNode(InNodeID);
}

bool FMetaSoundFrontendDocumentBuilder::FindNodeClassInterfaces(const FGuid& InNodeID, TSet<FMetasoundFrontendVersion>& OutInterfaces) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	const FMetasoundFrontendDocument& Document = GetDocument();
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const FMetasoundFrontendNode* Node = NodeCache.FindNode(InNodeID))
	{
		if (const FMetasoundFrontendClass* NodeClass = DocumentCache->FindDependency(Node->ClassID))
		{
			const FNodeRegistryKey NodeClassRegistryKey = FNodeRegistryKey(NodeClass->Metadata);
			return FMetasoundFrontendRegistryContainer::Get()->FindImplementedInterfacesFromRegistered(NodeClassRegistryKey, OutInterfaces);
		}
	}

	return false;
}

const FMetasoundFrontendVertex* FMetaSoundFrontendDocumentBuilder::FindNodeInput(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	using namespace Metasound::Frontend;
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	return NodeCache.FindInputVertex(InNodeID, InVertexID);
}

const FMetasoundFrontendVertex* FMetaSoundFrontendDocumentBuilder::FindNodeInput(const FGuid& InNodeID, FName InVertexName) const
{
	using namespace Metasound::Frontend;
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	return NodeCache.FindInputVertex(InNodeID, InVertexName);
}

TArray<const FMetasoundFrontendVertex*> FMetaSoundFrontendDocumentBuilder::FindNodeInputs(const FGuid& InNodeID, FName TypeName) const
{
	return DocumentCache->GetNodeCache().FindNodeInputs(InNodeID, TypeName);
}

TArray<const FMetasoundFrontendVertex*> FMetaSoundFrontendDocumentBuilder::FindNodeInputsConnectedToNodeOutput(const FGuid& InOutputNodeID, const FGuid& InOutputVertexID, TArray<const FMetasoundFrontendNode*>* ConnectedInputNodes) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();

	const FMetasoundFrontendDocument& Document = GetDocument();

	TArray<const FMetasoundFrontendVertex*> Inputs;
	const TArrayView<const int32> Indices = EdgeCache.FindEdgeIndicesFromNodeOutput(InOutputNodeID, InOutputVertexID);
	Algo::Transform(Indices, Inputs, [&Document, &NodeCache, &ConnectedInputNodes](const int32& Index)
	{
		const FMetasoundFrontendEdge& Edge = Document.RootGraph.Graph.Edges[Index];
		if (ConnectedInputNodes)
		{
			ConnectedInputNodes->Add(NodeCache.FindNode(Edge.ToNodeID));
		}
		return NodeCache.FindInputVertex(Edge.ToNodeID, Edge.ToVertexID);
	});
	return Inputs;
}

const FMetasoundFrontendVertex* FMetaSoundFrontendDocumentBuilder::FindNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	using namespace Metasound::Frontend;
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	return NodeCache.FindOutputVertex(InNodeID, InVertexID);
}

const FMetasoundFrontendVertex* FMetaSoundFrontendDocumentBuilder::FindNodeOutput(const FGuid& InNodeID, FName InVertexName) const
{
	using namespace Metasound::Frontend;
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	return NodeCache.FindOutputVertex(InNodeID, InVertexName);
}

TArray<const FMetasoundFrontendVertex*> FMetaSoundFrontendDocumentBuilder::FindNodeOutputs(const FGuid& InNodeID, FName TypeName) const
{
	return DocumentCache->GetNodeCache().FindNodeOutputs(InNodeID, TypeName);
}

const FMetasoundFrontendVertex* FMetaSoundFrontendDocumentBuilder::FindNodeOutputConnectedToNodeInput(const FGuid& InInputNodeID, const FGuid& InInputVertexID, const FMetasoundFrontendNode** ConnectedOutputNode) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();
	if (const int32* Index = EdgeCache.FindEdgeIndexToNodeInput(InInputNodeID, InInputVertexID))
	{
		const FMetasoundFrontendDocument& Document = GetDocument();
		const FMetasoundFrontendEdge& Edge = Document.RootGraph.Graph.Edges[*Index];
		const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
		if (ConnectedOutputNode)
		{
			(*ConnectedOutputNode) = NodeCache.FindNode(Edge.FromNodeID);
		}
		return NodeCache.FindOutputVertex(Edge.FromNodeID, Edge.FromVertexID);
	}

	if (ConnectedOutputNode)
	{
		*ConnectedOutputNode = nullptr;
	}
	return nullptr;
}

const FTopLevelAssetPath FMetaSoundFrontendDocumentBuilder::GetBuilderClassPath() const
{
	IMetaSoundDocumentInterface* Interface = DocumentInterface.GetInterface();
	checkf(Interface, TEXT("Failed to return class path; interface must always be valid while builder is operating on MetaSound UObject!"));
	return Interface->GetBaseMetaSoundUClass().GetClassPathName();
}

const FString FMetaSoundFrontendDocumentBuilder::GetDebugName() const
{
	using namespace Metasound::Frontend;

	return DocumentBuilderPrivate::GetDebugName(GetDocumentInterface());
}

FMetasoundFrontendDocument& FMetaSoundFrontendDocumentBuilder::GetDocument()
{
	IMetaSoundDocumentInterface* Interface = DocumentInterface.GetInterface();
	checkf(Interface, TEXT("Failed to return document; interface must always be valid while builder is operating on MetaSound UObject!"));
	return Interface->GetDocument();
}

const FMetasoundFrontendDocument& FMetaSoundFrontendDocumentBuilder::GetDocument() const
{
	return DocumentInterface->GetDocument();
}

const Metasound::Frontend::FDocumentModifyDelegates& FMetaSoundFrontendDocumentBuilder::GetDocumentDelegates() const
{
	return *DocumentDelegates;
}

const IMetaSoundDocumentInterface& FMetaSoundFrontendDocumentBuilder::GetDocumentInterface() const
{
	IMetaSoundDocumentInterface* Interface = DocumentInterface.GetInterface();
	check(Interface);
	return *Interface;
}

EMetasoundFrontendVertexAccessType FMetaSoundFrontendDocumentBuilder::GetNodeInputAccessType(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const int32* NodeIndex = NodeCache.FindNodeIndex(InNodeID))
	{
		const FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
		const FMetasoundFrontendNode& Node = Graph.Nodes[*NodeIndex];
		auto IsVertexID = [&InVertexID](const FMetasoundFrontendVertex& Vertex) { return Vertex.VertexID == InVertexID; };
		if (const FMetasoundFrontendClass* Class = DocumentCache->FindDependency(Node.ClassID))
		{
			const EMetasoundFrontendClassType ClassType = Class->Metadata.GetType();
			switch (ClassType)
			{
				case EMetasoundFrontendClassType::Template:
				{
					const FNodeRegistryKey Key = FNodeRegistryKey(Class->Metadata);
					const INodeTemplate* Template = INodeTemplateRegistry::Get().FindTemplate(Key);
					if (ensureMsgf(Template, TEXT("Failed to find MetaSound node template registered with key '%s'"), *Key.ToString()))
					{
						if (Template->IsInputAccessTypeDynamic())
						{
							return Template->GetNodeInputAccessType(*this, InNodeID, InVertexID);
						}
					}
				}
				break;

				case EMetasoundFrontendClassType::Output:
				{
					const FMetasoundFrontendClassInput& ClassInput = Class->Interface.Inputs.Last();
					return ClassInput.AccessType;
				}

				default:
				break;
			}
			static_assert(static_cast<uint32>(EMetasoundFrontendClassType::Invalid) == 10, "Potential missing case coverage for EMetasoundFrontendClassType");

			if (const FMetasoundFrontendVertex* Vertex = Node.Interface.Inputs.FindByPredicate(IsVertexID))
			{
				auto IsClassInput = [VertexName = Vertex->Name](const FMetasoundFrontendClassInput& Input) { return Input.Name == VertexName; };
				if (const FMetasoundFrontendClassInput* ClassInput = Class->Interface.Inputs.FindByPredicate(IsClassInput))
				{
					return ClassInput->AccessType;
				}
			}
		}
	}

	return EMetasoundFrontendVertexAccessType::Unset;
}

const FMetasoundFrontendLiteral* FMetaSoundFrontendDocumentBuilder::GetNodeInputClassDefault(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const int32* NodeIndex = NodeCache.FindNodeIndex(InNodeID))
	{
		const FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
		const FMetasoundFrontendNode& Node = Graph.Nodes[*NodeIndex];
		auto IsVertexID = [&InVertexID](const FMetasoundFrontendVertex& Vertex) { return Vertex.VertexID == InVertexID; };
		if (const FMetasoundFrontendVertex* Vertex = Node.Interface.Inputs.FindByPredicate(IsVertexID))
		{
			if (const FMetasoundFrontendClass* Class = DocumentCache->FindDependency(Node.ClassID))
			{
				const EMetasoundFrontendClassType ClassType = Class->Metadata.GetType();
				switch (ClassType)
				{
					case EMetasoundFrontendClassType::Output:
					{
						const FMetasoundFrontendClassInput& ClassInput = Class->Interface.Inputs.Last();
						return &ClassInput.DefaultLiteral;
					}
					break;

					default:
					{
						auto IsClassInput = [VertexName = Vertex->Name](const FMetasoundFrontendClassInput& Input) { return Input.Name == VertexName; };
						if (const FMetasoundFrontendClassInput* ClassInput = Class->Interface.Inputs.FindByPredicate(IsClassInput))
						{
							return &ClassInput->DefaultLiteral;
						}
						static_assert(static_cast<uint32>(EMetasoundFrontendClassType::Invalid) == 10, "Potential missing case coverage for EMetasoundFrontendClassType "
							"(default may not be sufficient for newly added class types)");
					}
					break;
				}
			}
		}
	}

	return nullptr;
}

const FMetasoundFrontendLiteral* FMetaSoundFrontendDocumentBuilder::GetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const int32* NodeIndex = NodeCache.FindNodeIndex(InNodeID))
	{
		const FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
		const FMetasoundFrontendNode& Node = Graph.Nodes[*NodeIndex];

		auto IsVertex = [&InVertexID](const FMetasoundFrontendVertex& Vertex) { return Vertex.VertexID == InVertexID; };
		const int32 VertexIndex = Node.Interface.Inputs.IndexOfByPredicate(IsVertex);
		if (VertexIndex != INDEX_NONE)
		{
			const FMetasoundFrontendVertex& NodeInput = Node.Interface.Inputs[VertexIndex];

			// If default not found on node, check class definition
			auto IsLiteral = [&InVertexID](const FMetasoundFrontendVertexLiteral& Literal) { return Literal.VertexID == InVertexID; };
			const int32 LiteralIndex = Node.InputLiterals.IndexOfByPredicate(IsLiteral);
			if (LiteralIndex != INDEX_NONE)
			{
				return &Node.InputLiterals[LiteralIndex].Value;
			}
		}
	}

	return nullptr;
}

EMetasoundFrontendVertexAccessType FMetaSoundFrontendDocumentBuilder::GetNodeOutputAccessType(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const int32* NodeIndex = NodeCache.FindNodeIndex(InNodeID))
	{
		const FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
		const FMetasoundFrontendNode& Node = Graph.Nodes[*NodeIndex];
		if (const FMetasoundFrontendClass* Class = DocumentCache->FindDependency(Node.ClassID))
		{
			const EMetasoundFrontendClassType ClassType = Class->Metadata.GetType();
			switch (ClassType)
			{
				case EMetasoundFrontendClassType::Template:
				{
					const FNodeRegistryKey Key = FNodeRegistryKey(Class->Metadata);
					const INodeTemplate* Template = INodeTemplateRegistry::Get().FindTemplate(Key);
					if (ensureMsgf(Template, TEXT("Failed to find MetaSound node template registered with key '%s'"), *Key.ToString()))
					{
						if (Template->IsOutputAccessTypeDynamic())
						{
							return Template->GetNodeOutputAccessType(*this, InNodeID, InVertexID);
						}
					}
				}
				break;

				case EMetasoundFrontendClassType::Input:
				{
					const FMetasoundFrontendClassOutput& ClassOutput = Class->Interface.Outputs.Last();
					return ClassOutput.AccessType;
				}

				default:
				break;
			}
			static_assert(static_cast<uint32>(EMetasoundFrontendClassType::Invalid) == 10, "Potential missing case coverage for EMetasoundFrontendClassType");

			auto IsVertexID = [&InVertexID](const FMetasoundFrontendVertex& Vertex) { return Vertex.VertexID == InVertexID; };
			if (const FMetasoundFrontendVertex* Vertex = Node.Interface.Outputs.FindByPredicate(IsVertexID))
			{
				auto IsClassInput = [VertexName = Vertex->Name](const FMetasoundFrontendClassInput& Output) { return Output.Name == VertexName; };
				if (const FMetasoundFrontendClassOutput* ClassOutput = Class->Interface.Outputs.FindByPredicate(IsClassInput))
				{
					return ClassOutput->AccessType;
				}
			}
		}
	}

	return EMetasoundFrontendVertexAccessType::Unset;
}

int32 FMetaSoundFrontendDocumentBuilder::GetTransactionCount() const
{
	using namespace Metasound::Frontend;

	if (DocumentCache.IsValid())
	{
		return StaticCastSharedPtr<FDocumentCache>(DocumentCache)->GetTransactionCount();
	}

	return 0;
}

void FMetaSoundFrontendDocumentBuilder::InitGraphClassMetadata(FMetasoundFrontendClassMetadata& InOutMetadata, bool bResetVersion, const FMetasoundFrontendClassName* NewClassName)
{
	if (NewClassName)
	{
		InOutMetadata.SetClassName(*NewClassName);
	}
	else
	{
		InOutMetadata.SetClassName(FMetasoundFrontendClassName(FName(), *FGuid::NewGuid().ToString(), FName()));
	}

	if (bResetVersion)
	{
		InOutMetadata.SetVersion({ 1, 0 });
	}

	InOutMetadata.SetType(EMetasoundFrontendClassType::Graph);
}

void FMetaSoundFrontendDocumentBuilder::InitDocument()
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetaSoundFrontendDocumentBuilder::InitDocument);

	FMetasoundFrontendDocument& Document = GetDocument();

	// 1. Set default class Metadata
	{
		constexpr bool bResetVersion = true;
		FMetasoundFrontendClassMetadata& ClassMetadata = Document.RootGraph.Metadata;
		InitGraphClassMetadata(ClassMetadata, bResetVersion);
	}

	// 2. Set default doc version Metadata
	{
		FMetasoundFrontendDocumentMetadata& DocMetadata = Document.Metadata;
		DocMetadata.Version.Number = FMetasoundFrontendDocument::GetMaxVersion();
	}

	// 3. Add default interfaces for given UClass
	{
		TArray<FMetasoundFrontendVersion> InitVersions = ISearchEngine::Get().FindUClassDefaultInterfaceVersions(GetBuilderClassPath());
		FModifyInterfaceOptions Options({ }, InitVersions);
		ModifyInterfaces(MoveTemp(Options));
	}
}

void FMetaSoundFrontendDocumentBuilder::InitNodeLocations()
{
#if WITH_EDITORONLY_DATA
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;

	FVector2D InputNodeLocation = FVector2D::ZeroVector;
	FVector2D ExternalNodeLocation = InputNodeLocation + DisplayStyle::NodeLayout::DefaultOffsetX;
	FVector2D OutputNodeLocation = ExternalNodeLocation + DisplayStyle::NodeLayout::DefaultOffsetX;

	TArray<FMetasoundFrontendNode>& Nodes = Graph.Nodes;
	for (FMetasoundFrontendNode& Node : Nodes)
	{
		if (const int32* ClassIndex = DocumentCache->FindDependencyIndex(Node.ClassID))
		{
			FMetasoundFrontendClass& Class = GetDocument().Dependencies[*ClassIndex];

			const EMetasoundFrontendClassType NodeType = Class.Metadata.GetType();
			FVector2D NewLocation;
			if (NodeType == EMetasoundFrontendClassType::Input)
			{
				NewLocation = InputNodeLocation;
				InputNodeLocation += DisplayStyle::NodeLayout::DefaultOffsetY;
			}
			else if (NodeType == EMetasoundFrontendClassType::Output)
			{
				NewLocation = OutputNodeLocation;
				OutputNodeLocation += DisplayStyle::NodeLayout::DefaultOffsetY;
			}
			else
			{
				NewLocation = ExternalNodeLocation;
				ExternalNodeLocation += DisplayStyle::NodeLayout::DefaultOffsetY;
			}

			// TODO: Find consistent location for controlling node locations.
			// Currently it is split between MetasoundEditor and MetasoundFrontend modules.
			FMetasoundFrontendNodeStyle& Style = Node.Style;
			if (Style.Display.Locations.IsEmpty())
			{
				Style.Display.Locations = { { FGuid::NewGuid(), NewLocation } };
			}
			// Initialize the position if the location hasn't been assigned yet.  This can happen
			// if default interfaces were assigned to the given MetaSound but not placed with respect
			// to one another.  In this case, node location initialization takes "priority" to avoid
			// visual overlap.
			else if (Style.Display.Locations.Num() == 1 && Style.Display.Locations.Contains(FGuid()))
			{
				Style.Display.Locations = { { FGuid::NewGuid(), NewLocation } };
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void FMetaSoundFrontendDocumentBuilder::InvalidateCache()
{
	constexpr bool bPrimeCache = false;
	InitCacheInternal(bPrimeCache);
}

bool FMetaSoundFrontendDocumentBuilder::IsDependencyReferenced(const FGuid& InClassID) const
{
	return DocumentCache->GetNodeCache().ContainsNodesOfClassID(InClassID);
}

bool FMetaSoundFrontendDocumentBuilder::IsNodeInputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	return DocumentCache->GetEdgeCache().IsNodeInputConnected(InNodeID, InVertexID);
}

bool FMetaSoundFrontendDocumentBuilder::IsNodeOutputConnected(const FGuid& InNodeID, const FGuid& InVertexID) const
{
	return DocumentCache->GetEdgeCache().IsNodeOutputConnected(InNodeID, InVertexID);
}

bool FMetaSoundFrontendDocumentBuilder::IsInterfaceDeclared(FName InInterfaceName) const
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendInterface Interface;
	if (ISearchEngine::Get().FindInterfaceWithHighestVersion(InInterfaceName, Interface))
	{
		return IsInterfaceDeclared(Interface.Version);
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::IsInterfaceDeclared(const FMetasoundFrontendVersion& InInterfaceVersion) const
{
	return GetDocument().Interfaces.Contains(InInterfaceVersion);
}

bool FMetaSoundFrontendDocumentBuilder::IsPreset() const
{
	return GetDocument().RootGraph.PresetOptions.bIsPreset;
}

Metasound::Frontend::EInvalidEdgeReason FMetaSoundFrontendDocumentBuilder::IsValidEdge(const FMetasoundFrontendEdge& InEdge) const
{
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();

	const FMetasoundFrontendVertex* OutputVertex = NodeCache.FindOutputVertex(InEdge.FromNodeID, InEdge.FromVertexID);
	if (!OutputVertex)
	{
		return EInvalidEdgeReason::MissingOutput;
	}

	const FMetasoundFrontendVertex* InputVertex = NodeCache.FindInputVertex(InEdge.ToNodeID, InEdge.ToVertexID);
	if (!InputVertex)
	{
		return EInvalidEdgeReason::MissingInput;
	}

	if (OutputVertex->TypeName != InputVertex->TypeName)
	{
		return EInvalidEdgeReason::MismatchedDataType;
	}

	// TODO: Add cycle detection here

	const EMetasoundFrontendVertexAccessType OutputAccessType = GetNodeOutputAccessType(InEdge.FromNodeID, InEdge.FromVertexID);
	const EMetasoundFrontendVertexAccessType InputAccessType = GetNodeInputAccessType(InEdge.ToNodeID, InEdge.ToVertexID);
	if (!FMetasoundFrontendClassVertex::CanConnectVertexAccessTypes(OutputAccessType, InputAccessType))
	{
		return EInvalidEdgeReason::MismatchedAccessType;
	}

	return EInvalidEdgeReason::None;
}

bool FMetaSoundFrontendDocumentBuilder::ModifyInterfaces(Metasound::Frontend::FModifyInterfaceOptions&& InOptions)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Doc = GetDocument();
	DocumentBuilderPrivate::FModifyInterfacesImpl Context(Doc, MoveTemp(InOptions));
	return Context.Execute(*this, *DocumentDelegates);
}

bool FMetaSoundFrontendDocumentBuilder::TransformTemplateNodes()
{
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetaSoundFrontendDocumentBuilder::TransformTemplateNodes);

	struct FTemplateTransformParams
	{
		const Metasound::Frontend::INodeTemplate* Template;
		TArray<FGuid> NodeIDs;
	};
	using FTemplateTransformParamsMap = TSortedMap<FGuid, FTemplateTransformParams>;

	FMetasoundFrontendDocument& Document = GetDocument();
	FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
	TArray<FMetasoundFrontendClass>& Dependencies = Document.Dependencies;

	FTemplateTransformParamsMap TemplateParams;
	for (const FMetasoundFrontendClass& Dependency : Dependencies)
	{
		if (Dependency.Metadata.GetType() == EMetasoundFrontendClassType::Template)
		{
			const FNodeRegistryKey Key = FNodeRegistryKey(Dependency.Metadata);
			const INodeTemplate* Template = INodeTemplateRegistry::Get().FindTemplate(Key);
			ensureMsgf(Template, TEXT("Template not found for template class reference '%s'"), *Dependency.Metadata.GetClassName().ToString());
			TemplateParams.Add(Dependency.ID, FTemplateTransformParams { Template });
		}
	}

	if (TemplateParams.IsEmpty())
	{
		return false;
	}

	// 1. Execute generated template node transform on copy of node array,
	// which allows for addition/removal of nodes to/from original array container
	// without template transform having to worry about mutation while iterating
	TArray<FGuid> TemplateNodeIDs;
	for (const FMetasoundFrontendNode& Node : Graph.Nodes)
	{
		if (FTemplateTransformParams* Params = TemplateParams.Find(Node.ClassID))
		{
			Params->NodeIDs.Add(Node.GetID());
		}
	}

	// 2. Transform nodes
	bool bModified = false;
	for (const TPair<FGuid, FTemplateTransformParams>& Pair : TemplateParams)
	{
		const FTemplateTransformParams& Params = Pair.Value;
		if (Params.Template)
		{
			TUniquePtr<INodeTransform> NodeTransform = Params.Template->GenerateNodeTransform();
			check(NodeTransform.IsValid());

			for (const FGuid& NodeID : Params.NodeIDs)
			{
				bModified = true;
				NodeTransform->Transform(NodeID, *this);
			}
		}
	}

	// 3. Remove template classes from dependency list
	for (int32 i = Dependencies.Num() - 1; i >= 0; --i)
	{
		const FMetasoundFrontendClass& Class = Dependencies[i];
		if (TemplateParams.Contains(Class.ID))
		{
			DocumentDelegates->OnRemoveSwappingDependency.Broadcast(i, Dependencies.Num() - 1);
			Dependencies.RemoveAtSwap(i, 1, EAllowShrinking::No);
		}
	}
	Dependencies.Shrink();

	return bModified;
}

void FMetaSoundFrontendDocumentBuilder::InitCacheInternal(bool bPrimeCache)
{
	using namespace Metasound::Frontend;
	DocumentCache = FDocumentCache::Create(GetDocument(), DocumentDelegates, bPrimeCache);
}

void FMetaSoundFrontendDocumentBuilder::BeginBuilding()
{
	if (DocumentInterface)
	{
		DocumentInterface->OnBeginActiveBuilder();
	}
}

void FMetaSoundFrontendDocumentBuilder::FinishBuilding()
{
	if (DocumentInterface)
	{
		DocumentInterface->OnFinishActiveBuilder();
	}
	DocumentInterface = TScriptInterface<IMetaSoundDocumentInterface>();
}

bool FMetaSoundFrontendDocumentBuilder::RemoveDependency(const FGuid& InClassID)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Document = GetDocument();
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();

	if (const int32* IndexPtr = DocumentCache->FindDependencyIndex(InClassID))
	{
		TArray<FMetasoundFrontendClass>& Dependencies = Document.Dependencies;
		const int32 Index = *IndexPtr;

		TArray<const FMetasoundFrontendNode*> Nodes = NodeCache.FindNodesOfClassID(InClassID);
		for (const FMetasoundFrontendNode* Node : Nodes)
		{
			if (!RemoveNode(Node->GetID()))
			{
				return false;
			}
		}

		const int32 LastIndex = Dependencies.Num() - 1;
		DocumentDelegates->OnRemoveSwappingDependency.Broadcast(Index, LastIndex);
		Dependencies.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}

	return true;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveDependency(EMetasoundFrontendClassType ClassType, const FMetasoundFrontendClassName& InClassName, const FMetasoundFrontendVersionNumber& InClassVersionNumber)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Document = GetDocument();
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();

	const FNodeRegistryKey ClassKey(ClassType, InClassName, InClassVersionNumber);
	if (const int32* IndexPtr = DocumentCache->FindDependencyIndex(ClassKey))
	{
		TArray<FMetasoundFrontendClass>& Dependencies = Document.Dependencies;
		const int32 Index = *IndexPtr;

		TArray<const FMetasoundFrontendNode*> Nodes = NodeCache.FindNodesOfClassID(Dependencies[Index].ID);
		for (const FMetasoundFrontendNode* Node : Nodes)
		{
			if (!RemoveNode(Node->GetID()))
			{
				return false;
			}
		}

		const int32 LastIndex = Dependencies.Num() - 1;
		DocumentDelegates->OnRemoveSwappingDependency.Broadcast(Index, LastIndex);
		Dependencies.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}

	return true;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveEdge(const FMetasoundFrontendEdge& EdgeToRemove)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Document = GetDocument();
	FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
	TArray<FMetasoundFrontendEdge>& Edges = Graph.Edges;
	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();
	if (const int32* IndexPtr = EdgeCache.FindEdgeIndexToNodeInput(EdgeToRemove.ToNodeID, EdgeToRemove.ToVertexID))
	{
		const int32 Index = *IndexPtr;
		const int32 LastIndex = Edges.Num() - 1;
		DocumentDelegates->EdgeDelegates.OnRemoveSwappingEdge.Broadcast(Index, LastIndex);
		Edges.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveNamedEdges(const TSet<Metasound::Frontend::FNamedEdge>& InNamedEdgesToRemove, TArray<FMetasoundFrontendEdge>* OutRemovedEdges)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument& Document = GetDocument();
	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();

	if (OutRemovedEdges)
	{
		OutRemovedEdges->Reset();
	}

	bool bSuccess = true;

	TArray<FMetasoundFrontendEdge> EdgesToRemove;
	for (const FNamedEdge& NamedEdge : InNamedEdgesToRemove)
	{
		const FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
		const FMetasoundFrontendVertex* OutputVertex = NodeCache.FindOutputVertex(NamedEdge.OutputNodeID, NamedEdge.OutputName);
		const FMetasoundFrontendVertex* InputVertex = NodeCache.FindInputVertex(NamedEdge.InputNodeID, NamedEdge.InputName);

		if (OutputVertex && InputVertex)
		{
			FMetasoundFrontendEdge NewEdge = { NamedEdge.OutputNodeID, OutputVertex->VertexID, NamedEdge.InputNodeID, InputVertex->VertexID };
			if (ContainsEdge(NewEdge))
			{
				EdgesToRemove.Add(MoveTemp(NewEdge));
			}
			else
			{
				bSuccess = false;
				UE_LOG(LogMetaSound, Warning, TEXT("Failed to remove connection between MetaSound node output '%s' and input '%s': No connection found."), *NamedEdge.OutputName.ToString(), *NamedEdge.InputName.ToString());
			}
		}
	}

	for (const FMetasoundFrontendEdge& EdgeToRemove : EdgesToRemove)
	{
		const bool bRemovedEdge = RemoveEdgeToNodeInput(EdgeToRemove.ToNodeID, EdgeToRemove.ToVertexID);
		if (ensureAlwaysMsgf(bRemovedEdge, TEXT("Failed to remove MetaSound graph edge via DocumentBuilder when prior step validated edge remove was valid")))
		{
			if (OutRemovedEdges)
			{
				OutRemovedEdges->Add(EdgeToRemove);
			}
		}
		else
		{
			bSuccess = false;
		}
	}

	return bSuccess;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID)
{
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const int32* NodeIndex = NodeCache.FindNodeIndex(InNodeID))
	{
		FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
		FMetasoundFrontendNode& Node = Graph.Nodes[*NodeIndex];

		auto IsVertex = [&InVertexID](const FMetasoundFrontendVertex& Vertex) { return Vertex.VertexID == InVertexID; };
		const int32 VertexIndex = Node.Interface.Inputs.IndexOfByPredicate(IsVertex);
		if (VertexIndex != INDEX_NONE)
		{
			auto IsLiteral = [&InVertexID](const FMetasoundFrontendVertexLiteral& Literal) { return Literal.VertexID == InVertexID; };
			const int32 LiteralIndex = Node.InputLiterals.IndexOfByPredicate(IsLiteral);
			if (LiteralIndex != INDEX_NONE)
			{
				const FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray& OnRemovingNodeInputLiteral = DocumentDelegates->NodeDelegates.OnRemovingNodeInputLiteral;
				const int32 LastIndex = Node.InputLiterals.Num() - 1;
				OnRemovingNodeInputLiteral.Broadcast(*NodeIndex, VertexIndex, LastIndex);
				if (LiteralIndex != LastIndex)
				{
					OnRemovingNodeInputLiteral.Broadcast(*NodeIndex, VertexIndex, LiteralIndex);
				}

				Node.InputLiterals.RemoveAtSwap(LiteralIndex, 1, EAllowShrinking::No);
				if (LiteralIndex != LastIndex)
				{
					const FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray& OnNodeInputLiteralSet = DocumentDelegates->NodeDelegates.OnNodeInputLiteralSet;
					OnNodeInputLiteralSet.Broadcast(*NodeIndex, VertexIndex, LiteralIndex);
				}
				return true;
			}
		}
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveEdgesByNodeClassInterfaceBindings(const FGuid& InFromNodeID, const FGuid& InToNodeID)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	TSet<FMetasoundFrontendVersion> FromInterfaceVersions;
	TSet<FMetasoundFrontendVersion> ToInterfaceVersions;

	if (FindNodeClassInterfaces(InFromNodeID, FromInterfaceVersions) && FindNodeClassInterfaces(InToNodeID, ToInterfaceVersions))
	{
		TSet<FNamedEdge> NamedEdges;
		if (DocumentBuilderPrivate::TryGetInterfaceBoundEdges(InFromNodeID, FromInterfaceVersions, InToNodeID, ToInterfaceVersions, NamedEdges))
		{
			return RemoveNamedEdges(NamedEdges);
		}
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveEdgesFromNodeOutput(const FGuid& InNodeID, const FGuid& InVertexID)
{
	using namespace Metasound::Frontend;

	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();
	const TArrayView<const int32> Indices = EdgeCache.FindEdgeIndicesFromNodeOutput(InNodeID, InVertexID);
	if (!Indices.IsEmpty())
	{
		FMetasoundFrontendDocument& Document = GetDocument();
		FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;

		// Copy off indices and sort descending as the edge array will be modified when notifying the cache in the loop below
		TArray<int32> IndicesCopy(Indices.GetData(), Indices.Num());
		Algo::Sort(IndicesCopy, [](const int32& L, const int32& R) { return L > R; });
		for (int32 Index : IndicesCopy)
		{
			const int32 LastIndex = Graph.Edges.Num() - 1;
			DocumentDelegates->EdgeDelegates.OnRemoveSwappingEdge.Broadcast(Index, LastIndex);
			Graph.Edges.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}

		return true;
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveEdgeToNodeInput(const FGuid& InNodeID, const FGuid& InVertexID)
{
	using namespace Metasound::Frontend;

	const IDocumentGraphEdgeCache& EdgeCache = DocumentCache->GetEdgeCache();
	if (const int32* IndexPtr = EdgeCache.FindEdgeIndexToNodeInput(InNodeID, InVertexID))
	{
		FMetasoundFrontendDocument& Document = GetDocument();
		FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
		const int32 Index = *IndexPtr; // Copy off indices as the pointer may be modified when notifying the cache below
		const int32 LastIndex = Graph.Edges.Num() - 1;
		DocumentDelegates->EdgeDelegates.OnRemoveSwappingEdge.Broadcast(Index, LastIndex);
		Graph.Edges.RemoveAtSwap(Index, 1, EAllowShrinking::No);

		return true;
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveGraphInput(FName InInputName)
{
	if (const FMetasoundFrontendNode* Node = FindGraphInputNode(InInputName))
	{
		const FGuid ClassID = Node->ClassID;
		if (RemoveNode(Node->GetID()))
		{
			TArray<FMetasoundFrontendClassInput>& Inputs = GetDocument().RootGraph.Interface.Inputs;
			auto InputNameMatches = [InInputName](const FMetasoundFrontendClassInput& Input) { return Input.Name == InInputName; };
			const int32 Index = Inputs.IndexOfByPredicate(InputNameMatches);
			if (Index != INDEX_NONE)
			{
				DocumentDelegates->InterfaceDelegates.OnRemovingInput.Broadcast(Index);

				const int32 LastIndex = Inputs.Num() - 1;
				if (Index != LastIndex)
				{
					DocumentDelegates->InterfaceDelegates.OnRemovingInput.Broadcast(LastIndex);
				}
				Inputs.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				if (Index != LastIndex)
				{
					DocumentDelegates->InterfaceDelegates.OnInputAdded.Broadcast(Index);
				}

				if (IsDependencyReferenced(ClassID))
				{
					return true;
				}
				else
				{
					if (RemoveDependency(ClassID))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveGraphOutput(FName InOutputName)
{
	if (const FMetasoundFrontendNode* Node = FindGraphOutputNode(InOutputName))
	{
		const FGuid ClassID = Node->ClassID;
		if (RemoveNode(Node->GetID()))
		{
			TArray<FMetasoundFrontendClassOutput>& Outputs = GetDocument().RootGraph.Interface.Outputs;
			auto OutputNameMatches = [InOutputName](const FMetasoundFrontendClassOutput& Output) { return Output.Name == InOutputName; };
			const int32 Index = Outputs.IndexOfByPredicate(OutputNameMatches);
			if (Index != INDEX_NONE)
			{
				DocumentDelegates->InterfaceDelegates.OnRemovingOutput.Broadcast(Index);

				const int32 LastIndex = Outputs.Num() - 1;
				if (Index != LastIndex)
				{
					DocumentDelegates->InterfaceDelegates.OnRemovingOutput.Broadcast(LastIndex);
				}
				Outputs.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				if (Index != LastIndex)
				{
					DocumentDelegates->InterfaceDelegates.OnOutputAdded.Broadcast(Index);
				}

				if (IsDependencyReferenced(ClassID))
				{
					return true;
				}
				else
				{
					if (RemoveDependency(ClassID))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveInterface(FName InterfaceName)
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendInterface Interface;
	if (ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceName, Interface))
	{
		if (!GetDocument().Interfaces.Contains(Interface.Version))
		{
			UE_LOG(LogMetaSound, VeryVerbose, TEXT("MetaSound interface '%s' not found on document. MetaSoundBuilder skipping remove request."), *InterfaceName.ToString());
			return true;
		}

		const FTopLevelAssetPath BuilderClassPath = GetBuilderClassPath();
		const FInterfaceRegistryKey Key = GetInterfaceRegistryKey(Interface.Version);
		if (const IInterfaceRegistryEntry* Entry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(Key))
		{
			const FMetasoundFrontendInterfaceUClassOptions* ClassOptions = Entry->GetInterface().FindClassOptions(BuilderClassPath);
			if (ClassOptions && !ClassOptions->bIsModifiable)
			{
				UE_LOG(LogMetaSound, Error, TEXT("DocumentBuilder failed to remove MetaSound Interface '%s' to document: is not set to be modifiable for given UClass '%s'"), *InterfaceName.ToString(), *BuilderClassPath.ToString());
				return false;
			}

			TArray<FMetasoundFrontendInterface> InterfacesToRemove;
			InterfacesToRemove.Add(Entry->GetInterface());
			FModifyInterfaceOptions Options(MoveTemp(InterfacesToRemove), { });
			return ModifyInterfaces(MoveTemp(Options));
		}
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RemoveNode(const FGuid& InNodeID)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetaSoundFrontendDocumentBuilder::RemoveNode);

	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();

	if (const int32* IndexPtr = NodeCache.FindNodeIndex(InNodeID))
	{
		const int32 Index = *IndexPtr; // Copy off indices as the pointer may be modified when notifying the cache below

		FMetasoundFrontendDocument& Document = GetDocument();
		FMetasoundFrontendGraph& Graph = Document.RootGraph.Graph;
		TArray<FMetasoundFrontendNode>& Nodes = Graph.Nodes;
		const FMetasoundFrontendNode& Node = Nodes[Index];

		for (const FMetasoundFrontendVertex& Vertex : Node.Interface.Inputs)
		{
			RemoveEdgeToNodeInput(InNodeID, Vertex.VertexID);
		}

		for (const FMetasoundFrontendVertex& Vertex : Node.Interface.Outputs)
		{
			RemoveEdgesFromNodeOutput(InNodeID, Vertex.VertexID);
		}

		const int32 LastIndex = Nodes.Num() - 1;
		DocumentDelegates->NodeDelegates.OnRemoveSwappingNode.Broadcast(Index, LastIndex);
		Nodes.RemoveAtSwap(Index, 1, EAllowShrinking::No);

#if WITH_EDITORONLY_DATA
		Document.Metadata.ModifyContext.AddNodeIDModified(InNodeID);
#endif // WITH_EDITORONLY_DATA

		return true;
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::RenameRootGraphClass(const FMetasoundFrontendClassName& InName)
{
	FGuid NameGuid;
	if (FGuid::Parse(InName.Name.ToString(), NameGuid))
	{
		return Metasound::Frontend::FRenameRootGraphClass::Generate(GetDocument(), NameGuid, InName.Namespace, InName.Variant);
	}
	else
	{
		UE_LOG(LogMetaSound, Error, TEXT("Attempting to rename a root graph class with class name '%s' which contains an invalid name guid."), *InName.GetFullName().ToString());
		return false;
	}
}

void FMetaSoundFrontendDocumentBuilder::ReloadCache()
{
	constexpr bool bPrimeCache = true;
	InitCacheInternal(bPrimeCache);
}

#if WITH_EDITOR
void FMetaSoundFrontendDocumentBuilder::SetAuthor(const FString& InAuthor)
{
	FMetasoundFrontendClassMetadata& ClassMetadata = GetDocument().RootGraph.Metadata;
	ClassMetadata.SetAuthor(InAuthor);
}
#endif // WITH_EDITOR

bool FMetaSoundFrontendDocumentBuilder::SetGraphInputDefault(FName InputName, const FMetasoundFrontendLiteral& InDefaultLiteral)
{
	using namespace Metasound::Frontend;

	auto NameMatchesInput = [&InputName](const FMetasoundFrontendClassInput& Input) { return Input.Name == InputName; };
	TArray<FMetasoundFrontendClassInput>& Inputs = GetDocument().RootGraph.Interface.Inputs;

	const int32 Index = Inputs.IndexOfByPredicate(NameMatchesInput);
	if (Index != INDEX_NONE)
	{
		FMetasoundFrontendClassInput& Input = Inputs[Index];
		if (IDataTypeRegistry::Get().IsLiteralTypeSupported(Input.TypeName, InDefaultLiteral.GetType()))
		{
			Input.DefaultLiteral = InDefaultLiteral;
			DocumentDelegates->InterfaceDelegates.OnInputDefaultChanged.Broadcast(Index);

			// Set the input as no longer inheriting default for presets
			if (IsPreset())
			{
				constexpr bool bInputInheritsDefault = false;
				return SetGraphInputInheritsDefault(InputName, bInputInheritsDefault);
			}

			return true;
		}
		UE_LOG(LogMetaSound, Error, TEXT("Attempting to set graph input of type '%s' with unsupported literal type"), *Input.TypeName.ToString());
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::SetGraphInputInheritsDefault(FName InName, bool bInputInheritsDefault)
{
	FMetasoundFrontendGraphClassPresetOptions& PresetOptions = GetDocument().RootGraph.PresetOptions;
	if (bInputInheritsDefault)
	{
		if (PresetOptions.bIsPreset)
		{
			return PresetOptions.InputsInheritingDefault.Add(InName).IsValidId();
		}
	}
	else
	{
		if (PresetOptions.bIsPreset)
		{
			return PresetOptions.InputsInheritingDefault.Remove(InName) > 0;
		}
	}

	return false;
}

bool FMetaSoundFrontendDocumentBuilder::SetNodeInputDefault(const FGuid& InNodeID, const FGuid& InVertexID, const FMetasoundFrontendLiteral& InLiteral)
{
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const int32* NodeIndex = NodeCache.FindNodeIndex(InNodeID))
	{
		FMetasoundFrontendGraph& Graph = GetDocument().RootGraph.Graph;
		FMetasoundFrontendNode& Node = Graph.Nodes[*NodeIndex];

		auto IsVertex = [&InVertexID](const FMetasoundFrontendVertex& Vertex) { return Vertex.VertexID == InVertexID; };
		int32 VertexIndex = Node.Interface.Inputs.IndexOfByPredicate(IsVertex);
		if (VertexIndex != INDEX_NONE)
		{
			FMetasoundFrontendVertexLiteral NewVertexLiteral;
			NewVertexLiteral.VertexID = InVertexID;
			NewVertexLiteral.Value = InLiteral;

			auto IsLiteral = [&InVertexID](const FMetasoundFrontendVertexLiteral& Literal) { return Literal.VertexID == InVertexID; };
			int32 LiteralIndex = Node.InputLiterals.IndexOfByPredicate(IsLiteral);
			if (LiteralIndex == INDEX_NONE)
			{
				LiteralIndex = Node.InputLiterals.Num();
				Node.InputLiterals.Add(MoveTemp(NewVertexLiteral));
			}
			else
			{
				Node.InputLiterals[LiteralIndex] = MoveTemp(NewVertexLiteral);
			}

			const FOnMetaSoundFrontendDocumentMutateNodeInputLiteralArray& OnNodeInputLiteralSet = DocumentDelegates->NodeDelegates.OnNodeInputLiteralSet;
			OnNodeInputLiteralSet.Broadcast(*NodeIndex, VertexIndex, LiteralIndex);
			return true;
		}
	}

	return false;
}

#if WITH_EDITOR
bool FMetaSoundFrontendDocumentBuilder::SetNodeLocation(const FGuid& InNodeID, const FVector2D& InLocation)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	if (const int32* NodeIndex = NodeCache.FindNodeIndex(InNodeID))
	{
		FMetasoundFrontendNode& Node = GetDocument().RootGraph.Graph.Nodes[*NodeIndex];
		FMetasoundFrontendNodeStyle& Style = Node.Style;
		if (Style.Display.Locations.IsEmpty())
		{
			Style.Display.Locations = { { FGuid::NewGuid(), InLocation } };
		}
		else
		{
			Algo::ForEach(Style.Display.Locations, [InLocation](TPair<FGuid, FVector2D>& Pair)
			{
				Pair.Value = InLocation;
			});
		}

		return true;
	}

	return false;
}
#endif // WITH_EDITOR

bool FMetaSoundFrontendDocumentBuilder::SwapGraphInput(const FMetasoundFrontendClassVertex& InExistingInputVertex, const FMetasoundFrontendClassVertex& InNewInputVertex)
{
	using namespace Metasound::Frontend;

	// 1. Check if equivalent and early out if functionally do not match
	{
		const FMetasoundFrontendClassInput* ClassInput = FindGraphInput(InExistingInputVertex.Name);
		if (!ClassInput || !FMetasoundFrontendVertex::IsFunctionalEquivalent(*ClassInput, InExistingInputVertex))
		{
			return false;
		}
	}

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	const IDocumentGraphInterfaceCache& InterfaceCache = DocumentCache->GetInterfaceCache();

	// 2. Gather data from existing member/node needed to swap
	TArray<FMetasoundFrontendEdge> RemovedEdges;

	const FMetasoundFrontendClassInput* ExistingInputClass = InterfaceCache.FindInput(InExistingInputVertex.Name);
	checkf(ExistingInputClass, TEXT("'SwapGraphInput' failed to find original graph input"));
	const FGuid NodeID = ExistingInputClass->NodeID;

#if WITH_EDITOR
	TMap<FGuid, FVector2D> Locations;
#endif // WITH_EDITOR
	{
		const FMetasoundFrontendNode* ExistingInputNode = NodeCache.FindNode(NodeID);
		check(ExistingInputNode);

#if WITH_EDITOR
		Locations = ExistingInputNode->Style.Display.Locations;
#endif // WITH_EDITOR

		const FGuid VertexID = ExistingInputNode->Interface.Outputs.Last().VertexID;
		TArray<const FMetasoundFrontendEdge*> Edges = DocumentCache->GetEdgeCache().FindEdges(NodeID, VertexID);
		Algo::Transform(Edges, RemovedEdges, [](const FMetasoundFrontendEdge* Edge) { return *Edge; });
	}

	// 3. Remove existing graph vertex
	{
		const bool bRemovedVertex = RemoveGraphOutput(InExistingInputVertex.Name);
		checkf(bRemovedVertex, TEXT("Failed to swap MetaSound input expected to exist"));
	}

	// 4. Add new graph vertex
	FMetasoundFrontendClassInput NewInput = InNewInputVertex;
	NewInput.NodeID = NodeID;
#if WITH_EDITOR
	NewInput.Metadata.SetSerializeText(InExistingInputVertex.Metadata.GetSerializeText());
#endif // WITH_EDITOR

	const FMetasoundFrontendNode* NewInputNode = AddGraphInput(NewInput);
	checkf(NewInputNode, TEXT("Failed to add new Input node when swapping graph inputs"));

#if WITH_EDITOR
	// 5a. Add to new copy existing node locations
	if (!Locations.IsEmpty())
	{
		const int32* NodeIndex = NodeCache.FindNodeIndex(NewInputNode->GetID());
		checkf(NodeIndex, TEXT("Cache was not updated to reflect newly added input node"));
		FMetasoundFrontendNode& NewNode = GetDocument().RootGraph.Graph.Nodes[*NodeIndex];
		NewNode.Style.Display.Locations = Locations;
	}
#endif // WITH_EDITOR

	// 5b. Add to new copy existing node edges
	for (const FMetasoundFrontendEdge& RemovedEdge : RemovedEdges)
	{
		FMetasoundFrontendEdge NewEdge = RemovedEdge;
		NewEdge.FromNodeID = NewInputNode->GetID();
		NewEdge.FromVertexID = NewInputNode->Interface.Outputs.Last().VertexID;
		AddEdge(MoveTemp(NewEdge));
	}
	return true;
}

bool FMetaSoundFrontendDocumentBuilder::SwapGraphOutput(const FMetasoundFrontendClassVertex& InExistingOutputVertex, const FMetasoundFrontendClassVertex& InNewOutputVertex)
{
	using namespace Metasound::Frontend;

	// 1. Check if equivalent and early out if functionally do not match
	{
		const FMetasoundFrontendClassOutput* ClassOutput = FindGraphOutput(InExistingOutputVertex.Name);
		if (!ClassOutput || !FMetasoundFrontendVertex::IsFunctionalEquivalent(*ClassOutput, InExistingOutputVertex))
		{
			return false;
		}
	}

	const IDocumentGraphNodeCache& NodeCache = DocumentCache->GetNodeCache();
	const IDocumentGraphInterfaceCache& InterfaceCache = DocumentCache->GetInterfaceCache();

	// 2. Gather data from existing member/node needed to swap
	TArray<FMetasoundFrontendEdge> RemovedEdges;

	const FMetasoundFrontendClassOutput* ExistingOutputClass = InterfaceCache.FindOutput(InExistingOutputVertex.Name);
	checkf(ExistingOutputClass, TEXT("'SwapGraphOutput' failed to find original graph output"));
	const FGuid NodeID = ExistingOutputClass->NodeID;

#if WITH_EDITOR
	TMap<FGuid, FVector2D> Locations;
#endif // WITH_EDITOR
	{
		const FMetasoundFrontendNode* ExistingOutputNode = NodeCache.FindNode(NodeID);
		check(ExistingOutputNode);

#if WITH_EDITOR
		Locations = ExistingOutputNode->Style.Display.Locations;
#endif // WITH_EDITOR

		const FGuid VertexID = ExistingOutputNode->Interface.Inputs.Last().VertexID;
		TArray<const FMetasoundFrontendEdge*> Edges = DocumentCache->GetEdgeCache().FindEdges(ExistingOutputNode->GetID(), VertexID);
		Algo::Transform(Edges, RemovedEdges, [](const FMetasoundFrontendEdge* Edge) { return *Edge; });
	}

	// 3. Remove existing graph vertex
	{
		const bool bRemovedVertex = RemoveGraphOutput(InExistingOutputVertex.Name);
		checkf(bRemovedVertex, TEXT("Failed to swap output expected to exist while swapping MetaSound outputs"));
	}
	
	// 4. Add new graph vertex
	FMetasoundFrontendClassOutput NewOutput = InNewOutputVertex;
	NewOutput.NodeID = NodeID;
#if WITH_EDITOR
	NewOutput.Metadata.SetSerializeText(InExistingOutputVertex.Metadata.GetSerializeText());
#endif // WITH_EDITOR

	const FMetasoundFrontendNode* NewOutputNode = AddGraphOutput(NewOutput);
	checkf(NewOutputNode, TEXT("Failed to add new output node when swapping graph outputs"));

#if WITH_EDITOR
	// 5a. Add to new copy existing node locations
	if (!Locations.IsEmpty())
	{
		const int32* NodeIndex = NodeCache.FindNodeIndex(NewOutputNode->GetID());
		checkf(NodeIndex, TEXT("Cache was not updated to reflect newly added input node"));
		FMetasoundFrontendNode& NewNode = GetDocument().RootGraph.Graph.Nodes[*NodeIndex];
		NewNode.Style.Display.Locations = Locations;
	}
#endif // WITH_EDITOR

	// 5b. Add to new copy existing node edges
	for (const FMetasoundFrontendEdge& RemovedEdge : RemovedEdges)
	{
		FMetasoundFrontendEdge NewEdge = RemovedEdge;
		NewEdge.ToNodeID = NewOutputNode->GetID();
		NewEdge.ToVertexID = NewOutputNode->Interface.Inputs.Last().VertexID;
		AddEdge(MoveTemp(NewEdge));
	}

	return true;
}

bool FMetaSoundFrontendDocumentBuilder::UpdateDependencyClassNames(const TMap<FMetasoundFrontendClassName, FMetasoundFrontendClassName>& OldToNewReferencedClassNames)
{
	for (FMetasoundFrontendClass& Dependency : GetDocument().Dependencies)
	{
		const FMetasoundFrontendClassName OldName = Dependency.Metadata.GetClassName();
		if (const FMetasoundFrontendClassName* NewName = OldToNewReferencedClassNames.Find(OldName))
		{
			const int32* DependencyIndex = DocumentCache->FindDependencyIndex(Dependency.ID);
			check(DependencyIndex);
			GetDocumentDelegates().OnRenamingDependencyClass.Broadcast(*DependencyIndex, *NewName);
			Dependency.Metadata.SetClassName(*NewName);
		}
	}
	
	return true;
}
