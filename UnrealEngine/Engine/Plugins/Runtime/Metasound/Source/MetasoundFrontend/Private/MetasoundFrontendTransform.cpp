// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundFrontendTransform.h"

#include "Algo/Transform.h"
#include "Interfaces/MetasoundFrontendInterface.h"
#include "MetasoundAccessPtr.h"
#include "MetasoundAssetBase.h"
#include "MetasoundFrontendArchetypeRegistry.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentController.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLog.h"
#include "MetasoundTrace.h"
#include "Misc/App.h"


namespace Metasound
{
	namespace Frontend
	{
		namespace DocumentTransform
		{
#if WITH_EDITOR
			FGetNodeDisplayNameProjection NodeDisplayNameProjection;

			void RegisterNodeDisplayNameProjection(FGetNodeDisplayNameProjection&& InNameProjection)
			{
				NodeDisplayNameProjection = MoveTemp(InNameProjection);
			}

			FGetNodeDisplayNameProjectionRef GetNodeDisplayNameProjection()
			{
				return NodeDisplayNameProjection;
			}
#endif // WITH_EDITOR
		} // namespace DocumentTransform

		bool IDocumentTransform::Transform(FMetasoundFrontendDocument& InOutDocument) const
		{
			FDocumentAccessPtr DocAccessPtr = MakeAccessPtr<FDocumentAccessPtr>(InOutDocument.AccessPoint, InOutDocument);
			return Transform(FDocumentController::CreateDocumentHandle(DocAccessPtr));
		}

		FModifyRootGraphInterfaces::FModifyRootGraphInterfaces(const TArray<FMetasoundFrontendInterface>& InInterfacesToRemove, const TArray<FMetasoundFrontendInterface>& InInterfacesToAdd)
			: InterfacesToRemove(InInterfacesToRemove)
			, InterfacesToAdd(InInterfacesToAdd)
		{
			Init();
		}

		FModifyRootGraphInterfaces::FModifyRootGraphInterfaces(const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToRemove, const TArray<FMetasoundFrontendVersion>& InInterfaceVersionsToAdd)
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

			Init();
		}

#if WITH_EDITOR
		void FModifyRootGraphInterfaces::SetDefaultNodeLocations(bool bInSetDefaultNodeLocations)
		{
			bSetDefaultNodeLocations = bInSetDefaultNodeLocations;
		}
#endif // WITH_EDITOR

		void FModifyRootGraphInterfaces::SetNamePairingFunction(const TFunction<bool(FName, FName)>& InNamePairingFunction)
		{
			// Reinit required to rebuild list of pairs
			Init(&InNamePairingFunction);
		}

		void FModifyRootGraphInterfaces::Init(const TFunction<bool(FName, FName)>* InNamePairingFunction)
		{
			InputsToRemove.Reset();
			InputsToAdd.Reset();
			OutputsToRemove.Reset();
			OutputsToAdd.Reset();
			PairedInputs.Reset();
			PairedOutputs.Reset();

			for (const FMetasoundFrontendInterface& FromInterface : InterfacesToRemove)
			{
				InputsToRemove.Append(FromInterface.Inputs);
				OutputsToRemove.Append(FromInterface.Outputs);
			}

			// This function combines all the inputs of all interfaces into one input list and ptrs to their originating interfaces.
			// The interface ptr will be used to query the interface for required validations on inputs. Interfaces define required inputs (and possibly other validation requirements).
			for (const FMetasoundFrontendInterface& ToInterface : InterfacesToAdd)
			{
				TArray<FInputData> NewInputDataArray;
				for (const FMetasoundFrontendClassInput& Input : ToInterface.Inputs)
				{
					FInputData NewData;
					NewData.Input = Input;
					NewData.InputInterface = &ToInterface;
					NewInputDataArray.Add(NewData);
				}

				InputsToAdd.Append(NewInputDataArray);

				TArray<FOutputData> NewOutputDataArray;
				for (const FMetasoundFrontendClassOutput& Output : ToInterface.Outputs)
				{
					FOutputData NewData;
					NewData.Output = Output;
					NewData.OutputInterface = &ToInterface;
					NewOutputDataArray.Add(NewData);
				}

				OutputsToAdd.Append(NewOutputDataArray);
			}

			// Iterate in reverse to allow removal from `InputsToAdd`
			for (int32 AddIndex = InputsToAdd.Num() - 1; AddIndex >= 0; AddIndex--)
			{
				const FMetasoundFrontendClassVertex& VertexToAdd = InputsToAdd[AddIndex].Input;

				const int32 RemoveIndex = InputsToRemove.IndexOfByPredicate([&](const FMetasoundFrontendClassVertex& VertexToRemove)
				{
					if (VertexToAdd.TypeName != VertexToRemove.TypeName)
					{
						return false;
					}

					if (InNamePairingFunction && *InNamePairingFunction)
					{
						return (*InNamePairingFunction)(VertexToAdd.Name, VertexToRemove.Name);
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
					PairedInputs.Add(FVertexPair{InputsToRemove[RemoveIndex], InputsToAdd[AddIndex].Input});
					InputsToRemove.RemoveAtSwap(RemoveIndex);
					InputsToAdd.RemoveAtSwap(AddIndex);
				}
			}

			// Iterate in reverse to allow removal from `OutputsToAdd`
			for (int32 AddIndex = OutputsToAdd.Num() - 1; AddIndex >= 0; AddIndex--)
			{
				const FMetasoundFrontendClassVertex& VertexToAdd = OutputsToAdd[AddIndex].Output;

				const int32 RemoveIndex = OutputsToRemove.IndexOfByPredicate([&](const FMetasoundFrontendClassVertex& VertexToRemove)
				{
					if (VertexToAdd.TypeName != VertexToRemove.TypeName)
					{
						return false;
					}

					if (InNamePairingFunction && *InNamePairingFunction)
					{
						return (*InNamePairingFunction)(VertexToAdd.Name, VertexToRemove.Name);
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
					PairedOutputs.Add(FVertexPair{OutputsToRemove[RemoveIndex], OutputsToAdd[AddIndex].Output});
					OutputsToRemove.RemoveAtSwap(RemoveIndex);
					OutputsToAdd.RemoveAtSwap(AddIndex);
				}
			}			
		}

		bool FModifyRootGraphInterfaces::Transform(FDocumentHandle InDocument) const
		{
			bool bDidEdit = false;

			FGraphHandle GraphHandle = InDocument->GetRootGraph();
			if (!ensure(GraphHandle->IsValid()))
			{
				return false;
			}

			for (const FMetasoundFrontendInterface& Interface : InterfacesToRemove)
			{
				InDocument->RemoveInterfaceVersion(Interface.Version);
			}

			for (const FMetasoundFrontendInterface& Interface : InterfacesToAdd)
			{
				InDocument->AddInterfaceVersion(Interface.Version);
			}

			// Remove unsupported inputs
			for (const FMetasoundFrontendClassVertex& InputToRemove : InputsToRemove)
			{
				if (const FMetasoundFrontendClassInput* ClassInput = GraphHandle->FindClassInputWithName(InputToRemove.Name).Get())
				{
					if (FMetasoundFrontendClassInput::IsFunctionalEquivalent(*ClassInput, InputToRemove))
					{
						bDidEdit = true;
						GraphHandle->RemoveInputVertex(InputToRemove.Name);
					}
				}
			}

			// Remove unrequired outputs
			for (const FMetasoundFrontendClassVertex& OutputToRemove : OutputsToRemove)
			{
				if (const FMetasoundFrontendClassOutput* ClassOutput = GraphHandle->FindClassOutputWithName(OutputToRemove.Name).Get())
				{
					if (FMetasoundFrontendClassOutput::IsFunctionalEquivalent(*ClassOutput, OutputToRemove))
					{
						bDidEdit = true;
						GraphHandle->RemoveOutputVertex(OutputToRemove.Name);
					}
				}
			}

			auto InputDataTypeCompareFilter = [](FConstNodeHandle NodeHandle, FName DataType)
			{
				bool bMatchesDataType = false;
				NodeHandle->IterateConstOutputs([&](FConstOutputHandle OutputHandle)
				{
					if (OutputHandle->GetDataType() == DataType)
					{
						bMatchesDataType = true;
					}
				});

				return bMatchesDataType;
			};

			auto OutputDataTypeCompareFilter = [](FConstNodeHandle NodeHandle, FName DataType)
			{
				bool bMatchesDataType = false;
				NodeHandle->IterateConstInputs([&](FConstInputHandle InputHandle)
				{
					if (InputHandle->GetDataType() == DataType)
					{
						bMatchesDataType = true;
					}
				});

				return bMatchesDataType;
			};

#if WITH_EDITOR
			auto FindLowestNodeLocationOfClassType = [](EMetasoundFrontendClassType ClassType, FGraphHandle Graph, FName DataType, TFunctionRef<bool(FConstNodeHandle, FName)> NodeDataTypeFilter)
			{
				FVector2D LowestLocation = { TNumericLimits<float>::Min(), TNumericLimits<float>::Min() };
				bool bFoundLocation = false;

				Graph->IterateConstNodes([&](FConstNodeHandle NodeHandle)
				{
					const TMap<FGuid, FVector2D>& Locations = NodeHandle->GetNodeStyle().Display.Locations;
					for (const TPair<FGuid, FVector2D>& Pair : Locations)
					{
						if (Pair.Value.Y > LowestLocation.Y)
						{
							LowestLocation = Pair.Value;
							bFoundLocation = true;
						}
					}
				}, ClassType);

				if (!bFoundLocation)
				{
					LowestLocation = { DisplayStyle::NodeLayout::DefaultOffsetX.X * 2.0f, 0.0f };
				}

				return LowestLocation;
			};
#endif // WITH_EDITOR

			// Add missing inputs
			for (const FInputData& InputData : InputsToAdd)
			{
				bDidEdit = true;
				const FMetasoundFrontendClassInput& InputToAdd = InputData.Input;
				FNodeHandle NewInputNode = GraphHandle->AddInputVertex(InputToAdd);

#if WITH_EDITOR
				FName InputName = InputToAdd.Name;
				bool bRequiredInput = InputData.InputInterface->GetInputStyle().RequiredMembers.Contains(InputName);

				if (bSetDefaultNodeLocations || bRequiredInput)
				{
					FMetasoundFrontendNodeStyle Style = NewInputNode->GetNodeStyle();
					const FVector2D LastOutputLocation = FindLowestNodeLocationOfClassType(EMetasoundFrontendClassType::Input, GraphHandle, InputToAdd.TypeName, InputDataTypeCompareFilter);
					Style.Display.Locations.Add(FGuid(), LastOutputLocation + DisplayStyle::NodeLayout::DefaultOffsetY);
					NewInputNode->SetNodeStyle(Style);
				}
#endif // WITH_EDITOR
			}

			// Add missing outputs
			for (const FOutputData& OutputData : OutputsToAdd)
			{
				bDidEdit = true;
				const FMetasoundFrontendClassOutput& OutputToAdd = OutputData.Output;
				FNodeHandle NewOutputNode = GraphHandle->AddOutputVertex(OutputToAdd);

#if WITH_EDITOR
				FName OutputName = OutputToAdd.Name;
				bool bRequiredOutput = OutputData.OutputInterface->GetOutputStyle().RequiredMembers.Contains(OutputName);

				if (bSetDefaultNodeLocations || bRequiredOutput)
				{
					FMetasoundFrontendNodeStyle Style = NewOutputNode->GetNodeStyle();
					const FVector2D LastOutputLocation = FindLowestNodeLocationOfClassType(EMetasoundFrontendClassType::Output, GraphHandle, OutputToAdd.TypeName, OutputDataTypeCompareFilter);
					Style.Display.Locations.Add(FGuid(), LastOutputLocation + DisplayStyle::NodeLayout::DefaultOffsetY);
					NewOutputNode->SetNodeStyle(Style);
				}
#endif // WITH_EDITOR
			}

			// Swap paired inputs.
			for (const FVertexPair& InputPair : PairedInputs)
			{
				bDidEdit = true;

				const FMetasoundFrontendClassVertex& OriginalVertex = InputPair.Get<0>();
				FMetasoundFrontendClassInput NewVertex = InputPair.Get<1>();

				// Cache off node locations and connections to push to new node
				TMap<FGuid, FVector2D> Locations;
				TArray<FInputHandle> ConnectedInputs;
				if (const FMetasoundFrontendClassInput* ClassInput = GraphHandle->FindClassInputWithName(OriginalVertex.Name).Get())
				{
					if (FMetasoundFrontendVertex::IsFunctionalEquivalent(*ClassInput, OriginalVertex))
					{
						NewVertex.DefaultLiteral = ClassInput->DefaultLiteral;
						NewVertex.NodeID = ClassInput->NodeID;
						FNodeHandle OriginalInputNode = GraphHandle->GetInputNodeWithName(OriginalVertex.Name);

#if WITH_EDITOR
						Locations = OriginalInputNode->GetNodeStyle().Display.Locations;
#endif // WITH_EDITOR

						FOutputHandle OriginalInputNodeOutput = OriginalInputNode->GetOutputWithVertexName(OriginalVertex.Name);
						ConnectedInputs = OriginalInputNodeOutput->GetConnectedInputs();
						GraphHandle->RemoveInputVertex(OriginalVertex.Name);
					}
				}

				FNodeHandle NewInputNode = GraphHandle->AddInputVertex(NewVertex);

#if WITH_EDITOR
				// Copy prior node locations
				if (!Locations.IsEmpty())
				{
					// TODO: copy entire style.
					FMetasoundFrontendNodeStyle Style = NewInputNode->GetNodeStyle();
					Style.Display.Locations = Locations;
					NewInputNode->SetNodeStyle(Style);
				}
#endif // WITH_EDITOR

				// Copy prior node connections
				FOutputHandle OutputHandle = NewInputNode->GetOutputWithVertexName(NewVertex.Name);
				for (FInputHandle& ConnectedInput : ConnectedInputs)
				{
					OutputHandle->Connect(*ConnectedInput);
				}
			}

			// Swap paired outputs.
			for (const FVertexPair& OutputPair : PairedOutputs)
			{
				bDidEdit = true;

				const FMetasoundFrontendClassVertex& OriginalVertex = OutputPair.Get<0>();
				FMetasoundFrontendClassVertex NewVertex = OutputPair.Get<1>();

#if WITH_EDITOR
				// Cache off node locations to push to new node
				// Default add output node to origin.
				TMap<FGuid, FVector2D> Locations;
				Locations.Add(FGuid(), FVector2D{0.f, 0.f});
#endif // WITH_EDITOR

				FOutputHandle ConnectedOutput = IOutputController::GetInvalidHandle();
				if (const FMetasoundFrontendClassOutput* ClassOutput = GraphHandle->FindClassOutputWithName(OriginalVertex.Name).Get())
				{
					if (FMetasoundFrontendVertex::IsFunctionalEquivalent(*ClassOutput, OriginalVertex))
					{
						NewVertex.NodeID = ClassOutput->NodeID;

#if WITH_EDITOR
						// Interface members do not serialize text to avoid localization
						// mismatches between assets and interfaces defined in code.
						NewVertex.Metadata.SetSerializeText(false);
#endif // WITH_EDITOR

						FNodeHandle OriginalOutputNode = GraphHandle->GetOutputNodeWithName(OriginalVertex.Name);

#if WITH_EDITOR
						Locations = OriginalOutputNode->GetNodeStyle().Display.Locations;
#endif // WITH_EDITOR

						FInputHandle Input = OriginalOutputNode->GetInputWithVertexName(OriginalVertex.Name);
						ConnectedOutput = Input->GetConnectedOutput();
						GraphHandle->RemoveOutputVertex(OriginalVertex.Name);
					}
				}

				FNodeHandle NewOutputNode = GraphHandle->AddOutputVertex(NewVertex);

#if WITH_EDITOR
				if (Locations.Num() > 0)
				{
					FMetasoundFrontendNodeStyle Style = NewOutputNode->GetNodeStyle();
					Style.Display.Locations = Locations;
					NewOutputNode->SetNodeStyle(Style);
				}
#endif // WITH_EDITOR

				// Copy prior node connections
				FInputHandle InputHandle = NewOutputNode->GetInputWithVertexName(NewVertex.Name);
				ConnectedOutput->Connect(*InputHandle);
			}

			return bDidEdit;
		}

		bool FUpdateRootGraphInterface::Transform(FDocumentHandle InDocument) const
		{
			bool bDidEdit = false;

			if (!ensure(InDocument->IsValid()))
			{
				return bDidEdit;
			}

			// Find registered target interface.
			FMetasoundFrontendInterface TargetInterface;
			bool bFoundTargetInterface = ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceVersion.Name, TargetInterface);
			if (!bFoundTargetInterface)
			{
				UE_LOG(LogMetaSound, Error, TEXT("Target interface is not registered [InterfaceVersion:%s]"), *InterfaceVersion.ToString());
				return false;
			}

			if (TargetInterface.Version == InterfaceVersion)
			{
				return false;
			}

			// Attempt to upgrade
			TArray<const IInterfaceRegistryEntry*> UpgradePath;
			GetUpdatePathForDocument(InterfaceVersion, TargetInterface.Version, UpgradePath);
			return UpdateDocumentInterface(UpgradePath, InDocument);
		}

		void FUpdateRootGraphInterface::GetUpdatePathForDocument(const FMetasoundFrontendVersion& InCurrentVersion, const FMetasoundFrontendVersion& InTargetVersion, TArray<const IInterfaceRegistryEntry*>& OutUpgradePath) const
		{
			if (InCurrentVersion.Name == InTargetVersion.Name)
			{
				// Get all associated registered interfaces
				TArray<FMetasoundFrontendVersion> RegisteredVersions = ISearchEngine::Get().FindAllRegisteredInterfacesWithName(InTargetVersion.Name);

				// Filter registry entries that exist between current version and target version
				auto FilterRegistryEntries = [&InCurrentVersion, &InTargetVersion](const FMetasoundFrontendVersion& InVersion)
				{
					const bool bIsGreaterThanCurrent = InVersion.Number > InCurrentVersion.Number;
					const bool bIsLessThanOrEqualToTarget = InVersion.Number <= InTargetVersion.Number;

					return bIsGreaterThanCurrent && bIsLessThanOrEqualToTarget;
				};
				RegisteredVersions = RegisteredVersions.FilterByPredicate(FilterRegistryEntries);

				// sort registry entries to create an ordered upgrade path.
				RegisteredVersions.Sort();

				// Get registry entries from registry keys.
				auto GetRegistryEntry = [](const FMetasoundFrontendVersion& InVersion)
				{
					FInterfaceRegistryKey Key = GetInterfaceRegistryKey(InVersion);
					return IInterfaceRegistry::Get().FindInterfaceRegistryEntry(Key);
				};
				Algo::Transform(RegisteredVersions, OutUpgradePath, GetRegistryEntry);
			}
		}

		bool FUpdateRootGraphInterface::UpdateDocumentInterface(const TArray<const IInterfaceRegistryEntry*>& InUpgradePath, FDocumentHandle InDocument) const
		{
			const FMetasoundFrontendVersionNumber* LastVersionUpdated = nullptr;
			for (const IInterfaceRegistryEntry* Entry : InUpgradePath)
			{
				if (ensure(nullptr != Entry))
				{
					if (Entry->UpdateRootGraphInterface(InDocument))
					{
						LastVersionUpdated = &Entry->GetInterface().Version.Number;
					}
				}
			}

			if (LastVersionUpdated)
			{
#if WITH_EDITOR
				const FString AssetName = *InDocument->GetRootGraphClass().Metadata.GetDisplayName().ToString();
#else
				const FString AssetName = *InDocument->GetRootGraphClass().Metadata.GetClassName().ToString();
#endif // !WITH_EDITOR
				UE_LOG(LogMetaSound, Display, TEXT("Asset '%s' interface '%s' updated: '%s' --> '%s'"),
					*AssetName,
					*InterfaceVersion.Name.ToString(),
					*InterfaceVersion.Number.ToString(),
					*LastVersionUpdated->ToString());
				return true;
			}

			return false;
		}

		bool FAutoUpdateRootGraph::Transform(FDocumentHandle InDocument) const
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FAutoUpdateRootGraph::Transform);
			bool bDidEdit = false;

			FMetasoundAssetBase* PresetReferencedMetaSoundAsset = nullptr;
			TArray<TPair<FNodeHandle, FMetasoundFrontendVersionNumber>> NodesToUpdate;

			FGraphHandle RootGraph = InDocument->GetRootGraph();
			const bool bIsPreset = RootGraph->GetGraphPresetOptions().bIsPreset;
			RootGraph->IterateNodes([&](FNodeHandle NodeHandle)
			{
				using namespace Metasound::Frontend;

				const FMetasoundFrontendClassMetadata& ClassMetadata = NodeHandle->GetClassMetadata();

				if (bIsPreset)
				{
					const FNodeRegistryKey RegistryKey = FMetasoundFrontendRegistryContainer::Get()->GetRegistryKey(ClassMetadata);
					PresetReferencedMetaSoundAsset = IMetaSoundAssetManager::GetChecked().TryLoadAssetFromKey(RegistryKey);
					if (!PresetReferencedMetaSoundAsset)
					{
						UE_LOG(LogMetaSound, Error, TEXT("Auto-Updating preset '%s' failed: Referenced class '%s' missing."), *DebugAssetPath, *ClassMetadata.GetClassName().ToString());
					}
					return;
				}

				FClassInterfaceUpdates InterfaceUpdates;
				if (!NodeHandle->CanAutoUpdate(InterfaceUpdates))
				{
					return;
				}

				// Check if a updated minor version exists.
				FMetasoundFrontendClass ClassWithHighestMinorVersion;
				bool bFoundClassInSearchEngine = Frontend::ISearchEngine::Get().FindClassWithHighestMinorVersion(ClassMetadata.GetClassName(), ClassMetadata.GetVersion().Major, ClassWithHighestMinorVersion);

				if (bFoundClassInSearchEngine && (ClassWithHighestMinorVersion.Metadata.GetVersion() > ClassMetadata.GetVersion()))
				{
					const FMetasoundFrontendVersionNumber UpdateVersion = ClassWithHighestMinorVersion.Metadata.GetVersion();

					UE_LOG(LogMetaSound, Display, TEXT("Auto-Updating '%s' node class '%s': Newer version '%s' found."), *DebugAssetPath, *ClassMetadata.GetClassName().ToString(), *UpdateVersion.ToString());

					NodesToUpdate.Add(TPair<FNodeHandle, FMetasoundFrontendVersionNumber>(NodeHandle, UpdateVersion));
				}
				else if (InterfaceUpdates.ContainsChanges())
				{
					const FMetasoundFrontendVersionNumber UpdateVersion = ClassMetadata.GetVersion();
					UE_LOG(LogMetaSound, Display, TEXT("Auto-Updating '%s' node class '%s (%s)': Interface change detected."), *DebugAssetPath, *ClassMetadata.GetClassName().ToString(), *UpdateVersion.ToString());

					NodesToUpdate.Add(TPair<FNodeHandle, FMetasoundFrontendVersionNumber>(NodeHandle, UpdateVersion));
				}

				// Only update the node at this point if editor data is loaded. If it isn't and their are no interface
				// changes but auto-update returned it was eligible, then the auto-update only contains cosmetic changes.
#if WITH_EDITORONLY_DATA
				else
				{
					NodesToUpdate.Add(TPair<FNodeHandle, FMetasoundFrontendVersionNumber>(NodeHandle, ClassMetadata.GetVersion()));
				}
#endif // WITH_EDITORONLY_DATA
			}, EMetasoundFrontendClassType::External);

			if (PresetReferencedMetaSoundAsset)
			{
				if (bIsPreset)
				{
					bDidEdit |= FRebuildPresetRootGraph(PresetReferencedMetaSoundAsset->GetDocumentHandle()).Transform(InDocument);
					if (bDidEdit)
					{
						FMetasoundFrontendClassMetadata PresetMetadata = InDocument->GetRootGraphClass().Metadata;
						PresetMetadata.SetType(EMetasoundFrontendClassType::External);
						const FNodeRegistryKey RegistryKey = FMetasoundFrontendRegistryContainer::Get()->GetRegistryKey(PresetMetadata);
						FMetasoundAssetBase* PresetMetaSoundAsset = IMetaSoundAssetManager::GetChecked().TryLoadAssetFromKey(RegistryKey);
						if (ensure(PresetMetaSoundAsset))
						{
							PresetMetaSoundAsset->ConformObjectDataToInterfaces();
						}

						InDocument->RemoveUnreferencedDependencies();
						InDocument->SynchronizeDependencyMetadata();
					}
				}
			}
			else
			{
				using FVertexNameAndType = INodeController::FVertexNameAndType;

				bDidEdit |= !NodesToUpdate.IsEmpty();
				for (const TPair<FNodeHandle, FMetasoundFrontendVersionNumber>& Pair : NodesToUpdate)
				{
					FNodeHandle ExistingNode = Pair.Key;
					FMetasoundFrontendVersionNumber InitialVersion = ExistingNode->GetClassMetadata().GetVersion();

					TArray<FVertexNameAndType> DisconnectedInputs;
					TArray<FVertexNameAndType> DisconnectedOutputs;
					FNodeHandle NewNode = ExistingNode->ReplaceWithVersion(Pair.Value, &DisconnectedInputs, &DisconnectedOutputs);

					// Log warnings for any disconnections
					if (bLogWarningOnDroppedConnection)
					{
						if ((DisconnectedInputs.Num() > 0) || (DisconnectedOutputs.Num() > 0))
						{
							const FString NodeClassName = NewNode->GetClassMetadata().GetClassName().ToString();
							const FString NewClassVersion = Pair.Value.ToString();

							for (const FVertexNameAndType& InputPin : DisconnectedInputs)
							{
								UE_LOG(LogMetaSound, Warning, TEXT("Auto-Updating '%s' node class '%s (%s)': Previously connected input '%s' with data type '%s' no longer exists."), *DebugAssetPath, *NodeClassName, *NewClassVersion, *InputPin.Get<0>().ToString(), *InputPin.Get<1>().ToString());
							}

							for (const FVertexNameAndType& OutputPin : DisconnectedOutputs)
							{
								UE_LOG(LogMetaSound, Warning, TEXT("Auto-Updating '%s' node class '%s (%s)': Previously connected output '%s' with data type '%s' no longer exists."), *DebugAssetPath, *NodeClassName, *NewClassVersion, *OutputPin.Get<0>().ToString(), *OutputPin.Get<1>().ToString());
							}
						}
					}
				}

				InDocument->RemoveUnreferencedDependencies();
				InDocument->SynchronizeDependencyMetadata();
			}

			return bDidEdit;
		}

		bool FRebuildPresetRootGraph::Transform(FDocumentHandle InDocument) const
		{
			FGraphHandle PresetGraphHandle = InDocument->GetRootGraph();
			if (!ensure(PresetGraphHandle->IsValid()))
			{
				return false;
			}

			// Callers of this transform should check that the graph is supposed to
			// be managed externally before calling this transform. If a scenario
			// arises where this transform is used outside of AutoUpdate, then this
			// early exist should be removed as it's mostly here to protect against
			// accidental manipulation of metasound graphs.
			if (!ensure(PresetGraphHandle->GetGraphPresetOptions().bIsPreset))
			{
				return false;
			}

			FConstGraphHandle ReferencedGraphHandle = ReferencedDocument->GetRootGraph();
			if (!ensure(ReferencedGraphHandle->IsValid()))
			{
				return false;
			}

			// Determine the inputs and outputs needed in the wrapping graph. Also
			// cache any exiting literals that have been set on the wrapping graph.
			TSet<FName> InputsInheritingDefault;
			TArray<FMetasoundFrontendClassInput> ClassInputs = GenerateRequiredClassInputs(PresetGraphHandle, InputsInheritingDefault);
			TArray<FMetasoundFrontendClassOutput> ClassOutputs = GenerateRequiredClassOutputs(PresetGraphHandle);

			FGuid PresetNodeID;
			PresetGraphHandle->IterateConstNodes([InPresetNodeID = &PresetNodeID](FConstNodeHandle PresetNodeHandle)
			{
				*InPresetNodeID = PresetNodeHandle->GetID();
			}, EMetasoundFrontendClassType::External);

			// Clear the root graph so it can be rebuilt.
			PresetGraphHandle->ClearGraph();

			// Ensure preset interfaces match those found in referenced graph.  Referenced graph is assumed to be
			// well-formed (i.e. all inputs/outputs/environment variables declared by interfaces are present, and
			// of proper name & data type).
			const TSet<FMetasoundFrontendVersion>& RefInterfaceVersions = ReferencedDocument->GetInterfaceVersions();
			for (const FMetasoundFrontendVersion& Version : RefInterfaceVersions)
			{
				InDocument->AddInterfaceVersion(Version);
			}

			// Add referenced node
			FMetasoundFrontendClassMetadata ReferencedClassMetadata = ReferencedGraphHandle->GetGraphMetadata();
			// Swap type on look-up as it will be referenced as an externally defined class relative to the new Preset asset
			ReferencedClassMetadata.SetType(EMetasoundFrontendClassType::External);

			FNodeHandle ReferencedNodeHandle = PresetGraphHandle->AddNode(ReferencedClassMetadata, PresetNodeID);

#if WITH_EDITOR
			// Set node location.
			FMetasoundFrontendNodeStyle RefNodeStyle;

			// Offset to be to the right of input nodes
			RefNodeStyle.Display.Locations.Add(FGuid(), DisplayStyle::NodeLayout::DefaultOffsetX);
			ReferencedNodeHandle->SetNodeStyle(RefNodeStyle);
#endif // WITH_EDITOR

			// Connect parent graph to referenced graph
			PresetGraphHandle->SetInputsInheritingDefault(MoveTemp(InputsInheritingDefault));

			AddAndConnectInputs(ClassInputs, PresetGraphHandle, ReferencedNodeHandle);
			AddAndConnectOutputs(ClassOutputs, PresetGraphHandle, ReferencedNodeHandle);

			return true;
		}

		void FRebuildPresetRootGraph::AddAndConnectInputs(const TArray<FMetasoundFrontendClassInput>& InClassInputs, FGraphHandle& InPresetGraphHandle, FNodeHandle& InReferencedNode) const
		{
			// Add inputs and space appropriately
			FVector2D InputNodeLocation = FVector2D::ZeroVector;

			FConstGraphHandle ReferencedGraphHandle = ReferencedDocument->GetRootGraph();

			TArray<FNodeHandle> NodeHandles;
			for (const FMetasoundFrontendClassInput& ClassInput : InClassInputs)
			{
				FNodeHandle InputNode = InPresetGraphHandle->AddInputVertex(ClassInput);
				if (ensure(InputNode->IsValid()))
				{
					// Connect input node to corresponding referencing node.
					FOutputHandle OutputToConnect = InputNode->GetOutputWithVertexName(ClassInput.Name);
					FInputHandle InputToConnect = InReferencedNode->GetInputWithVertexName(ClassInput.Name);
					ensure(OutputToConnect->Connect(*InputToConnect));
					NodeHandles.Add(MoveTemp(InputNode));
				}
			}

#if WITH_EDITOR
			// Sort before adding nodes to graph layout & copy to preset (must be done after all
			// inputs/outputs are added but before setting locations to propagate effectively)
			FMetasoundFrontendInterfaceStyle Style = ReferencedGraphHandle->GetInputStyle();
			InPresetGraphHandle->SetInputStyle(Style);

			Style.SortDefaults(NodeHandles, DocumentTransform::GetNodeDisplayNameProjection());

			for (const FNodeHandle& InputNode : NodeHandles)
			{
				// Set input node location
				FMetasoundFrontendNodeStyle NodeStyle;
				NodeStyle.Display.Locations.Add(FGuid(), InputNodeLocation);
				InputNode->SetNodeStyle(NodeStyle);
				InputNodeLocation += DisplayStyle::NodeLayout::DefaultOffsetY;
			}
#endif // WITH_EDITOR
		}

		void FRebuildPresetRootGraph::AddAndConnectOutputs(const TArray<FMetasoundFrontendClassOutput>& InClassOutputs, FGraphHandle& InPresetGraphHandle, FNodeHandle& InReferencedNode) const
		{
			// Add outputs and space appropriately
			FVector2D OutputNodeLocation = (2 * DisplayStyle::NodeLayout::DefaultOffsetX);

			FConstGraphHandle ReferencedGraphHandle = ReferencedDocument->GetRootGraph();

			TArray<FNodeHandle> NodeHandles;
			for (const FMetasoundFrontendClassOutput& ClassOutput : InClassOutputs)
			{
				FNodeHandle OutputNode = InPresetGraphHandle->AddOutputVertex(ClassOutput);
				if (ensure(OutputNode->IsValid()))
				{
					// Connect input node to corresponding referenced node. 
					FInputHandle InputToConnect = OutputNode->GetInputWithVertexName(ClassOutput.Name);
					FOutputHandle OutputToConnect = InReferencedNode->GetOutputWithVertexName(ClassOutput.Name);
					ensure(InputToConnect->Connect(*OutputToConnect));
					NodeHandles.Add(MoveTemp(OutputNode));
				}
			}

#if WITH_EDITOR
			// Sort before adding nodes to graph layout & copy to preset (must be done after all
			// inputs/outputs are added but before setting locations to propagate effectively)
			FMetasoundFrontendInterfaceStyle Style = ReferencedGraphHandle->GetOutputStyle();
			InPresetGraphHandle->SetOutputStyle(Style);

			Style.SortDefaults(NodeHandles, DocumentTransform::GetNodeDisplayNameProjection());

			// Set input node location
			for (const FNodeHandle& OutputNode : NodeHandles)
			{
				FMetasoundFrontendNodeStyle NodeStyle;
				NodeStyle.Display.Locations.Add(FGuid(), OutputNodeLocation);
				OutputNode->SetNodeStyle(NodeStyle);
				OutputNodeLocation += DisplayStyle::NodeLayout::DefaultOffsetY;
			}
#endif // WITH_EDITOR
		}

		TArray<FMetasoundFrontendClassInput> FRebuildPresetRootGraph::GenerateRequiredClassInputs(const FConstGraphHandle& InPresetGraph, TSet<FName>& OutInputsInheritingDefault) const
		{
			TArray<FMetasoundFrontendClassInput> ClassInputs;

			FConstGraphHandle ReferencedGraph = ReferencedDocument->GetRootGraph();

			// Iterate through all input nodes of referenced graph
			ReferencedGraph->IterateConstNodes([&](FConstNodeHandle InputNode)
			{
				const FName NodeName = InputNode->GetNodeName();
				FConstInputHandle Input = InputNode->GetConstInputWithVertexName(NodeName);
				if (ensure(Input->IsValid()))
				{
					FMetasoundFrontendClassInput ClassInput;

					ClassInput.Name = NodeName;
					ClassInput.TypeName = Input->GetDataType();
					ClassInput.AccessType = Input->GetVertexAccessType();

#if WITH_EDITOR
					ClassInput.Metadata.SetDescription(InputNode->GetDescription());
					ClassInput.Metadata.SetDisplayName(Input->GetMetadata().GetDisplayName());
#endif // WITH_EDITOR

					ClassInput.VertexID = FGuid::NewGuid();

					if (const FMetasoundFrontendClassInput* ExistingClassInput = InPresetGraph->FindClassInputWithName(NodeName).Get())
					{
						ClassInput.NodeID = ExistingClassInput->NodeID;
					}

					if (InPresetGraph->ContainsInputVertex(NodeName, ClassInput.TypeName))
					{
						// If the input vertex already exists in the parent graph,
						// check if parent should be used or not from set of managed
						// input names.
						if (InPresetGraph->GetInputsInheritingDefault().Contains(NodeName))
						{
							const FGuid ReferencedVertexID = ReferencedGraph->GetVertexIDForInputVertex(NodeName);
							ClassInput.DefaultLiteral = ReferencedGraph->GetDefaultInput(ReferencedVertexID);
						}
						else
						{
							FGuid VertexID = InPresetGraph->GetVertexIDForInputVertex(NodeName);
							ClassInput.DefaultLiteral = InPresetGraph->GetDefaultInput(VertexID);
						}
					}
					else
					{
						// If the input vertex does not exist on the parent graph,
						// then it is a new vertex and should use the default value
						// of the referenced graph.
						const FGuid ReferencedVertexID = ReferencedGraph->GetVertexIDForInputVertex(NodeName);
						ClassInput.DefaultLiteral = ReferencedGraph->GetDefaultInput(ReferencedVertexID);
					}

					ClassInputs.Add(MoveTemp(ClassInput));
				}
			}, EMetasoundFrontendClassType::Input);

			// Fill new managed inputs set with names of all class inputs & if the old input was explicitly not
			// marked as a managed input, then remove it from the new managed inputs if found.
			OutInputsInheritingDefault.Reset();
			Algo::Transform(ClassInputs, OutInputsInheritingDefault, [](const FMetasoundFrontendClassInput& Input) { return Input.Name; });
			const TSet<FName>& InputsInheritingDefault = InPresetGraph->GetInputsInheritingDefault();
			InPresetGraph->IterateConstNodes([&InputsInheritingDefault, &OutInputsInheritingDefault](FConstNodeHandle Input)
			{
				if (!InputsInheritingDefault.Contains(Input->GetNodeName()))
				{
					OutInputsInheritingDefault.Remove(Input->GetNodeName());
				}
			}, EMetasoundFrontendClassType::Input);

			return ClassInputs;
		}

		TArray<FMetasoundFrontendClassOutput> FRebuildPresetRootGraph::GenerateRequiredClassOutputs(const FConstGraphHandle& InPresetGraph) const
		{
			TArray<FMetasoundFrontendClassOutput> ClassOutputs;

			FConstGraphHandle ReferencedGraph = ReferencedDocument->GetRootGraph();

			// Iterate over the referenced graph's output nodes.
			ReferencedGraph->IterateConstNodes([&](FConstNodeHandle OutputNode)
			{
				const FName NodeName = OutputNode->GetNodeName();
				FConstOutputHandle Output = OutputNode->GetConstOutputWithVertexName(NodeName);
				if (ensure(Output->IsValid()))
				{
					FMetasoundFrontendClassOutput ClassOutput;

					ClassOutput.Name = NodeName;
					ClassOutput.TypeName = Output->GetDataType();
					ClassOutput.AccessType = Output->GetVertexAccessType();

#if WITH_EDITOR
					ClassOutput.Metadata.SetDescription(OutputNode->GetDescription());
					ClassOutput.Metadata.SetDisplayName(Output->GetMetadata().GetDisplayName());
#endif // WITH_EDITOR

					ClassOutput.VertexID = FGuid::NewGuid();

					if (const FMetasoundFrontendClassOutput* ExistingClassOutput = InPresetGraph->FindClassOutputWithName(NodeName).Get())
					{
						ClassOutput.NodeID = ExistingClassOutput->NodeID;
					}

					ClassOutputs.Add(MoveTemp(ClassOutput));
				}
			}, EMetasoundFrontendClassType::Output);

			return ClassOutputs;
		}

		bool FRenameRootGraphClass::Transform(FDocumentHandle InDocument) const
		{
			FGraphHandle RootGraph = InDocument->GetRootGraph();

			FMetasoundFrontendClassMetadata Metadata = RootGraph->GetGraphMetadata();
			Metadata.SetClassName(NewClassName);
			RootGraph->SetGraphMetadata(Metadata);

			return true;
		}
	} // namespace Frontend
} // namespace Metasound
