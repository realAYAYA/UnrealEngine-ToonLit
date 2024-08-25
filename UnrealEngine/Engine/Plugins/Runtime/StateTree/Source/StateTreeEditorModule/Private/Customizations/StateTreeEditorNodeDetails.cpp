// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorNodeDetails.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "IDetailChildrenBuilder.h"
#include "StateTree.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorData.h"
#include "StateTreePropertyRef.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateIconFinder.h"
#include "Modules/ModuleManager.h"
#include "InstancedStructDetails.h"
#include "StateTreeBindingExtension.h"
#include "StateTreeDelegates.h"
#include "StateTreePropertyHelpers.h"
#include "StateTreeEditorStyle.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "StateTreeEditorModule.h"
#include "StateTreeNodeClassCache.h"
#include "Styling/StyleColors.h"
#include "StateTreeCompiler.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SSearchBox.h"
#include "EditorFontGlyphs.h"
#include "StateTreeEditorNodeUtils.h"
#include "StateTreeEditorSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Debugger/StateTreeDebuggerUIExtensions.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

/** Helper class to detect if there were issues when calling ImportText() */
class FStateTreeDefaultValueImportErrorContext : public FOutputDevice
{
public:

	int32 NumErrors;

	FStateTreeDefaultValueImportErrorContext()
		: FOutputDevice()
		, NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		NumErrors++;
	}
};

namespace UE::StateTreeEditor::Internal
{
	/**
	 * This function recursively instantiates instanced objects of a given struct.
	 * It is needed to fixup nodes pasted from clipboard, which seem to give shallow copy.
	 */
	void InstantiateStructSubobjects(UObject& OuterObject, FStructView Struct)
	{
		// Empty struct, nothing to do.
		if (!Struct.IsValid())
		{
			return;
		}

		for (TPropertyValueIterator<FProperty> It(Struct.GetScriptStruct(), Struct.GetMemory()); It; ++It)
		{
			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(It->Key))
			{
				// Duplicate instanced objects.
				if (ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_PersistentInstance))
				{
					if (UObject* Object = ObjectProperty->GetObjectPropertyValue(It->Value))
					{
						UObject* DuplicatedObject = DuplicateObject(Object, &OuterObject);
						ObjectProperty->SetObjectPropertyValue(const_cast<void*>(It->Value), DuplicatedObject);
					}
				}
			}
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(It->Key))
			{
				// If we encounter instanced struct, recursively handle it too.
				if (StructProperty->Struct == TBaseStructure<FInstancedStruct>::Get())
				{
					FInstancedStruct& InstancedStruct = *static_cast<FInstancedStruct*>(const_cast<void*>(It->Value));
					InstantiateStructSubobjects(OuterObject, InstancedStruct);
				}
			}
		}
	}

	/** @return text describing the pin type, matches SPinTypeSelector. */
	FText GetPinTypeText(const FEdGraphPinType& PinType)
	{
		const FName PinSubCategory = PinType.PinSubCategory;
		const UObject* PinSubCategoryObject = PinType.PinSubCategoryObject.Get();
		if (PinSubCategory != UEdGraphSchema_K2::PSC_Bitmask && PinSubCategoryObject)
		{
			if (const UField* Field = Cast<const UField>(PinSubCategoryObject))
			{
				return Field->GetDisplayNameText();
			}
			return FText::FromString(PinSubCategoryObject->GetName());
		}

		return UEdGraphSchema_K2::GetCategoryText(PinType.PinCategory, NAME_None, true);
	}

	/** @return UClass or UScriptStruct of class or struct property, nullptr for others. */
	UStruct* GetPropertyStruct(TSharedPtr<IPropertyHandle> PropHandle)
	{
		if (!PropHandle.IsValid())
		{
			return nullptr;
		}

		if (const FStructProperty* StructProperty = CastField<FStructProperty>(PropHandle->GetProperty()))
		{
			return StructProperty->Struct;
		}
		
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropHandle->GetProperty()))
		{
			return ObjectProperty->PropertyClass;
		}

		return nullptr;
	}

	void ModifyRow(IDetailPropertyRow& ChildRow, const FGuid& ID, UStateTreeEditorData* EditorData)
	{
		FStateTreeEditorPropertyBindings* EditorPropBindings = EditorData ? EditorData->GetPropertyEditorBindings() : nullptr;
		if (!EditorPropBindings)
		{
			return;
		}
		
		TSharedPtr<IPropertyHandle> ChildPropHandle = ChildRow.GetPropertyHandle();
		check(ChildPropHandle.IsValid());
		
		const EStateTreePropertyUsage Usage = UE::StateTree::GetUsageFromMetaData(ChildPropHandle->GetProperty());
		const FProperty* Property = ChildPropHandle->GetProperty();
		
		// Conditionally control visibility of the value field of bound properties.
		if (Usage != EStateTreePropertyUsage::Invalid && ID.IsValid())
		{
			// Pass the node ID to binding extension. Since the properties are added using AddChildStructure(), we break the hierarchy and cannot access parent.
			ChildPropHandle->SetInstanceMetaData(UE::StateTree::PropertyBinding::StateTreeNodeIDName, LexToString(ID));

			FStateTreePropertyPath Path(ID, *Property->GetFName().ToString());
			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;
			FDetailWidgetRow Row;
			ChildRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

			auto IsValueVisible = TAttribute<EVisibility>::Create([Path, EditorPropBindings]() -> EVisibility
				{
					return EditorPropBindings->HasPropertyBinding(Path) ? EVisibility::Collapsed : EVisibility::Visible;
				});

			bool bShowChildren = true;
			
			if (Usage == EStateTreePropertyUsage::Input || Usage == EStateTreePropertyUsage::Output || Usage == EStateTreePropertyUsage::Context)
			{
				// Do not show children for input, output and context.
				bShowChildren = false;
				
				FEdGraphPinType PinType;
				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

				// Show referenced type for property refs.
				if (UE::StateTree::PropertyRefHelpers::IsPropertyRef(*Property))
				{
					// Use internal type to construct PinType if it's property of PropertyRef type.
					PinType = UE::StateTree::PropertyRefHelpers::GetPropertyRefInternalTypeAsPin(*Property);
				}
				else
				{
					Schema->ConvertPropertyToPinType(Property, PinType);
				}
				
				const FSlateBrush* Icon = FBlueprintEditorUtils::GetIconFromPin(PinType, true);
				FText Text = GetPinTypeText(PinType);
				
				FText ToolTip; 
				FLinearColor IconColor = Schema->GetPinTypeColor(PinType);
				FText Label;
				FText LabelToolTip;

				if (Usage == EStateTreePropertyUsage::Input)
				{
					Label = LOCTEXT("LabelInput", "IN");
					LabelToolTip = LOCTEXT("InputToolTip", "This is Input property. It is always expected to be bound to some other property.");
				}
				else if (Usage == EStateTreePropertyUsage::Output)
				{
					Label = LOCTEXT("LabelOutput", "OUT");
					LabelToolTip = LOCTEXT("OutputToolTip", "This is Output property. The node will always set it's value, other nodes can bind to it.");
				}
				else if (Usage == EStateTreePropertyUsage::Context)
				{
					Label = LOCTEXT("LabelContext", "CONTEXT");
					LabelToolTip = LOCTEXT("ContextObjectToolTip", "This is Context property. It is automatically connected to one of the Contex objects, or can be overridden with property binding.");

					if (UStruct* Struct = GetPropertyStruct(ChildPropHandle))
					{
						const FStateTreeBindableStructDesc Desc = EditorData->FindContextData(Struct, ChildPropHandle->GetProperty()->GetName());
						if (Desc.IsValid())
						{
							// Show as connected.
							Icon = FCoreStyle::Get().GetBrush("Icons.Link");
							Text = FText::FromName(Desc.Name);
							
							ToolTip = FText::Format(
								LOCTEXT("ToolTipConnected", "Connected to Context {0}."),
									FText::FromName(Desc.Name));
						}
						else
						{
							// Show as unconnected.
							Icon = FCoreStyle::Get().GetBrush("Icons.Warning");
							ToolTip = LOCTEXT("ToolTipNotConnected", "Could not connect Context property automatically.");
						}
					}
					else
					{
						// Mismatching type.
						Text = LOCTEXT("ContextObjectInvalidType", "Invalid type");
						ToolTip = LOCTEXT("ContextObjectInvalidTypeTooltip", "Context properties must be Object references or Structs.");
						Icon = FCoreStyle::Get().GetBrush("Icons.ErrorWithColor");
						IconColor = FLinearColor::White;
					}
				}
				
				ChildRow
					.CustomWidget(bShowChildren)
					.NameContent()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							NameWidget.ToSharedRef()
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(SBorder)
							.Padding(FMargin(6, 1))
							.BorderImage(FStateTreeEditorStyle::Get().GetBrush("StateTree.Param.Background"))
							.Visibility(Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							[
								SNew(STextBlock)
								.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Param.Label")
								.ColorAndOpacity(FStyleColors::Foreground)
								.Text(Label)
								.ToolTipText(LabelToolTip)
							]
						]

					]
					.ValueContent()
					[
						SNew(SHorizontalBox)
						.Visibility(IsValueVisible)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(4.0f, 0.0f)
						[
							SNew(SImage)
							.Image(Icon)
							.ColorAndOpacity(IconColor)
							.ToolTipText(ToolTip)
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Font(IDetailLayoutBuilder::GetDetailFont())
							.Text(Text)
							.ToolTipText(ToolTip)
						]
					];
			}
		}
	}

	struct FNodeRetainPropertyData
	{
		FStateTreeNodeBase* NodeBase = nullptr;
		const UScriptStruct* NodeBaseStruct = nullptr;
		const UStruct* InstanceStruct = nullptr;
		void* InstanceData = nullptr;
	};

	FNodeRetainPropertyData GetNodeData(FStateTreeEditorNode& EditorNode)
	{
		FNodeRetainPropertyData Data;
		Data.NodeBase = EditorNode.Node.GetMutablePtr<FStateTreeNodeBase>();

		if (Data.NodeBase)
		{
			Data.NodeBaseStruct = EditorNode.Node.GetScriptStruct();
			if (const UStruct* InstanceDataType = Data.NodeBase->GetInstanceDataType())
			{
				if (InstanceDataType->IsA<UScriptStruct>())
				{
					Data.InstanceStruct = EditorNode.Instance.GetScriptStruct();
					Data.InstanceData = EditorNode.Instance.GetMutableMemory();
				}
				else if (InstanceDataType->IsA<UClass>())
				{
					Data.InstanceStruct = EditorNode.InstanceObject.GetClass();
					Data.InstanceData = EditorNode.InstanceObject;
				}
			}
		}

		return Data;
	}

	void CopyPropertyValues(const UStruct* OldStruct, const void* OldData, const UStruct* NewStruct, void* NewData)
	{
		for (TFieldIterator<FProperty> It(OldStruct); It; ++It)
		{
			const FProperty* OldProperty = *It;
			const FProperty* NewProperty = NewStruct->FindPropertyByName(OldProperty->GetFName());
			if (!NewProperty)
			{
				// Let's check if we have the same property present but with(out) the 'b' prefix
				const FBoolProperty* BoolProperty = ExactCastField<const FBoolProperty>(OldProperty);
				if (!BoolProperty)
					continue;

				FString String = OldProperty->GetName();
				if (String.IsEmpty())
					continue;

				if (String[0] == TEXT('b'))
					String.RightChopInline(1, EAllowShrinking::No);
				else
					String.InsertAt(0, TEXT('b'));

				NewProperty = NewStruct->FindPropertyByName(FName(String));
			}

			constexpr uint64 WantedFlags = CPF_Edit;
			constexpr uint64 UnwantedFlags = CPF_DisableEditOnInstance | CPF_EditConst;

			if (NewProperty
				&& OldProperty->HasAllPropertyFlags(WantedFlags)
				&& NewProperty->HasAllPropertyFlags(WantedFlags)
				&& !OldProperty->HasAnyPropertyFlags(UnwantedFlags)
				&& !NewProperty->HasAnyPropertyFlags(UnwantedFlags)
				&& NewProperty->SameType(OldProperty))
			{
				OldProperty->CopyCompleteValue(
					NewProperty->ContainerPtrToValuePtr<void>(NewData),
					OldProperty->ContainerPtrToValuePtr<void>(OldData)
				);
			}
		}
	}

	void RetainProperties(FStateTreeEditorNode& OldNode, FStateTreeEditorNode& NewNode)
	{
		const FNodeRetainPropertyData OldNodeData = GetNodeData(OldNode);
		const FNodeRetainPropertyData NewNodeData = GetNodeData(NewNode);

		if (OldNodeData.NodeBase && NewNodeData.NodeBase)
		{
			// Copy node -> node
			CopyPropertyValues(
				OldNodeData.NodeBaseStruct, OldNodeData.NodeBase,
				NewNodeData.NodeBaseStruct, NewNodeData.NodeBase
			);

			if (OldNodeData.InstanceStruct && OldNodeData.InstanceData)
			{
				// Copy instance data -> node
				CopyPropertyValues(
					OldNodeData.InstanceStruct, OldNodeData.InstanceData,
					NewNodeData.NodeBaseStruct, NewNodeData.NodeBase
				);

				if (NewNodeData.InstanceStruct && NewNodeData.InstanceData)
				{
					// Copy instance data -> instance data
					CopyPropertyValues(
						OldNodeData.InstanceStruct, OldNodeData.InstanceData,
						NewNodeData.InstanceStruct, NewNodeData.InstanceData
					);
				}
			}

			if (NewNodeData.InstanceStruct && NewNodeData.InstanceData)
			{
				// Copy node -> instance data
				CopyPropertyValues(
					OldNodeData.NodeBaseStruct, OldNodeData.NodeBase,
					NewNodeData.InstanceStruct, NewNodeData.InstanceData
				);
			}
		}
	}
} // UE::StateTreeEditor::Internal

// Customized version of FInstancedStructDataDetails used to hide bindable properties.
class FBindableNodeInstanceDetails : public FInstancedStructDataDetails
{
public:

	FBindableNodeInstanceDetails(TSharedPtr<IPropertyHandle> InStructProperty, FGuid InID, UStateTreeEditorData* InEditorData)
		: FInstancedStructDataDetails(InStructProperty)
		, EditorData(InEditorData)
	{
		ID = InID;
	}

	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow)
	{
		UE::StateTreeEditor::Internal::ModifyRow(ChildRow, ID, EditorData);
	}

	UStateTreeEditorData* EditorData;
	FGuid ID;
};

////////////////////////////////////

TMap<FObjectKey, FStateTreeEditorNodeDetails::FCategoryExpansionState> FStateTreeEditorNodeDetails::CategoryExpansionStates;

TSharedRef<IPropertyTypeCustomization> FStateTreeEditorNodeDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeEditorNodeDetails);
}

FStateTreeEditorNodeDetails::~FStateTreeEditorNodeDetails()
{
	UE::StateTree::PropertyBinding::OnStateTreePropertyBindingChanged.Remove(OnBindingChangedHandle);
}

void FStateTreeEditorNodeDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	NodeProperty = StructProperty->GetChildHandle(TEXT("Node"));
	InstanceProperty = StructProperty->GetChildHandle(TEXT("Instance"));
	InstanceObjectProperty = StructProperty->GetChildHandle(TEXT("InstanceObject"));
	IDProperty = StructProperty->GetChildHandle(TEXT("ID"));

	IndentProperty = StructProperty->GetChildHandle(TEXT("ConditionIndent"));
	OperandProperty = StructProperty->GetChildHandle(TEXT("ConditionOperand"));

	check(NodeProperty.IsValid());
	check(InstanceProperty.IsValid());
	check(IDProperty.IsValid());
	check(IndentProperty.IsValid());
	check(OperandProperty.IsValid());

	// Find base class and struct from meta data.
	static const FName BaseStructMetaName(TEXT("BaseStruct")); // TODO: move these names into one central place.
	static const FName BaseClassMetaName(TEXT("BaseClass")); // TODO: move these names into one central place.
	
	const FString BaseStructName = StructProperty->GetMetaData(BaseStructMetaName);
	BaseScriptStruct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName);

	const FString BaseClassName = StructProperty->GetMetaData(BaseClassMetaName);
	BaseClass = UClass::TryFindTypeSlow<UClass>(BaseClassName);
	
	const FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateSP(this, &FStateTreeEditorNodeDetails::ShouldResetToDefault);
	const FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateSP(this, &FStateTreeEditorNodeDetails::ResetToDefault);
	const FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FStateTreeEditorNodeDetails::OnIdentifierChanged);
	OnBindingChangedHandle = UE::StateTree::PropertyBinding::OnStateTreePropertyBindingChanged.AddRaw(this, &FStateTreeEditorNodeDetails::OnBindingChanged);

	FindOuterObjects();

	HeaderRow
		.WholeRowContent()
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(SHorizontalBox)
				// Indent
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(this, &FStateTreeEditorNodeDetails::GetIndentSize)
					.Visibility(this, &FStateTreeEditorNodeDetails::IsConditionVisible)
					[
						SNew(SComboButton)
						.ComboButtonStyle(FStateTreeEditorStyle::Get(), "StateTree.Node.Indent.ComboBox")
						.ContentPadding(2.0f)
						.HasDownArrow(false)
						.VAlign(VAlign_Center)
						.OnGetMenuContent(this, &FStateTreeEditorNodeDetails::OnGetIndentContent)
						.ButtonContent()
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT(">")))
							.TextStyle(FStateTreeEditorStyle::Get(), "Details.Normal")
							.ToolTipText(LOCTEXT("IndentTooltip", "Indent of the expression row, controls parentheses and evaluation order."))
						]
					]
				]
				// Operand
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(30.0f)
					.Padding(FMargin(2, 4))
					.VAlign(VAlign_Center)
					.Visibility(this, &FStateTreeEditorNodeDetails::IsConditionVisible)
					[
						SNew(SComboButton)
						.IsEnabled(TAttribute<bool>(this, &FStateTreeEditorNodeDetails::IsOperandEnabled))
						.ComboButtonStyle(FStateTreeEditorStyle::Get(), "StateTree.Node.Operand.ComboBox")
						.ButtonColorAndOpacity(this, &FStateTreeEditorNodeDetails::GetOperandColor)
						.HasDownArrow(false)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnGetMenuContent(this, &FStateTreeEditorNodeDetails::OnGetOperandContent)
						.ButtonContent()
						[
							SNew(STextBlock)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Node.Operand")
							.Text(this, &FStateTreeEditorNodeDetails::GetOperandText)
						]
					]
				]
				// Open parens
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Node.Parens")
					.Text(this, &FStateTreeEditorNodeDetails::GetOpenParens)
					.Visibility(this, &FStateTreeEditorNodeDetails::IsConditionVisible)
				]
				// Name & type selection
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(FMargin(4.0f, 0.0f, 0.0f, 0.0f)))
				.VAlign(VAlign_Center)
				[
					SAssignNew(ComboButton, SComboButton)
					.OnGetMenuContent(this, &FStateTreeEditorNodeDetails::GeneratePicker)
					.ToolTipText(this, &FStateTreeEditorNodeDetails::GetDisplayValueString)
					.ContentPadding(FMargin(2, 2, 2, 2))
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0, 0, 4, 0)
						[
							SNew(STextBlock)
							.Text(FEditorFontGlyphs::Paper_Plane)
							.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.DetailsIcon")
							.Visibility(this, &FStateTreeEditorNodeDetails::IsTaskVisible)
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(0, 0, 8, 0)
						[
							SNew(SEditableText)
							.IsEnabled(TAttribute<bool>(this, &FStateTreeEditorNodeDetails::IsNameEnabled))
							.Text(this, &FStateTreeEditorNodeDetails::GetName)
							.OnTextCommitted(this, &FStateTreeEditorNodeDetails::OnNameCommitted)
							.SelectAllTextWhenFocused(true)
							.RevertTextOnEscape(true)
							.Style(FStateTreeEditorStyle::Get(), "StateTree.Node.Name")
							.Visibility(this, &FStateTreeEditorNodeDetails::IsNameVisible)
						]
					]
				]
				// Close parens
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Node.Parens")
					.Text(this, &FStateTreeEditorNodeDetails::GetCloseParens)
					.Visibility(this, &FStateTreeEditorNodeDetails::IsConditionVisible)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					StructPropertyHandle->CreateDefaultPropertyButtonWidgets()
				]

				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Right)
				.FillWidth(1.0f)
				[
					UE::StateTreeEditor::DebuggerExtensions::CreateEditorNodeWidget(StructPropertyHandle, EditorData)
				]
			]
		]
		.OverrideResetToDefault(ResetOverride)
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::OnCopyNode)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::OnPasteNode)));
}

void FStateTreeEditorNodeDetails::OnCopyNode()
{
	FString Value;
	// Use PPF_Copy so that all properties get copied.
	if (StructProperty->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	}
}

void FStateTreeEditorNodeDetails::OnPasteNode()
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	if (PastedText.IsEmpty())
	{
		return;
	}
	
	// Create node from the clipboard data to figure out the node type. 
	FStateTreeEditorNode TempNode;
	UScriptStruct* NodeScriptStruct = TBaseStructure<FStateTreeEditorNode>::Get();

	TArray<UObject*> OuterObjects; 
	StructProperty->GetOuterObjects(OuterObjects);
	if (OuterObjects.IsEmpty())
	{
		return;
	}

	FStateTreeDefaultValueImportErrorContext ErrorPipe;
	NodeScriptStruct->ImportText(*PastedText, &TempNode, nullptr, PPF_None, &ErrorPipe, NodeScriptStruct->GetName());
	
	const UStruct* NodeTypeStruct = TempNode.Node.GetScriptStruct(); 
	// Only allow valid node types for this property (e.g. do not mix task with conditions).
	if (ErrorPipe.NumErrors > 0 || !NodeTypeStruct || !NodeTypeStruct->IsChildOf(BaseScriptStruct))
	{
		FNotificationInfo NotificationInfo(FText::GetEmpty());
		NotificationInfo.Text = FText::Format(LOCTEXT("NotSupportedByType", "This property only accepts nodes of type {0}."), BaseScriptStruct->GetDisplayNameText());
		NotificationInfo.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		return;
	}
	
	// Reject nodes that are not allowed by the schema.
	const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;
	if (Schema)
	{
		bool bNodeIsAllowed = false;
		
		// BP nodes are identified by the instance type.
		if (NodeTypeStruct->IsChildOf(FStateTreeBlueprintEvaluatorWrapper::StaticStruct())
			|| NodeTypeStruct->IsChildOf(FStateTreeBlueprintTaskWrapper::StaticStruct())
			|| NodeTypeStruct->IsChildOf(FStateTreeBlueprintConditionWrapper::StaticStruct()))
		{
			if (const FStateTreeNodeBase* Node = TempNode.Node.GetPtr<FStateTreeNodeBase>())
			{
				NodeTypeStruct = Node->GetInstanceDataType(); // Report error with the BP node type, as that is what the user expects to see.
				if (const UClass* InstanceClass = Cast<UClass>(NodeTypeStruct))
				{
					bNodeIsAllowed = Schema->IsClassAllowed(InstanceClass);
				}
			}
		}
		else
		{
			bNodeIsAllowed = Schema->IsStructAllowed(TempNode.Node.GetScriptStruct());
		}
		
		if (!bNodeIsAllowed)
		{

			FNotificationInfo NotificationInfo(FText::GetEmpty());
			NotificationInfo.Text = FText::Format(LOCTEXT("NotSupportedBySchema", "Node {0} is not supported by {1} schema."),
										NodeTypeStruct->GetDisplayNameText(), Schema->GetClass()->GetDisplayNameText());
			NotificationInfo.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
			return;
		}
	}
	
	FScopedTransaction Transaction(LOCTEXT("PasteNode", "Paste Node"));

	StructProperty->NotifyPreChange();

	// Reset GUIDs on paste
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	if (OuterObjects.Num() == RawNodeData.Num())
	{
		for (int32 Index = 0; Index < RawNodeData.Num(); Index++)
		{
			UObject* OuterObject = OuterObjects[Index];
			FStateTreeEditorNode* EditorNode = static_cast<FStateTreeEditorNode*>(RawNodeData[Index]);
			if (EditorNode && OuterObject)
			{
				// Copy
				*EditorNode = TempNode;

				// Ensure unique instance value
				UE::StateTreeEditor::Internal::InstantiateStructSubobjects(*OuterObject, EditorNode->Node);
				if (EditorNode->InstanceObject)
				{
					EditorNode->InstanceObject = DuplicateObject(EditorNode->InstanceObject, OuterObject);
				}
				else
				{
					UE::StateTreeEditor::Internal::InstantiateStructSubobjects(*OuterObject, EditorNode->Instance);
				}
				
				if (FStateTreeNodeBase* Node = EditorNode->Node.GetMutablePtr<FStateTreeNodeBase>())
				{
					Node->Name = FName(Node->Name.ToString() + TEXT(" Copy"));
				}

				const FGuid OldStructID = EditorNode->ID; 
				EditorNode->ID = FGuid::NewGuid();

				// Copy bindings from the copied node.
				if (OldStructID.IsValid() && EditorData)
				{
					if (FStateTreeEditorPropertyBindings* Bindings = EditorData->GetPropertyEditorBindings())
					{
						Bindings->CopyBindings(OldStructID, EditorNode->ID);
					}
				}

			}
		}
	}

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

bool FStateTreeEditorNodeDetails::ShouldResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle) const
{
	check(StructProperty);
	
	bool bAnyValid = false;
	
	TArray<const void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	for (const void* Data : RawNodeData)
	{
		if (const FStateTreeEditorNode* Node = static_cast<const FStateTreeEditorNode*>(Data))
		{
			if (Node->Node.IsValid())
			{
				bAnyValid = true;
				break;
			}
		}
	}
	
	// Assume that the default value is empty. Any valid means that some can be reset to empty.
	return bAnyValid;
}


void FStateTreeEditorNodeDetails::ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	UE::StateTreeEditor::EditorNodeUtils::ModifyNodeInTransaction(LOCTEXT("OnTaskEnableToggled", "Toggled Task Enabled"),
		StructProperty,
		[](IPropertyHandle& StructPropertyHandle)
		{
	TArray<void*> RawNodeData;
			StructPropertyHandle.AccessRawData(RawNodeData);
	for (void* Data : RawNodeData)
	{
		if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(Data))
		{
			Node->Reset();
		}
	}
		});

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

void FStateTreeEditorNodeDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FGuid ID;
	UE::StateTree::PropertyHelpers::GetStructValue<FGuid>(IDProperty, ID);

	// ID
	if (UE::StateTree::Editor::GbDisplayItemIds)
	{
		// ID
		StructBuilder.AddProperty(IDProperty.ToSharedRef());
	}
	
	// Node
	TSharedRef<FBindableNodeInstanceDetails> NodeDetails = MakeShareable(new FBindableNodeInstanceDetails(NodeProperty, FGuid(), EditorData));
	StructBuilder.AddCustomBuilder(NodeDetails);

	// Instance
	TSharedRef<FBindableNodeInstanceDetails> InstanceDetails = MakeShareable(new FBindableNodeInstanceDetails(InstanceProperty, ID, EditorData));
	StructBuilder.AddCustomBuilder(InstanceDetails);

	// InstanceObject
	// Get the actual UObject from the pointer.
	TSharedPtr<IPropertyHandle> InstanceObjectValueProperty = GetInstancedObjectValueHandle(InstanceObjectProperty);
	if (InstanceObjectValueProperty.IsValid())
	{
		uint32 NumChildren = 0;
		InstanceObjectValueProperty->GetNumChildren(NumChildren);

		// Find visible child properties and sort them so in order: Context, Input, Param, Output.
		struct FSortedChild
		{
			TSharedPtr<IPropertyHandle> PropertyHandle;
			EStateTreePropertyUsage Usage = EStateTreePropertyUsage::Invalid;
		};
		
		TArray<FSortedChild> SortedChildren;
		for (uint32 Index = 0; Index < NumChildren; Index++)
		{
			if (TSharedPtr<IPropertyHandle> ChildHandle = InstanceObjectValueProperty->GetChildHandle(Index); ChildHandle.IsValid())
			{
				FSortedChild Child;
				Child.PropertyHandle = ChildHandle;
				Child.Usage = UE::StateTree::GetUsageFromMetaData(Child.PropertyHandle->GetProperty());

				// If the property is set to one of these usages, display it even if it is not edit on instance.
				// It is a common mistake to forget to set the "eye" on these properties it and wonder why it does not show up.
				const bool bShouldShowByUsage = Child.Usage == EStateTreePropertyUsage::Input || Child.Usage == EStateTreePropertyUsage::Output || Child.Usage == EStateTreePropertyUsage::Context;
        		const bool bIsEditable = !Child.PropertyHandle->GetProperty()->HasAllPropertyFlags(CPF_DisableEditOnInstance);

				if (bShouldShowByUsage || bIsEditable)
				{
					SortedChildren.Add(Child);
				}
			}
		}

		SortedChildren.StableSort([](const FSortedChild& LHS, const FSortedChild& RHS) { return LHS.Usage < RHS.Usage; });

		for (FSortedChild& Child : SortedChildren)
		{
			IDetailPropertyRow& ChildRow = StructBuilder.AddProperty(Child.PropertyHandle.ToSharedRef());
			UE::StateTreeEditor::Internal::ModifyRow(ChildRow, ID, EditorData);
		}
	}
}

TSharedPtr<IPropertyHandle> FStateTreeEditorNodeDetails::GetInstancedObjectValueHandle(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	TSharedPtr<IPropertyHandle> ChildHandle;

	uint32 NumChildren;
	PropertyHandle->GetNumChildren(NumChildren);

	if (NumChildren > 0)
	{
		// when the property is a (inlined) object property, the first child will be
		// the object instance, and its properties are the children underneath that
		ensure(NumChildren == 1);
		ChildHandle = PropertyHandle->GetChildHandle(0);
	}

	return ChildHandle;
}

void FStateTreeEditorNodeDetails::OnIdentifierChanged(const UStateTree& InStateTree)
{
	if (PropUtils && StateTree == &InStateTree)
	{
		PropUtils->ForceRefresh();
	}
}

void FStateTreeEditorNodeDetails::OnBindingChanged(const FStateTreePropertyPath& SourcePath, const FStateTreePropertyPath& TargetPath)
{
	check(StructProperty);

	if (!EditorData)
	{
		return;
	}

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);

	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	if (OuterObjects.Num() != RawNodeData.Num())
	{
		return;
	}

	const FStateTreeBindingLookup BindingLookup(EditorData);

	for (int32 i = 0; i < OuterObjects.Num(); i++)
	{
		FStateTreeEditorNode* EditorNode = static_cast<FStateTreeEditorNode*>(RawNodeData[i]);
		UObject* OuterObject = OuterObjects[i]; // Immediate outer, i.e StateTreeState
		if (EditorNode && OuterObject && EditorNode->ID == TargetPath.GetStructID())
		{
			FStateTreeNodeBase* Node = EditorNode->Node.GetMutablePtr<FStateTreeNodeBase>();
			FStateTreeDataView InstanceView = EditorNode->GetInstance(); 

			if (Node && InstanceView.IsValid())
			{
				OuterObject->Modify();
				Node->OnBindingChanged(EditorNode->ID, InstanceView, SourcePath, TargetPath, BindingLookup);
			}
		}
	}
}

void FStateTreeEditorNodeDetails::FindOuterObjects()
{
	check(StructProperty);
	
	EditorData = nullptr;
	StateTree = nullptr;

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (UObject* Outer : OuterObjects)
	{
		UStateTreeEditorData* OuterEditorData = Cast<UStateTreeEditorData>(Outer);
		if (OuterEditorData == nullptr)
		{
			OuterEditorData = Outer->GetTypedOuter<UStateTreeEditorData>();
		}
		
		UStateTree* OuterStateTree = OuterEditorData ? OuterEditorData->GetTypedOuter<UStateTree>() : nullptr;
		if (OuterEditorData && OuterStateTree)
		{
			StateTree = OuterStateTree;
			EditorData = OuterEditorData;
			break;
		}
	}
}

FOptionalSize FStateTreeEditorNodeDetails::GetIndentSize() const
{
	return FOptionalSize(15.0f + GetIndent() * 30.0f);
}

TSharedRef<SWidget> FStateTreeEditorNodeDetails::OnGetIndentContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	for (int32 Indent = 0; Indent < UE::StateTree::MaxConditionIndent; Indent++)
	{
		FUIAction ItemAction(
			FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::SetIndent, Indent),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FStateTreeEditorNodeDetails::IsIndent, Indent));
		MenuBuilder.AddMenuEntry(FText::AsNumber(Indent), TAttribute<FText>(), FSlateIcon(), ItemAction, FName(), EUserInterfaceActionType::Check);
	}

	return MenuBuilder.MakeWidget();
}

int32 FStateTreeEditorNodeDetails::GetIndent() const
{
	check(IndentProperty);
	
	uint8 Indent = 0;
	IndentProperty->GetValue(Indent);

	return Indent;
}

void FStateTreeEditorNodeDetails::SetIndent(const int32 Indent) const
{
	check(IndentProperty);
	
	IndentProperty->SetValue((uint8)FMath::Clamp(Indent, 0, UE::StateTree::MaxConditionIndent - 1));
}

bool FStateTreeEditorNodeDetails::IsIndent(const int32 Indent) const
{
	return Indent == GetIndent();
}

bool FStateTreeEditorNodeDetails::IsFirstItem() const
{
	check(StructProperty);
	return StructProperty->GetIndexInArray() == 0;
}

int32 FStateTreeEditorNodeDetails::GetCurrIndent() const
{
	// First item needs to be zero indent to make the parentheses counting to work properly.
	return IsFirstItem() ? 0 : GetIndent();
}

int32 FStateTreeEditorNodeDetails::GetNextIndent() const
{
	// Find the intent of the next item by finding the item in the parent array.
	check(StructProperty);
	TSharedPtr<IPropertyHandle> ParentProp = StructProperty->GetParentHandle();
	if (!ParentProp.IsValid())
	{
		return 0;
	}
	TSharedPtr<IPropertyHandleArray> ParentArray = ParentProp->AsArray();
	if (!ParentArray.IsValid())
	{
		return 0;
	}

	uint32 NumElements = 0;
	if (ParentArray->GetNumElements(NumElements) != FPropertyAccess::Success)
	{
		return 0;
	}
	
	const int32 NextIndex = StructProperty->GetIndexInArray() + 1;
	if (NextIndex >= (int32)NumElements)
	{
		return 0;
	}

	TSharedPtr<IPropertyHandle> NextStructProperty = ParentArray->GetElement(NextIndex);
	if (!NextStructProperty.IsValid())
	{
		return 0;
	}
	
	TSharedPtr<IPropertyHandle> NextIndentProperty = NextStructProperty->GetChildHandle(TEXT("ConditionIndent"));
	if (!NextIndentProperty.IsValid())
	{
		return 0;
	}
	
	uint8 Indent = 0;
	NextIndentProperty->GetValue(Indent);

	return Indent;
}

FText FStateTreeEditorNodeDetails::GetOpenParens() const
{
	check(IndentProperty);

	const int32 CurrIndent = GetCurrIndent();
	const int32 NextIndent = GetNextIndent();
	const int32 DeltaIndent = NextIndent - CurrIndent;
	const int32 OpenParens = FMath::Max(0, DeltaIndent);

	static_assert(UE::StateTree::MaxConditionIndent == 4);
	switch (OpenParens)
	{
		case 1: return FText::FromString(TEXT("("));
		case 2: return FText::FromString(TEXT("(("));
		case 3: return FText::FromString(TEXT("((("));
		case 4: return FText::FromString(TEXT("(((("));
	}
	return FText::GetEmpty();
}

FText FStateTreeEditorNodeDetails::GetCloseParens() const
{
	check(IndentProperty);

	const int32 CurrIndent = GetCurrIndent();
	const int32 NextIndent = GetNextIndent();
	const int32 DeltaIndent = NextIndent - CurrIndent;
	const int32 CloseParens = FMath::Max(0, -DeltaIndent);

	static_assert(UE::StateTree::MaxConditionIndent == 4);
	switch (CloseParens)
	{
	case 1: return FText::FromString(TEXT(")"));
	case 2: return FText::FromString(TEXT("))"));
	case 3: return FText::FromString(TEXT(")))"));
	case 4: return FText::FromString(TEXT("))))"));
	}
	return FText::GetEmpty();
}

FText FStateTreeEditorNodeDetails::GetOperandText() const
{
	check(OperandProperty);

	// First item does not relate to anything existing, it could be empty, we return IF to indicate that we're building condition. 
	if (IsFirstItem())
	{
		return LOCTEXT("IfOperand", "IF");
	}

	uint8 Value = 0;
	OperandProperty->GetValue(Value);
	const EStateTreeConditionOperand Operand = (EStateTreeConditionOperand)Value;

	if (Operand == EStateTreeConditionOperand::And)
	{
		return LOCTEXT("AndOperand", "AND");
	}
	else if (Operand == EStateTreeConditionOperand::Or)
	{
		return LOCTEXT("OrOperand", "OR");
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
	}

	return FText::GetEmpty();
}

FSlateColor FStateTreeEditorNodeDetails::GetOperandColor() const
{
	check(OperandProperty);

	if (IsFirstItem())
	{
		return FStyleColors::Transparent;
	}

	uint8 Value = 0; 
	OperandProperty->GetValue(Value);
	const EStateTreeConditionOperand Operand = (EStateTreeConditionOperand)Value;

	if (Operand == EStateTreeConditionOperand::And)
	{
		return FStyleColors::AccentPink;
	}
	else if (Operand == EStateTreeConditionOperand::Or)
	{
		return FStyleColors::AccentBlue;
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled operand %s"), *UEnum::GetValueAsString(Operand));
	}

	return FStyleColors::Transparent;
}

TSharedRef<SWidget> FStateTreeEditorNodeDetails::OnGetOperandContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	FUIAction AndAction(
		FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::SetOperand, EStateTreeConditionOperand::And),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FStateTreeEditorNodeDetails::IsOperand, EStateTreeConditionOperand::And));
	MenuBuilder.AddMenuEntry(LOCTEXT("AndOperand", "AND"), TAttribute<FText>(), FSlateIcon(), AndAction, FName(), EUserInterfaceActionType::Check);

	FUIAction OrAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::SetOperand, EStateTreeConditionOperand::Or),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FStateTreeEditorNodeDetails::IsOperand, EStateTreeConditionOperand::Or));
	MenuBuilder.AddMenuEntry(LOCTEXT("OrOperand", "OR"), TAttribute<FText>(), FSlateIcon(), OrAction, FName(), EUserInterfaceActionType::Check);

	return MenuBuilder.MakeWidget();
}

bool FStateTreeEditorNodeDetails::IsOperandEnabled() const
{
	return !IsFirstItem();
}

bool FStateTreeEditorNodeDetails::IsOperand(const EStateTreeConditionOperand Operand) const
{
	check(OperandProperty);

	uint8 Value = 0; 
	OperandProperty->GetValue(Value);
	const EStateTreeConditionOperand CurrOperand = (EStateTreeConditionOperand)Value;

	return CurrOperand == Operand;
}

void FStateTreeEditorNodeDetails::SetOperand(const EStateTreeConditionOperand Operand) const
{
	check(OperandProperty);

	OperandProperty->SetValue((uint8)Operand);
}

EVisibility FStateTreeEditorNodeDetails::IsConditionVisible() const
{
	return UE::StateTreeEditor::EditorNodeUtils::IsConditionVisible(StructProperty);
}

EVisibility FStateTreeEditorNodeDetails::IsTaskVisible() const
{
	return UE::StateTreeEditor::EditorNodeUtils::IsTaskVisible(StructProperty);
}

FText FStateTreeEditorNodeDetails::GetName() const
{
	check(StructProperty);

	// Multiple names do not make sense, just if only one node is selected.
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	if (RawNodeData.Num() == 1)
	{
		// Dig out name from the struct without knowing the type.
		FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(RawNodeData[0]);

		// Make sure the associated property has valid data (e.g. while moving an node in array)
		const UScriptStruct* ScriptStruct = Node != nullptr ? Node->Node.GetScriptStruct() : nullptr;
		if (ScriptStruct != nullptr)
		{
			if (FProperty* NameProperty = ScriptStruct->FindPropertyByName(TEXT("Name")))
			{
				if (NameProperty->IsA(FNameProperty::StaticClass()))
				{
					void* Ptr = const_cast<void*>(static_cast<const void*>(Node->Node.GetMemory()));
					const FName NameValue = *NameProperty->ContainerPtrToValuePtr<FName>(Ptr);
					if (NameValue.IsNone())
					{
						return GetDisplayValueString();
					}
					return FText::FromName(NameValue);
				}
			}
		}
		return LOCTEXT("Empty", "Empty");
	}

	return LOCTEXT("MultipleSelected", "Multiple Selected");
}

EVisibility FStateTreeEditorNodeDetails::IsNameVisible() const
{
	const UScriptStruct* ScriptStruct = nullptr;
	if (const FStateTreeEditorNode* Node = UE::StateTreeEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		ScriptStruct = Node->Node.GetScriptStruct();
	}

	if (ScriptStruct != nullptr && ScriptStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()))
	{
		const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;
		if (Schema && Schema->AllowMultipleTasks() == false)
		{
			// Single task states use the state name as task name.
			return EVisibility::Collapsed;
		}
	}
	
	return EVisibility::Visible;
}

void FStateTreeEditorNodeDetails::OnNameCommitted(const FText& NewText, ETextCommit::Type InTextCommit) const
{
	check(StructProperty);

	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		// Remove excess whitespace and prevent categories with just spaces
		FString NewName = FText::TrimPrecedingAndTrailing(NewText).ToString();

		if (GEditor)
		{
			GEditor->BeginTransaction(LOCTEXT("SetName", "Set Name"));
		}
		StructProperty->NotifyPreChange();

		TArray<void*> RawNodeData;
		StructProperty->AccessRawData(RawNodeData);
		
		for (void* Data : RawNodeData)
		{
			if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(Data))
			{
				// Set Name
				if (const UScriptStruct* ScriptStruct = Node->Node.GetScriptStruct())
				{
					if (FProperty* NameProperty = ScriptStruct->FindPropertyByName(TEXT("Name")))
					{
						if (NameProperty->IsA(FNameProperty::StaticClass()))
						{
							void* Ptr = const_cast<void*>(static_cast<const void*>(Node->Node.GetMemory()));
							FName& NameValue = *NameProperty->ContainerPtrToValuePtr<FName>(Ptr);
							NameValue = FName(NewName);
						}
					}
				}
			}
		}

		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);

		if (StateTree)
		{
			UE::StateTree::Delegates::OnIdentifierChanged.Broadcast(*StateTree);
		}

		if (GEditor)
		{
			GEditor->EndTransaction();
		}

		StructProperty->NotifyFinishedChangingProperties();
	}
}

bool FStateTreeEditorNodeDetails::IsNameEnabled() const
{
	// Can only edit if we have valid instantiated type.
	const FStateTreeEditorNode* Node = UE::StateTreeEditor::EditorNodeUtils::GetCommonNode(StructProperty);
	return Node && Node->Node.IsValid();
}

FText FStateTreeEditorNodeDetails::GetDisplayValueString() const
{
	if (const FStateTreeEditorNode* Node = UE::StateTreeEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (const UScriptStruct* ScriptStruct = Node->Node.GetScriptStruct())
		{
			if (ScriptStruct->IsChildOf(FStateTreeBlueprintEvaluatorWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FStateTreeBlueprintTaskWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FStateTreeBlueprintConditionWrapper::StaticStruct()))
			{
				if (Node->InstanceObject != nullptr
					&& Node->InstanceObject->GetClass() != nullptr)
				{
					return Node->InstanceObject->GetClass()->GetDisplayNameText();
				}
			}
			else
			{
				return ScriptStruct->GetDisplayNameText();
			}
		}
	}
	return FText();
}

const FSlateBrush* FStateTreeEditorNodeDetails::GetDisplayValueIcon() const
{
	if (const FStateTreeEditorNode* Node = UE::StateTreeEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (const UScriptStruct* ScriptStruct = Node->Node.GetScriptStruct())
		{
			if (ScriptStruct->IsChildOf(FStateTreeBlueprintEvaluatorWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FStateTreeBlueprintTaskWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FStateTreeBlueprintConditionWrapper::StaticStruct()))
			{
				if (Node->InstanceObject != nullptr)
				{
					return FSlateIconFinder::FindIconBrushForClass(Node->InstanceObject->GetClass());
				}
			}
		}
	}
	
	return FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
}

void FStateTreeEditorNodeDetails::SortNodeTypesFunctionItemsRecursive(TArray<TSharedPtr<FStateTreeNodeTypeItem>>& Items)
{
	Items.Sort([](const TSharedPtr<FStateTreeNodeTypeItem>& A, const TSharedPtr<FStateTreeNodeTypeItem>& B)
	{
		if (!A->GetCategoryName().IsEmpty() && !B->GetCategoryName().IsEmpty())
		{
			return A->GetCategoryName() < B->GetCategoryName();
		}
		if (!A->GetCategoryName().IsEmpty() && B->GetCategoryName().IsEmpty())
		{
			return true;
		}
		if (A->GetCategoryName().IsEmpty() && !B->GetCategoryName().IsEmpty())
		{
			return false;
		}
		if (A->Struct != nullptr && B->Struct != nullptr)
		{
			return A->Struct->GetDisplayNameText().CompareTo(B->Struct->GetDisplayNameText()) <= 0;
		}
		return true;
	});

	for (const TSharedPtr<FStateTreeNodeTypeItem>& Item : Items)
	{
		SortNodeTypesFunctionItemsRecursive(Item->Children);
	}
}

TSharedPtr<FStateTreeEditorNodeDetails::FStateTreeNodeTypeItem> FStateTreeEditorNodeDetails::FindOrCreateItemForCategory(TArray<TSharedPtr<FStateTreeNodeTypeItem>>& Items, TArrayView<FString> CategoryPath)
{
	check(CategoryPath.Num() > 0);

	const FString& CategoryName = CategoryPath.Last();

	int32 Idx = 0;
	for (; Idx < Items.Num(); ++Idx)
	{
		// found item
		if (Items[Idx]->GetCategoryName() == CategoryName)
		{
			return Items[Idx];
		}

		// passed the place where it should have been, break out
		if (Items[Idx]->GetCategoryName() > CategoryName)
		{
			break;
		}
	}

	TSharedPtr<FStateTreeNodeTypeItem> NewItem = Items.Insert_GetRef(MakeShared<FStateTreeNodeTypeItem>(), Idx);
	NewItem->CategoryPath = CategoryPath;
	return NewItem;
}

void FStateTreeEditorNodeDetails::AddNode(const UStruct* Struct)
{
	if (!Struct || !RootNode.IsValid())
	{
		return;
	}
		
	const FText CategoryName = Struct->GetMetaDataText("Category");
	if (CategoryName.IsEmpty())
	{
		TSharedPtr<FStateTreeNodeTypeItem>& Item = RootNode->Children.Add_GetRef(MakeShared<FStateTreeNodeTypeItem>());
		Item->Struct = Struct;
		return;
	}

	// Split into subcategories and trim
	TArray<FString> CategoryPath;
	CategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));
	for (FString& SubCategory : CategoryPath)
	{
		SubCategory.TrimStartAndEndInline();
	}

	TSharedPtr<FStateTreeNodeTypeItem> ParentItem = RootNode;

	// Create items for the entire category path
	// eg. "Math|Boolean|AND" 
	// Math 
	//   > Boolean
	//     > AND
	for (int32 PathIndex = 0; PathIndex < CategoryPath.Num(); ++PathIndex)
	{
		ParentItem = FindOrCreateItemForCategory(ParentItem->Children, MakeArrayView(CategoryPath.GetData(), PathIndex + 1));
	}

	if (ParentItem)
	{
		TSharedPtr<FStateTreeNodeTypeItem>& Item = ParentItem->Children.Add_GetRef(MakeShared<FStateTreeNodeTypeItem>());
		Item->Struct = Struct;
	}
}

void FStateTreeEditorNodeDetails::CacheNodeTypes()
{
	const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;

	// Get all usable nodes from the node class cache.
	FStateTreeEditorModule& EditorModule = FModuleManager::GetModuleChecked<FStateTreeEditorModule>(TEXT("StateTreeEditorModule"));
	FStateTreeNodeClassCache* ClassCache = EditorModule.GetNodeClassCache().Get();
	check(ClassCache);

	TArray<TSharedPtr<FStateTreeNodeClassData>> StructNodes;
	TArray<TSharedPtr<FStateTreeNodeClassData>> ObjectNodes;
	
	ClassCache->GetScripStructs(BaseScriptStruct, StructNodes);
	ClassCache->GetClasses(BaseClass, ObjectNodes);

	// Create tree of node types based on category.
	RootNode = MakeShared<FStateTreeNodeTypeItem>();
	
	for (const TSharedPtr<FStateTreeNodeClassData>& Data : StructNodes)
	{
		if (const UScriptStruct* ScriptStruct = Data->GetScriptStruct())
		{
			if (ScriptStruct == BaseScriptStruct)
			{
				continue;
			}
			if (ScriptStruct->HasMetaData(TEXT("Hidden")))
			{
				continue;				
			}
			if (Schema && !Schema->IsStructAllowed(ScriptStruct))
			{
				continue;				
			}

			AddNode(ScriptStruct);
		}
	}

	for (const TSharedPtr<FStateTreeNodeClassData>& Data : ObjectNodes)
	{
		if (Data->GetClass() != nullptr)
		{
			const UClass* Class = Data->GetClass();
			if (Class == BaseClass)
			{
				continue;
			}
			if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Hidden | CLASS_HideDropDown))
			{
				continue;				
			}
			if (Class->HasMetaData(TEXT("Hidden")))
			{
				continue;
			}
			if (Schema && !Schema->IsClassAllowed(Class))
			{
				continue;
			}

			AddNode(Class);
		}
	}

	SortNodeTypesFunctionItemsRecursive(RootNode->Children);

	// Empty node
	RootNode->Children.Insert(MakeShared<FStateTreeNodeTypeItem>(), 0);

	FilteredRootNode = RootNode;
}

TSharedRef<ITableRow> FStateTreeEditorNodeDetails::GenerateNodeTypeRow(TSharedPtr<FStateTreeNodeTypeItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	FText DisplayName = LOCTEXT("None", "None");

	if (Item->IsNode())
	{
		DisplayName = Item->Struct->GetDisplayNameText();
	}
	else if (Item->IsCategory())
	{
		DisplayName = FText::FromString(Item->GetCategoryName());
	}
	
	FText Tooltip = Item->Struct ? Item->Struct->GetMetaDataText("Tooltip") : FText::GetEmpty();
	if (Tooltip.IsEmpty())
	{
		Tooltip = DisplayName;
	}

	const FSlateBrush* Icon = nullptr;
	if (!Item->IsCategory())
	{
		if (const UClass* ItemClass = Cast<UClass>(Item->Struct))
		{
			Icon = FSlateIconFinder::FindIconBrushForClass(ItemClass);
		}
		else if (const UScriptStruct* ItemScriptStruct = Cast<UScriptStruct>(Item->Struct))
		{
			Icon = FSlateIconFinder::FindIconBrushForClass(UScriptStruct::StaticClass());
		}
		else
		{
			// None
			Icon = FSlateIconFinder::FindIconBrushForClass(nullptr);
		}
	}

	return SNew(STableRow<TSharedPtr<FStateTreeNodeTypeItem>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0, 2.0f, 4.0f, 2.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.Visibility(Icon ? EVisibility::Visible : EVisibility::Collapsed)
				.DesiredSizeOverride(FVector2D(16.0f, 16.0f))
				.Image(Icon)
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(Item->IsCategory() ? FAppStyle::Get().GetFontStyle("BoldFont") : FAppStyle::Get().GetFontStyle("NormalText"))
				.Text(DisplayName)
				.ToolTipText(Tooltip)
				.HighlightText_Lambda([this]() { return SearchBox.IsValid() ? SearchBox->GetText() : FText::GetEmpty(); })
			]
		];
}

void FStateTreeEditorNodeDetails::GetNodeTypeChildren(TSharedPtr<FStateTreeNodeTypeItem> Item, TArray<TSharedPtr<FStateTreeNodeTypeItem>>& OutItems) const
{
	if (Item.IsValid())
	{
		OutItems = Item->Children;
	}
}

void FStateTreeEditorNodeDetails::OnNodeTypeSelected(TSharedPtr<FStateTreeNodeTypeItem> SelectedItem, ESelectInfo::Type Type)
{
	// Skip selection set via code, or if Selected Item is invalid
	if (Type == ESelectInfo::Direct || !SelectedItem.IsValid())
	{
		return;
	}
	
	if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(SelectedItem->Struct))
	{
		OnStructPicked(ScriptStruct);
	}
	else if (const UClass* Class = Cast<UClass>(SelectedItem->Struct))
	{
		OnClassPicked(Class);
	}
	else if (SelectedItem->CategoryPath.IsEmpty())
	{
		// None
		OnStructPicked(nullptr);
	}
	else
	{
		// Clicked on category
	}
}

void FStateTreeEditorNodeDetails::OnNodeTypeExpansionChanged(TSharedPtr<FStateTreeNodeTypeItem> ExpandedItem, bool bInExpanded)
{
	// Do not save expansion state we're restoring expansion state, or when showing filtered results. 
	if (bIsRestoringExpansion || FilteredRootNode != RootNode)
	{
		return;
	}

	if (ExpandedItem.IsValid() && ExpandedItem->CategoryPath.Num() > 0)
	{
		FCategoryExpansionState& ExpansionState = CategoryExpansionStates.FindOrAdd(FObjectKey(BaseScriptStruct));
		const FString Path = FString::Join(ExpandedItem->CategoryPath, TEXT("|"));
		if (bInExpanded)
		{
			ExpansionState.ExpandedCategories.Add(Path);
		}
		else
		{
			ExpansionState.ExpandedCategories.Remove(Path);
		}
	}
}

void FStateTreeEditorNodeDetails::OnSearchBoxTextChanged(const FText& NewText)
{
	if (!NodeTypeTree.IsValid())
	{
		return;
	}
	
	FilteredRootNode.Reset();

	TArray<FString> FilterStrings;
	NewText.ToString().ParseIntoArrayWS(FilterStrings);
	FilterStrings.RemoveAll([](const FString& String) { return String.IsEmpty(); });
	
	if (FilterStrings.IsEmpty())
	{
		// Show all when there's no filter string.
		FilteredRootNode = RootNode;
		NodeTypeTree->SetTreeItemsSource(&FilteredRootNode->Children);
		RestoreExpansionState();
		NodeTypeTree->RequestTreeRefresh();
		return;
	}

	FilteredRootNode = MakeShared<FStateTreeNodeTypeItem>();
	FilterNodeTypesChildren(FilterStrings, /*bParentMatches*/false, RootNode->Children, FilteredRootNode->Children);

	NodeTypeTree->SetTreeItemsSource(&FilteredRootNode->Children);
	ExpandAll(FilteredRootNode->Children);
	NodeTypeTree->RequestTreeRefresh();
}


int32 FStateTreeEditorNodeDetails::FilterNodeTypesChildren(const TArray<FString>& FilterStrings, const bool bParentMatches,
															const TArray<TSharedPtr<FStateTreeNodeTypeItem>>& SourceArray,
															TArray<TSharedPtr<FStateTreeNodeTypeItem>>& OutDestArray)
{
	int32 NumFound = 0;

	auto MatchFilter = [&FilterStrings](const TSharedPtr<FStateTreeNodeTypeItem>& SourceItem)
	{
		const FString ItemName = SourceItem->Struct ? SourceItem->Struct->GetDisplayNameText().ToString() : SourceItem->GetCategoryName();
		for (const FString& Filter : FilterStrings)
		{
			if (ItemName.Contains(Filter))
			{
				return true;
			}
		}
		return false;
	};

	for (const TSharedPtr<FStateTreeNodeTypeItem>& SourceItem : SourceArray)
	{
		// Check if our name matches the filters
		// If bParentMatches is true, the search matched a parent category.
		const bool bMatchesFilters = bParentMatches || MatchFilter(SourceItem);

		int32 NumChildren = 0;
		if (bMatchesFilters)
		{
			NumChildren++;
		}

		// if we don't match, then we still want to check all our children
		TArray<TSharedPtr<FStateTreeNodeTypeItem>> FilteredChildren;
		NumChildren += FilterNodeTypesChildren(FilterStrings, bMatchesFilters, SourceItem->Children, FilteredChildren);

		// then add this item to the destination array
		if (NumChildren > 0)
		{
			TSharedPtr<FStateTreeNodeTypeItem>& NewItem = OutDestArray.Add_GetRef(MakeShared<FStateTreeNodeTypeItem>());
			NewItem->CategoryPath = SourceItem->CategoryPath;
			NewItem->Struct = SourceItem->Struct; 
			NewItem->Children = FilteredChildren;

			NumFound += NumChildren;
		}
	}

	return NumFound;
}

void FStateTreeEditorNodeDetails::SaveExpansionState()
{
	if (!NodeTypeTree.IsValid())
	{
		return;
	}
	
	TSet<TSharedPtr<FStateTreeNodeTypeItem>> ExpandedItems;
	NodeTypeTree->GetExpandedItems(ExpandedItems);

	FCategoryExpansionState& ExpansionState = CategoryExpansionStates.FindOrAdd(FObjectKey(BaseScriptStruct));
	
	ExpansionState.ExpandedCategories.Reset();
	for (const TSharedPtr<FStateTreeNodeTypeItem>& Item : ExpandedItems)
	{
		if (Item->CategoryPath.Num() > 0)
		{
			const FString Path = FString::Join(Item->CategoryPath, TEXT("|"));
			ExpansionState.ExpandedCategories.Add(Path);
		}
	}
}

void FStateTreeEditorNodeDetails::RestoreExpansionState()
{
	FCategoryExpansionState& ExpansionState = CategoryExpansionStates.FindOrAdd(FObjectKey(BaseScriptStruct));

	TSet<TSharedPtr<FStateTreeNodeTypeItem>> ExpandedNodes;
	for (const FString& Category : ExpansionState.ExpandedCategories)
	{
		TArray<FString> CategoryPath;
		Category.ParseIntoArray(CategoryPath, TEXT("|"));

		TSharedPtr<FStateTreeNodeTypeItem> CurrentParent = RootNode;

		for (const FString& SubCategory : CategoryPath)
		{
			TSharedPtr<FStateTreeNodeTypeItem>* FoundItem = 
				CurrentParent->Children.FindByPredicate([&SubCategory](const TSharedPtr<FStateTreeNodeTypeItem>& Item)
				{
					return Item->GetCategoryName() == SubCategory;
				});

			if (FoundItem != nullptr)
			{
				ExpandedNodes.Add(*FoundItem);
				CurrentParent = *FoundItem;
			}
		}
	}

	if (NodeTypeTree.IsValid())
	{
		bIsRestoringExpansion = true;
		
		NodeTypeTree->ClearExpandedItems();
		for (const TSharedPtr<FStateTreeNodeTypeItem>& Node : ExpandedNodes)
		{
			NodeTypeTree->SetItemExpansion(Node, true);
		}

		bIsRestoringExpansion = false;
	}
}

void FStateTreeEditorNodeDetails::ExpandAll(const TArray<TSharedPtr<FStateTreeNodeTypeItem>>& Items)
{
	for (const TSharedPtr<FStateTreeNodeTypeItem>& Item : Items)
	{
		NodeTypeTree->SetItemExpansion(Item, true);
		ExpandAll(Item->Children);
	}
}

TArray<TSharedPtr<FStateTreeEditorNodeDetails::FStateTreeNodeTypeItem>> FStateTreeEditorNodeDetails::GetPathToItemStruct(const UStruct* Struct) const
{
	TArray<TSharedPtr<FStateTreeNodeTypeItem>> Path;

	TSharedPtr<FStateTreeNodeTypeItem> CurrentParent = FilteredRootNode;

	FText FullCategoryName = Struct->GetMetaDataText("Category");
	if (!FullCategoryName.IsEmpty())
	{
		TArray<FString> CategoryPath;
		FullCategoryName.ToString().ParseIntoArray(CategoryPath, TEXT("|"));

		for (const FString& SubCategory : CategoryPath)
		{
			const FString Trimmed = SubCategory.TrimStartAndEnd();

			TSharedPtr<FStateTreeNodeTypeItem>* FoundItem = 
				CurrentParent->Children.FindByPredicate([&Trimmed](const TSharedPtr<FStateTreeNodeTypeItem>& Item)
				{
					return Item->GetCategoryName() == Trimmed;
				});

			if (FoundItem != nullptr)
			{
				Path.Add(*FoundItem);
				CurrentParent = *FoundItem;
			}
		}
	}

	const TSharedPtr<FStateTreeNodeTypeItem>* FoundItem = 
		CurrentParent->Children.FindByPredicate([Struct](const TSharedPtr<FStateTreeNodeTypeItem>& Item)
		{
			return Item->Struct == Struct;
		});

	if (FoundItem != nullptr)
	{
		Path.Add(*FoundItem);
	}

	return Path;
}

TSharedRef<SWidget> FStateTreeEditorNodeDetails::GeneratePicker()
{
	CacheNodeTypes();

	TSharedRef<SWidget> MenuWidget = 
		SNew(SBox)
		.MinDesiredWidth(400)
		.MinDesiredHeight(300)
		.MaxDesiredHeight(300)
		.Padding(2)	
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.Padding(4, 2, 4, 2)
			.AutoHeight()
			[
				SAssignNew(SearchBox, SSearchBox)
				.OnTextChanged(this, &FStateTreeEditorNodeDetails::OnSearchBoxTextChanged)
			]
			+ SVerticalBox::Slot()
			[
				SAssignNew(NodeTypeTree, STreeView<TSharedPtr<FStateTreeNodeTypeItem>>)
				.SelectionMode(ESelectionMode::Single)
				.ItemHeight(20.0f)
				.TreeItemsSource(&FilteredRootNode->Children)
				.OnGenerateRow(this, &FStateTreeEditorNodeDetails::GenerateNodeTypeRow)
				.OnGetChildren(this, &FStateTreeEditorNodeDetails::GetNodeTypeChildren)
				.OnSelectionChanged(this, &FStateTreeEditorNodeDetails::OnNodeTypeSelected)
				.OnExpansionChanged(this, &FStateTreeEditorNodeDetails::OnNodeTypeExpansionChanged)
			]
		];

	// Restore category expansion state from previous use.
	RestoreExpansionState();
	
	// Expand and select currently selected item.
	const UStruct* CommonStruct  = nullptr;
	if (const FStateTreeEditorNode* Node = UE::StateTreeEditor::EditorNodeUtils::GetCommonNode(StructProperty))
	{
		if (const UScriptStruct* ScriptStruct = Node->Node.GetScriptStruct())
		{
			if (ScriptStruct->IsChildOf(FStateTreeBlueprintEvaluatorWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FStateTreeBlueprintTaskWrapper::StaticStruct())
				|| ScriptStruct->IsChildOf(FStateTreeBlueprintConditionWrapper::StaticStruct()))
			{
				if (Node->InstanceObject != nullptr)
				{
					CommonStruct = Node->InstanceObject->GetClass();
				}
			}
			else
			{
				CommonStruct = ScriptStruct;
			}
		}
	}
	if (CommonStruct)
	{
		const TArray<TSharedPtr<FStateTreeNodeTypeItem>> Path = GetPathToItemStruct(CommonStruct);
		if (Path.Num() > 0)
		{
			// Expand all categories up to the selected item.
			bIsRestoringExpansion = true;
			for (const TSharedPtr<FStateTreeNodeTypeItem>& Item : Path)
			{
				NodeTypeTree->SetItemExpansion(Item, true);
			}
			bIsRestoringExpansion = false;
			
			NodeTypeTree->SetItemSelection(Path.Last(), true);
			NodeTypeTree->RequestScrollIntoView(Path.Last());
		}
	}
	
	ComboButton->SetMenuContentWidgetToFocus(SearchBox);

	return MenuWidget;
}

void FStateTreeEditorNodeDetails::OnStructPicked(const UScriptStruct* InStruct) const
{
	check(StructProperty);
	check(StateTree);

	TArray<UObject*> OuterObjects;
	TArray<void*> RawNodeData;
	StructProperty->GetOuterObjects(OuterObjects);
	StructProperty->AccessRawData(RawNodeData);

	GEditor->BeginTransaction(LOCTEXT("SelectNode", "Select Node"));

	StructProperty->NotifyPreChange();

	if (OuterObjects.Num() == RawNodeData.Num())
	{
		for (int32 Index = 0; Index < RawNodeData.Num(); Index++)
		{
			if (UObject* Outer = OuterObjects[Index])
			{
				if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(RawNodeData[Index]))
				{
					const bool bRetainProperties = InStruct && UStateTreeEditorSettings::Get().bRetainNodePropertyValues;
					FStateTreeEditorNode OldNode = bRetainProperties ? *Node : FStateTreeEditorNode();

					Node->Reset();
					
					if (InStruct)
					{
						// Generate new ID.
						Node->ID = FGuid::NewGuid();

						// Initialize node
						Node->Node.InitializeAs(InStruct);
						
						// Generate new name and instantiate instance data.
						if (InStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()))
						{
							FStateTreeTaskBase& Task = Node->Node.GetMutable<FStateTreeTaskBase>();
							Task.Name = FName(InStruct->GetDisplayNameText().ToString());

							if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Task.GetInstanceDataType()))
							{
								Node->Instance.InitializeAs(InstanceType);
							}
							else if (const UClass* InstanceClass = Cast<const UClass>(Task.GetInstanceDataType()))
							{
								Node->InstanceObject = NewObject<UObject>(Outer, InstanceClass);
							}
						}
						else if (InStruct->IsChildOf(FStateTreeEvaluatorBase::StaticStruct()))
						{
							FStateTreeEvaluatorBase& Eval = Node->Node.GetMutable<FStateTreeEvaluatorBase>();
							Eval.Name = FName(InStruct->GetDisplayNameText().ToString());

							if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Eval.GetInstanceDataType()))
							{
								Node->Instance.InitializeAs(InstanceType);
							}
							else if (const UClass* InstanceClass = Cast<const UClass>(Eval.GetInstanceDataType()))
							{
								Node->InstanceObject = NewObject<UObject>(Outer, InstanceClass);
							}
						}
						else if (InStruct->IsChildOf(FStateTreeConditionBase::StaticStruct()))
						{
							FStateTreeConditionBase& Cond = Node->Node.GetMutable<FStateTreeConditionBase>();
							Cond.Name = FName(InStruct->GetDisplayNameText().ToString());

							if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Cond.GetInstanceDataType()))
							{
								Node->Instance.InitializeAs(InstanceType);
							}
							else if (const UClass* InstanceClass = Cast<const UClass>(Cond.GetInstanceDataType()))
							{
								Node->InstanceObject = NewObject<UObject>(Outer, InstanceClass);
							}
						}

						if (bRetainProperties)
						{
							UE::StateTreeEditor::Internal::RetainProperties(OldNode, *Node);
						}
					}
				}
			}
		}
	}

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();

	GEditor->EndTransaction();

	ComboButton->SetIsOpen(false);

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

void FStateTreeEditorNodeDetails::OnClassPicked(const UClass* InClass) const
{
	check(StructProperty);
	check(StateTree);

	TArray<UObject*> OuterObjects;
	TArray<void*> RawNodeData;
	StructProperty->GetOuterObjects(OuterObjects);
	StructProperty->AccessRawData(RawNodeData);

	GEditor->BeginTransaction(LOCTEXT("SelectBlueprintNode", "Select Blueprint Node"));

	StructProperty->NotifyPreChange();

	if (OuterObjects.Num() == RawNodeData.Num())
	{
		for (int32 Index = 0; Index < RawNodeData.Num(); Index++)
		{
			if (UObject* Outer = OuterObjects[Index])
			{
				if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(RawNodeData[Index]))
				{
					bool bRetainProperties = InClass && UStateTreeEditorSettings::Get().bRetainNodePropertyValues;
					FStateTreeEditorNode OldNode = bRetainProperties ? *Node : FStateTreeEditorNode();

					Node->Reset();

					if (InClass && InClass->IsChildOf(UStateTreeTaskBlueprintBase::StaticClass()))
					{
						Node->Node.InitializeAs(FStateTreeBlueprintTaskWrapper::StaticStruct());
						FStateTreeBlueprintTaskWrapper& Task = Node->Node.GetMutable<FStateTreeBlueprintTaskWrapper>();
						Task.TaskClass = const_cast<UClass*>(InClass);
						Task.Name = FName(InClass->GetDisplayNameText().ToString());
						
						Node->InstanceObject = NewObject<UObject>(Outer, InClass);

						Node->ID = FGuid::NewGuid();
					}
					else if (InClass && InClass->IsChildOf(UStateTreeEvaluatorBlueprintBase::StaticClass()))
					{
						Node->Node.InitializeAs(FStateTreeBlueprintEvaluatorWrapper::StaticStruct());
						FStateTreeBlueprintEvaluatorWrapper& Eval = Node->Node.GetMutable<FStateTreeBlueprintEvaluatorWrapper>();
						Eval.EvaluatorClass = const_cast<UClass*>(InClass);
						Eval.Name = FName(InClass->GetDisplayNameText().ToString());
						
						Node->InstanceObject = NewObject<UObject>(Outer, InClass);

						Node->ID = FGuid::NewGuid();
					}
					else if (InClass && InClass->IsChildOf(UStateTreeConditionBlueprintBase::StaticClass()))
					{
						Node->Node.InitializeAs(FStateTreeBlueprintConditionWrapper::StaticStruct());
						FStateTreeBlueprintConditionWrapper& Cond = Node->Node.GetMutable<FStateTreeBlueprintConditionWrapper>();
						Cond.ConditionClass = const_cast<UClass*>(InClass);
						Cond.Name = FName(InClass->GetDisplayNameText().ToString());

						Node->InstanceObject = NewObject<UObject>(Outer, InClass);

						Node->ID = FGuid::NewGuid();
					}
					else
					{
						// Not retaining properties if we haven't initialized a new node
						bRetainProperties = false;
					}

					if (bRetainProperties)
					{
						UE::StateTreeEditor::Internal::RetainProperties(OldNode, *Node);
					}
				}
			}
		}
	}

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();

	GEditor->EndTransaction();

	ComboButton->SetIsOpen(false);

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
