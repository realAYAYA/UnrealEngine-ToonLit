// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorNodeDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "IDetailPropertyRow.h"
#include "IDetailChildrenBuilder.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "InstancedStruct.h"
#include "Styling/SlateIconFinder.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "InstancedStructDetails.h"
#include "Engine/UserDefinedStruct.h"
#include "StateTreeBindingExtension.h"
#include "StateTreeDelegates.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"
#include "StateTreePropertyHelpers.h"
#include "StateTreeEditorStyle.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "StateTreeEditorModule.h"
#include "StateTreeNodeClassCache.h"
#include "Styling/StyleColors.h"
#include "ObjectEditorUtils.h"
#include "StateTreeCompiler.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"

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
		
		const EStateTreePropertyUsage Usage = UE::StateTree::Compiler::GetUsageFromMetaData(ChildPropHandle->GetProperty());
		const FProperty* Property = ChildPropHandle->GetProperty();
		
		// Conditionally control visibility of the value field of bound properties.
		if (Usage != EStateTreePropertyUsage::Invalid && ID.IsValid())
		{
			// Pass the node ID to binding extension. Since the properties are added using AddChildStructure(), we break the hierarchy and cannot access parent.
			ChildPropHandle->SetInstanceMetaData(UE::StateTree::PropertyBinding::StateTreeNodeIDName, LexToString(ID));

			FStateTreeEditorPropertyPath Path(ID, *Property->GetFName().ToString());
			TSharedPtr<SWidget> NameWidget;
			TSharedPtr<SWidget> ValueWidget;
			FDetailWidgetRow Row;
			ChildRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

			auto IsValueVisible = TAttribute<EVisibility>::Create([Path, EditorPropBindings]() -> EVisibility
				{
					return EditorPropBindings->HasPropertyBinding(Path) ? EVisibility::Collapsed : EVisibility::Visible;
				});

			if (Usage == EStateTreePropertyUsage::Input || Usage == EStateTreePropertyUsage::Output || Usage == EStateTreePropertyUsage::Context)
			{
				FEdGraphPinType PinType;
				const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
				Schema->ConvertPropertyToPinType(Property, PinType);
				
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
					.CustomWidget(/*bShowChildren*/false)
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
							.BorderImage(new FSlateRoundedBoxBrush(FStyleColors::Hover, 6))
							.Visibility(Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
							[
								SNew(STextBlock)
								.TextStyle(FStateTreeEditorStyle::Get(), "StateTree.Node.Operand")
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
			else
			{
				ChildRow
					.CustomWidget(/*bShowChildren*/true)
					.NameContent()
					[
						NameWidget.ToSharedRef()
					]
					.ValueContent()
					[
						SNew(SBox)
						.Visibility(IsValueVisible)
						[
							ValueWidget.ToSharedRef()
						]
					];
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

TSharedRef<IPropertyTypeCustomization> FStateTreeEditorNodeDetails::MakeInstance()
{
	return MakeShareable(new FStateTreeEditorNodeDetails);
}

FStateTreeEditorNodeDetails::~FStateTreeEditorNodeDetails()
{
	UE::StateTree::PropertyBinding::OnStateTreeBindingChanged.Remove(OnBindingChangedHandle);
}

void FStateTreeEditorNodeDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

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
	OnBindingChangedHandle = UE::StateTree::PropertyBinding::OnStateTreeBindingChanged.AddRaw(this, &FStateTreeEditorNodeDetails::OnBindingChanged);

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
				// Name (Eval/Task)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SEditableTextBox)
					.IsEnabled(TAttribute<bool>(this, &FStateTreeEditorNodeDetails::IsNameEnabled))
					.Text(this, &FStateTreeEditorNodeDetails::GetName)
					.OnTextCommitted(this, &FStateTreeEditorNodeDetails::OnNameCommitted)
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.Style(FStateTreeEditorStyle::Get(), "StateTree.Node.Name")
					.Visibility(this, &FStateTreeEditorNodeDetails::IsNameVisible)
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
			]
			// Class picker
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(FMargin(FMargin(4.0f, 0.0f, 0.0f, 0.0f)))
			.VAlign(VAlign_Center)
			[
				SAssignNew(ComboButton, SComboButton)
				.OnGetMenuContent(this, &FStateTreeEditorNodeDetails::GeneratePicker)
				.ContentPadding(0.f)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SImage)
						.Image(this, &FStateTreeEditorNodeDetails::GetDisplayValueIcon)
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &FStateTreeEditorNodeDetails::GetDisplayValueString)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				StructPropertyHandle->CreateDefaultPropertyButtonWidgets()
			]
		]
		.OverrideResetToDefault(ResetOverride)
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::OnCopyNode)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::OnPasteNode)));

}

void FStateTreeEditorNodeDetails::OnCopyNode()
{
	FString Value;
	if (StructProperty->GetValueAsFormattedString(Value) == FPropertyAccess::Success)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	}
}

void FStateTreeEditorNodeDetails::OnPasteNode()
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	if (!PastedText.IsEmpty())
	{
		// create node from the 
		FStateTreeEditorNode TempNode;
		UScriptStruct* NodeScriptStruct = TBaseStructure<FStateTreeEditorNode>::Get();

		FStateTreeDefaultValueImportErrorContext ErrorPipe;
		NodeScriptStruct->ImportText(*PastedText, &TempNode, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, NodeScriptStruct->GetName());

		if (ErrorPipe.NumErrors == 0)
		{
			// Do not allow to mix and match types.
			// @todo: Check Schema too and warn user about mismatching schema.
			if (TempNode.Node.GetScriptStruct() && TempNode.Node.GetScriptStruct()->IsChildOf(BaseScriptStruct))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteNode", "Paste Node"));

				StructProperty->NotifyPreChange();

				StructProperty->SetValueFromFormattedString(PastedText);

				// Reset GUIDs on paste
				TArray<void*> RawNodeData;
				StructProperty->AccessRawData(RawNodeData);
				for (void* Data : RawNodeData)
				{
					if (FStateTreeEditorNode* EditorNode = static_cast<FStateTreeEditorNode*>(Data))
					{
						if (FStateTreeNodeBase* Node = EditorNode->Node.GetMutablePtr<FStateTreeNodeBase>())
						{
							Node->Name = FName(Node->Name.ToString() + TEXT(" Copy"));
						}
						EditorNode->ID = FGuid::NewGuid();
					}
				}

				StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
				StructProperty->NotifyFinishedChangingProperties();

				if (PropUtils)
				{
					PropUtils->ForceRefresh();
				}
			}
		}
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
	check(StructProperty);
	
	GEditor->BeginTransaction(LOCTEXT("OnResetToDefault", "Reset to default"));

	StructProperty->NotifyPreChange();
	
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	for (void* Data : RawNodeData)
	{
		if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(Data))
		{
			Node->Reset();
		}
	}

	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();

	GEditor->EndTransaction();

	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

void FStateTreeEditorNodeDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FGuid ID;
	UE::StateTree::PropertyHelpers::GetStructValue<FGuid>(IDProperty, ID);

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
				Child.Usage = UE::StateTree::Compiler::GetUsageFromMetaData(Child.PropertyHandle->GetProperty());

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

		SortedChildren.Sort([](const FSortedChild& LHS, const FSortedChild& RHS) { return LHS.Usage < RHS.Usage; });

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

void FStateTreeEditorNodeDetails::OnBindingChanged(const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath)
{
	check(StructProperty);

	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);

	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	if (OuterObjects.Num() != RawNodeData.Num())
	{
		return;
	}

	for (int32 i = 0; i < OuterObjects.Num(); i++)
	{
		const FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(RawNodeData[i]);
		UObject* OuterObject = OuterObjects[i]; // Immediate outer, i.e StateTreeState
		if (Node != nullptr && EditorData != nullptr && Node->Node.IsValid() && Node->Instance.IsValid())
		{
			if (Node->ID == TargetPath.StructID)
			{
				if (FStateTreeConditionBase* Condition = Node->Node.GetMutablePtr<FStateTreeConditionBase>())
				{
					const FStateTreeBindingLookup BindingLookup(EditorData);

					OuterObject->Modify();
					Condition->OnBindingChanged(Node->ID, FStateTreeDataView(Node->Instance), SourcePath, TargetPath, BindingLookup);
				}
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
	const UScriptStruct* ScriptStruct = nullptr;
	if (const FStateTreeEditorNode* Node = GetCommonNode())
	{
		ScriptStruct = Node->Node.GetScriptStruct();
	}

	return ScriptStruct != nullptr && ScriptStruct->IsChildOf(FStateTreeConditionBase::StaticStruct()) ? EVisibility::Visible : EVisibility::Collapsed;
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
	if (const FStateTreeEditorNode* Node = GetCommonNode())
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
	const FStateTreeEditorNode* Node = GetCommonNode();
	return Node && Node->Node.IsValid();
}

const FStateTreeEditorNode* FStateTreeEditorNodeDetails::GetCommonNode() const
{
	check(StructProperty);

	const UScriptStruct* CommonScriptStruct = nullptr;
	bool bMultipleValues = false;
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	const FStateTreeEditorNode* CommonNode = nullptr;
	
	for (void* Data : RawNodeData)
	{
		if (const FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(Data))
		{
			if (!bMultipleValues && !CommonNode)
			{
				CommonNode = Node;
			}
			else if (CommonNode != Node)
			{
				CommonNode = nullptr;
				bMultipleValues = true;
			}
		}
	}

	return CommonNode;
}

FText FStateTreeEditorNodeDetails::GetDisplayValueString() const
{
	if (const FStateTreeEditorNode* Node = GetCommonNode())
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
	if (const FStateTreeEditorNode* Node = GetCommonNode())
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

TSharedRef<SWidget> FStateTreeEditorNodeDetails::GeneratePicker()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	FStateTreeEditorModule& EditorModule = FModuleManager::GetModuleChecked<FStateTreeEditorModule>(TEXT("StateTreeEditorModule"));
	FStateTreeNodeClassCache* ClassCache = EditorModule.GetNodeClassCache().Get();
	check(ClassCache);

	FUIAction ClearAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::OnStructPicked, (const UScriptStruct*)nullptr));
	MenuBuilder.AddMenuEntry(LOCTEXT("ClearNode", "Clear"), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "Cross"), ClearAction);

	MenuBuilder.AddMenuSeparator();

	TArray<TSharedPtr<FStateTreeNodeClassData>> StructNodes;
	TArray<TSharedPtr<FStateTreeNodeClassData>> ObjectNodes;
	
	ClassCache->GetScripStructs(BaseScriptStruct, StructNodes);
	ClassCache->GetClasses(BaseClass, ObjectNodes);

	const FSlateIcon ScripStructIcon = FSlateIconFinder::FindIconForClass(UScriptStruct::StaticClass());
	const UStateTreeSchema* Schema = EditorData ? EditorData->Schema : nullptr;
	
	for (const TSharedPtr<FStateTreeNodeClassData>& Data : StructNodes)
	{
		if (Data->GetScriptStruct() != nullptr)
		{
			const UScriptStruct* ScriptStruct = Data->GetScriptStruct();
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
			
			FUIAction ItemAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::OnStructPicked, ScriptStruct));
			MenuBuilder.AddMenuEntry(ScriptStruct->GetDisplayNameText(), TAttribute<FText>(), ScripStructIcon, ItemAction);
		}
	}

	if (StructNodes.Num() > 0 && ObjectNodes.Num() > 0)
	{
		MenuBuilder.AddMenuSeparator();
	}
	
	for (const TSharedPtr<FStateTreeNodeClassData>& Data : ObjectNodes)
	{
		if (Data->GetClass() != nullptr)
		{
			UClass* Class = Data->GetClass();
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

			FUIAction ItemAction(FExecuteAction::CreateSP(this, &FStateTreeEditorNodeDetails::OnClassPicked, Class));
			MenuBuilder.AddMenuEntry(Class->GetDisplayNameText(), TAttribute<FText>(), FSlateIconFinder::FindIconForClass(Data->GetClass()), ItemAction);
		}
	}

	return MenuBuilder.MakeWidget();
}

void FStateTreeEditorNodeDetails::OnStructPicked(const UScriptStruct* InStruct) const
{
	check(StructProperty);
	check(StateTree);

	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	GEditor->BeginTransaction(LOCTEXT("SelectNode", "Select Node"));

	StructProperty->NotifyPreChange();

	for (void* Data : RawNodeData)
	{
		if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(Data))
		{
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
						Node->InstanceObject = NewObject<UObject>(EditorData, InstanceClass);
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
						Node->InstanceObject = NewObject<UObject>(EditorData, InstanceClass);
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
						Node->InstanceObject = NewObject<UObject>(EditorData, InstanceClass);
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

void FStateTreeEditorNodeDetails::OnClassPicked(UClass* InClass) const
{
	check(StructProperty);
	check(StateTree);

	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);

	GEditor->BeginTransaction(LOCTEXT("SelectBlueprintNode", "Select Blueprint Node"));

	StructProperty->NotifyPreChange();

	for (void* Data : RawNodeData)
	{
		if (FStateTreeEditorNode* Node = static_cast<FStateTreeEditorNode*>(Data))
		{
			Node->Reset();

			if (InClass && InClass->IsChildOf(UStateTreeTaskBlueprintBase::StaticClass()))
			{
				Node->Node.InitializeAs(FStateTreeBlueprintTaskWrapper::StaticStruct());
				FStateTreeBlueprintTaskWrapper& Task = Node->Node.GetMutable<FStateTreeBlueprintTaskWrapper>();
				Task.TaskClass = InClass;
				Task.Name = FName(InClass->GetDisplayNameText().ToString());
				
				Node->InstanceObject = NewObject<UObject>(EditorData, InClass);

				Node->ID = FGuid::NewGuid();
			}
			else if (InClass && InClass->IsChildOf(UStateTreeEvaluatorBlueprintBase::StaticClass()))
			{
				Node->Node.InitializeAs(FStateTreeBlueprintEvaluatorWrapper::StaticStruct());
				FStateTreeBlueprintEvaluatorWrapper& Eval = Node->Node.GetMutable<FStateTreeBlueprintEvaluatorWrapper>();
				Eval.EvaluatorClass = InClass;
				Eval.Name = FName(InClass->GetDisplayNameText().ToString());
				
				Node->InstanceObject = NewObject<UObject>(EditorData, InClass);

				Node->ID = FGuid::NewGuid();
			}
			else if (InClass && InClass->IsChildOf(UStateTreeConditionBlueprintBase::StaticClass()))
			{
				Node->Node.InitializeAs(FStateTreeBlueprintConditionWrapper::StaticStruct());
				FStateTreeBlueprintConditionWrapper& Cond = Node->Node.GetMutable<FStateTreeBlueprintConditionWrapper>();
				Cond.ConditionClass = InClass;
				Cond.Name = FName(InClass->GetDisplayNameText().ToString());

				Node->InstanceObject = NewObject<UObject>(EditorData, InClass);

				Node->ID = FGuid::NewGuid();
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
