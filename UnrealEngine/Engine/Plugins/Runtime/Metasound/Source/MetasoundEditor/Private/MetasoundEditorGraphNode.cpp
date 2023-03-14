// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundEditorGraphNode.h"

#include "EdGraph/EdGraphPin.h"
#include "Editor/EditorEngine.h"
#include "Engine/Font.h"
#include "Framework/Commands/GenericCommands.h"
#include "GraphEditorActions.h"
#include "Logging/TokenizedMessage.h"
#include "Metasound.h"
#include "MetasoundAssetManager.h"
#include "MetasoundEditorCommands.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphMemberDefaults.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorGraphValidation.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLiteral.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundUObjectRegistry.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"
#include "ScopedTransaction.h"
#include "ToolMenus.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorGraphNode)

#define LOCTEXT_NAMESPACE "MetaSoundEditor"

namespace Metasound
{
	namespace Editor
	{
		namespace GraphNodePrivate
		{
			static const FString MissingConcreteOutputConnectionFormat = TEXT(
				"Reroute connection for pin '{0}' does not provide a concrete output. "
				"Resulting literal value is undefined and may result in unintended results."
			);
		}
	} 
}

UMetasoundEditorGraphNode::UMetasoundEditorGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMetasoundEditorGraphNode::UpdateFrontendNodeLocation(const FVector2D& InLocation)
{
	using namespace Metasound::Frontend;
	
	FNodeHandle NodeHandle = GetNodeHandle();
	FMetasoundFrontendNodeStyle Style = NodeHandle->GetNodeStyle();
	Style.Display.Locations.FindOrAdd(NodeGuid) = InLocation;
	NodeHandle->SetNodeStyle(Style);
}

void UMetasoundEditorGraphNode::SetNodeLocation(const FVector2D& InLocation)
{
	NodePosX = FMath::TruncToInt(InLocation.X);
	NodePosY = FMath::TruncToInt(InLocation.Y);

	UpdateFrontendNodeLocation(InLocation);
}

void UMetasoundEditorGraphNode::PostLoad()
{
	Super::PostLoad();

	for (int32 Index = 0; Index < Pins.Num(); ++Index)
	{
		UEdGraphPin* Pin = Pins[Index];
		if (Pin->PinName.IsNone())
		{
			// Makes sure pin has a name for lookup purposes but user will never see it
			if (Pin->Direction == EGPD_Input)
			{
				Pin->PinName = CreateUniquePinName("Input");
			}
			else
			{
				Pin->PinName = CreateUniquePinName("Output");
			}
			Pin->PinFriendlyName = FText::GetEmpty();
		}
	}
}

void UMetasoundEditorGraphNode::CreateInputPin()
{
	// TODO: Implement for nodes supporting variadic inputs
	if (ensure(false))
	{
		return;
	}

	FString PinName; // get from UMetaSoundPatch
	UEdGraphPin* NewPin = CreatePin(EGPD_Input, TEXT("MetasoundEditorGraphNode"), *PinName);
	if (NewPin->PinName.IsNone())
	{
		// Pin must have a name for lookup purposes but is not user-facing
// 		NewPin->PinName = 
// 		NewPin->PinFriendlyName =
	}
}

int32 UMetasoundEditorGraphNode::EstimateNodeWidth() const
{
	const FString NodeTitle = GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	if (const UFont* Font = GetDefault<UEditorEngine>()->EditorFont)
	{
		return Font->GetStringSize(*NodeTitle);
	}
	else
	{
		static const int32 EstimatedCharWidth = 6;
		return NodeTitle.Len() * EstimatedCharWidth;
	}
}

UObject& UMetasoundEditorGraphNode::GetMetasoundChecked()
{
	UMetasoundEditorGraph* EdGraph = CastChecked<UMetasoundEditorGraph>(GetGraph());
	return EdGraph->GetMetasoundChecked();
}

const UObject& UMetasoundEditorGraphNode::GetMetasoundChecked() const
{
	UMetasoundEditorGraph* EdGraph = CastChecked<UMetasoundEditorGraph>(GetGraph());
	return EdGraph->GetMetasoundChecked();
}

Metasound::Frontend::FConstGraphHandle UMetasoundEditorGraphNode::GetConstRootGraphHandle() const
{
	const FMetasoundAssetBase* ConstMetasoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&GetMetasoundChecked());
	FMetasoundAssetBase* MetasoundAsset = const_cast<FMetasoundAssetBase*>(ConstMetasoundAsset);
	check(MetasoundAsset);

	return MetasoundAsset->GetRootGraphHandle();
}

Metasound::Frontend::FGraphHandle UMetasoundEditorGraphNode::GetRootGraphHandle() const
{
	const FMetasoundAssetBase* ConstMetasoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&GetMetasoundChecked());
	FMetasoundAssetBase* MetasoundAsset = const_cast<FMetasoundAssetBase*>(ConstMetasoundAsset);
	check(MetasoundAsset);

	return MetasoundAsset->GetRootGraphHandle();
}

Metasound::Frontend::FConstNodeHandle UMetasoundEditorGraphNode::GetConstNodeHandle() const
{
	const FGuid NodeID = GetNodeID();
	return GetConstRootGraphHandle()->GetNodeWithID(NodeID);
}

Metasound::Frontend::FNodeHandle UMetasoundEditorGraphNode::GetNodeHandle() const
{
	const FGuid NodeID = GetNodeID();
	return GetRootGraphHandle()->GetNodeWithID(NodeID);
}

void UMetasoundEditorGraphNode::IteratePins(TUniqueFunction<void(UEdGraphPin& /* Pin */, int32 /* Index */)> InFunc, EEdGraphPinDirection InPinDirection)
{
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (InPinDirection == EGPD_MAX || Pins[PinIndex]->Direction == InPinDirection)
		{
			InFunc(*Pins[PinIndex], PinIndex);
		}
	}
}

void UMetasoundEditorGraphNode::AllocateDefaultPins()
{
	using namespace Metasound;

	ensureAlways(Pins.IsEmpty());
	Editor::FGraphBuilder::RebuildNodePins(*this);
}

void UMetasoundEditorGraphNode::SyncChangeIDs()
{
	using namespace Metasound::Frontend;
	FConstNodeHandle NodeHandle = GetConstNodeHandle();

	MetadataChangeID = NodeHandle->GetClassMetadata().GetChangeID();
	InterfaceChangeID = NodeHandle->GetClassInterface().GetChangeID();
	StyleChangeID = NodeHandle->GetClassStyle().GetChangeID();
}

void UMetasoundEditorGraphNode::CacheTitle()
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FConstNodeHandle NodeHandle = GetNodeHandle();
	CachedTitle = NodeHandle->GetDisplayTitle();
}

void UMetasoundEditorGraphNode::Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

#if WITH_EDITOR
	// Validate that non-reroute inputs are connected to "real" outputs
	if (GetClassName() != FRerouteNodeTemplate::ClassName)
	{
		for (UEdGraphPin* Pin : Pins)
		{
			OutResult.SetPinOrphaned(*Pin, false);
			if (Pin->Direction == EGPD_Input)
			{
				if (!Pin->LinkedTo.IsEmpty())
				{
					UEdGraphPin* ReroutedPin = FGraphBuilder::FindReroutedOutputPin(Pin->LinkedTo.Last());
					if (ReroutedPin)
					{
						if (UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(ReroutedPin->GetOwningNode()))
						{
							if (ExternalNode->GetClassName() == FRerouteNodeTemplate::ClassName)
							{
								FConstInputHandle InputHandle = FGraphBuilder::GetConstInputHandleFromPin(Pin);
								const FString Msg = FString::Format(*GraphNodePrivate::MissingConcreteOutputConnectionFormat, { InputHandle->GetDisplayName().ToString() });
								OutResult.SetMessage(EMessageSeverity::Warning, Msg);
							}
						}
					}
				}
			}
		}
	}
#endif // WITH_EDITOR
}

bool UMetasoundEditorGraphNode::ContainsClassChange() const
{
	using namespace Metasound::Frontend;
	FConstNodeHandle NodeHandle = GetConstNodeHandle();

	return InterfaceChangeID != NodeHandle->GetClassInterface().GetChangeID()
	|| StyleChangeID != NodeHandle->GetClassStyle().GetChangeID()
	|| MetadataChangeID != NodeHandle->GetClassMetadata().GetChangeID();
}

void UMetasoundEditorGraphNode::ReconstructNode()
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	// Don't remove unused pins here. Reconstruction can occur while duplicating or pasting nodes,
	// and subsequent steps clean-up unused pins.  This can be called mid-copy, which means the node
	// handle may be invalid.  Setting to remove unused causes premature removal and then default values
	// are lost.
	FConstNodeHandle NodeHandle = GetNodeHandle();
	if (NodeHandle->IsValid())
	{
		FGraphBuilder::SynchronizeNodePins(*this, NodeHandle, false /* bRemoveUnusedPins */, false /* bLogChanges */);
	}

	CacheTitle();
}

void UMetasoundEditorGraphNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin)
	{
		const UMetasoundEditorGraphSchema* Schema = CastChecked<UMetasoundEditorGraphSchema>(GetSchema());

		TSet<UEdGraphNode*> NodeList;

		// auto-connect from dragged pin to first compatible pin on the new node
		for (int32 i = 0; i < Pins.Num(); i++)
		{
			UEdGraphPin* Pin = Pins[i];
			check(Pin);
			FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, Pin);
			if (ECanCreateConnectionResponse::CONNECT_RESPONSE_MAKE == Response.Response) //-V1051
			{
				if (Schema->TryCreateConnection(FromPin, Pin))
				{
					NodeList.Add(FromPin->GetOwningNode());
					NodeList.Add(this);
				}
				break;
			}
			else if (ECanCreateConnectionResponse::CONNECT_RESPONSE_BREAK_OTHERS_A == Response.Response)
			{
				// TODO: Implement default connections in GraphBuilder
				break;
			}
		}

		// Send all nodes that received a new pin connection a notification
		for (auto It = NodeList.CreateConstIterator(); It; ++It)
		{
			UEdGraphNode* Node = (*It);
			Node->NodeConnectionListChanged();
		}
	}
}

bool UMetasoundEditorGraphNode::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UMetasoundEditorGraphSchema::StaticClass());
}

bool UMetasoundEditorGraphNode::CanUserDeleteNode() const
{
	return true;
}

FString UMetasoundEditorGraphNode::GetDocumentationLink() const
{
	return TEXT("Shared/GraphNodes/Metasound");
}

FText UMetasoundEditorGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return CachedTitle;
}

void UMetasoundEditorGraphNode::GetPinHoverText(const UEdGraphPin& Pin, FString& OutHoverText) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (Pin.Direction == EGPD_Input)
	{
		// Report if connected to reroute network is not connected to concrete output 
		FConstInputHandle InputHandle = FGraphBuilder::GetConstInputHandleFromPin(&Pin);
		if (Pin.bOrphanedPin && InputHandle->IsValid())
		{
			OutHoverText = FString::Format(*GraphNodePrivate::MissingConcreteOutputConnectionFormat, { InputHandle->GetDisplayName().ToString() });
		}
		else
		{
			OutHoverText = InputHandle->GetTooltip().ToString();
		}
	}
	else // Pin.Direction == EGPD_Output
	{
		FConstOutputHandle OutputHandle = FGraphBuilder::FindReroutedConstOutputHandleFromPin(&Pin);
		OutHoverText = OutputHandle->GetTooltip().ToString();
	}
}

void UMetasoundEditorGraphNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (Pin && Pin->Direction == EGPD_Input)
	{
		GetMetasoundChecked().Modify();

		FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
		if (InputHandle->IsValid())
		{
			FMetasoundFrontendLiteral LiteralValue;
			if (FGraphBuilder::GetPinLiteral(*Pin, LiteralValue))
			{
				InputHandle->SetLiteral(LiteralValue);
			}
		}
	}
}

Metasound::Frontend::FDataTypeRegistryInfo UMetasoundEditorGraphNode::GetPinDataTypeInfo(const UEdGraphPin& InPin) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

 	Metasound::Frontend::FDataTypeRegistryInfo DataTypeInfo;

	if (InPin.Direction == EGPD_Input)
	{
		FConstInputHandle Handle = FGraphBuilder::GetConstInputHandleFromPin(&InPin);
		ensure(IDataTypeRegistry::Get().GetDataTypeInfo(Handle->GetDataType(), DataTypeInfo));
	}
	else // InPin.Direction == EGPD_Output
	{
		FConstOutputHandle Handle = FGraphBuilder::GetConstOutputHandleFromPin(&InPin);
		ensure(IDataTypeRegistry::Get().GetDataTypeInfo(Handle->GetDataType(), DataTypeInfo));
	}

	return DataTypeInfo;
}

TSet<FString> UMetasoundEditorGraphNode::GetDisallowedPinClassNames(const UEdGraphPin& InPin) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	const IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");

	const FDataTypeRegistryInfo DataTypeInfo = GetPinDataTypeInfo(InPin);
	if (DataTypeInfo.PreferredLiteralType != Metasound::ELiteralType::UObjectProxy)
	{
		return { };
	}

	UClass* ProxyGenClass = DataTypeInfo.ProxyGeneratorClass;
	if (!ProxyGenClass)
	{
		return { };
	}

	TSet<FString> DisallowedClasses;
	const FTopLevelAssetPath ClassName = ProxyGenClass->GetClassPathName();
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (!Class->IsNative())
		{
			continue;
		}

		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			continue;
		}

		if (ClassIt->GetClassPathName() == ClassName)
		{
			continue;
		}

		if (EditorModule.IsExplicitProxyClass(*ProxyGenClass) && Class->IsChildOf(ProxyGenClass))
		{
			DisallowedClasses.Add(ClassIt->GetClassPathName().ToString());
		}
	}

	return DisallowedClasses;
}

FString UMetasoundEditorGraphNode::GetPinMetaData(FName InPinName, FName InKey)
{
	if (InKey == "DisallowedClasses")
	{
		if (UEdGraphPin* Pin = FindPin(InPinName, EGPD_Input))
		{
			TSet<FString> DisallowedClasses = GetDisallowedPinClassNames(*Pin);
			return FString::Join(DisallowedClasses.Array(), TEXT(","));
		}

		return FString();
	}

	return Super::GetPinMetaData(InPinName, InKey);
}

void UMetasoundEditorGraphNode::PreSave(FObjectPreSaveContext InSaveContext)
{
	using namespace Metasound::Editor;

	Super::PreSave(InSaveContext);

	// Required to refresh upgrade nodes that are stale when saving.
	if (TSharedPtr<FEditor> MetaSoundEditor = FGraphBuilder::GetEditorForMetasound(GetMetasoundChecked()))
	{
		if (TSharedPtr<SGraphEditor> GraphEditor = MetaSoundEditor->GetGraphEditor())
		{
			GraphEditor->RefreshNode(*this);
		}
	}
}

void UMetasoundEditorGraphNode::PostEditImport()
{
}

void UMetasoundEditorGraphNode::PostEditUndo()
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	UEdGraphPin::ResolveAllPinReferences();

	// This can trigger and the handle is no longer valid if transaction
	// is being undone on a graph node that is orphaned.  If orphaned,
	// bail early.
	FNodeHandle NodeHandle = GetNodeHandle();
	if (!NodeHandle->IsValid())
	{
		return;
	}

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			FGraphBuilder::SynchronizePinLiteral(*Pin);
		}
	}
}

void UMetasoundEditorGraphNode::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		CreateNewGuid();
	}
}

void UMetasoundEditorGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	using namespace Metasound::Editor;

	if (Context->Node)
	{
		{
			FToolMenuSection& Section = Menu->AddSection("MetasoundEditorGraphNodeAlignment");
			Section.AddSubMenu("Alignment", LOCTEXT("AlignmentHeader", "Alignment"), FText(), FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
			{
				{
					FToolMenuSection& SubMenuSection = SubMenu->AddSection("EdGraphSchemaAlignment", LOCTEXT("AlignHeader", "Align"));
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
				}

				{
					FToolMenuSection& SubMenuSection = SubMenu->AddSection("EdGraphSchemaDistribution", LOCTEXT("DistributionHeader", "Distribution"));
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesHorizontally);
					SubMenuSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesVertically);
				}
			}));
		}
	}
}

FText UMetasoundEditorGraphNode::GetTooltipText() const
{
	return GetConstNodeHandle()->GetDescription();
}

FText UMetasoundEditorGraphNode::GetDisplayName() const
{
	constexpr bool bIncludeNamespace = true;
	return Metasound::Editor::FGraphBuilder::GetDisplayName(*GetConstNodeHandle(), bIncludeNamespace);
}

FString UMetasoundEditorGraphNode::GetDocumentationExcerptName() const
{
	// Default the node to searching for an excerpt named for the C++ node class name, including the U prefix.
	// This is done so that the excerpt name in the doc file can be found by find-in-files when searching for the full class name.
	return FString::Printf(TEXT("%s%s"), UMetaSoundPatch::StaticClass()->GetPrefixCPP(), *UMetaSoundPatch::StaticClass()->GetName());
}

bool UMetasoundEditorGraphMemberNode::ClampFloatLiteral(const UMetasoundEditorGraphMemberDefaultFloat* DefaultFloatLiteral, FMetasoundFrontendLiteral& LiteralValue)
{
	bool bClampedFloatLiteral = false;
	if (DefaultFloatLiteral->ClampDefault)
	{
		float LiteralFloatValue = 0.0f;
		float ClampedFloatValue = 0.0f;

		LiteralValue.TryGet(LiteralFloatValue);
		ClampedFloatValue = FMath::Clamp(LiteralFloatValue, DefaultFloatLiteral->Range.X, DefaultFloatLiteral->Range.Y);
		bClampedFloatLiteral = !FMath::IsNearlyEqual(ClampedFloatValue, LiteralFloatValue);
		LiteralValue.Set(ClampedFloatValue);
	}
	return bClampedFloatLiteral;
}

void UMetasoundEditorGraphOutputNode::PinDefaultValueChanged(UEdGraphPin* InPin)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (InPin && InPin->Direction == EGPD_Input)
	{
		UObject& MetaSound = GetMetasoundChecked();
		MetaSound.Modify();
		
		FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(InPin);
		if (InputHandle->IsValid())
		{
			FMetasoundFrontendLiteral LiteralValue;
			if (FGraphBuilder::GetPinLiteral(*InPin, LiteralValue))
			{
				if (Output)
				{
					UMetasoundEditorGraphMemberDefaultLiteral* Literal = Output->GetLiteral();
					if (ensure(Literal))
					{
						// Clamp float literal if necessary 
						bool bClampedFloatLiteral = false;
						if (const UMetasoundEditorGraphMemberDefaultFloat* DefaultFloatLiteral = Cast<UMetasoundEditorGraphMemberDefaultFloat>(Literal))
						{
							bClampedFloatLiteral = ClampFloatLiteral(DefaultFloatLiteral, LiteralValue);
						}

						Literal->SetFromLiteral(LiteralValue);

						constexpr bool bPostTransaction = false;
						Output->UpdateFrontendDefaultLiteral(bPostTransaction);

						// Update graph node if it was clamped
						if (bClampedFloatLiteral)
						{
							FGraphBuilder::RegisterGraphWithFrontend(MetaSound);
							if (FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound))
							{
								MetaSoundAsset->GetModifyContext().AddMemberIDsModified({ Output->GetMemberID() });
							}
						}
					}
				}
			}
		}
	}
}

bool UMetasoundEditorGraphOutputNode::EnableInteractWidgets() const
{
	using namespace Metasound::Frontend;

	bool bEnabled = true;
	GetConstNodeHandle()->IterateConstInputs([bIsEnabled = &bEnabled](FConstInputHandle InputHandle)
	{
		if (InputHandle->IsConnectionUserModifiable())
		{
			*bIsEnabled &= !InputHandle->IsConnected();
		}
	});
	return bEnabled;
}

void UMetasoundEditorGraphOutputNode::Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult)
{
#if WITH_EDITOR
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	Super::Validate(OutResult);

	// 2. Check if node is invalid, version is missing and cache if interface changes exist between the document's records and the registry
	FNodeHandle NodeHandle = GetNodeHandle();
	const FMetasoundFrontendVersion& MetasoundFrontendVersion = NodeHandle->GetInterfaceVersion();

	FName InterfaceNameToValidate = MetasoundFrontendVersion.Name;
	FMetasoundFrontendInterface InterfaceToValidate;
	if (ISearchEngine::Get().FindInterfaceWithHighestVersion(InterfaceNameToValidate, InterfaceToValidate))
	{
		const FName& NodeName = NodeHandle->GetNodeName();
		FText RequiredText;
		if (InterfaceToValidate.IsMemberOutputRequired(NodeName, RequiredText))
		{
			TArray<FConstInputHandle> InputHandles = NodeHandle->GetConstInputs();
			if (ensure(!InputHandles.IsEmpty()))
			{
				bool bIsConnected = InputHandles.Last()->IsConnected();
				if (!bIsConnected)
				{
					OutResult.SetMessage(EMessageSeverity::Warning, *RequiredText.ToString());
				}
			}
		}
	}
#endif // #if WITH_EDITOR
}

FMetasoundFrontendClassName UMetasoundEditorGraphOutputNode::GetClassName() const
{
	if (ensure(Output))
	{
		return Output->ClassName;
	}
	return FMetasoundFrontendClassName();
}

FGuid UMetasoundEditorGraphOutputNode::GetNodeID() const
{
	if (Output)
	{
		return Output->NodeID;
	}
	return FGuid();
}

bool UMetasoundEditorGraphOutputNode::CanUserDeleteNode() const
{
	return !GetNodeHandle()->IsInterfaceMember();
}

void UMetasoundEditorGraphOutputNode::SetNodeID(FGuid InNodeID)
{
	if (ensure(Output))
	{
		Output->NodeID = InNodeID;
	}
}

FLinearColor UMetasoundEditorGraphOutputNode::GetNodeTitleColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		return EditorSettings->OutputNodeTitleColor;
	}

	return Super::GetNodeTitleColor();
}

UMetasoundEditorGraphMember* UMetasoundEditorGraphOutputNode::GetMember() const
{
	return Output;
}

FSlateIcon UMetasoundEditorGraphOutputNode::GetNodeTitleIcon() const
{
	return FSlateIcon("MetaSoundStyle", "MetasoundEditor.Graph.Node.Class.Output");
}

void UMetasoundEditorGraphExternalNode::ReconstructNode()
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	Super::ReconstructNode();
}

FMetasoundFrontendVersionNumber UMetasoundEditorGraphExternalNode::FindHighestVersionInRegistry() const
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendClass HighestVersionClass;
	FMetasoundFrontendVersionNumber HighestVersionNumber = FMetasoundFrontendVersionNumber::GetInvalid();

	Metasound::Frontend::FConstNodeHandle NodeHandle = GetConstNodeHandle();
	const FMetasoundFrontendClassMetadata& Metadata = NodeHandle->GetClassMetadata();
	if (ISearchEngine::Get().FindClassWithHighestVersion(Metadata.GetClassName(), HighestVersionClass))
	{
		HighestVersionNumber = HighestVersionClass.Metadata.GetVersion();
	}

	return HighestVersionNumber;
}

bool UMetasoundEditorGraphExternalNode::CanAutoUpdate() const
{
	using namespace Metasound::Frontend;

	FClassInterfaceUpdates InterfaceUpdates;
	return GetConstNodeHandle()->CanAutoUpdate(InterfaceUpdates);
}

void UMetasoundEditorGraphExternalNode::CacheTitle()
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	constexpr bool bIncludeNamespace = false;
	FConstNodeHandle NodeHandle = GetNodeHandle();
	CachedTitle = FGraphBuilder::GetDisplayName(*NodeHandle, bIncludeNamespace);
}

void UMetasoundEditorGraphExternalNode::GetPinHoverText(const UEdGraphPin& Pin, FString& OutHoverText) const
{
	using namespace Metasound::Frontend;

	if (ClassName == FRerouteNodeTemplate::ClassName)
	{
		if (!ErrorMsg.IsEmpty())
		{
			OutHoverText = ErrorMsg;
			return;
		}
	}

	Super::GetPinHoverText(Pin, OutHoverText);
}

void UMetasoundEditorGraphExternalNode::Validate(Metasound::Editor::FGraphNodeValidationResult& OutResult)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

#if WITH_EDITOR
	Super::Validate(OutResult);

	FConstNodeHandle NodeHandle = GetNodeHandle();
	const FMetasoundFrontendClassMetadata& Metadata = NodeHandle->GetClassMetadata();

	// 1. Validate referenced graph recursively if defined as asset node class
	const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(Metadata);
	if (IMetaSoundAssetManager* AssetManager = IMetaSoundAssetManager::Get())
	{
		if (const FSoftObjectPath* Path = AssetManager->FindObjectPathFromKey(RegistryKey))
		{
			if (UObject* AssetObject = Path->ResolveObject())
			{
				FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(AssetObject);
				check(MetaSoundAsset);
				const UMetasoundEditorGraph* NodeGraph = CastChecked<UMetasoundEditorGraph>(&MetaSoundAsset->GetGraphChecked());
				const EMessageSeverity::Type MaxGraphMsg = static_cast<EMessageSeverity::Type>(NodeGraph->GetHighestMessageSeverity());
				switch (MaxGraphMsg)
				{
					case EMessageSeverity::Error:
					{
						OutResult.SetMessage(MaxGraphMsg, TEXT("Referenced asset class contains error(s). Check implementation for details."));
					}
					break;

					case EMessageSeverity::PerformanceWarning:
					case EMessageSeverity::Warning:
					{
						OutResult.SetMessage(MaxGraphMsg, TEXT("Referenced asset class contains warning(s). Check implementation for details."));
					}
					break;

					case EMessageSeverity::Info:
					default:
					{
					}
					break;
				}
			}
		}
	}

	// 2. Validate template nodes
	if (Metadata.GetType() == EMetasoundFrontendClassType::Template)
	{
		const FNodeRegistryKey Key = NodeRegistryKey::CreateKey(Metadata);
		if (const INodeTemplate* Template = INodeTemplateRegistry::Get().FindTemplate(Key))
		{
			const bool bIsValidInterface = Template->IsValidNodeInterface(NodeHandle->GetNodeInterface());
			if (!bIsValidInterface)
			{
				OutResult.SetMessage(EMessageSeverity::Error, FString::Format(TEXT("Cannot implement template interface for node class '{0}"), { *Metadata.GetClassName().ToString() }));
			}
			else
			{
#if WITH_EDITOR
				if (!Template->HasRequiredConnections(NodeHandle))
				{
					OutResult.SetMessage(EMessageSeverity::Warning, TEXT("Reroute node(s) missing non-reroute input connection(s)."));
				}
#endif // WITH_EDITOR
			}
		}
		else
		{
			OutResult.SetMessage(EMessageSeverity::Error, FString::Format(TEXT("Template node interface missing for node class '{0}'"), { *Metadata.GetClassName().ToString()}));
		}
	}

	// 3. Check if node is invalid, version is missing and cache if interface changes exist between the document's records and the registry
	FClassInterfaceUpdates InterfaceUpdates;
	if (!NodeHandle->DiffAgainstRegistryInterface(InterfaceUpdates, false /* bUseHighestMinorVersion */))
	{
		if (NodeHandle->IsValid())
		{
			const FText* PromptIfMissing = nullptr;
			FString FormattedClassName;
			if (bIsClassNative)
			{
				PromptIfMissing = &Metadata.GetPromptIfMissing();
				FormattedClassName = FString::Format(TEXT("{0} {1} ({2})"), { *Metadata.GetDisplayName().ToString(), *Metadata.GetVersion().ToString(), *Metadata.GetClassName().ToString() });
			}
			else
			{
				static const FText AssetPromptIfMissing = LOCTEXT("PromptIfAssetMissing", "Asset may have not been saved, deleted or is not loaded (ex. in an unloaded plugin).");
				PromptIfMissing = &AssetPromptIfMissing;
				FormattedClassName = FString::Format(TEXT("{0} {1} ({2})"), { *Metadata.GetDisplayName().ToString(), *Metadata.GetVersion().ToString(), *Metadata.GetClassName().Name.ToString() });
			}

			const FString NewErrorMsg = FString::Format(TEXT("Class definition '{0}' not found: {1}"),
			{
				*FormattedClassName,
				*PromptIfMissing->ToString()
			});

			OutResult.SetMessage(EMessageSeverity::Error, NewErrorMsg);
		}
		else
		{
			if (bIsClassNative)
			{
				OutResult.SetMessage(EMessageSeverity::Error, FString::Format(
					TEXT("Class '{0}' definition missing for last known natively defined node."),
					{ *ClassName.ToString() }));
			}
			else
			{
				OutResult.SetMessage(EMessageSeverity::Error,
					FString::Format(TEXT("Class definition missing for asset with guid '{0}': Asset is either missing or invalid"),
					{ *ClassName.Name.ToString() }));
			}
		}
	}

	// 4. Report if node was nativized
	bool bNewIsClassNative = FMetasoundFrontendRegistryContainer::Get()->IsNodeNative(RegistryKey);
	if (bIsClassNative != bNewIsClassNative)
	{
		if (bNewIsClassNative)
		{
			NodeUpgradeMessage = FText::Format(LOCTEXT("MetaSoundNode_NativizedMessage", "Class '{0}' has been nativized."), Metadata.GetDisplayName());
		}

		bIsClassNative = bNewIsClassNative;
	}

	// 5. Report if node was auto-updated
	FMetasoundFrontendNodeStyle Style = NodeHandle->GetNodeStyle();
	if (Style.bMessageNodeUpdated)
	{
		NodeUpgradeMessage = FText::Format(LOCTEXT("MetaSoundNode_UpgradedMessage", "Node class '{0}' updated to version {1}"),
			Metadata.GetDisplayName(),
			FText::FromString(Metadata.GetVersion().ToString())
		);
	}

	// 6. Reset pin state (if pin was orphaned or clear if no longer orphaned)
	for (UEdGraphPin* Pin : Pins)
	{
		bool bWasRemoved = false;
		if (Pin->Direction == EGPD_Input)
		{
			FInputHandle Input = FGraphBuilder::GetInputHandleFromPin(Pin);
			bWasRemoved |= InterfaceUpdates.RemovedInputs.ContainsByPredicate([&](const FMetasoundFrontendClassInput* ClassInput)
			{
				return Input->GetName() == ClassInput->Name && Input->GetDataType() == ClassInput->TypeName;
			});
		}

		if (Pin->Direction == EGPD_Output)
		{
			FOutputHandle Output = FGraphBuilder::GetOutputHandleFromPin(Pin);
			bWasRemoved |= InterfaceUpdates.RemovedOutputs.ContainsByPredicate([&](const FMetasoundFrontendClassOutput* ClassOutput)
			{
				return Output->GetName() == ClassOutput->Name && Output->GetDataType() == ClassOutput->TypeName;
			});
		}

		OutResult.SetPinOrphaned(*Pin, bWasRemoved);
	}

	// 7. Report if node class is deprecated
	FMetasoundFrontendClass RegisteredClass;
	if (FMetasoundFrontendRegistryContainer::Get()->GetFrontendClassFromRegistered(RegistryKey, RegisteredClass))
	{
		if (RegisteredClass.Metadata.GetIsDeprecated())
		{
			constexpr bool bIncludeNamespace = true;
			OutResult.SetMessage(EMessageSeverity::Warning,
				FString::Format(TEXT("Class '{0} {1}' is deprecated."),
				{
					*FGraphBuilder::GetDisplayName(RegisteredClass.Metadata, { }, bIncludeNamespace).ToString(),
					*RegisteredClass.Metadata.GetVersion().ToString()
				}));
		}
	}

	// 8. Find all available versions & report if upgrade available
	const Metasound::FNodeClassName NodeClassName = Metadata.GetClassName().ToNodeClassName();
	const TArray<FMetasoundFrontendClass> SortedClasses = ISearchEngine::Get().FindClassesWithName(NodeClassName, true /* bSortByVersion */);
	if (SortedClasses.IsEmpty())
	{
		OutResult.SetMessage(EMessageSeverity::Error,
			FString::Format(TEXT("Class '{0} {1}' not registered."),
			{
				*Metadata.GetClassName().ToString(),
				*Metadata.GetVersion().ToString()
			}));
	}
	else
	{
		const FMetasoundFrontendVersionNumber& CurrentVersion = Metadata.GetVersion();
		const FMetasoundFrontendClass& HighestRegistryClass = SortedClasses[0];
		if (HighestRegistryClass.Metadata.GetVersion() > CurrentVersion)
		{
			FMetasoundFrontendClass HighestMinorVersionClass;
			FString NodeMsg;
			EMessageSeverity::Type Severity;

			const bool bClassVersionExists = SortedClasses.ContainsByPredicate([InCurrentVersion = &CurrentVersion](const FMetasoundFrontendClass& AvailableClass)
			{
				return AvailableClass.Metadata.GetVersion() == *InCurrentVersion;
			});
			if (bClassVersionExists)
			{
				NodeMsg = FString::Format(TEXT("Node class '{0} {1}' is prior version: Eligible for upgrade to {2}"),
				{
					*Metadata.GetClassName().ToString(),
					*Metadata.GetVersion().ToString(),
					*HighestRegistryClass.Metadata.GetVersion().ToString()
				});
				Severity = EMessageSeverity::Warning;
			}
			else
			{
				NodeMsg = FString::Format(TEXT("Node class '{0} {1}' is missing and ineligible for auto-update.  Highest version '{2}' found."),
				{
					*Metadata.GetClassName().ToString(),
					*Metadata.GetVersion().ToString(),
					*HighestRegistryClass.Metadata.GetVersion().ToString()
				});
				Severity = EMessageSeverity::Error;
			}

			OutResult.SetMessage(Severity, NodeMsg);
		}
		else if (HighestRegistryClass.Metadata.GetVersion() == CurrentVersion)
		{
			if (InterfaceUpdates.ContainsChanges())
			{
				OutResult.SetMessage(EMessageSeverity::Error,
					FString::Format(TEXT("Node & registered class interface mismatch: '{0} {1}'. Class either versioned improperly, class key collision exists, or AutoUpdate disabled in 'MetaSound' Developer Settings."),
					{
						*Metadata.GetClassName().ToString(),
						*Metadata.GetVersion().ToString()
					}));
			}
		}
		else
		{
			OutResult.SetMessage(EMessageSeverity::Error,
				FString::Format(TEXT("Node with class '{0} {1}' interface version higher than that of highest minor revision ({2}) in class registry."),
				{
					*Metadata.GetClassName().ToString(),
					*Metadata.GetVersion().ToString(),
					*HighestRegistryClass.Metadata.GetVersion().ToString()
				}));
		}
	}
#endif // WITH_EDITOR
}

FLinearColor UMetasoundEditorGraphExternalNode::GetNodeTitleColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		if (bIsClassNative)
		{
			return EditorSettings->NativeNodeTitleColor;
		}

		return EditorSettings->AssetReferenceNodeTitleColor;
	}

	return Super::GetNodeTitleColor();
}

FSlateIcon UMetasoundEditorGraphExternalNode::GetNodeTitleIcon() const
{
	if (bIsClassNative)
	{
		return FSlateIcon("MetaSoundStyle", "MetasoundEditor.Graph.Node.Class.Native");
	}
	else
	{
		return FSlateIcon("MetasoundStyle", "MetasoundEditor.Graph.Node.Class.Graph");
	}
}

bool UMetasoundEditorGraphExternalNode::ShouldDrawNodeAsControlPointOnly(int32& OutInputPinIndex, int32& OutOutputPinIndex) const
{
	using namespace Metasound::Frontend;

	if (GetClassName() == FRerouteNodeTemplate::ClassName)
	{
		OutInputPinIndex = 0;
		OutOutputPinIndex = 1;
		return true;
	}

	return Super::ShouldDrawNodeAsControlPointOnly(OutInputPinIndex, OutOutputPinIndex);
}

UMetasoundEditorGraphMember* UMetasoundEditorGraphVariableNode::GetMember() const
{
	return Variable;
}

bool UMetasoundEditorGraphVariableNode::EnableInteractWidgets() const
{
	using namespace Metasound::Frontend;

	bool bEnabled = false;

	if (Variable)
	{
		FConstVariableHandle VariableHandle = Variable->GetConstVariableHandle();
		FConstNodeHandle MutatorNode = VariableHandle->FindMutatorNode();
		if (MutatorNode->IsValid())
		{
			if (MutatorNode->GetID() == NodeID)
			{
				bEnabled = true;
				MutatorNode->IterateConstInputs([bIsEnabled = &bEnabled](FConstInputHandle InputHandle)
				{
					if (InputHandle->IsConnectionUserModifiable())
					{
						// Don't enable if variable input is connected
						*bIsEnabled &= !InputHandle->IsConnected();
					}
				});
			}
		}
	}

	return bEnabled;
}

FMetasoundFrontendClassName UMetasoundEditorGraphVariableNode::GetClassName() const
{
	return ClassName;
}

EMetasoundFrontendClassType UMetasoundEditorGraphVariableNode::GetClassType() const
{
	return ClassType;
}

FGuid UMetasoundEditorGraphVariableNode::GetNodeID() const
{
	return NodeID;
}

FName UMetasoundEditorGraphVariableNode::GetCornerIcon() const
{
	if (ClassType == EMetasoundFrontendClassType::VariableDeferredAccessor)
	{
		return TEXT("Graph.Latent.LatentIcon");
	}

	return Super::GetCornerIcon();
}

void UMetasoundEditorGraphVariableNode::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (Pin && Pin->Direction == EGPD_Input)
	{
		UObject& MetaSound = GetMetasoundChecked();
		MetaSound.Modify();

		FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
		if (InputHandle->IsValid())
		{
			FMetasoundFrontendLiteral LiteralValue;
			if (FGraphBuilder::GetPinLiteral(*Pin, LiteralValue))
			{
				// If this is the mutator node, synchronize the variable default literal with this default.
				FNodeHandle MutatorNode = Variable->GetVariableHandle()->FindMutatorNode();
				if (MutatorNode->IsValid())
				{
					if (MutatorNode->GetID() == NodeID)
					{
						UMetasoundEditorGraphMemberDefaultLiteral* Literal = Variable->GetLiteral();
						if (ensure(Literal))
						{
							// Clamp float literal if necessary 
							bool bClampedFloatLiteral = false;
							if (const UMetasoundEditorGraphMemberDefaultFloat* DefaultFloatLiteral = Cast<UMetasoundEditorGraphMemberDefaultFloat>(Literal))
							{
								bClampedFloatLiteral = ClampFloatLiteral(DefaultFloatLiteral, LiteralValue);
							}
							Literal->SetFromLiteral(LiteralValue);

							constexpr bool bPostTransaction = false;
							Variable->UpdateFrontendDefaultLiteral(bPostTransaction);

							if (bClampedFloatLiteral)
							{
								// Update graph node if it was clamped
								FGraphBuilder::RegisterGraphWithFrontend(MetaSound);
								if (FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound))
								{
									MetaSoundAsset->GetModifyContext().AddNodeIDsModified({ NodeID });
								}
							}
						}
					}
				}
			}
		}
	}
}

FLinearColor UMetasoundEditorGraphVariableNode::GetNodeTitleColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		return EditorSettings->VariableNodeTitleColor;
	}

	return Super::GetNodeTitleColor();
}

FSlateIcon UMetasoundEditorGraphVariableNode::GetNodeTitleIcon() const
{
	return FSlateIcon();
}

void UMetasoundEditorGraphVariableNode::SetNodeID(FGuid InNodeID)
{
	NodeID = InNodeID;
}
#undef LOCTEXT_NAMESPACE

