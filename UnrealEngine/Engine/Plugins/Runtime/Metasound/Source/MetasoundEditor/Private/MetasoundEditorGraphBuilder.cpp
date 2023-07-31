// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorGraphBuilder.h"

#include "MetasoundEditorGraphBuilder.h"
#include "Algo/AnyOf.h"
#include "Algo/Sort.h"
#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GraphEditor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphInputNode.h"
#include "MetasoundEditorGraphMemberDefaults.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorGraphValidation.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentAccessPtr.h"
#include "MetasoundFrontendDocumentVersioning.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundLiteral.h"
#include "MetasoundTime.h"
#include "MetasoundUObjectRegistry.h"
#include "MetasoundVariableNodes.h"
#include "MetasoundVertex.h"
#include "MetasoundWaveTable.h"
#include "Modules/ModuleManager.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"
#include "Templates/Tuple.h"
#include "Toolkits/ToolkitManager.h"
#include "WaveTable.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "MetaSoundEditor"


namespace Metasound
{
	namespace Editor
	{
		namespace GraphBuilderPrivate
		{
			void DeleteNode(UObject& InMetaSound, Frontend::FNodeHandle InNodeHandle)
			{
				if (InNodeHandle->IsValid())
				{
					Frontend::FGraphHandle GraphHandle = InNodeHandle->GetOwningGraph();
					if (GraphHandle->IsValid())
					{
						GraphHandle->RemoveNode(*InNodeHandle);
					}
				}
			}

			FName GenerateUniqueName(const TArray<FName>& InExistingNames, const FString& InBaseName)
			{
				int32 PostFixInt = 0;
				FString NewName = InBaseName;

				while (InExistingNames.Contains(*NewName))
				{
					PostFixInt++;
					NewName = FString::Format(TEXT("{0} {1}"), { InBaseName, PostFixInt });
				}

				return FName(*NewName);
			}

			void RecurseClearDocumentModified(FMetasoundAssetBase& InAssetBase)
			{
				using namespace Metasound::Frontend;

				InAssetBase.GetModifyContext().ClearDocumentModified();

				TArray<FMetasoundAssetBase*> References;
				ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(InAssetBase, References));
				for (FMetasoundAssetBase* Reference : References)
				{
					check(Reference);
					Reference->GetModifyContext().ClearDocumentModified();
					RecurseClearDocumentModified(*Reference);
				}
			};

			bool SynchronizeGraphRecursively(UObject& InMetaSound, bool bEditorGraphModified = false)
			{
				using namespace Frontend;

				FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
				check(MetaSoundAsset);

				// Synchronize referenced graphs first to ensure all editor data
				// is up-to-date prior to synchronizing this referencing graph.
				TArray<FMetasoundAssetBase*> References;
				ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(*MetaSoundAsset, References));
				for (FMetasoundAssetBase* Reference : References)
				{
					check(Reference);
					bEditorGraphModified |= SynchronizeGraphRecursively(*Reference->GetOwningAsset(), bEditorGraphModified);
				}

				if (!MetaSoundAsset->GetModifyContext().GetDocumentModified())
				{
					return bEditorGraphModified;
				}

				// If no graph is set, MetaSound has been created outside of asset factory, so initialize it here.
				// TODO: Move factory initialization and this code to single builder function (in header so cannot move
				// until 5.1+).
				if (!MetaSoundAsset->GetGraph())
				{
					FString Author = UKismetSystemLibrary::GetPlatformUserName();
					if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
					{
						if (!EditorSettings->DefaultAuthor.IsEmpty())
						{
							Author = EditorSettings->DefaultAuthor;
						}
					}

					FGraphBuilder::InitMetaSound(InMetaSound, Author);

					// Initial graph generation is not something to be managed by the transaction
					// stack, so don't track dirty state until after initial setup if necessary.
					UMetasoundEditorGraph* Graph = NewObject<UMetasoundEditorGraph>(&InMetaSound, FName(), RF_Transactional);
					Graph->Schema = UMetasoundEditorGraphSchema::StaticClass();
					MetaSoundAsset->SetGraph(Graph);
				}

				bEditorGraphModified |= FGraphBuilder::SynchronizeGraphMembers(InMetaSound);
				bEditorGraphModified |= FGraphBuilder::SynchronizeNodeMembers(InMetaSound);
				bEditorGraphModified |= FGraphBuilder::SynchronizeNodes(InMetaSound);
				bEditorGraphModified |= FGraphBuilder::SynchronizeConnections(InMetaSound);

				return bEditorGraphModified;
			}
		} // namespace GraphBuilderPrivate

		// Categories corresponding with POD DataTypes
		const FName FGraphBuilder::PinCategoryObject = "object"; // Basket for all UObject proxy types (corresponds to multiple DataTypes)
		const FName FGraphBuilder::PinCategoryBoolean = GetMetasoundDataTypeName<bool>();
		const FName FGraphBuilder::PinCategoryFloat = GetMetasoundDataTypeName<float>();
		const FName FGraphBuilder::PinCategoryInt32 = GetMetasoundDataTypeName<int32>();
		const FName FGraphBuilder::PinCategoryString = GetMetasoundDataTypeName<FString>();

		// Categories corresponding with MetaSound DataTypes with custom visualization
		const FName FGraphBuilder::PinCategoryAudio = GetMetasoundDataTypeName<FAudioBuffer>();
		const FName FGraphBuilder::PinCategoryTime = GetMetasoundDataTypeName<FTime>();
		const FName FGraphBuilder::PinCategoryTimeArray = GetMetasoundDataTypeName<TArray<FTime>>();
		const FName FGraphBuilder::PinCategoryTrigger = GetMetasoundDataTypeName<FTrigger>();
		const FName FGraphBuilder::PinCategoryWaveTable = GetMetasoundDataTypeName<WaveTable::FWaveTable>();

		bool FGraphBuilder::IsPinCategoryMetaSoundCustomDataType(FName InPinCategoryName)
		{
			return InPinCategoryName == PinCategoryAudio
				|| InPinCategoryName == PinCategoryTime
				|| InPinCategoryName == PinCategoryTimeArray
				|| InPinCategoryName == PinCategoryTrigger
				|| InPinCategoryName == PinCategoryWaveTable;
		}

		bool FGraphBuilder::CanInspectPin(const UEdGraphPin* InPin)
		{
			// Can't inspect the value on an invalid pin object.
			if (!InPin || InPin->IsPendingKill())
			{
				return false;
			}

			// Can't inspect the value on an orphaned pin object.
			if (InPin->bOrphanedPin)
			{
				return false;
			}

			// Currently only inspection of connected pins is supported.
			if (InPin->LinkedTo.IsEmpty())
			{
				return false;
			}

			// Can't inspect the value on an unknown pin object or if the owning node is disabled.
			const UEdGraphNode* OwningNode = InPin->GetOwningNodeUnchecked();
			if (!OwningNode || !OwningNode->IsNodeEnabled())
			{
				return false;
			}

			TSharedPtr<FEditor> Editor = GetEditorForPin(*InPin);
			if (!Editor.IsValid())
			{
				return false;
			}

			if (!Editor->IsPlaying())
			{
				return false;
			}

			FName DataType;
			if (InPin->Direction == EGPD_Input)
			{
				Frontend::FConstInputHandle InputHandle = GetConstInputHandleFromPin(InPin);
				DataType = InputHandle->GetDataType();
			}
			else
			{
				Frontend::FConstOutputHandle OutputHandle = GetConstOutputHandleFromPin(InPin);
				DataType = OutputHandle->GetDataType();
			}

			const bool bIsSupportedType = DataType == GetMetasoundDataTypeName<float>()
				|| DataType == GetMetasoundDataTypeName<int32>()
				|| DataType == GetMetasoundDataTypeName<FString>()
				|| DataType == GetMetasoundDataTypeName<bool>();

			if (!bIsSupportedType)
			{
				return false;
			}

			const UEdGraphPin* ReroutedPin = FindReroutedOutputPin(InPin);
			if (ReroutedPin != InPin)
			{
				return false;
			}

			return true;
		}

		FText FGraphBuilder::GetDisplayName(const FMetasoundFrontendClassMetadata& InClassMetadata, FName InNodeName, bool bInIncludeNamespace)
		{
			using namespace Frontend;

			FName Namespace;
			FName ParameterName;
			Audio::FParameterPath::SplitName(InNodeName, Namespace, ParameterName);

			FText DisplayName;
			auto GetAssetDisplayNameFromMetadata = [&DisplayName](const FMetasoundFrontendClassMetadata& Metadata)
			{
				DisplayName = Metadata.GetDisplayName();
				if (DisplayName.IsEmptyOrWhitespace())
				{
					const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(Metadata);
					bool bIsClassNative = FMetasoundFrontendRegistryContainer::Get()->IsNodeNative(RegistryKey);
					if (!bIsClassNative)
					{
						if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
						{
							if (const FSoftObjectPath* Path = AssetManager->FindObjectPathFromKey(RegistryKey))
							{
								DisplayName = FText::FromString(Path->GetAssetName());
							}
						}
					}
				}
			};

			// 1. Try to get display name from metadata or asset if one can be found from the asset manager
			GetAssetDisplayNameFromMetadata(InClassMetadata);

			// 2. If version is missing from the registry or from asset system, then this node
			// will not provide a useful DisplayName.  In that case, attempt to find the next highest
			// class & associated DisplayName.
			if (DisplayName.IsEmptyOrWhitespace())
			{
				FMetasoundFrontendClass ClassWithHighestVersion;
				if (ISearchEngine::Get().FindClassWithHighestVersion(InClassMetadata.GetClassName(), ClassWithHighestVersion))
				{
					GetAssetDisplayNameFromMetadata(ClassWithHighestVersion.Metadata);
				}
			}

			// 3. If that cannot be found, build a title from the cached node registry FName.
			if (DisplayName.IsEmptyOrWhitespace())
			{
				DisplayName = FText::FromString(ParameterName.ToString());
			}

			// 4. Tack on the namespace if requested
			if (bInIncludeNamespace)
			{
				if (!Namespace.IsNone())
				{
					return FText::Format(LOCTEXT("ClassMetadataDisplayNameWithNamespaceFormat", "{0} ({1})"), DisplayName, FText::FromName(Namespace));
				}
			}

			return DisplayName;
		}

		FText FGraphBuilder::GetDisplayName(const Frontend::INodeController& InFrontendNode, bool bInIncludeNamespace)
		{
			using namespace Frontend;

			FText DisplayName = InFrontendNode.GetDisplayName();
			if (!DisplayName.IsEmptyOrWhitespace())
			{
				return DisplayName;
			}

			return GetDisplayName(InFrontendNode.GetClassMetadata(), InFrontendNode.GetNodeName(), bInIncludeNamespace);
		}

		FText FGraphBuilder::GetDisplayName(const Frontend::IInputController& InFrontendInput)
		{
			FText DisplayName = InFrontendInput.GetDisplayName();
			if (DisplayName.IsEmptyOrWhitespace())
			{
				DisplayName = FText::FromName(InFrontendInput.GetName());
			}
			return DisplayName;
		}

		FText FGraphBuilder::GetDisplayName(const Frontend::IOutputController& InFrontendOutput)
		{
			FText DisplayName = InFrontendOutput.GetDisplayName();
			if (DisplayName.IsEmptyOrWhitespace())
			{
				DisplayName = FText::FromName(InFrontendOutput.GetName());
			}
			return DisplayName;
		}

		FText FGraphBuilder::GetDisplayName(const Frontend::IVariableController& InFrontendVariable, bool bInIncludeNamespace)
		{
			FText DisplayName = InFrontendVariable.GetDisplayName();
			if (DisplayName.IsEmptyOrWhitespace())
			{
				FName Namespace;
				FName ParameterName;
				Audio::FParameterPath::SplitName(InFrontendVariable.GetName(), Namespace, ParameterName);

				DisplayName = FText::FromName(ParameterName);
				if (bInIncludeNamespace && !Namespace.IsNone())
				{
					return FText::Format(LOCTEXT("ClassMetadataDisplayNameWithNamespaceFormat", "{0} ({1})"), DisplayName, FText::FromName(Namespace));
				}
			}

			return DisplayName;
		}

		FName FGraphBuilder::GetPinName(const Frontend::IOutputController& InFrontendOutput)
		{
			using namespace VariableNames; 

			Frontend::FConstNodeHandle OwningNode = InFrontendOutput.GetOwningNode();
			EMetasoundFrontendClassType OwningNodeClassType = OwningNode->GetClassMetadata().GetType();

			switch (OwningNodeClassType)
			{
				case EMetasoundFrontendClassType::Variable:
				case EMetasoundFrontendClassType::VariableAccessor:
				case EMetasoundFrontendClassType::VariableDeferredAccessor:
				case EMetasoundFrontendClassType::VariableMutator:
				{
					// All variables nodes use the same pin name for user-modifiable node
					// inputs and outputs and the editor does not display the pin's name. The
					// editor instead displays the variable's name in place of the pin name to
					// maintain a consistent look and behavior to input and output nodes.
					return METASOUND_GET_PARAM_NAME(OutputData);
				}
				case EMetasoundFrontendClassType::Input:
				case EMetasoundFrontendClassType::Output:
				{
					return OwningNode->GetNodeName();
				}

				default:
				{
					return InFrontendOutput.GetName();
				}
			}
		}

		FName FGraphBuilder::GetPinName(const Frontend::IInputController& InFrontendInput)
		{
			using namespace VariableNames;
			
			Frontend::FConstNodeHandle OwningNode = InFrontendInput.GetOwningNode();
			EMetasoundFrontendClassType OwningNodeClassType = OwningNode->GetClassMetadata().GetType();

			switch (OwningNodeClassType)
			{
				case EMetasoundFrontendClassType::Variable:
				case EMetasoundFrontendClassType::VariableAccessor:
				case EMetasoundFrontendClassType::VariableDeferredAccessor:
				case EMetasoundFrontendClassType::VariableMutator:
				{
					// All variables nodes use the same pin name for user-modifiable node
					// inputs and outputs and the editor does not display the pin's name. The
					// editor instead displays the variable's name in place of the pin name to
					// maintain a consistent look and behavior to input and output nodes.
					return METASOUND_GET_PARAM_NAME(InputData);
				}

				case EMetasoundFrontendClassType::Input:
				case EMetasoundFrontendClassType::Output:
				{
					return OwningNode->GetNodeName();
				}

				default:
				{
					return InFrontendInput.GetName();
				}
			}
		}

		UMetasoundEditorGraphExternalNode* FGraphBuilder::AddExternalNode(UObject& InMetaSound, Frontend::FNodeHandle& InNodeHandle, FVector2D InLocation, bool bInSelectNewNode)
		{
			using namespace Frontend;

			UMetasoundEditorGraphExternalNode* NewGraphNode = nullptr;

			const EMetasoundFrontendClassType ClassType = InNodeHandle->GetClassMetadata().GetType();
			const bool bIsExternalNode = ClassType == EMetasoundFrontendClassType::External;
			const bool bIsTemplateNode = ClassType == EMetasoundFrontendClassType::Template;
			if (!ensure(bIsExternalNode || bIsTemplateNode))
			{
				return nullptr;
			}

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			UEdGraph& Graph = MetaSoundAsset->GetGraphChecked();
			FGraphNodeCreator<UMetasoundEditorGraphExternalNode> NodeCreator(Graph);

			NewGraphNode = NodeCreator.CreateNode(bInSelectNewNode);
			if (ensure(NewGraphNode))
			{
				const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(InNodeHandle->GetClassMetadata());
				NewGraphNode->bIsClassNative = FMetasoundFrontendRegistryContainer::Get()->IsNodeNative(RegistryKey);
				NewGraphNode->ClassName = InNodeHandle->GetClassMetadata().GetClassName();
				NewGraphNode->CacheTitle();

				NodeCreator.Finalize();
				InitGraphNode(InNodeHandle, NewGraphNode, InMetaSound);
				NewGraphNode->SetNodeLocation(InLocation);

				// Adding external node may introduce referenced asset so rebuild referenced keys.
				MetaSoundAsset->RebuildReferencedAssetClasses();
			}

			return NewGraphNode;
		}

		UMetasoundEditorGraphExternalNode* FGraphBuilder::AddExternalNode(UObject& InMetaSound, const FMetasoundFrontendClassMetadata& InMetadata, FVector2D InLocation, bool bInSelectNewNode)
		{
			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			Frontend::FNodeHandle NodeHandle = MetaSoundAsset->GetRootGraphHandle()->AddNode(InMetadata);
			return AddExternalNode(InMetaSound, NodeHandle, InLocation, bInSelectNewNode);
		}

		UMetasoundEditorGraphVariableNode* FGraphBuilder::AddVariableNode(UObject& InMetaSound, Frontend::FNodeHandle& InNodeHandle, FVector2D InLocation, bool bInSelectNewNode)
		{
			using namespace Frontend;

			EMetasoundFrontendClassType ClassType = InNodeHandle->GetClassMetadata().GetType();
			const bool bIsSupportedClassType = (ClassType == EMetasoundFrontendClassType::VariableAccessor) 
				|| (ClassType == EMetasoundFrontendClassType::VariableDeferredAccessor)
				|| (ClassType == EMetasoundFrontendClassType::VariableMutator);

			if (!ensure(bIsSupportedClassType))
			{
				return nullptr;
			}

			FConstVariableHandle FrontendVariable = InNodeHandle->GetOwningGraph()->FindVariableContainingNode(InNodeHandle->GetID());
			if (!ensure(FrontendVariable->IsValid()))
			{
				return nullptr;
			}

			UMetasoundEditorGraphVariableNode* NewGraphNode = nullptr;
			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			if (ensure(nullptr != MetaSoundAsset))
			{
				if (UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph()))
				{
					FGraphNodeCreator<UMetasoundEditorGraphVariableNode> NodeCreator(*MetasoundGraph);

					NewGraphNode = NodeCreator.CreateNode(bInSelectNewNode);
					if (ensure(NewGraphNode))
					{
						NewGraphNode->ClassName = InNodeHandle->GetClassMetadata().GetClassName();
						NewGraphNode->ClassType = ClassType;
						NodeCreator.Finalize();
						InitGraphNode(InNodeHandle, NewGraphNode, InMetaSound);

						UMetasoundEditorGraphVariable* Variable = MetasoundGraph->FindOrAddVariable(FrontendVariable);
						if (ensure(Variable))
						{
							NewGraphNode->Variable = Variable;

							// Ensures the variable node value is synced with the editor literal value should it be set
							constexpr bool bPostTransaction = false;
							Variable->UpdateFrontendDefaultLiteral(bPostTransaction);
						}

						MetasoundGraph->GetModifyContext().AddNodeIDsModified({ NewGraphNode->GetNodeID() });
						NewGraphNode->SetNodeLocation(InLocation);
					}
				}
			}

			return NewGraphNode;
		}

		UMetasoundEditorGraphOutputNode* FGraphBuilder::AddOutputNode(UObject& InMetaSound, Frontend::FNodeHandle& InNodeHandle, FVector2D InLocation, bool bInSelectNewNode)
		{
			using namespace Frontend;

			UMetasoundEditorGraphOutputNode* NewGraphNode = nullptr;
			if (!ensure(InNodeHandle->GetClassMetadata().GetType() == EMetasoundFrontendClassType::Output))
			{
				return nullptr;
			}

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			UEdGraph& Graph = MetaSoundAsset->GetGraphChecked();
			FGraphNodeCreator<UMetasoundEditorGraphOutputNode> NodeCreator(Graph);

			NewGraphNode = NodeCreator.CreateNode(bInSelectNewNode);
			if (ensure(NewGraphNode))
			{
				UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(&Graph);

				UMetasoundEditorGraphOutput* Output = MetasoundGraph->FindOrAddOutput(InNodeHandle);
				if (ensure(Output))
				{
					NewGraphNode->Output = Output;
					NodeCreator.Finalize();
					InitGraphNode(InNodeHandle, NewGraphNode, InMetaSound);

					// Ensures the output node value is synced with the editor literal value should it be set
					constexpr bool bPostTransaction = false;
					Output->UpdateFrontendDefaultLiteral(bPostTransaction);

					MetasoundGraph->GetModifyContext().AddNodeIDsModified({ NewGraphNode->GetNodeID() });
				}

				NewGraphNode->CacheTitle();
				NewGraphNode->SetNodeLocation(InLocation);
			}

			return NewGraphNode;
		}

		void FGraphBuilder::InitGraphNode(Frontend::FNodeHandle& InNodeHandle, UMetasoundEditorGraphNode* NewGraphNode, UObject& InMetaSound)
		{
			NewGraphNode->SetNodeID(InNodeHandle->GetID());
			RebuildNodePins(*NewGraphNode);
		}

		FGraphValidationResults FGraphBuilder::ValidateGraph(UObject& InMetaSound)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FGraphBuilder::ValidateGraph);

			using namespace Frontend;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			// Validate referenced graphs first to ensure all editor data
			// is up-to-date prior to validating this referencing graph to
			// allow errors to bubble up.
			TArray<FMetasoundAssetBase*> References;
			ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(*MetaSoundAsset, References));
			for (FMetasoundAssetBase* Reference : References)
			{
				check(Reference);
				ValidateGraph(*Reference->GetOwningAsset());
			}

			FGraphValidationResults Results;
			UMetasoundEditorGraph& Graph = *CastChecked<UMetasoundEditorGraph>(&MetaSoundAsset->GetGraphChecked());
			Graph.ValidateInternal(Results);
			return Results;
		}

		TArray<FString> FGraphBuilder::GetDataTypeNameCategories(const FName& InDataTypeName)
		{
			FString CategoryString = InDataTypeName.ToString();

			TArray<FString> Categories;
			CategoryString.ParseIntoArray(Categories, TEXT(":"));

			if (Categories.Num() > 0)
			{
				// Remove name
				Categories.RemoveAt(Categories.Num() - 1);
			}

			return Categories;
		}

		FName FGraphBuilder::GenerateUniqueNameByClassType(const UObject& InMetaSound, EMetasoundFrontendClassType InClassType, const FString& InBaseName)
		{
			const FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			// Get existing names.
			TArray<FName> ExistingNames;
			MetaSoundAsset->GetRootGraphHandle()->IterateConstNodes([&](const Frontend::FConstNodeHandle& Node)
			{
				ExistingNames.Add(Node->GetNodeName());
			}, InClassType);

			return GraphBuilderPrivate::GenerateUniqueName(ExistingNames, InBaseName);
		}

		TSharedPtr<FEditor> FGraphBuilder::GetEditorForMetasound(const UObject& Metasound)
		{
			// TODO: FToolkitManager is deprecated. Replace with UAssetEditorSubsystem.
			if (TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(&Metasound))
			{
				if (FEditor::EditorName == FoundAssetEditor->GetToolkitFName())
				{
					return StaticCastSharedPtr<FEditor, IToolkit>(FoundAssetEditor);
				}
			}

			return { };
		}

		TSharedPtr<FEditor> FGraphBuilder::GetEditorForGraph(const UEdGraph& EdGraph)
		{
			if (const UMetasoundEditorGraph* MetasoundGraph = Cast<const UMetasoundEditorGraph>(&EdGraph))
			{
				return GetEditorForMetasound(MetasoundGraph->GetMetasoundChecked());
			}

			return { };
		}

		TSharedPtr<FEditor> FGraphBuilder::GetEditorForNode(const UEdGraphNode& InEdNode)
		{
			if (const UMetasoundEditorGraph* Graph = Cast<UMetasoundEditorGraph>(InEdNode.GetGraph()))
			{
				return FGraphBuilder::GetEditorForGraph(*Graph);
			}

			return { };
		}

		TSharedPtr<FEditor> FGraphBuilder::GetEditorForPin(const UEdGraphPin& InEdPin)
		{
			if (const UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(InEdPin.GetOwningNode()))
			{
				return GetEditorForNode(*Node);
			}

			return { };
		}

		FLinearColor FGraphBuilder::GetPinCategoryColor(const FEdGraphPinType& PinType)
		{
			const UMetasoundEditorSettings* Settings = GetDefault<UMetasoundEditorSettings>();
			check(Settings);

			if (PinType.PinCategory == PinCategoryAudio)
			{
				return Settings->AudioPinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryBoolean)
			{
				return Settings->BooleanPinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryFloat)
			{
				return Settings->FloatPinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryInt32)
			{
				return Settings->IntPinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryObject)
			{
				return Settings->ObjectPinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryString)
			{
				return Settings->StringPinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryTime || PinType.PinCategory == PinCategoryTimeArray)
			{
				return Settings->TimePinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryTrigger)
			{
				return Settings->TriggerPinTypeColor;
			}

			if (PinType.PinCategory == PinCategoryWaveTable)
			{
				return Settings->WaveTablePinTypeColor;
			}

			return Settings->DefaultPinTypeColor;
		}

		Frontend::FInputHandle FGraphBuilder::GetInputHandleFromPin(const UEdGraphPin* InPin)
		{
			using namespace Frontend;
			using namespace VariableNames;

			if (InPin && ensure(InPin->Direction == EGPD_Input))
			{
				if (UMetasoundEditorGraphVariableNode* EdVariableNode = Cast<UMetasoundEditorGraphVariableNode>(InPin->GetOwningNode()))
				{
					// UEdGraphPins on variable nodes use the variable's name for display
					// purposes instead of the underlying vertex's name. The frontend vertices
					// of a variable node have consistent names no matter what the 
					// variable is named.
					return EdVariableNode->GetNodeHandle()->GetInputWithVertexName(METASOUND_GET_PARAM_NAME(InputData));
				}
				else if (UMetasoundEditorGraphNode* EdNode = CastChecked<UMetasoundEditorGraphNode>(InPin->GetOwningNode()))
				{
					return EdNode->GetNodeHandle()->GetInputWithVertexName(InPin->GetFName());
				}
			}

			return IInputController::GetInvalidHandle();
		}

		Frontend::FConstInputHandle FGraphBuilder::GetConstInputHandleFromPin(const UEdGraphPin* InPin)
		{
			return GetInputHandleFromPin(InPin);
		}

		FName FGraphBuilder::GetPinDataType(const UEdGraphPin* InPin)
		{
			using namespace Frontend;

			if (InPin)
			{
				if (InPin->Direction == EGPD_Input)
				{
					FConstInputHandle InputHandle = GetConstInputHandleFromPin(InPin);
					return InputHandle->GetDataType();
				}
				else // EGPD_Output
				{
					FConstOutputHandle OutputHandle = GetConstOutputHandleFromPin(InPin);
					return OutputHandle->GetDataType();
				}
			}

			return { };
		}

		Frontend::FOutputHandle FGraphBuilder::GetOutputHandleFromPin(const UEdGraphPin* InPin)
		{
			using namespace Frontend;
			using namespace VariableNames; 

			if (InPin && ensure(InPin->Direction == EGPD_Output))
			{
				if (UMetasoundEditorGraphVariableNode* EdVariableNode = Cast<UMetasoundEditorGraphVariableNode>(InPin->GetOwningNode()))
				{
					// UEdGraphPins on variable nodes use the variable's name for display
					// purposes instead of the underlying vertex's name. The frontend vertices
					// of a variable node have consistent names no matter what the 
					// variable is named.
					return EdVariableNode->GetNodeHandle()->GetOutputWithVertexName(METASOUND_GET_PARAM_NAME(OutputData));
				}
				else if (UMetasoundEditorGraphNode* EdNode = CastChecked<UMetasoundEditorGraphNode>(InPin->GetOwningNode()))
				{
					return EdNode->GetNodeHandle()->GetOutputWithVertexName(InPin->GetFName());
				}
			}

			return IOutputController::GetInvalidHandle();
		}

		const FMetasoundFrontendEdgeStyle* FGraphBuilder::GetOutputEdgeStyle(Frontend::FConstOutputHandle InOutputHandle)
		{
			using namespace Frontend;

			if (InOutputHandle->IsValid())
			{
				FConstNodeHandle NodeHandle = InOutputHandle->GetOwningNode();
				FConstGraphHandle GraphHandle = NodeHandle->GetOwningGraph();

				const TArray<FMetasoundFrontendEdgeStyle>& EdgeStyles = GraphHandle->GetGraphStyle().EdgeStyles;
				return EdgeStyles.FindByPredicate([&InOutputHandle](const FMetasoundFrontendEdgeStyle& InCandidate)
				{
					return InCandidate.NodeID == InOutputHandle->GetOwningNodeID() && InCandidate.OutputName == InOutputHandle->GetName();
				});
			}

			return nullptr;
		}

		const FMetasoundFrontendEdgeStyle* FGraphBuilder::GetOutputEdgeStyle(const UEdGraphPin* InGraphPin)
		{
			using namespace Frontend;

			FConstOutputHandle OutputHandle = FindReroutedConstOutputHandleFromPin(InGraphPin);
			return GetOutputEdgeStyle(OutputHandle);
		}

		Frontend::FConstOutputHandle FGraphBuilder::GetConstOutputHandleFromPin(const UEdGraphPin* InPin)
		{
			return GetOutputHandleFromPin(InPin);
		}

		UEdGraphPin* FGraphBuilder::FindReroutedOutputPin(UEdGraphPin* OutputPin)
		{
			using namespace Frontend;

			if (OutputPin)
			{
				if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(OutputPin->GetOwningNode()))
				{
					if (ExternalNode->GetClassName() == FRerouteNodeTemplate::ClassName)
					{
						auto IsInput = [](UEdGraphPin* Pin) { check(Pin); return Pin->Direction == EGPD_Input; };
						if (UEdGraphPin* RerouteInput = *ExternalNode->Pins.FindByPredicate(IsInput))
						{
							TArray<UEdGraphPin*>& LinkedTo = RerouteInput->LinkedTo;
							if (!LinkedTo.IsEmpty())
							{
								UEdGraphPin* ReroutedOutput = LinkedTo.Last();
								return FindReroutedOutputPin(ReroutedOutput);
							}
						}
					}
				}
			}

			return OutputPin;
		}

		const UEdGraphPin* FGraphBuilder::FindReroutedOutputPin(const UEdGraphPin* OutputPin)
		{
			using namespace Frontend;

			if (OutputPin)
			{
				if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(OutputPin->GetOwningNode()))
				{
					if (ExternalNode->GetClassName() == FRerouteNodeTemplate::ClassName)
					{
						auto IsInput = [](const UEdGraphPin* Pin) { check(Pin); return Pin->Direction == EGPD_Input; };
						if (const UEdGraphPin* RerouteInput = *ExternalNode->Pins.FindByPredicate(IsInput))
						{
							const TArray<UEdGraphPin*>& LinkedTo = RerouteInput->LinkedTo;
							if (!LinkedTo.IsEmpty())
							{
								const UEdGraphPin* ReroutedOutput = LinkedTo.Last();
								return FindReroutedOutputPin(ReroutedOutput);
							}
						}
					}
				}
			}

			return OutputPin;
		}

		Frontend::FOutputHandle FGraphBuilder::FindReroutedOutputHandleFromPin(const UEdGraphPin* OutputPin)
		{
			using namespace Frontend;

			if (OutputPin)
			{
				if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(OutputPin->GetOwningNode()))
				{
					if (ExternalNode->GetClassName() == FRerouteNodeTemplate::ClassName)
					{
						auto IsInput = [](UEdGraphPin* Pin) { check(Pin); return Pin->Direction == EGPD_Input; };
						if (UEdGraphPin* RerouteInput = *ExternalNode->Pins.FindByPredicate(IsInput))
						{
							TArray<UEdGraphPin*>& LinkedTo = RerouteInput->LinkedTo;
							if (!LinkedTo.IsEmpty())
							{
								UEdGraphPin* ReroutedOutput = LinkedTo.Last();
								return FindReroutedOutputHandleFromPin(ReroutedOutput);
							}
						}
					}
				}

				return GetOutputHandleFromPin(OutputPin);
			}

			return IOutputController::GetInvalidHandle();
		}

		Frontend::FConstOutputHandle FGraphBuilder::FindReroutedConstOutputHandleFromPin(const UEdGraphPin* InPin)
		{
			return FindReroutedOutputHandleFromPin(InPin);
		}

		void FGraphBuilder::FindReroutedInputPins(UEdGraphPin* InPinToCheck, TArray<UEdGraphPin*>& InOutInputPins)
		{
			using namespace Frontend;

			if (InPinToCheck && InPinToCheck->Direction == EGPD_Input)
			{
				if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(InPinToCheck->GetOwningNode()))
				{
					if (ExternalNode->GetClassName() == FRerouteNodeTemplate::ClassName)
					{
						for (UEdGraphPin* Pin : ExternalNode->Pins)
						{
							if (Pin->Direction == EGPD_Output)
							{
								for (UEdGraphPin* LinkedInput : Pin->LinkedTo)
								{
									FindReroutedInputPins(LinkedInput, InOutInputPins);
								}
							}
						}

						return;
					}
				}

				InOutInputPins.Add(InPinToCheck);
			}
		}

		bool FGraphBuilder::GraphContainsErrors(const UObject& InMetaSound)
		{
			const FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			const UMetasoundEditorGraph* EditorGraph = CastChecked<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());

			// Get all editor nodes from editor graph (some nodes on graph may *NOT* be metasound ed nodes,
			// such as comment boxes, etc, so just get nodes of class UMetasoundEditorGraph).
			TArray<const UMetasoundEditorGraphNode*> EditorNodes;
			EditorGraph->GetNodesOfClass(EditorNodes);

			// Do not synchronize with errors present as the graph is expected to be malformed.
			return Algo::AnyOf(EditorNodes, [](const UMetasoundEditorGraphNode* Node) { return Node->ErrorType == EMessageSeverity::Error; });
		}

		bool FGraphBuilder::SynchronizeNodeLocation(const Frontend::FConstNodeHandle& InNode, UMetasoundEditorGraphNode& OutGraphNode)
		{
			bool bModified = false;

			const FMetasoundFrontendNodeStyle& Style = InNode->GetNodeStyle();

			const FVector2D* Location = Style.Display.Locations.Find(OutGraphNode.NodeGuid);
			if (!Location)
			{
				// If no specific location found, use default location if provided (zero guid
				// for example, provided by preset defaults.)
				Location = Style.Display.Locations.Find({ });
			}

			if (Location)
			{
				const int32 LocX = FMath::TruncToInt(Location->X);
				const int32 LocY = FMath::TruncToInt(Location->Y);
				const bool bXChanged = static_cast<bool>(LocX - OutGraphNode.NodePosX);
				const bool bYChanged = static_cast<bool>(LocY - OutGraphNode.NodePosY);
				if (bXChanged || bYChanged)
				{
					OutGraphNode.NodePosX = LocX;
					OutGraphNode.NodePosY = LocY;
					bModified = true;
				}
			}

			return bModified;
		}

		UMetasoundEditorGraphInputNode* FGraphBuilder::AddInputNode(UObject& InMetaSound, Frontend::FNodeHandle InNodeHandle, FVector2D InLocation, bool bInSelectNewNode)
		{
			using namespace Frontend;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			UMetasoundEditorGraph* MetasoundGraph = Cast<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());
			if (!ensure(MetasoundGraph))
			{
				return nullptr;
			}

			UMetasoundEditorGraphInputNode* NewGraphNode = MetasoundGraph->CreateInputNode(InNodeHandle, bInSelectNewNode);
			if (ensure(NewGraphNode))
			{
				NewGraphNode->SetNodeLocation(InLocation);
				RebuildNodePins(*NewGraphNode);
				MetasoundGraph->GetModifyContext().AddNodeIDsModified({ NewGraphNode->GetNodeID() });
				return NewGraphNode;
			}

			return nullptr;
		}

		bool FGraphBuilder::GetPinLiteral(UEdGraphPin& InInputPin, FMetasoundFrontendLiteral& OutDefaultLiteral)
		{
			using namespace Frontend;

			IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");

			FInputHandle InputHandle = GetInputHandleFromPin(&InInputPin);
			if (!ensure(InputHandle->IsValid()))
			{
				return false;
			}

			const FString& InStringValue = InInputPin.DefaultValue;
			const FName TypeName = InputHandle->GetDataType();

			FDataTypeRegistryInfo DataTypeInfo;
			IDataTypeRegistry::Get().GetDataTypeInfo(TypeName, DataTypeInfo);
			switch (DataTypeInfo.PreferredLiteralType)
			{
				case ELiteralType::Boolean:
				{
					// Currently don't support triggers being initialized to boolean in-graph
					if (GetMetasoundDataTypeName<FTrigger>() != TypeName)
					{
						OutDefaultLiteral.Set(FCString::ToBool(*InStringValue));
					}
				}
				break;

				case ELiteralType::Float:
				{
					OutDefaultLiteral.Set(FCString::Atof(*InStringValue));
				}
				break;

				case ELiteralType::Integer:
				{
					OutDefaultLiteral.Set(FCString::Atoi(*InStringValue));
				}
				break;

				case ELiteralType::String:
				{
					OutDefaultLiteral.Set(InStringValue);
				}
				break;

				case ELiteralType::UObjectProxy:
				{
					bool bObjectFound = false;
					if (!InInputPin.DefaultValue.IsEmpty())
					{
						if (UClass* Class = IDataTypeRegistry::Get().GetUClassForDataType(TypeName))
						{
							FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

							// Remove class prefix if included in default value path
							FString ObjectPath = InInputPin.DefaultValue;
							ObjectPath.RemoveFromStart(Class->GetName() + TEXT(" "));

							FARFilter Filter;
							Filter.bRecursiveClasses = false;
							Filter.SoftObjectPaths.Add(FSoftObjectPath(ObjectPath));

							TArray<FAssetData> AssetData;
							AssetRegistryModule.Get().GetAssets(Filter, AssetData);
							if (!AssetData.IsEmpty())
							{
								if (UObject* AssetObject = AssetData.GetData()->GetAsset())
								{
									const UClass* AssetClass = AssetObject->GetClass();
									if (ensureAlways(AssetClass))
									{
										if (AssetClass->IsChildOf(Class))
										{
											Filter.ClassPaths.Add(Class->GetClassPathName());
											OutDefaultLiteral.Set(AssetObject);
											bObjectFound = true;
										}
									}
								}
							}
						}
					}
					
					if (!bObjectFound)
					{
						OutDefaultLiteral.Set(static_cast<UObject*>(nullptr));
					}
				}
				break;

				case ELiteralType::BooleanArray:
				{
					OutDefaultLiteral.Set(TArray<bool>());
				}
				break;

				case ELiteralType::FloatArray:
				{
					OutDefaultLiteral.Set(TArray<float>());
				}
				break;

				case ELiteralType::IntegerArray:
				{
					OutDefaultLiteral.Set(TArray<int32>());
				}
				break;

				case ELiteralType::NoneArray:
				{
					OutDefaultLiteral.Set(FMetasoundFrontendLiteral::FDefaultArray());
				}
				break;

				case ELiteralType::StringArray:
				{
					OutDefaultLiteral.Set(TArray<FString>());
				}
				break;

				case ELiteralType::UObjectProxyArray:
				{
					OutDefaultLiteral.Set(TArray<UObject*>());
				}
				break;

				case ELiteralType::None:
				{
					OutDefaultLiteral.Set(FMetasoundFrontendLiteral::FDefault());
				}
				break;

				case ELiteralType::Invalid:
				default:
				{
					static_assert(static_cast<int32>(ELiteralType::COUNT) == 13, "Possible missing ELiteralType case coverage.");
					ensureMsgf(false, TEXT("Failed to set input node default: Literal type not supported"));
					return false;
				}
				break;
			}

			return true;
		}

		Frontend::FNodeHandle FGraphBuilder::AddNodeHandle(UObject& InMetaSound, UMetasoundEditorGraphNode& InGraphNode)
		{
			using namespace Frontend;

			FNodeHandle NodeHandle = INodeController::GetInvalidHandle();
			if (UMetasoundEditorGraphInputNode* InputNode = Cast<UMetasoundEditorGraphInputNode>(&InGraphNode))
			{
				const TArray<UEdGraphPin*>& Pins = InGraphNode.GetAllPins();
				const UEdGraphPin* Pin = Pins.IsEmpty() ? nullptr : Pins[0];
				if (ensure(Pin) && ensure(Pin->Direction == EGPD_Output))
				{
					UMetasoundEditorGraphInput* Input = InputNode->Input;
					if (ensure(Input))
					{
						const FName PinName = Pin->GetFName();
						FCreateNodeVertexParams VertexParams;
						VertexParams.DataType = Input->GetDataType();

						NodeHandle = AddInputNodeHandle(InMetaSound, VertexParams, nullptr, &PinName);
						NodeHandle->SetDescription(InGraphNode.GetTooltipText());
					}
				}
			}

			else if (UMetasoundEditorGraphOutputNode* OutputNode = Cast<UMetasoundEditorGraphOutputNode>(&InGraphNode))
			{
				const TArray<UEdGraphPin*>& Pins = InGraphNode.GetAllPins();
				const UEdGraphPin* Pin = Pins.IsEmpty() ? nullptr : Pins[0];
				if (ensure(Pin) && ensure(Pin->Direction == EGPD_Input))
				{
					UMetasoundEditorGraphOutput* Output = OutputNode->Output;
					if (ensure(Output))
					{
						const FName PinName = Pin->GetFName();
						FCreateNodeVertexParams VertexParams;
						VertexParams.DataType = Output->GetDataType();

						NodeHandle = FGraphBuilder::AddOutputNodeHandle(InMetaSound, VertexParams, &PinName);
						NodeHandle->SetDescription(InGraphNode.GetTooltipText());
					}
				}
			}
			else if (UMetasoundEditorGraphVariableNode* VariableNode = Cast<UMetasoundEditorGraphVariableNode>(&InGraphNode))
			{
				NodeHandle = FGraphBuilder::AddVariableNodeHandle(InMetaSound, VariableNode->Variable->GetVariableID(), VariableNode->GetClassName().ToNodeClassName());
			}
			else if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(&InGraphNode))
			{
				FMetasoundFrontendClass FrontendClass;
				bool bDidFindClassWithName = ISearchEngine::Get().FindClassWithHighestVersion(ExternalNode->ClassName.ToNodeClassName(), FrontendClass);
				if (ensure(bDidFindClassWithName))
				{
					FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
					check(MetaSoundAsset);

					Frontend::FNodeHandle NewNode = MetaSoundAsset->GetRootGraphHandle()->AddNode(FrontendClass.Metadata);
					ExternalNode->SetNodeID(NewNode->GetID());

					NodeHandle = NewNode;
				}
			}

			if (NodeHandle->IsValid())
			{
				FMetasoundFrontendNodeStyle Style = NodeHandle->GetNodeStyle();
				Style.Display.Locations.Add(InGraphNode.NodeGuid, FVector2D(InGraphNode.NodePosX, InGraphNode.NodePosY));
				NodeHandle->SetNodeStyle(Style);
			}

			return NodeHandle;
		}

		Frontend::FNodeHandle FGraphBuilder::AddInputNodeHandle(UObject& InMetaSound, const FName InTypeName, const FMetasoundFrontendLiteral* InDefaultValue, const FName* InNameBase)
		{
			UE_LOG(LogMetaSound, Error, TEXT("FGraphBuilder::AddInputNodeHandle with these parameters is no longer supported and should not be called. Use the one with FCreateNodeVertexParams instead."));
			return Frontend::INodeController::GetInvalidHandle();
		}

		Frontend::FNodeHandle FGraphBuilder::AddInputNodeHandle(UObject& InMetaSound, const FCreateNodeVertexParams& InParams, const FMetasoundFrontendLiteral* InDefaultValue, const FName* InNameBase)
		{
			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			const FGuid VertexID = FGuid::NewGuid();

			FMetasoundFrontendClassInput ClassInput;
			ClassInput.Name = GenerateUniqueNameByClassType(InMetaSound, EMetasoundFrontendClassType::Input, InNameBase ? InNameBase->ToString() : TEXT("Input"));
			ClassInput.TypeName = InParams.DataType;
			ClassInput.VertexID = VertexID;

			// Can be unset if attempting to mirror parameters from a reroute, so default to reference
			ClassInput.AccessType = InParams.AccessType == EMetasoundFrontendVertexAccessType::Unset ? EMetasoundFrontendVertexAccessType::Reference : InParams.AccessType;

			if (nullptr != InDefaultValue)
			{
				ClassInput.DefaultLiteral = *InDefaultValue;
			}
			else
			{
				Metasound::FLiteral Literal = Frontend::IDataTypeRegistry::Get().CreateDefaultLiteral(InParams.DataType);
				ClassInput.DefaultLiteral.SetFromLiteral(Literal);
			}

			return MetaSoundAsset->GetRootGraphHandle()->AddInputVertex(ClassInput);
		}
		
		Frontend::FNodeHandle FGraphBuilder::AddOutputNodeHandle(UObject& InMetaSound, const FName InTypeName, const FName* InNameBase)
		{
			UE_LOG(LogMetaSound, Error, TEXT("FGraphBuilder::AddOutputNodeHandle with these parameters is no longer supported and should not be called. Use the one with FCreateNodeVertexParams instead."));
			return Frontend::INodeController::GetInvalidHandle();
		}

		Frontend::FNodeHandle FGraphBuilder::AddOutputNodeHandle(UObject& InMetaSound, const FCreateNodeVertexParams& InParams, const FName* InNameBase)
		{
			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			const FGuid VertexID = FGuid::NewGuid();

			const FName NewName = GenerateUniqueNameByClassType(InMetaSound, EMetasoundFrontendClassType::Output, InNameBase ? InNameBase->ToString() : TEXT("Output"));

			FMetasoundFrontendClassOutput ClassOutput;
			ClassOutput.Name = NewName;
			ClassOutput.TypeName = InParams.DataType;
			ClassOutput.VertexID = VertexID;

			// Can be unset if attempting to mirror parameters from a reroute, so default to reference
			ClassOutput.AccessType = InParams.AccessType == EMetasoundFrontendVertexAccessType::Unset ? EMetasoundFrontendVertexAccessType::Reference : InParams.AccessType;

			return MetaSoundAsset->GetRootGraphHandle()->AddOutputVertex(ClassOutput);
		}

		FName FGraphBuilder::GenerateUniqueVariableName(const Frontend::FConstGraphHandle& InFrontendGraph, const FString& InBaseName)
		{
			using namespace Frontend;

			TArray<FName> ExistingVariableNames;

			// Get all the names from the existing variables on the graph
			// and place into the ExistingVariableNames array.
			Algo::Transform(InFrontendGraph->GetVariables(), ExistingVariableNames, [](const FConstVariableHandle& Var) { return Var->GetName(); });

			return GraphBuilderPrivate::GenerateUniqueName(ExistingVariableNames, InBaseName);
		}

		Frontend::FVariableHandle FGraphBuilder::AddVariableHandle(UObject& InMetaSound, const FName& InTypeName)
		{
			using namespace Frontend;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			FGraphHandle FrontendGraph = MetaSoundAsset->GetRootGraphHandle();

			FText BaseDisplayName = LOCTEXT("VariableDefaultDisplayName", "Variable");

			FString BaseName = BaseDisplayName.ToString();
			FName VariableName = GenerateUniqueVariableName(FrontendGraph, BaseName);
			FVariableHandle Variable = FrontendGraph->AddVariable(InTypeName);

			Variable->SetDisplayName(FText::GetEmpty());
			Variable->SetName(VariableName);

			return Variable;
		}

		Frontend::FNodeHandle FGraphBuilder::AddVariableNodeHandle(UObject& InMetaSound, const FGuid& InVariableID, const Metasound::FNodeClassName& InVariableNodeClassName, UMetasoundEditorGraphVariableNode* InVariableNode)
		{
			using namespace Frontend;

			FNodeHandle FrontendNode = INodeController::GetInvalidHandle();

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			
			if (ensure(MetaSoundAsset))
			{
				FMetasoundFrontendClass FrontendClass;
				bool bDidFindClassWithName = ISearchEngine::Get().FindClassWithHighestVersion(InVariableNodeClassName, FrontendClass);
				if (ensure(bDidFindClassWithName))
				{
					FGraphHandle Graph = MetaSoundAsset->GetRootGraphHandle();

					switch (FrontendClass.Metadata.GetType())
					{
						case EMetasoundFrontendClassType::VariableDeferredAccessor:
							FrontendNode = Graph->AddVariableDeferredAccessorNode(InVariableID);
							break;

						case EMetasoundFrontendClassType::VariableAccessor:
							FrontendNode = Graph->AddVariableAccessorNode(InVariableID);
							break;

						case EMetasoundFrontendClassType::VariableMutator:
							{
								FConstVariableHandle Variable = Graph->FindVariable(InVariableID);
								FConstNodeHandle ExistingMutator = Variable->FindMutatorNode();
								if (!ExistingMutator->IsValid())
								{
									FrontendNode = Graph->FindOrAddVariableMutatorNode(InVariableID);
								}
								else
								{
									UE_LOG(LogMetaSound, Error, TEXT("Cannot add node because \"%s\" already exists for variable \"%s\""), *ExistingMutator->GetDisplayName().ToString(), *Variable->GetDisplayName().ToString());
								}
							}
							break;

						default:
							{
								checkNoEntry();
							}
					}
				}
			}

			if (InVariableNode)
			{
				InVariableNode->ClassName = FrontendNode->GetClassMetadata().GetClassName();
				InVariableNode->ClassType = FrontendNode->GetClassMetadata().GetType();
				InVariableNode->SetNodeID(FrontendNode->GetID());
			}

			return FrontendNode;
		}

		UMetasoundEditorGraphNode* FGraphBuilder::AddNode(UObject& InMetaSound, Frontend::FNodeHandle InNodeHandle, FVector2D InLocation, bool bInSelectNewNode)
		{
			switch (InNodeHandle->GetClassMetadata().GetType())
			{
				case EMetasoundFrontendClassType::Input:
				{
					return CastChecked<UMetasoundEditorGraphNode>(AddInputNode(InMetaSound, InNodeHandle, InLocation, bInSelectNewNode));
				}
				break;

				case EMetasoundFrontendClassType::External:
				case EMetasoundFrontendClassType::Template:
				{
					return CastChecked<UMetasoundEditorGraphNode>(AddExternalNode(InMetaSound, InNodeHandle, InLocation, bInSelectNewNode));
				}
				break;

				case EMetasoundFrontendClassType::Output:
				{
					return CastChecked<UMetasoundEditorGraphNode>(AddOutputNode(InMetaSound, InNodeHandle, InLocation, bInSelectNewNode));
				}
				break;

				case EMetasoundFrontendClassType::VariableMutator:
				case EMetasoundFrontendClassType::VariableAccessor:
				case EMetasoundFrontendClassType::VariableDeferredAccessor:
				case EMetasoundFrontendClassType::Variable:
				{
					return CastChecked<UMetasoundEditorGraphNode>(AddVariableNode(InMetaSound, InNodeHandle, InLocation, bInSelectNewNode));
				}
				break;

				case EMetasoundFrontendClassType::Invalid:
				case EMetasoundFrontendClassType::Graph:
				
				case EMetasoundFrontendClassType::Literal: // Not yet supported in editor
				
				default:
				{
					checkNoEntry();
					static_assert(static_cast<int32>(EMetasoundFrontendClassType::Invalid) == 10, "Possible missing FMetasoundFrontendClassType case coverage");
				}
				break;
			}

			return nullptr;
		}

		bool FGraphBuilder::ConnectNodes(UEdGraphPin& InInputPin, UEdGraphPin& InOutputPin, bool bInConnectEdPins)
		{
			using namespace Frontend;

			// When true, will recursively call back into this function
			// from the schema if the editor pins are successfully connected
			if (bInConnectEdPins)
			{
				const UEdGraphSchema* Schema = InInputPin.GetSchema();
				if (ensure(Schema))
				{
					if (!Schema->TryCreateConnection(&InInputPin, &InOutputPin))
					{
						return false;
					}
				}
				else
				{
					return false;
				}
			}

			FInputHandle InputHandle = GetInputHandleFromPin(&InInputPin);
			FOutputHandle OutputHandle = GetOutputHandleFromPin(&InOutputPin);
			if (!InputHandle->IsValid() || !OutputHandle->IsValid())
			{
				return false;
			}

			if (!ensure(InputHandle->Connect(*OutputHandle)))
			{
				InInputPin.BreakLinkTo(&InOutputPin);
				return false;
			}

			return true;
		}

		void FGraphBuilder::DisconnectPinVertex(UEdGraphPin& InPin, bool bAddLiteralInputs)
		{
			using namespace Editor;
			using namespace Frontend;

			TArray<FInputHandle> InputHandles;
			TArray<UEdGraphPin*> InputPins;

			UMetasoundEditorGraphNode* Node = CastChecked<UMetasoundEditorGraphNode>(InPin.GetOwningNode());

			if (InPin.Direction == EGPD_Input)
			{
				const FName PinName = InPin.GetFName();

				FNodeHandle NodeHandle = Node->GetNodeHandle();
				FInputHandle InputHandle = NodeHandle->GetInputWithVertexName(PinName);

				// Input can be invalid if renaming a vertex member
				if (InputHandle->IsValid())
				{
					InputHandles.Add(InputHandle);
					InputPins.Add(&InPin);
				}
			}
			else
			{
				check(InPin.Direction == EGPD_Output);
				for (UEdGraphPin* Pin : InPin.LinkedTo)
				{
					check(Pin);
					FNodeHandle NodeHandle = CastChecked<UMetasoundEditorGraphNode>(Pin->GetOwningNode())->GetNodeHandle();
					FInputHandle InputHandle = NodeHandle->GetInputWithVertexName(Pin->GetFName());

					// Input can be invalid if renaming a vertex member
					if (InputHandle->IsValid())
					{
						InputHandles.Add(InputHandle);
						InputPins.Add(Pin);
					}
				}
			}

			for (int32 i = 0; i < InputHandles.Num(); ++i)
			{
				FInputHandle InputHandle = InputHandles[i];
				FConstOutputHandle OutputHandle = InputHandle->GetConnectedOutput();

				InputHandle->Disconnect();

				if (bAddLiteralInputs)
				{
					FNodeHandle NodeHandle = InputHandle->GetOwningNode();
					SynchronizePinLiteral(*InputPins[i]);
				}
			}

			UObject& MetaSound = Node->GetMetasoundChecked();
			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound);
			MetaSoundAsset->GetModifyContext().SetDocumentModified();
		}

		void FGraphBuilder::InitMetaSound(UObject& InMetaSound, const FString& InAuthor)
		{
			using namespace Frontend;
			using namespace GraphBuilderPrivate;

			FMetasoundFrontendClassMetadata Metadata;

			// 1. Set default class Metadata
			Metadata.SetClassName(FMetasoundFrontendClassName(FName(), *FGuid::NewGuid().ToString(), FName()));
			Metadata.SetVersion({ 1, 0 });
			Metadata.SetType(EMetasoundFrontendClassType::Graph);
			Metadata.SetAuthor(InAuthor);

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			MetaSoundAsset->SetMetadata(Metadata);

			// 2. Set default doc version Metadata
			FDocumentHandle DocumentHandle = MetaSoundAsset->GetDocumentHandle();
			FMetasoundFrontendDocumentMetadata* DocMetadata = DocumentHandle->GetMetadata();
			check(DocMetadata);
			DocMetadata->Version.Number = FMetasoundFrontendDocument::GetMaxVersion();

			MetaSoundAsset->AddDefaultInterfaces();

			FGraphHandle GraphHandle = MetaSoundAsset->GetRootGraphHandle();
			FVector2D InputNodeLocation = FVector2D::ZeroVector;
			FVector2D ExternalNodeLocation = InputNodeLocation + DisplayStyle::NodeLayout::DefaultOffsetX;
			FVector2D OutputNodeLocation = ExternalNodeLocation + DisplayStyle::NodeLayout::DefaultOffsetX;

			TArray<FNodeHandle> NodeHandles = GraphHandle->GetNodes();
			for (FNodeHandle& NodeHandle : NodeHandles)
			{
				const EMetasoundFrontendClassType NodeType = NodeHandle->GetClassMetadata().GetType();
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
				FMetasoundFrontendNodeStyle Style = NodeHandle->GetNodeStyle();
				// TODO: Find consistent location for controlling node locations.
				// Currently it is split between MetasoundEditor and MetasoundFrontend modules.
				Style.Display.Locations = {{FGuid::NewGuid(), NewLocation}};
				NodeHandle->SetNodeStyle(Style);
			}
		}

		void FGraphBuilder::InitMetaSoundPreset(UObject& InMetaSoundReferenced, UObject& InMetaSoundPreset)
		{
			using namespace Frontend;
			using namespace GraphBuilderPrivate;

			FMetasoundAssetBase* PresetAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSoundPreset);
			check(PresetAsset);

			// Mark preset as auto-update and non-editable
			FGraphHandle PresetGraphHandle = PresetAsset->GetRootGraphHandle();
			FMetasoundFrontendGraphStyle Style = PresetGraphHandle->GetGraphStyle();
			Style.bIsGraphEditable = false;
			PresetGraphHandle->SetGraphStyle(Style);

			// Mark all inputs as inherited by default
			TSet<FName> InputsInheritingDefault;
			Algo::Transform(PresetGraphHandle->GetInputNodes(), InputsInheritingDefault, [](FConstNodeHandle NodeHandle)
			{
				return NodeHandle->GetNodeName();
			});
			PresetGraphHandle->SetInputsInheritingDefault(MoveTemp(InputsInheritingDefault));

			FGraphBuilder::RegisterGraphWithFrontend(InMetaSoundReferenced);

			const FMetasoundAssetBase* ReferencedAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSoundReferenced);
			check(ReferencedAsset);

			FRebuildPresetRootGraph(ReferencedAsset->GetDocumentHandle()).Transform(PresetAsset->GetDocumentHandle());

			PresetAsset->ConformObjectDataToInterfaces();
		}

		bool FGraphBuilder::DeleteNode(UEdGraphNode& InNode)
		{
			using namespace Frontend;

			if (!InNode.CanUserDeleteNode())
			{
				return false;
			}

			// If node isn't a MetasoundEditorGraphNode, just remove and return (ex. comment nodes)
			UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(&InNode);
			UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(InNode.GetGraph());
			if (!Node)
			{
				Graph->RemoveNode(&InNode);
				return true;
			}


			// Remove connects only to pins associated with this EdGraph node
			// only (Iterate pins and not Frontend representation to preserve
			// other input/output EditorGraph reference node associations)
			Node->IteratePins([](UEdGraphPin& Pin, int32 Index)
			{
				// Only add literal inputs for output pins as adding when disconnecting
				// inputs would immediately orphan them on EditorGraph node removal below.
				const bool bAddLiteralInputs = Pin.Direction == EGPD_Output;
				FGraphBuilder::DisconnectPinVertex(Pin, bAddLiteralInputs);
			});

			FNodeHandle NodeHandle = Node->GetNodeHandle();
			Frontend::FGraphHandle GraphHandle = NodeHandle->GetOwningGraph();

			auto RemoveNodeLocation = [](FNodeHandle InNodeHandle, const FGuid& InNodeGuid)
			{
				FMetasoundFrontendNodeStyle Style = InNodeHandle->GetNodeStyle();
				Style.Display.Locations.Remove(InNodeGuid);
				InNodeHandle->SetNodeStyle(Style);
			};	

			auto RemoveNodeHandle = [] (FGraphHandle InGraphHandle, FNodeHandle InNodeHandle)
			{
				if (ensure(InGraphHandle->RemoveNode(*InNodeHandle)))
				{
					InGraphHandle->GetOwningDocument()->RemoveUnreferencedDependencies();
				}
			};

			if (GraphHandle->IsValid())
			{
				const EMetasoundFrontendClassType ClassType = NodeHandle->GetClassMetadata().GetType();
				switch (ClassType)
				{
					// NodeHandle does not get removed in these cases as EdGraph Inputs/Outputs
					// Frontend node is represented by the editor graph as a respective member
					// (not a node) on the MetasoundGraph. Therefore, just the editor position
					// data is removed.
					case EMetasoundFrontendClassType::Output:
					case EMetasoundFrontendClassType::Input:
					{
						RemoveNodeLocation(NodeHandle, InNode.NodeGuid);
					}
					break;
					
					// NodeHandle is only removed for variable accessors if the editor graph
					// no longer contains nodes representing the given accessor on the MetasoundGraph.
					// Therefore, just the editor position data is removed unless no location remains
					// on the Frontend node.
					case EMetasoundFrontendClassType::VariableAccessor:
					case EMetasoundFrontendClassType::VariableDeferredAccessor:
					{
						RemoveNodeLocation(NodeHandle, InNode.NodeGuid);
						if (NodeHandle->GetNodeStyle().Display.Locations.IsEmpty())
						{
							RemoveNodeHandle(GraphHandle, NodeHandle);
						}
					}
					break;

					case EMetasoundFrontendClassType::Graph:
					case EMetasoundFrontendClassType::Literal:
					case EMetasoundFrontendClassType::VariableMutator:
					case EMetasoundFrontendClassType::Variable:
					case EMetasoundFrontendClassType::External:
					case EMetasoundFrontendClassType::Template:
					default:
					{
						static_assert(static_cast<int32>(EMetasoundFrontendClassType::Invalid) == 10, "Possible missing MetasoundFrontendClassType switch case coverage.");
						RemoveNodeHandle(GraphHandle, NodeHandle);
					}
					break;
				}
			}

			return ensure(Graph->RemoveNode(&InNode));
		}

		void FGraphBuilder::RebuildNodePins(UMetasoundEditorGraphNode& InGraphNode)
		{
			using namespace Frontend;
		
			for (int32 i = InGraphNode.Pins.Num() - 1; i >= 0; i--)
			{
				InGraphNode.RemovePin(InGraphNode.Pins[i]);
			}

			// TODO: Make this a utility in Frontend (ClearInputLiterals())
			FNodeHandle NodeHandle = InGraphNode.GetNodeHandle();
			TArray<FInputHandle> Inputs = NodeHandle->GetInputs();
			for (FInputHandle& Input : Inputs)
			{
				NodeHandle->ClearInputLiteral(Input->GetID());
			}

			TArray<FInputHandle> InputHandles = NodeHandle->GetInputs();
			NodeHandle->GetInputStyle().SortDefaults(InputHandles, [](const Frontend::FInputHandle& Handle) { return FGraphBuilder::GetDisplayName(*Handle); });
			for (const FInputHandle& InputHandle : InputHandles)
			{
				// Only add pins of the node if the connection is user modifiable. 
				// Connections which the user cannot modify are controlled elsewhere.
				if (InputHandle->IsConnectionUserModifiable())
				{
					AddPinToNode(InGraphNode, InputHandle);
				}
			}

			TArray<FOutputHandle> OutputHandles = NodeHandle->GetOutputs();
			NodeHandle->GetOutputStyle().SortDefaults(OutputHandles, [](const Frontend::FOutputHandle& Handle) { return FGraphBuilder::GetDisplayName(*Handle); });
			for (const FOutputHandle& OutputHandle : OutputHandles)
			{
				// Only add pins of the node if the connection is user modifiable. 
				// Connections which the user cannot modify are controlled elsewhere.
				if (OutputHandle->IsConnectionUserModifiable())
				{
					AddPinToNode(InGraphNode, OutputHandle);
				}
			}
		}

		void FGraphBuilder::RefreshPinMetadata(UEdGraphPin& InPin, const FMetasoundFrontendVertexMetadata& InMetadata)
		{
			// Pin ToolTips are no longer cached on pins, and are instead dynamically generated via UMetasoundEditorGraphNode::GetPinHoverText
			InPin.PinToolTip = { };
			InPin.bAdvancedView = InMetadata.bIsAdvancedDisplay;
			if (InPin.bAdvancedView)
			{
				UEdGraphNode* OwningNode = InPin.GetOwningNode();
				check(OwningNode);
				if (OwningNode->AdvancedPinDisplay == ENodeAdvancedPins::NoPins)
				{
					OwningNode->AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
				}
			}
		}

		void FGraphBuilder::RegisterGraphWithFrontend(UObject& InMetaSound, bool bInForceViewSynchronization)
		{
			using namespace Frontend;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			TArray<FMetasoundAssetBase*> EditedReferencingMetaSounds;
			if (GEditor)
			{
				TArray<UObject*> EditedAssets = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetAllEditedAssets();
				for (UObject* Asset : EditedAssets)
				{
					if (Asset != &InMetaSound)
					{
						if (FMetasoundAssetBase* EditedMetaSound = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Asset))
						{
							EditedMetaSound->RebuildReferencedAssetClasses();
							if (EditedMetaSound->IsReferencedAsset(*MetaSoundAsset))
							{
								EditedReferencingMetaSounds.Add(EditedMetaSound);
							}
						}
					}
				}
			}

			FMetaSoundAssetRegistrationOptions RegOptions;
			RegOptions.bForceReregister = true;
			RegOptions.bForceViewSynchronization = bInForceViewSynchronization;
			// if EditedReferencingMetaSounds is empty, then no MetaSounds are open
			// that reference this MetaSound, so just register this asset. Otherwise,
			// this graph will recursively get updated when the open referencing graphs
			// are registered recursively via bRegisterDependencies flag.
			if (EditedReferencingMetaSounds.IsEmpty())
			{
				MetaSoundAsset->RegisterGraphWithFrontend(RegOptions);
			}
			else
			{
				for (FMetasoundAssetBase* MetaSound : EditedReferencingMetaSounds)
				{
					MetaSound->RegisterGraphWithFrontend(RegOptions);
				}
			}
		}

		void FGraphBuilder::UnregisterGraphWithFrontend(UObject& InMetaSound)
		{
			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			if (!ensure(MetaSoundAsset))
			{
				return;
			}

			if (GEditor)
			{
				TArray<UObject*> EditedAssets = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetAllEditedAssets();
				for (UObject* Asset : EditedAssets)
				{
					if (Asset != &InMetaSound)
					{
						if (FMetasoundAssetBase* EditedMetaSound = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Asset))
						{
							EditedMetaSound->RebuildReferencedAssetClasses();
							if (EditedMetaSound->IsReferencedAsset(*MetaSoundAsset))
							{
								EditedMetaSound->GetModifyContext().SetDocumentModified();
							}
						}
					}
				}
			}

			MetaSoundAsset->UnregisterGraphWithFrontend();
		}

		bool FGraphBuilder::IsMatchingInputHandleAndPin(const Frontend::FConstInputHandle& InInputHandle, const UEdGraphPin& InEditorPin)
		{
			if (InEditorPin.Direction != EGPD_Input)
			{
				return false;
			}

			Frontend::FInputHandle PinInputHandle = GetInputHandleFromPin(&InEditorPin);
			if (PinInputHandle->GetID() == InInputHandle->GetID())
			{
				return true;
			}

			return false;
		}

		bool FGraphBuilder::IsMatchingOutputHandleAndPin(const Frontend::FConstOutputHandle& InOutputHandle, const UEdGraphPin& InEditorPin)
		{
			if (InEditorPin.Direction != EGPD_Output)
			{
				return false;
			}

			Frontend::FOutputHandle PinOutputHandle = GetOutputHandleFromPin(&InEditorPin);
			if (PinOutputHandle->GetID() == InOutputHandle->GetID())
			{
				return true;
			}

			return false;
		}

		void FGraphBuilder::DepthFirstTraversal(UEdGraphNode* InInitialNode, FDepthFirstVisitFunction InVisitFunction)
		{
			// Non recursive depth first traversal.
			TArray<UEdGraphNode*> Stack({InInitialNode});
			TSet<UEdGraphNode*> Visited;

			while (Stack.Num() > 0)
			{
				UEdGraphNode* CurrentNode = Stack.Pop();
				if (Visited.Contains(CurrentNode))
				{
					// Do not revisit a node that has already been visited. 
					continue;
				}

				TArray<UEdGraphNode*> Children = InVisitFunction(CurrentNode).Array();
				Stack.Append(Children);

				Visited.Add(CurrentNode);
			}
		}

		UEdGraphPin* FGraphBuilder::AddPinToNode(UMetasoundEditorGraphNode& InEditorNode, Frontend::FConstInputHandle InInputHandle)
		{
			using namespace Frontend;

			FEdGraphPinType PinType;
			FName DataTypeName = InInputHandle->GetDataType();

			const IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
			if (const FEdGraphPinType* RegisteredPinType = EditorModule.FindPinType(DataTypeName))
			{
				PinType = *RegisteredPinType;
			}

			FName PinName = FGraphBuilder::GetPinName(*InInputHandle);
			UEdGraphPin* NewPin = InEditorNode.CreatePin(EGPD_Input, PinType, PinName);
			if (ensure(NewPin))
			{
				RefreshPinMetadata(*NewPin, InInputHandle->GetMetadata());
				SynchronizePinLiteral(*NewPin);
			}

			return NewPin;
		}

		UEdGraphPin* FGraphBuilder::AddPinToNode(UMetasoundEditorGraphNode& InEditorNode, Frontend::FConstOutputHandle InOutputHandle)
		{
			FEdGraphPinType PinType;
			FName DataTypeName = InOutputHandle->GetDataType();

			IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
			if (const FEdGraphPinType* RegisteredPinType = EditorModule.FindPinType(DataTypeName))
			{
				PinType = *RegisteredPinType;
			}

			FName PinName = FGraphBuilder::GetPinName(*InOutputHandle);
			UEdGraphPin* NewPin = InEditorNode.CreatePin(EGPD_Output, PinType, PinName);
			if (ensure(NewPin))
			{
				RefreshPinMetadata(*NewPin, InOutputHandle->GetMetadata());
			}

			return NewPin;
		}

		bool FGraphBuilder::RecurseGetDocumentModified(FMetasoundAssetBase& InAssetBase)
		{
			using namespace Metasound::Frontend;

			if (InAssetBase.GetModifyContext().GetDocumentModified())
			{
				return true;
			}

			TArray<FMetasoundAssetBase*> References;
			ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(InAssetBase, References));
			for (FMetasoundAssetBase* Reference : References)
			{
				check(Reference);
				const bool bReferenceDocumentModified = RecurseGetDocumentModified(*Reference);
				if (bReferenceDocumentModified)
				{
					return true;
				}
			}

			return false;
		}

		bool FGraphBuilder::SynchronizePinType(const IMetasoundEditorModule& InEditorModule, UEdGraphPin& InPin, const FName InDataType)
		{
			FEdGraphPinType PinType;
			if (const FEdGraphPinType* RegisteredPinType = InEditorModule.FindPinType(InDataType))
			{
				PinType = *RegisteredPinType;
			}

			if (InPin.PinType != PinType)
			{
				if (const UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(InPin.GetOwningNodeUnchecked()))
				{
					const FString NodeName = Node->GetDisplayName().ToString();
					UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Pin '%s' on Node '%s': Type converted to '%s'"), *NodeName, *InPin.GetName(), *InDataType.ToString());
				}
				InPin.PinType = PinType;
				return true;
			}

			return false;
		}

		bool FGraphBuilder::SynchronizeConnections(UObject& InMetaSound)
		{
			using namespace Frontend;

			bool bIsGraphDirty = false;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			FGraphHandle GraphHandle = MetaSoundAsset->GetRootGraphHandle();

			UMetasoundEditorGraph* EditorGraph = CastChecked<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());

			TArray<UMetasoundEditorGraphNode*> EditorNodes;
			EditorGraph->GetNodesOfClass(EditorNodes);

			TMap<FGuid, TArray<UMetasoundEditorGraphNode*>> EditorNodesByFrontendID;
			for (UMetasoundEditorGraphNode* EditorNode : EditorNodes)
			{
				EditorNodesByFrontendID.FindOrAdd(EditorNode->GetNodeID()).Add(EditorNode);
			}

			// Iterate through all nodes in metasound editor graph and synchronize connections.
			for (UMetasoundEditorGraphNode* EditorNode : EditorNodes)
			{
				bool bIsNodeDirty = false;

				FConstNodeHandle Node = EditorNode->GetNodeHandle();

				TArray<UEdGraphPin*> Pins = EditorNode->GetAllPins();
				TArray<FConstInputHandle> NodeInputs = Node->GetConstInputs();

				// Ignore connections which are not handled by the editor.
				NodeInputs.RemoveAll([](const FConstInputHandle& FrontendInput) { return !FrontendInput->IsConnectionUserModifiable(); });

				for (FConstInputHandle& NodeInput : NodeInputs)
				{
					auto IsMatchingInputPin = [&](const UEdGraphPin* Pin) -> bool
					{
						return IsMatchingInputHandleAndPin(NodeInput, *Pin);
					};

					UEdGraphPin* MatchingPin = nullptr;
					if (UEdGraphPin** DoublePointer = Pins.FindByPredicate(IsMatchingInputPin))
					{
						MatchingPin = *DoublePointer;
					}

					if (!ensure(MatchingPin))
					{
						continue;
					}

					// Remove pin so it isn't used twice.
					Pins.Remove(MatchingPin);

					FConstOutputHandle OutputHandle = NodeInput->GetConnectedOutput();
					if (OutputHandle->IsValid())
					{
						// Both input and output handles be user modifiable for a
						// connection to be controlled by the editor.
						check(OutputHandle->IsConnectionUserModifiable());

						bool bAddLink = false;

						if (MatchingPin->LinkedTo.IsEmpty())
						{
							// No link currently exists. Add the appropriate link.
							bAddLink = true;
						}
						else if (!IsMatchingOutputHandleAndPin(OutputHandle, *MatchingPin->LinkedTo[0]))
						{
							// The wrong link exists.
							MatchingPin->BreakAllPinLinks();
							bAddLink = true;
						}

						if (bAddLink)
						{
							const FGuid NodeID = OutputHandle->GetOwningNodeID();
							TArray<UMetasoundEditorGraphNode*>* OutputEditorNode = EditorNodesByFrontendID.Find(NodeID);
							if (ensure(OutputEditorNode))
							{
								if (ensure(!OutputEditorNode->IsEmpty()))
								{
									UEdGraphPin* OutputPin = (*OutputEditorNode)[0]->FindPinChecked(OutputHandle->GetName(), EEdGraphPinDirection::EGPD_Output);
									const FText& OwningNodeName = EditorNode->GetDisplayName();

									UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Node '%s' Connection: Linking Pin '%s' to '%s'"), *OwningNodeName.ToString(), *MatchingPin->GetName(), *OutputPin->GetName());
									MatchingPin->MakeLinkTo(OutputPin);
									bIsNodeDirty = true;
								}
							}
						}
					}
					else
					{
						// No link should exist.
						if (!MatchingPin->LinkedTo.IsEmpty())
						{
							MatchingPin->BreakAllPinLinks();
							const FText OwningNodeName = EditorNode->GetDisplayName();
							const FText InputName = FGraphBuilder::GetDisplayName(*NodeInput);
							UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Node '%s' Connection: Breaking all pin links to '%s'"), *OwningNodeName.ToString(), *InputName.ToString());
							bIsNodeDirty = true;
						}
					}

					SynchronizePinLiteral(*MatchingPin);
				}

				bIsGraphDirty |= bIsNodeDirty;
			}

			return bIsGraphDirty;
		}

		bool FGraphBuilder::SynchronizeGraph(UObject& InMetaSound)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FGraphBuilder::SynchronizeGraph);

			using namespace Frontend;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);

			if (RecurseGetDocumentModified(*MetaSoundAsset))
			{
				TArray<FMetasoundAssetBase*> EditedReferencingMetaSounds;
				if (GEditor)
				{
					TArray<UObject*> EditedAssets = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetAllEditedAssets();
					for (UObject* Asset : EditedAssets)
					{
						if (Asset != &InMetaSound)
						{
							if (FMetasoundAssetBase* EditedMetaSound = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Asset))
							{
								if (EditedMetaSound->IsReferencedAsset(*MetaSoundAsset))
								{
									EditedReferencingMetaSounds.Add(EditedMetaSound);
								}
							}
						}
					}
				}

				if (EditedReferencingMetaSounds.IsEmpty())
				{
					MetaSoundAsset->CacheRegistryMetadata();
					const bool bEditorGraphModified = GraphBuilderPrivate::SynchronizeGraphRecursively(InMetaSound);
					GraphBuilderPrivate::RecurseClearDocumentModified(*MetaSoundAsset);
				}
				else
				{
					for (FMetasoundAssetBase* EditedMetaSound : EditedReferencingMetaSounds)
					{
						check(EditedMetaSound);
						UObject* OwningMetaSound = EditedMetaSound->GetOwningAsset();
						check(OwningMetaSound);
						SynchronizeGraph(*OwningMetaSound);
					}
				}

				return true;
			}

			return false;
		}

		bool FGraphBuilder::SynchronizeNodeMembers(UObject& InMetaSound)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FGraphBuilder::SynchronizeNodeMembers);

			using namespace Frontend;

			bool bEditorGraphModified = false;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			FGraphHandle GraphHandle = MetaSoundAsset->GetRootGraphHandle();
			UMetasoundEditorGraph* EditorGraph = CastChecked<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());

			TArray<UMetasoundEditorGraphInputNode*> InputNodes;
			EditorGraph->GetNodesOfClassEx<UMetasoundEditorGraphInputNode>(InputNodes);
			for (UMetasoundEditorGraphInputNode* Node : InputNodes)
			{
				check(Node);
				FConstNodeHandle NodeHandle = Node->GetConstNodeHandle();
				if (!NodeHandle->IsValid())
				{
					for (UEdGraphPin* Pin : Node->Pins)
					{
						check(Pin);

						FConstClassInputAccessPtr ClassInputPtr = GraphHandle->FindClassInputWithName(Pin->PinName);
						if (const FMetasoundFrontendClassInput* Input = ClassInputPtr.Get())
						{
							const FGuid& InitialID = Node->GetNodeID();
							if (Node->GetNodeHandle()->GetID() != Input->NodeID)
							{
								Node->SetNodeID(Input->NodeID);

								// Requery handle as the id has been fixed up
								NodeHandle = Node->GetConstNodeHandle();
								FText InputDisplayName = Node->GetDisplayName();
								UE_LOG(LogMetasoundEditor, Verbose, TEXT("Editor Input Node '%s' interface versioned"), *InputDisplayName.ToString());

								bEditorGraphModified = true;
							}
						}
					}
				}
			}

			TArray<UMetasoundEditorGraphOutputNode*> OutputNodes;
			EditorGraph->GetNodesOfClassEx<UMetasoundEditorGraphOutputNode>(OutputNodes);
			for (UMetasoundEditorGraphOutputNode* Node : OutputNodes)
			{
				FConstNodeHandle NodeHandle = Node->GetConstNodeHandle();
				if (!NodeHandle->IsValid())
				{
					for (UEdGraphPin* Pin : Node->Pins)
					{
						check(Pin);
						FConstClassOutputAccessPtr ClassOutputPtr = GraphHandle->FindClassOutputWithName(Pin->PinName);
						if (const FMetasoundFrontendClassOutput* Output = ClassOutputPtr.Get())
						{
							const FGuid& InitialID = Node->GetNodeID();
							if (Node->GetNodeHandle()->GetID() != Output->NodeID)
							{
								Node->SetNodeID(Output->NodeID);

								// Requery handle as the id has been fixed up
								NodeHandle = Node->GetConstNodeHandle();
								FText OutputDisplayName = Node->GetDisplayName();
								UE_LOG(LogMetasoundEditor, Verbose, TEXT("Editor Output Node '%s' interface versioned"), *OutputDisplayName.ToString());

								bEditorGraphModified = true;
							}
						}
					}
				}
			}

			return bEditorGraphModified;
		}

		bool FGraphBuilder::SynchronizeNodes(UObject& InMetaSound)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FGraphBuilder::SynchronizeNodes);

			using namespace Frontend;

			bool bEditorGraphModified = false;

			// Get all external nodes from Frontend graph.  Input and output references will only be added/synchronized
			// if required when synchronizing connections (as they are not required to inhabit editor graph).
			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			FGraphHandle GraphHandle = MetaSoundAsset->GetRootGraphHandle();
			TArray<FNodeHandle> FrontendNodes = GraphHandle->GetNodes();
			UMetasoundEditorGraph* EditorGraph = CastChecked<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());
			TArray<UMetasoundEditorGraphNode*> EditorNodes;
			EditorGraph->GetNodesOfClass(EditorNodes);

			TMap<FGuid, UMetasoundEditorGraphNode*> EditorNodesByEdNodeGuid;
			for (UMetasoundEditorGraphNode* Node : EditorNodes)
			{
				EditorNodesByEdNodeGuid.Add(Node->NodeGuid, Node);
			}

			// Find existing array of editor nodes associated with Frontend node
			struct FAssociatedNodes
			{
				TArray<UMetasoundEditorGraphNode*> EditorNodes;
				FConstNodeHandle Node = Metasound::Frontend::INodeController::GetInvalidHandle();
			};
			TMap<FGuid, FAssociatedNodes> AssociatedNodes;

			// Reverse iterate so paired nodes can safely be removed from the array.
			for (int32 i = FrontendNodes.Num() - 1; i >= 0; i--)
			{
				const FConstNodeHandle& Node = FrontendNodes[i];
				bool bFoundEditorNode = false;
				for (int32 j = EditorNodes.Num() - 1; j >= 0; --j)
				{
					UMetasoundEditorGraphNode* EditorNode = EditorNodes[j];
					if (EditorNode->GetNodeID() == Node->GetID())
					{
						bFoundEditorNode = true;
						FAssociatedNodes& AssociatedNodeData = AssociatedNodes.FindOrAdd(Node->GetID());
						if (AssociatedNodeData.Node->IsValid())
						{
							ensure(AssociatedNodeData.Node == Node);
						}
						else
						{
							AssociatedNodeData.Node = Node;
						}

						bEditorGraphModified |= SynchronizeNodeLocation(Node, *EditorNode);
						AssociatedNodeData.EditorNodes.Add(EditorNode);
						EditorNodes.RemoveAtSwap(j, 1, false /* bAllowShrinking */);
					}
				}

				if (bFoundEditorNode)
				{
					FrontendNodes.RemoveAtSwap(i, 1, false /* bAllowShrinking */);
				}
			}

			// FrontendNodes now contains nodes which need to be added to the editor graph.
			// EditorNodes now contains nodes that need to be removed from the editor graph.
			// AssociatedNodes contains pairs which we have to check have synchronized pins

			// Add and remove nodes first in order to make sure correct editor nodes
			// exist before attempting to synchronize connections.
			for (UMetasoundEditorGraphNode* EditorNode : EditorNodes)
			{
				bEditorGraphModified |= EditorGraph->RemoveNode(EditorNode);
			}

			// Add missing editor nodes marked as visible.
			for (const FNodeHandle& Node : FrontendNodes)
			{
				const FMetasoundFrontendNodeStyle& CurrentStyle = Node->GetNodeStyle();
				if (CurrentStyle.Display.Locations.IsEmpty())
				{
					continue;
				}

				FMetasoundFrontendNodeStyle NewStyle = CurrentStyle;
				bEditorGraphModified = true;

				TArray<UMetasoundEditorGraphNode*> AddedNodes;
				for (const TPair<FGuid, FVector2D>& Location : NewStyle.Display.Locations)
				{
					UMetasoundEditorGraphNode* NewNode = AddNode(InMetaSound, Node, Location.Value, false /* bInSelectNewNode */);
					if (ensure(NewNode))
					{
						FAssociatedNodes& AssociatedNodeData = AssociatedNodes.FindOrAdd(Node->GetID());
						if (AssociatedNodeData.Node->IsValid())
						{
							ensure(AssociatedNodeData.Node == Node);
						}
						else
						{
							AssociatedNodeData.Node = Node;
						}

						AddedNodes.Add(NewNode);
						AssociatedNodeData.EditorNodes.Add(NewNode);
					}
				}

				NewStyle.Display.Locations.Reset();
				for (UMetasoundEditorGraphNode* EditorNode : AddedNodes)
				{
					NewStyle.Display.Locations.Add(EditorNode->NodeGuid, FVector2D(EditorNode->NodePosX, EditorNode->NodePosY));
				}
				Node->SetNodeStyle(NewStyle);
			}

			// Synchronize pins on node associations.
			for (const TPair<FGuid, FAssociatedNodes>& IdNodePair : AssociatedNodes)
			{
				for (UMetasoundEditorGraphNode* EditorNode : IdNodePair.Value.EditorNodes)
				{
					bEditorGraphModified |= SynchronizeNodePins(*EditorNode, IdNodePair.Value.Node);
				}
			}

			return bEditorGraphModified;
		}

		bool FGraphBuilder::SynchronizeNodePins(UMetasoundEditorGraphNode& InEditorNode, Frontend::FConstNodeHandle InNode, bool bRemoveUnusedPins, bool bLogChanges)
		{
			using namespace Frontend;

			bool bIsNodeDirty = false;

			IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");

			TArray<FConstInputHandle> InputHandles;
			TArray<FConstOutputHandle> OutputHandles;
			auto GetUserModifiableHandles = [InNode](TArray<FConstInputHandle>& InHandles, TArray<FConstOutputHandle>& OutHandles)
			{
				InHandles = InNode->GetConstInputs();
				OutHandles = InNode->GetConstOutputs();

				// Remove input and output handles which are not user modifiable
				InHandles.RemoveAll([](const FConstInputHandle& FrontendInput) { return !FrontendInput->IsConnectionUserModifiable(); });
				OutHandles.RemoveAll([](const FConstOutputHandle& FrontendOutput) { return !FrontendOutput->IsConnectionUserModifiable(); });
			};
			GetUserModifiableHandles(InputHandles, OutputHandles);

			// Filter out pins which are not paired.
			TArray<UEdGraphPin*> EditorPins = InEditorNode.Pins;
			for (int32 i = EditorPins.Num() - 1; i >= 0; i--)
			{
				UEdGraphPin* Pin = EditorPins[i];

				auto IsMatchingInputHandle = [&](const FConstInputHandle& InputHandle) -> bool
				{
					return IsMatchingInputHandleAndPin(InputHandle, *Pin);
				};

				auto IsMatchingOutputHandle = [&](const FConstOutputHandle& OutputHandle) -> bool
				{
					return IsMatchingOutputHandleAndPin(OutputHandle, *Pin);
				};

				switch (Pin->Direction)
				{
					case EEdGraphPinDirection::EGPD_Input:
					{
						int32 MatchingInputIndex = InputHandles.FindLastByPredicate(IsMatchingInputHandle);
						if (INDEX_NONE != MatchingInputIndex)
						{
							bIsNodeDirty |= SynchronizePinType(EditorModule, *EditorPins[i], InputHandles[MatchingInputIndex]->GetDataType());
							InputHandles.RemoveAtSwap(MatchingInputIndex);
							EditorPins.RemoveAtSwap(i);
						}
					}
					break;

					case EEdGraphPinDirection::EGPD_Output:
					{
						int32 MatchingOutputIndex = OutputHandles.FindLastByPredicate(IsMatchingOutputHandle);
						if (INDEX_NONE != MatchingOutputIndex)
						{
							bIsNodeDirty |= SynchronizePinType(EditorModule, *EditorPins[i], OutputHandles[MatchingOutputIndex]->GetDataType());
							OutputHandles.RemoveAtSwap(MatchingOutputIndex);
							EditorPins.RemoveAtSwap(i);
						}
					}
					break;
				}
			}

			// Remove any unused editor pins.
			if (bRemoveUnusedPins)
			{
				bIsNodeDirty |= !EditorPins.IsEmpty();
				for (UEdGraphPin* Pin : EditorPins)
				{
					if (bLogChanges)
					{
						constexpr bool bIncludeNamespace = true;
						const FText NodeDisplayName = FGraphBuilder::GetDisplayName(*InNode, bIncludeNamespace);
						UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Node '%s' Pins: Removing Excess Editor Pin '%s'"), *NodeDisplayName.ToString(), *Pin->GetName());
					}
					InEditorNode.RemovePin(Pin);
				}
			}


			if (!InputHandles.IsEmpty())
			{
				bIsNodeDirty = true;
				for (FConstInputHandle& InputHandle : InputHandles)
				{
					if (bLogChanges)
					{
						constexpr bool bIncludeNamespace = true;
						const FText NodeDisplayName = FGraphBuilder::GetDisplayName(*InNode, bIncludeNamespace);
						const FText InputDisplayName = FGraphBuilder::GetDisplayName(*InputHandle);
						UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Node '%s' Pins: Adding missing Editor Input Pin '%s'"), *NodeDisplayName.ToString(), *InputDisplayName.ToString());
					}
					AddPinToNode(InEditorNode, InputHandle);
				}
			}

			if (!OutputHandles.IsEmpty())
			{
				bIsNodeDirty = true;
				for (FConstOutputHandle& OutputHandle : OutputHandles)
				{
					if (bLogChanges)
					{
						constexpr bool bIncludeNamespace = true;
						const FText NodeDisplayName = FGraphBuilder::GetDisplayName(*InNode, bIncludeNamespace);
						const FText OutputDisplayName = FGraphBuilder::GetDisplayName(*OutputHandle);
						UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Node '%s' Pins: Adding missing Editor Output Pin '%s'"), *NodeDisplayName.ToString(), *OutputDisplayName.ToString());
					}
					AddPinToNode(InEditorNode, OutputHandle);
				}
			}

			// Order pins
			GetUserModifiableHandles(InputHandles, OutputHandles);

			InNode->GetInputStyle().SortDefaults(InputHandles, [](const FConstInputHandle& Handle) { return FGraphBuilder::GetDisplayName(*Handle); });
			InNode->GetOutputStyle().SortDefaults(OutputHandles, [](const FConstOutputHandle& Handle) { return FGraphBuilder::GetDisplayName(*Handle); });

			auto SwapAndDirty = [&](int32 IndexA, int32 IndexB)
			{
				const bool bRequiresSwap = IndexA != IndexB;
				if (bRequiresSwap)
				{
					InEditorNode.Pins.Swap(IndexA, IndexB);
					bIsNodeDirty |= bRequiresSwap;
				}
			};

			for (int32 i = InEditorNode.Pins.Num() - 1; i >= 0; --i)
			{
				UEdGraphPin* Pin = InEditorNode.Pins[i];
				if (Pin->Direction == EGPD_Input)
				{
					if (!InputHandles.IsEmpty())
					{
						constexpr bool bAllowShrinking = false;
						FConstInputHandle InputHandle = InputHandles.Pop(bAllowShrinking);
						for (int32 j = i; j >= 0; --j)
						{
							if (IsMatchingInputHandleAndPin(InputHandle, *InEditorNode.Pins[j]))
							{
								SwapAndDirty(i, j);
								break;
							}
						}
					}
				}
				else /* Pin->Direction == EGPD_Output */
				{
					if (!OutputHandles.IsEmpty())
					{
						constexpr bool bAllowShrinking = false;
						FConstOutputHandle OutputHandle = OutputHandles.Pop(bAllowShrinking);
						for (int32 j = i; j >= 0; --j)
						{
							if (IsMatchingOutputHandleAndPin(OutputHandle, *InEditorNode.Pins[j]))
							{
								SwapAndDirty(i, j);
								break;
							}
						}
					}
				}
			}

			return bIsNodeDirty;
		}

		bool FGraphBuilder::SynchronizePinLiteral(UEdGraphPin& InPin)
		{
			using namespace Frontend;

			if (!ensure(InPin.Direction == EGPD_Input))
			{
				return false;
			}

			const FString OldValue = InPin.DefaultValue;

			FInputHandle InputHandle = GetInputHandleFromPin(&InPin);
			if (const FMetasoundFrontendLiteral* NodeDefaultLiteral = InputHandle->GetLiteral())
			{
				InPin.DefaultValue = NodeDefaultLiteral->ToString();
				return OldValue != InPin.DefaultValue;
			}

			if (const FMetasoundFrontendLiteral* ClassDefaultLiteral = InputHandle->GetClassDefaultLiteral())
			{
				InPin.DefaultValue = ClassDefaultLiteral->ToString();
				return OldValue != InPin.DefaultValue;
			}

			FMetasoundFrontendLiteral DefaultLiteral;
			DefaultLiteral.SetFromLiteral(IDataTypeRegistry::Get().CreateDefaultLiteral(InputHandle->GetDataType()));

			InPin.DefaultValue = DefaultLiteral.ToString();
			return OldValue != InPin.DefaultValue;
		}

		bool FGraphBuilder::SynchronizeGraphMembers(UObject& InMetaSound)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Editor::FGraphBuilder::SynchronizeGraphMembers);

			using namespace Frontend;

			bool bEditorGraphModified = false;

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InMetaSound);
			check(MetaSoundAsset);
			UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());
			FGraphHandle GraphHandle = MetaSoundAsset->GetRootGraphHandle();

			TSet<UMetasoundEditorGraphInput*> Inputs;
			TSet<UMetasoundEditorGraphOutput*> Outputs;

			// Collect all editor graph inputs with corresponding frontend inputs. 
			GraphHandle->IterateNodes([&](FNodeHandle NodeHandle)
			{
				if (UMetasoundEditorGraphInput* Input = Graph->FindInput(NodeHandle->GetID()))
				{
					Inputs.Add(Input);
					return;
				}

				// Add an editor input if none exist for a frontend input.
				Inputs.Add(Graph->FindOrAddInput(NodeHandle));
				constexpr bool bIncludeNamespace = true;
				FText NodeDisplayName = FGraphBuilder::GetDisplayName(*NodeHandle, bIncludeNamespace);
				UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Inputs: Added missing input '%s'."), *NodeDisplayName.ToString());
				bEditorGraphModified = true;
			}, EMetasoundFrontendClassType::Input);

			// Collect all editor graph outputs with corresponding frontend outputs. 
			GraphHandle->IterateNodes([&](FNodeHandle NodeHandle)
			{
				if (UMetasoundEditorGraphOutput* Output = Graph->FindOutput(NodeHandle->GetID()))
				{
					Outputs.Add(Output);
					return;
				}

				// Add an editor output if none exist for a frontend output.
				Outputs.Add(Graph->FindOrAddOutput(NodeHandle));
				constexpr bool bIncludeNamespace = true;
				FText NodeDisplayName = FGraphBuilder::GetDisplayName(*NodeHandle, bIncludeNamespace);
				UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Outputs: Added missing output '%s'."), *NodeDisplayName.ToString());
				bEditorGraphModified = true;
			}, EMetasoundFrontendClassType::Output);

			// Collect editor inputs and outputs to remove which have no corresponding frontend input or output.
			TArray<UMetasoundEditorGraphMember*> ToRemove;
			Graph->IterateInputs([&](UMetasoundEditorGraphInput& Input)
			{
				if (!Inputs.Contains(&Input))
				{
					UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Inputs: Removing stale input '%s'."), *Input.GetName());
					ToRemove.Add(&Input);
				}
			});
			Graph->IterateOutputs([&](UMetasoundEditorGraphOutput& Output)
			{
				if (!Outputs.Contains(&Output))
				{
					UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Outputs: Removing stale output '%s'."), *Output.GetName());
					ToRemove.Add(&Output);
				}
			});

			// Remove stale inputs and outputs.
			bEditorGraphModified |= !ToRemove.IsEmpty();
			for (UMetasoundEditorGraphMember* GraphMember: ToRemove)
			{
				Graph->RemoveMember(*GraphMember);
			}

			UMetasoundEditorGraphMember* Member = nullptr;

			auto SynchronizeMemberDataType = [&](UMetasoundEditorGraphVertex& InVertex)
			{
				FConstNodeHandle NodeHandle = InVertex.GetConstNodeHandle();
				TArray<FConstInputHandle> InputHandles = NodeHandle->GetConstInputs();
				if (ensure(InputHandles.Num() == 1))
				{
					FConstInputHandle InputHandle = InputHandles.Last();
					const FName NewDataType = InputHandle->GetDataType();
					if (InVertex.GetDataType() != NewDataType)
					{
						constexpr bool bIncludeNamespace = true;
						FText NodeDisplayName = FGraphBuilder::GetDisplayName(*NodeHandle, bIncludeNamespace);
						UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing Member '%s': Updating DataType to '%s'."), *NodeDisplayName.ToString(), *NewDataType.ToString());

						FMetasoundFrontendLiteral DefaultLiteral;
						DefaultLiteral.SetFromLiteral(IDataTypeRegistry::Get().CreateDefaultLiteral(NewDataType));
						if (const FMetasoundFrontendLiteral* InputLiteral = InputHandle->GetLiteral())
						{
							DefaultLiteral = *InputLiteral;
						}

						InVertex.ClassName = NodeHandle->GetClassMetadata().GetClassName();

						constexpr bool bPostTransaction = false;
						InVertex.SetDataType(NewDataType, bPostTransaction);

						if (DefaultLiteral.IsValid())
						{
							InVertex.GetLiteral()->SetFromLiteral(DefaultLiteral);
						}
					}
				}
			};

			// Synchronize data types & default values for input nodes.
			GraphHandle->IterateNodes([&](FNodeHandle NodeHandle)
			{
				if (UMetasoundEditorGraphInput* Input = Graph->FindInput(NodeHandle->GetID()))
				{
					SynchronizeMemberDataType(*Input);

					if (UMetasoundEditorGraphMemberDefaultLiteral* Literal = Input->GetLiteral())
					{
						const FName NodeName = NodeHandle->GetNodeName();
						const FGuid VertexID = GraphHandle->GetVertexIDForInputVertex(NodeName);
						FMetasoundFrontendLiteral DefaultLiteral = GraphHandle->GetDefaultInput(VertexID);
						if (!DefaultLiteral.IsEqual(Literal->GetDefault()))
						{
							if (DefaultLiteral.GetType() != EMetasoundFrontendLiteralType::None)
							{
								UE_LOG(LogMetasoundEditor, Verbose, TEXT("Synchronizing default value to '%s' for input '%s'"), *DefaultLiteral.ToString(), *NodeName.ToString());
								Literal->SetFromLiteral(DefaultLiteral);
								bEditorGraphModified = true;
							}
						}
					}
				}
			}, EMetasoundFrontendClassType::Input);

			// Synchronize data types of output nodes.
			GraphHandle->IterateNodes([&](FNodeHandle NodeHandle)
			{
				if (UMetasoundEditorGraphOutput* Output = Graph->FindOutput(NodeHandle->GetID()))
				{
					SynchronizeMemberDataType(*Output);

					// Fix up corrupted assets with no literal set 
					if (!Output->GetLiteral())
					{
						Output->InitializeLiteral();
					}
				}
			}, EMetasoundFrontendClassType::Output);

			// Remove empty entries
			bEditorGraphModified |= Graph->Inputs.RemoveAllSwap([](const TObjectPtr<UMetasoundEditorGraphInput>& Input) { return !Input; }) > 0;
			bEditorGraphModified |= Graph->Outputs.RemoveAllSwap([](const TObjectPtr<UMetasoundEditorGraphOutput>& Output) { return !Output; }) > 0;
			bEditorGraphModified |= Graph->Variables.RemoveAllSwap([](const TObjectPtr<UMetasoundEditorGraphVariable>& Variable) { return !Variable; }) > 0;

			return bEditorGraphModified;
		}
	} // namespace Editor
} // namespace Metasound
#undef LOCTEXT_NAMESPACE
