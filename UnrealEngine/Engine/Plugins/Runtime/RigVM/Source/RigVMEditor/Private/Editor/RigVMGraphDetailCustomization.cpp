// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMGraphDetailCustomization.h"
#include "Widgets/SWidget.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Styling/AppStyle.h"
#include "SPinTypeSelector.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "PropertyCustomizationHelpers.h"
#include "NodeFactory.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "RigVMHost.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "EditorCategoryUtils.h"
#include "IPropertyUtilities.h"
#include "RigVMModel/Nodes/RigVMAggregateNode.h"
#include "Widgets/SRigVMGraphPinVariableBinding.h"
#include "InstancedPropertyBagStructureDataProvider.h"
#include "Widgets/SRigVMGraphPinEnumPicker.h"

#define LOCTEXT_NAMESPACE "RigVMGraphDetailCustomization"

static const FText RigVMGraphDetailCustomizationMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

FRigVMFunctionArgumentGroupLayout::FRigVMFunctionArgumentGroupLayout(
	URigVMGraph* InGraph, 
	URigVMBlueprint* InBlueprint, 
	TWeakPtr<FRigVMEditor> InEditor,
	bool bInputs)
	: GraphPtr(InGraph)
	, RigVMBlueprintPtr(InBlueprint)
	, RigVMEditorPtr(InEditor)
	, bIsInputGroup(bInputs)
{
	if (RigVMBlueprintPtr.IsValid())
	{
		RigVMBlueprintPtr.Get()->OnModified().AddRaw(this, &FRigVMFunctionArgumentGroupLayout::HandleModifiedEvent);
	}
}

FRigVMFunctionArgumentGroupLayout::~FRigVMFunctionArgumentGroupLayout()
{
	if (RigVMBlueprintPtr.IsValid())
	{
		RigVMBlueprintPtr.Get()->OnModified().RemoveAll(this);
	}
}

void FRigVMFunctionArgumentGroupLayout::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	bool WasContentAdded = false;
	if (GraphPtr.IsValid())
	{
		URigVMGraph* Graph = GraphPtr.Get();
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter()))
		{
			for (URigVMPin* Pin : LibraryNode->GetPins())
			{
				if ((bIsInputGroup && (Pin->GetDirection() == ERigVMPinDirection::Input || Pin->GetDirection() == ERigVMPinDirection::IO)) ||
					(!bIsInputGroup && (Pin->GetDirection() == ERigVMPinDirection::Output || Pin->GetDirection() == ERigVMPinDirection::IO)))
				{
					TSharedRef<class FRigVMFunctionArgumentLayout> ArgumentLayout = MakeShareable(new FRigVMFunctionArgumentLayout(
						Pin,
						Graph,
						RigVMBlueprintPtr.Get(),
						RigVMEditorPtr
					));
					ChildrenBuilder.AddCustomBuilder(ArgumentLayout);
					WasContentAdded = true;
				}
			}
		}
	}
	if (!WasContentAdded)
	{
		// Add a text widget to let the user know to hit the + icon to add parameters.
		ChildrenBuilder.AddCustomRow(FText::GetEmpty()).WholeRowContent()
		.MaxDesiredWidth(980.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoArgumentsAddedForRigVMHost", "Please press the + icon above to add parameters"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}
}

void FRigVMFunctionArgumentGroupLayout::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	if (!GraphPtr.IsValid())
	{
		return;
	}
	
	URigVMGraph* Graph = GraphPtr.Get();
	URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	if (LibraryNode == nullptr)
	{
		return;
	}

	switch (InNotifType)
	{
		case ERigVMGraphNotifType::PinAdded:
		case ERigVMGraphNotifType::PinRenamed:
		case ERigVMGraphNotifType::PinRemoved:
		case ERigVMGraphNotifType::PinIndexChanged:
		case ERigVMGraphNotifType::PinTypeChanged:
		{
			URigVMPin* Pin = CastChecked<URigVMPin>(InSubject);
			if (Pin->GetNode() == LibraryNode ||
				(Pin->GetNode()->IsA<URigVMFunctionEntryNode>() && Pin->GetNode()->GetOuter() == Graph) ||
				(Pin->GetNode()->IsA<URigVMFunctionReturnNode>() && Pin->GetNode()->GetOuter() == Graph))
			{
				OnRebuildChildren.ExecuteIfBound();
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

class FRigVMFunctionArgumentPinTypeSelectorFilter : public IPinTypeSelectorFilter
{
public:
	FRigVMFunctionArgumentPinTypeSelectorFilter(TWeakPtr<FRigVMEditor> InRigVMEditor, TWeakObjectPtr<URigVMGraph> InGraph)
		: RigVMEditorPtr(InRigVMEditor), GraphPtr(InGraph)
	{
	}
	
	virtual bool ShouldShowPinTypeTreeItem(FPinTypeTreeItem InItem) const override
	{
		if (!InItem.IsValid())
		{
			return false;
		}

		// Only allow an execute context pin if the graph doesnt have one already
		FString CPPType;
		UObject* CPPTypeObject = nullptr;
		RigVMTypeUtils::CPPTypeFromPinType(InItem.Get()->GetPinType(false), CPPType, &CPPTypeObject);
		if (UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject))
		{
			if (ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
			{
				if (GraphPtr.IsValid())
				{
					if (URigVMFunctionEntryNode* EntryNode = GraphPtr.Get()->GetEntryNode())
					{
						for (URigVMPin* Pin : EntryNode->GetPins())
						{
							if (Pin->IsExecuteContext())
							{
								return false;
							}
						}
					}
				}
			}
		}
		
		if (RigVMEditorPtr.IsValid())
		{
			TArray<TSharedPtr<IPinTypeSelectorFilter>> Filters;
			RigVMEditorPtr.Pin()->GetPinTypeSelectorFilters(Filters);
			for(const TSharedPtr<IPinTypeSelectorFilter>& Filter : Filters)
			{
				if(!Filter->ShouldShowPinTypeTreeItem(InItem))
				{
					return false;
				}
			}
			return true;
		}

		return false;
	}

private:

	TWeakPtr<FRigVMEditor> RigVMEditorPtr;
	
	TWeakObjectPtr<URigVMGraph> GraphPtr;
};

void FRigVMFunctionArgumentLayout::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	const UEdGraphSchema* Schema = GetDefault<URigVMEdGraphSchema>();

	ETypeTreeFilter TypeTreeFilter = ETypeTreeFilter::None;
	TypeTreeFilter |= ETypeTreeFilter::AllowExec;

	TArray<TSharedPtr<IPinTypeSelectorFilter>> CustomPinTypeFilters;
	if (RigVMEditorPtr.IsValid())
	{
		CustomPinTypeFilters.Add(MakeShared<FRigVMFunctionArgumentPinTypeSelectorFilter>(RigVMEditorPtr, GraphPtr));
	}
	
	NodeRow
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ArgumentNameWidget, SEditableTextBox)
				.Text(this, &FRigVMFunctionArgumentLayout::OnGetArgNameText)
				.OnTextCommitted(this, &FRigVMFunctionArgumentLayout::OnArgNameTextCommitted)
				.ToolTipText(this, &FRigVMFunctionArgumentLayout::OnGetArgToolTipText)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.IsEnabled(!ShouldPinBeReadOnly())
				.OnVerifyTextChanged_Lambda([&](const FText& InNewText, FText& OutErrorMessage) -> bool
				{
					if (InNewText.IsEmpty())
					{
						OutErrorMessage = LOCTEXT("ArgumentNameEmpty", "Cannot have an argument with an emtpy string name.");
						return false;
					}
					else if (InNewText.ToString().Len() >= NAME_SIZE)
					{
						OutErrorMessage = LOCTEXT("ArgumentNameTooLong", "Name of argument is too long.");
						return false;
					}

					EValidatorResult Result = NameValidator.IsValid(InNewText.ToString(), false);
					OutErrorMessage = INameValidatorInterface::GetErrorText(InNewText.ToString(), Result);	

					return Result == EValidatorResult::Ok || Result == EValidatorResult::ExistingName;
				})
			]
		]
		.ValueContent()
		.MaxDesiredWidth(980.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SPinTypeSelector, FGetPinTypeTree::CreateUObject(GetDefault<UEdGraphSchema_K2>(), &UEdGraphSchema_K2::GetVariableTypeTree))
				.TargetPinType(this, &FRigVMFunctionArgumentLayout::OnGetPinInfo)
				.OnPinTypePreChanged(this, &FRigVMFunctionArgumentLayout::OnPrePinInfoChange)
				.OnPinTypeChanged(this, &FRigVMFunctionArgumentLayout::PinInfoChanged)
				.Schema(Schema)
				.TypeTreeFilter(TypeTreeFilter)
				.bAllowArrays(!ShouldPinBeReadOnly())
				.IsEnabled(!ShouldPinBeReadOnly(true))
				.CustomFilters(CustomPinTypeFilters)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
				.ContentPadding(0)
				.IsEnabled_Raw(this, &FRigVMFunctionArgumentLayout::CanArgumentBeMoved, true)
				.OnClicked(this, &FRigVMFunctionArgumentLayout::OnArgMoveUp)
				.ToolTipText(LOCTEXT("FunctionArgDetailsArgMoveUpTooltip", "Move this parameter up in the list."))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.ChevronUp"))
				.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
				.ContentPadding(0)
				.IsEnabled_Raw(this, &FRigVMFunctionArgumentLayout::CanArgumentBeMoved, false)
				.OnClicked(this, &FRigVMFunctionArgumentLayout::OnArgMoveDown)
				.ToolTipText(LOCTEXT("FunctionArgDetailsArgMoveDownTooltip", "Move this parameter down in the list."))
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.ChevronDown"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(10, 0, 0, 0)
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FRigVMFunctionArgumentLayout::OnRemoveClicked), LOCTEXT("FunctionArgDetailsClearTooltip", "Remove this parameter."), !IsPinEditingReadOnly())
			]
		];
}

void FRigVMFunctionArgumentLayout::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	// we don't show defaults here - we rely on a SRigVMGraphNode widget in the top of the details
}

void FRigVMFunctionArgumentLayout::OnRemoveClicked()
{
	if (PinPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMPin* Pin = PinPtr.Get();
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Pin->GetNode()))
		{
			if (URigVMController* Controller = Blueprint->GetController(LibraryNode->GetContainedGraph()))
			{
				Controller->RemoveExposedPin(Pin->GetFName(), true, true);
			}
		}
	}
}

FReply FRigVMFunctionArgumentLayout::OnArgMoveUp()
{
	if (PinPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMPin* Pin = PinPtr.Get();
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Pin->GetNode()))
		{
			if (URigVMController* Controller = Blueprint->GetController(LibraryNode->GetContainedGraph()))
			{
				bool bIsInput = Pin->GetDirection() == ERigVMPinDirection::Input || Pin->GetDirection() == ERigVMPinDirection::IO;
				
				int32 NewPinIndex = Pin->GetPinIndex() - 1;
				while (NewPinIndex != INDEX_NONE)
				{
					URigVMPin* OtherPin = LibraryNode->GetPins()[NewPinIndex];
					if (bIsInput)
					{
						if (OtherPin->GetDirection() == ERigVMPinDirection::Input || OtherPin->GetDirection() == ERigVMPinDirection::IO)
						{
							break;
						}
					}	
					else
					{
						if (OtherPin->GetDirection() == ERigVMPinDirection::Output || OtherPin->GetDirection() == ERigVMPinDirection::IO)
						{
							break;
						}
					}
					--NewPinIndex;
				}
				if (NewPinIndex != INDEX_NONE)
				{
					Controller->SetExposedPinIndex(Pin->GetFName(), NewPinIndex, true, true);
				}
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

FReply FRigVMFunctionArgumentLayout::OnArgMoveDown()
{
	if (PinPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMPin* Pin = PinPtr.Get();
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Pin->GetNode()))
		{
			if (URigVMController* Controller = Blueprint->GetController(LibraryNode->GetContainedGraph()))
			{
				bool bIsInput = Pin->GetDirection() == ERigVMPinDirection::Input || Pin->GetDirection() == ERigVMPinDirection::IO;
				
				int32 NewPinIndex = Pin->GetPinIndex() + 1;
				while (NewPinIndex < LibraryNode->GetPins().Num())
				{
					URigVMPin* OtherPin = LibraryNode->GetPins()[NewPinIndex];
					if (bIsInput)
					{
						if (OtherPin->GetDirection() == ERigVMPinDirection::Input || OtherPin->GetDirection() == ERigVMPinDirection::IO)
						{
							break;
						}
					}	
					else
					{
						if (OtherPin->GetDirection() == ERigVMPinDirection::Output || OtherPin->GetDirection() == ERigVMPinDirection::IO)
						{
							break;
						}
					}
					++NewPinIndex;
				}
				if (NewPinIndex < LibraryNode->GetPins().Num())
				{
					Controller->SetExposedPinIndex(Pin->GetFName(), NewPinIndex, true, true);
				}
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

bool FRigVMFunctionArgumentLayout::ShouldPinBeReadOnly(bool bIsEditingPinType/* = false*/) const
{
	return IsPinEditingReadOnly(bIsEditingPinType);
}

bool FRigVMFunctionArgumentLayout::IsPinEditingReadOnly(bool bIsEditingPinType/* = false*/) const
{
	if (PinPtr.IsValid())
	{
		if(const URigVMPin* Pin = PinPtr.Get())
		{
			if(Pin->IsExecuteContext())
			{
				if(const URigVMNode* Node = Pin->GetNode())
				{
					if(Node->IsA<URigVMAggregateNode>())
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool FRigVMFunctionArgumentLayout::CanArgumentBeMoved(bool bMoveUp) const
{
	if(IsPinEditingReadOnly())
	{
		return false;
	}
	if (PinPtr.IsValid())
	{
		URigVMPin* Pin = PinPtr.Get();
		if(Pin->IsExecuteContext())
		{
			return false;
		}

		if(URigVMNode* Node = Pin->GetNode())
		{
			auto IsInput = [](URigVMPin* InPin) -> bool
			{
				return InPin->GetDirection() == ERigVMPinDirection::Input ||
					InPin->GetDirection() == ERigVMPinDirection::Visible;
			};

			const bool bLookForInput = IsInput(Pin);

			if(bMoveUp)
			{
				// if this is the first pin of its type
				for(int32 Index = 0; Index < Node->GetPins().Num(); Index++)
				{
					URigVMPin* OtherPin = Node->GetPins()[Index];
					if(OtherPin->IsExecuteContext())
					{
						continue;
					}
					if(IsInput(OtherPin) == bLookForInput)
					{
						return OtherPin != Pin;
					}
				}
			}
			else if(!bMoveUp)
			{
				// if this is the last pin of its type
				for(int32 Index = Node->GetPins().Num() - 1; Index >= 0; Index--)
				{
					URigVMPin* OtherPin = Node->GetPins()[Index];
					if(IsInput(OtherPin) == bLookForInput)
					{
						return OtherPin != Pin;
					}
				}
			}
		}
	}
	return true;
}

FText FRigVMFunctionArgumentLayout::OnGetArgNameText() const
{
	if (PinPtr.IsValid())
	{
		return FText::FromName(PinPtr.Get()->GetFName());
	}
	return FText();
}

FText FRigVMFunctionArgumentLayout::OnGetArgToolTipText() const
{
	return OnGetArgNameText(); // for now since we don't have tooltips
}

void FRigVMFunctionArgumentLayout::OnArgNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (InTextCommit == ETextCommit::OnEnter)
	{
		if (!NewText.IsEmpty() && PinPtr.IsValid() && RigVMBlueprintPtr.IsValid() && !ShouldPinBeReadOnly())
		{
			URigVMPin* Pin = PinPtr.Get();
			URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
			if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Pin->GetNode()))
			{
				if (URigVMController* Controller = Blueprint->GetController(LibraryNode->GetContainedGraph()))
				{
					const FString& NewName = NewText.ToString();
					Controller->RenameExposedPin(Pin->GetFName(), *NewName, true, true);
				}
			}
		}
	}
}

FEdGraphPinType FRigVMFunctionArgumentLayout::OnGetPinInfo() const
{
	if (PinPtr.IsValid())
	{
		return URigVMEdGraphNode::GetPinTypeForModelPin(PinPtr.Get());
	}
	return FEdGraphPinType();
}

void FRigVMFunctionArgumentLayout::PinInfoChanged(const FEdGraphPinType& PinType)
{
	if (PinPtr.IsValid() && RigVMBlueprintPtr.IsValid() && FBlueprintEditorUtils::IsPinTypeValid(PinType))
	{
		URigVMPin* Pin = PinPtr.Get();
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Pin->GetNode()))
		{
			if (URigVMController* Controller = Blueprint->GetController(LibraryNode->GetContainedGraph()))
			{
				FString CPPType;
				FName CPPTypeObjectName = NAME_None;
				RigVMTypeUtils::CPPTypeFromPinType(PinType, CPPType, CPPTypeObjectName);
				
				bool bSetupUndoRedo = true;
				Controller->ChangeExposedPinType(Pin->GetFName(), CPPType, CPPTypeObjectName, bSetupUndoRedo, false, true);

				// If the controller has identified this as a bulk change, it has not added the actions to the action stack
				// We need to disable the transaction from the UI as well to keep them synced
				if (!bSetupUndoRedo)
				{
					GEditor->CancelTransaction(0);
				}
			}
		}
	}
}

void FRigVMFunctionArgumentLayout::OnPrePinInfoChange(const FEdGraphPinType& PinType)
{
	// not needed for rig vm
}

FRigVMFunctionArgumentDefaultNode::FRigVMFunctionArgumentDefaultNode(
	URigVMGraph* InGraph,
	URigVMBlueprint* InBlueprint
)
	: GraphPtr(InGraph)
	, RigVMBlueprintPtr(InBlueprint)
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		RigVMBlueprintPtr.Get()->OnModified().AddRaw(this, &FRigVMFunctionArgumentDefaultNode::HandleModifiedEvent);

		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(GraphPtr->GetOuter()))
		{
			if (URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(RigVMBlueprintPtr->GetEdGraph(LibraryNode->GetGraph())))
			{
				EdGraphOuterPtr = RigGraph;
				GraphChangedDelegateHandle = RigGraph->AddOnGraphChangedHandler(
					FOnGraphChanged::FDelegate::CreateRaw(this, &FRigVMFunctionArgumentDefaultNode::OnGraphChanged)
				);
			}
		}	

	}
}

FRigVMFunctionArgumentDefaultNode::~FRigVMFunctionArgumentDefaultNode()
{
	if (RigVMBlueprintPtr.IsValid())
	{
		RigVMBlueprintPtr.Get()->OnModified().RemoveAll(this);
	}
	
	if (EdGraphOuterPtr.IsValid())
	{
		if (GraphChangedDelegateHandle.IsValid())
		{
			EdGraphOuterPtr->RemoveOnGraphChangedHandler(GraphChangedDelegateHandle);
		}		
	}
}

void FRigVMFunctionArgumentDefaultNode::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	if (!GraphPtr.IsValid() || !RigVMBlueprintPtr.IsValid())
	{
		return;
	}

	URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
	URigVMGraph* Graph = GraphPtr.Get();
	URigVMEdGraphNode* RigVMEdGraphNode = nullptr;
	if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter()))
	{
		if (URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(Blueprint->GetEdGraph(LibraryNode->GetGraph())))
		{
			RigVMEdGraphNode = Cast<URigVMEdGraphNode>(RigGraph->FindNodeForModelNodeName(LibraryNode->GetFName()));
		}
	}

	if (RigVMEdGraphNode == nullptr)
	{
		return;
	}

	ChildrenBuilder.AddCustomRow(FText::GetEmpty())
	.WholeRowContent()
	.MaxDesiredWidth(980.f)
	[
		SAssignNew(OwnedNodeWidget, SRigVMGraphNode).GraphNodeObj(RigVMEdGraphNode)
	];

	OwnedNodeWidget->SetIsEditable(true);
	TArray< TSharedRef<SWidget> > Pins;
	OwnedNodeWidget->GetPins(Pins);
	for (TSharedRef<SWidget> Pin : Pins)
	{
		TSharedRef<SGraphPin> SPin = StaticCastSharedRef<SGraphPin>(Pin);
		SPin->EnableDragAndDrop(false);
	}
}

void FRigVMFunctionArgumentDefaultNode::OnGraphChanged(const FEdGraphEditAction& InAction)
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		OnRebuildChildren.ExecuteIfBound();
	}
}

void FRigVMFunctionArgumentDefaultNode::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	if (!GraphPtr.IsValid())
	{
		return;
	}

	URigVMGraph* Graph = GraphPtr.Get();
	URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter());
	if (LibraryNode == nullptr)
	{
		return;
	}
	if (LibraryNode->GetGraph() != InGraph)
	{
		return;
	}

	switch (InNotifType)
	{
		case ERigVMGraphNotifType::PinAdded:
		case ERigVMGraphNotifType::PinRemoved:
		case ERigVMGraphNotifType::PinTypeChanged:
		case ERigVMGraphNotifType::PinIndexChanged:
		case ERigVMGraphNotifType::PinRenamed:
		{
			URigVMPin* Pin = CastChecked<URigVMPin>(InSubject);
			if (Pin->GetNode() == LibraryNode)
			{
				OnRebuildChildren.ExecuteIfBound();
			}
			break;
		}
		case ERigVMGraphNotifType::NodeRenamed:
		case ERigVMGraphNotifType::NodeColorChanged:
		{
			URigVMNode* Node = CastChecked<URigVMNode>(InSubject);
			if (Node == LibraryNode)
			{
				OnRebuildChildren.ExecuteIfBound();
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

TSharedPtr<IDetailCustomization> FRigVMGraphDetailCustomization::MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor, const UClass* InExpectedBlueprintClass)
{
	const TArray<UObject*>* Objects = (InBlueprintEditor.IsValid() ? InBlueprintEditor->GetObjectsCurrentlyBeingEdited() : nullptr);
	if (Objects && Objects->Num() == 1)
	{
		if (URigVMBlueprint* RigVMBlueprint = Cast<URigVMBlueprint>((*Objects)[0]))
		{
			if(RigVMBlueprint->GetClass() == InExpectedBlueprintClass)
			{
				return MakeShareable(new FRigVMGraphDetailCustomization(StaticCastSharedPtr<FRigVMEditor>(InBlueprintEditor), RigVMBlueprint));
			}
		}
	}

	return nullptr;
}

void FRigVMGraphDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	bIsPickingColor = false;

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);

	GraphPtr = CastChecked<URigVMEdGraph>(Objects[0].Get());
	URigVMEdGraph* Graph = GraphPtr.Get();

	URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
	URigVMGraph* Model = nullptr;
	URigVMController* Controller = nullptr;

	if (Blueprint)
	{
		Model = Blueprint->GetModel(Graph);
		Controller = Blueprint->GetController(Model);
	}

	if (Blueprint == nullptr || Model == nullptr || Controller == nullptr)
	{
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Graph", LOCTEXT("FunctionDetailsGraph", "Graph"));
		Category.AddCustomRow(FText::GetEmpty())
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GraphPresentButNotEditable", "Graph is not editable."))
		];
		return;
	}

	if (Model->IsTopLevelGraph())
	{
		IDetailCategoryBuilder& Category = DetailLayout.EditCategory("Graph", LOCTEXT("FunctionDetailsGraph", "Graph"));
		Category.AddCustomRow(FText::GetEmpty())
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GraphIsTopLevelGraph", "Top-level Graphs are not editable."))
			];
		return;
	}

	bool bIsFunction = false;
	bool bIsAggregate = false;
	if (Model)
	{
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Model->GetOuter()))
		{
			bIsFunction = LibraryNode->GetGraph()->IsA<URigVMFunctionLibrary>();
			bIsAggregate = LibraryNode->IsA<URigVMAggregateNode>();
		}
	}

	IDetailCategoryBuilder& InputsCategory = DetailLayout.EditCategory("Inputs", LOCTEXT("FunctionDetailsInputs", "Inputs"));
	TSharedRef<FRigVMFunctionArgumentGroupLayout> InputArgumentGroup = MakeShareable(new FRigVMFunctionArgumentGroupLayout(
		Model, 
		Blueprint,
		RigVMEditorPtr,
		true));
	InputsCategory.AddCustomBuilder(InputArgumentGroup);

	if(!bIsAggregate)
	{
		TSharedRef<SHorizontalBox> InputsHeaderContentWidget = SNew(SHorizontalBox);

		InputsHeaderContentWidget->AddSlot()
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(10.f, 0))
			.OnClicked(this, &FRigVMGraphDetailCustomization::OnAddNewInputClicked)
			.Visibility(this, &FRigVMGraphDetailCustomization::GetAddNewInputOutputVisibility)
			.HAlign(HAlign_Right)
			.ToolTipText(LOCTEXT("FunctionNewInputArgTooltip", "Create a new input argument"))
			.VAlign(VAlign_Center)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FunctionNewInputArg")))
			.IsEnabled(this, &FRigVMGraphDetailCustomization::IsAddNewInputOutputEnabled)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
		InputsCategory.HeaderContent(InputsHeaderContentWidget);
	}
	
	IDetailCategoryBuilder& OutputsCategory = DetailLayout.EditCategory("Outputs", LOCTEXT("FunctionDetailsOutputs", "Outputs"));
	TSharedRef<FRigVMFunctionArgumentGroupLayout> OutputArgumentGroup = MakeShareable(new FRigVMFunctionArgumentGroupLayout(
		Model, 
		Blueprint,
		RigVMEditorPtr,
		false));
	OutputsCategory.AddCustomBuilder(OutputArgumentGroup);

	if(!bIsAggregate)
	{
		TSharedRef<SHorizontalBox> OutputsHeaderContentWidget = SNew(SHorizontalBox);

		OutputsHeaderContentWidget->AddSlot()
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(10.f, 0))
			.OnClicked(this, &FRigVMGraphDetailCustomization::OnAddNewOutputClicked)
			.Visibility(this, &FRigVMGraphDetailCustomization::GetAddNewInputOutputVisibility)
			.HAlign(HAlign_Right)
			.ToolTipText(LOCTEXT("FunctionNewOutputArgTooltip", "Create a new output argument"))
			.VAlign(VAlign_Center)
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FunctionNewOutputArg")))
			.IsEnabled(this, &FRigVMGraphDetailCustomization::IsAddNewInputOutputEnabled)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
		OutputsCategory.HeaderContent(OutputsHeaderContentWidget);
	}
	
	IDetailCategoryBuilder& SettingsCategory = DetailLayout.EditCategory("NodeSettings", LOCTEXT("FunctionDetailsNodeSettings", "Node Settings"));

	if(bIsFunction)
	{
		// node category
		SettingsCategory.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Category")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigVMGraphDetailCustomization::GetNodeCategory)
			.OnTextCommitted(this, &FRigVMGraphDetailCustomization::SetNodeCategory)
			.OnVerifyTextChanged_Lambda([&](const FText& InNewText, FText& OutErrorMessage) -> bool
			{
				const FText NewText = FEditorCategoryUtils::GetCategoryDisplayString(InNewText);
				if (NewText.ToString().Len() >= NAME_SIZE)
				{
					OutErrorMessage = LOCTEXT("CategoryTooLong", "Name of category is too long.");
					return false;
				}
				
				if (RigVMBlueprintPtr.IsValid())
				{
					if (NewText.EqualTo(FText::FromString(RigVMBlueprintPtr.Get()->GetName())))
					{
						OutErrorMessage = LOCTEXT("CategoryEqualsBlueprintName", "Cannot add a category with the same name as the blueprint.");
						return false;
					}
				}
				return true;
			})
		];

		// node keywords
		SettingsCategory.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Keywords")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SEditableTextBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigVMGraphDetailCustomization::GetNodeKeywords)
			.OnTextCommitted(this, &FRigVMGraphDetailCustomization::SetNodeKeywords)
		];

		// description
		SettingsCategory.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Description")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SMultiLineEditableText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(this, &FRigVMGraphDetailCustomization::GetNodeDescription)
			.OnTextCommitted(this, &FRigVMGraphDetailCustomization::SetNodeDescription)
		];

		if(AccessSpecifierStrings.IsEmpty())
		{
			AccessSpecifierStrings.Add(TSharedPtr<FRigVMStringWithTag>(new FRigVMStringWithTag(TEXT("Public"))));
			AccessSpecifierStrings.Add(TSharedPtr<FRigVMStringWithTag>(new FRigVMStringWithTag(TEXT("Private"))));
		}

		// access specifier
		SettingsCategory.AddCustomRow( LOCTEXT( "AccessSpecifier", "Access Specifier" ) )
        .NameContent()
        [
            SNew(STextBlock)
                .Text( LOCTEXT( "AccessSpecifier", "Access Specifier" ) )
                .Font( IDetailLayoutBuilder::GetDetailFont() )
        ]
        .ValueContent()
        [
            SNew(SComboButton)
            .ContentPadding(0)
            .ButtonContent()
            [
                SNew(STextBlock)
                    .Text(this, &FRigVMGraphDetailCustomization::GetCurrentAccessSpecifierName)
                    .Font( IDetailLayoutBuilder::GetDetailFont() )
            ]
            .MenuContent()
            [
                SNew(SListView<TSharedPtr<FRigVMStringWithTag> >)
                    .ListItemsSource( &AccessSpecifierStrings )
                    .OnGenerateRow(this, &FRigVMGraphDetailCustomization::HandleGenerateRowAccessSpecifier)
                    .OnSelectionChanged(this, &FRigVMGraphDetailCustomization::OnAccessSpecifierSelected)
            ]
        ];
	}

	// node color
	if(!bIsAggregate)
	{
		SettingsCategory.AddCustomRow(FText::GetEmpty())
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("Color")))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "Menu.Button")
			.OnClicked(this, &FRigVMGraphDetailCustomization::OnNodeColorClicked)
			[
				SAssignNew(ColorBlock, SColorBlock)
				.Color(this, &FRigVMGraphDetailCustomization::GetNodeColor)
				.Size(FVector2D(77, 16))
			]
		];
	}

	IDetailCategoryBuilder& DefaultsCategory = DetailLayout.EditCategory("NodeDefaults", LOCTEXT("FunctionDetailsNodeDefaults", "Node Defaults"));
	TSharedRef<FRigVMFunctionArgumentDefaultNode> DefaultsArgumentNode = MakeShareable(new FRigVMFunctionArgumentDefaultNode(
		Model,
		Blueprint));
	DefaultsCategory.AddCustomBuilder(DefaultsArgumentNode);
}

bool FRigVMGraphDetailCustomization::IsAddNewInputOutputEnabled() const
{
	return true;
}

EVisibility FRigVMGraphDetailCustomization::GetAddNewInputOutputVisibility() const
{
	return EVisibility::Visible;
}

FReply FRigVMGraphDetailCustomization::OnAddNewInputClicked()
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get());
		if (URigVMController* Controller = Blueprint->GetController(Model))
		{
			FName ArgumentName = TEXT("Argument");
			FString CPPType = TEXT("bool");
			FName CPPTypeObjectPath = NAME_None;
			FString DefaultValue = TEXT("False");

			if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Model->GetOuter()))
			{
				if (LibraryNode->GetPins().Num() > 0)
				{
					URigVMPin* LastPin = LibraryNode->GetPins().Last();
					if (!LastPin->IsExecuteContext())
					{
						// strip off any tailing number from for example Argument_2
						FString StrippedArgumentName = LastPin->GetName();
						FString LastChars = StrippedArgumentName.Right(1);
						StrippedArgumentName.LeftChopInline(1);
						while(LastChars.IsNumeric() && !StrippedArgumentName.IsEmpty())
						{
							LastChars = StrippedArgumentName.Right(1);
							StrippedArgumentName.LeftChopInline(1);

							if(LastChars.StartsWith(TEXT("_")))
							{
								LastChars.Reset();
								break;
							}
						}

						StrippedArgumentName = StrippedArgumentName + LastChars;
						if(!StrippedArgumentName.IsEmpty())
						{
							ArgumentName = *StrippedArgumentName;
						}

						RigVMTypeUtils::CPPTypeFromPin(LastPin, CPPType, CPPTypeObjectPath);						
						DefaultValue = LastPin->GetDefaultValue();
					}
				}
			}

			Controller->AddExposedPin(ArgumentName, ERigVMPinDirection::Input, CPPType, CPPTypeObjectPath, DefaultValue, true, true);
		}
	}
	return FReply::Unhandled();
}

FReply FRigVMGraphDetailCustomization::OnAddNewOutputClicked()
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get());
		if (URigVMController* Controller = Blueprint->GetController(Model))
		{
			FName ArgumentName = TEXT("Argument");
			FString CPPType = TEXT("bool");
			FName CPPTypeObjectPath = NAME_None;
			FString DefaultValue = TEXT("False");
			// todo: base decisions on types on last argument

			Controller->AddExposedPin(ArgumentName, ERigVMPinDirection::Output, CPPType, CPPTypeObjectPath, DefaultValue, true, true);
		}
	}
	return FReply::Unhandled();
}

FText FRigVMGraphDetailCustomization::GetNodeCategory() const
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				return FText::FromString(OuterNode->GetNodeCategory());
			}
		}
	}

	return FText();
}

void FRigVMGraphDetailCustomization::SetNodeCategory(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				if (URigVMController* Controller = Blueprint->GetOrCreateController(OuterNode->GetGraph()))
				{
					Controller->SetNodeCategory(OuterNode, InNewText.ToString(), true, false, true);
				}
			}
		}
	}
}

FText FRigVMGraphDetailCustomization::GetNodeKeywords() const
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				return FText::FromString(OuterNode->GetNodeKeywords());
			}
		}
	}

	return FText();
}

void FRigVMGraphDetailCustomization::SetNodeKeywords(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				if (URigVMController* Controller = Blueprint->GetOrCreateController(OuterNode->GetGraph()))
				{
					Controller->SetNodeKeywords(OuterNode, InNewText.ToString(), true, false, true);
				}
			}
		}
	}
}

FText FRigVMGraphDetailCustomization::GetNodeDescription() const
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				return FText::FromString(OuterNode->GetNodeDescription());
			}
		}
	}

	return FText();
}

void FRigVMGraphDetailCustomization::SetNodeDescription(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				if (URigVMController* Controller = Blueprint->GetOrCreateController(OuterNode->GetGraph()))
				{
					Controller->SetNodeDescription(OuterNode, InNewText.ToString(), true, false, true);
				}
			}
		}
	}
}

FLinearColor FRigVMGraphDetailCustomization::GetNodeColor() const
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				return OuterNode->GetNodeColor();
			}
		}
	}
	return FLinearColor::White;
}

void FRigVMGraphDetailCustomization::SetNodeColor(FLinearColor InColor, bool bSetupUndoRedo)
{
	if (GraphPtr.IsValid() && RigVMBlueprintPtr.IsValid())
	{
		URigVMBlueprint* Blueprint = RigVMBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				if (URigVMController* Controller = Blueprint->GetOrCreateController(OuterNode->GetGraph()))
				{
					Controller->SetNodeColor(OuterNode, InColor, bSetupUndoRedo, bIsPickingColor, true);
				}
			}
		}
	}
}

void FRigVMGraphDetailCustomization::OnNodeColorBegin()
{
	bIsPickingColor = true;
}
void FRigVMGraphDetailCustomization::OnNodeColorEnd()
{ 
	bIsPickingColor = false; 
}

void FRigVMGraphDetailCustomization::OnNodeColorCancelled(FLinearColor OriginalColor)
{
	SetNodeColor(OriginalColor, true);
}

FReply FRigVMGraphDetailCustomization::OnNodeColorClicked()
{
	FColorPickerArgs PickerArgs = FColorPickerArgs(GetNodeColor(), FOnLinearColorValueChanged::CreateSP(this, &FRigVMGraphDetailCustomization::SetNodeColor, true));
	PickerArgs.ParentWidget = ColorBlock;
	PickerArgs.bUseAlpha = false;
	PickerArgs.DisplayGamma = false;
	PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP(this, &FRigVMGraphDetailCustomization::OnNodeColorBegin);
	PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP(this, &FRigVMGraphDetailCustomization::OnNodeColorEnd);
	PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &FRigVMGraphDetailCustomization::OnNodeColorCancelled);
	OpenColorPicker(PickerArgs);
	return FReply::Handled();
}

TArray<TSharedPtr<FRigVMStringWithTag>> FRigVMGraphDetailCustomization::AccessSpecifierStrings;

FText FRigVMGraphDetailCustomization::GetCurrentAccessSpecifierName() const
{
	if(RigVMBlueprintPtr.IsValid() && GraphPtr.IsValid())
	{
		URigVMEdGraph* Graph = GraphPtr.Get();
		URigVMBlueprint* RigVMBlueprint = RigVMBlueprintPtr.Get();

		if (URigVMGraph* RigVMGraph = Graph->GetModel())
		{
			if (URigVMLibraryNode* LibraryNode = RigVMGraph->GetTypedOuter<URigVMLibraryNode>())
			{
				if(RigVMBlueprint->IsFunctionPublic(LibraryNode->GetFName()))
				{
					return FText::FromString(AccessSpecifierStrings[0]->GetString()); // public
				}
			}
		}
	}

	return FText::FromString(AccessSpecifierStrings[1]->GetString()); // private
}

void FRigVMGraphDetailCustomization::OnAccessSpecifierSelected( TSharedPtr<FRigVMStringWithTag> SpecifierName, ESelectInfo::Type SelectInfo )
{
	if(RigVMBlueprintPtr.IsValid() && GraphPtr.IsValid())
	{
		URigVMEdGraph* Graph = GraphPtr.Get();
		URigVMBlueprint* RigVMBlueprint = RigVMBlueprintPtr.Get();

		if (URigVMGraph* RigVMGraph = Graph->GetModel())
		{
			if (URigVMLibraryNode* LibraryNode = RigVMGraph->GetTypedOuter<URigVMLibraryNode>())
			{
				if(SpecifierName->Equals(TEXT("Private")))
				{
					RigVMBlueprint->MarkFunctionPublic(LibraryNode->GetFName(), false);
				}
				else
				{
					RigVMBlueprint->MarkFunctionPublic(LibraryNode->GetFName(), true);
				}
			}
		}
	}
}

TSharedRef<ITableRow> FRigVMGraphDetailCustomization::HandleGenerateRowAccessSpecifier( TSharedPtr<FRigVMStringWithTag> SpecifierName, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow< TSharedPtr<FRigVMStringWithTag> >, OwnerTable)
        .Content()
        [
            SNew( STextBlock ) 
                .Text(FText::FromString(SpecifierName->GetString()) )
        ];
}

FRigVMWrappedNodeDetailCustomization::FRigVMWrappedNodeDetailCustomization()
: BlueprintBeingCustomized(nullptr)
{
}

TSharedRef<IDetailCustomization> FRigVMWrappedNodeDetailCustomization::MakeInstance()
{
	return MakeShareable(new FRigVMWrappedNodeDetailCustomization);
}

void FRigVMWrappedNodeDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailLayout.GetObjectsBeingCustomized(DetailObjects);
	if (DetailObjects.Num() == 0)
	{
		return;
	}

	for(TWeakObjectPtr<UObject> DetailObject : DetailObjects)
	{
		URigVMDetailsViewWrapperObject* WrapperObject = CastChecked<URigVMDetailsViewWrapperObject>(DetailObject.Get());
		if(BlueprintBeingCustomized == nullptr)
		{
			BlueprintBeingCustomized = WrapperObject->GetTypedOuter<URigVMBlueprint>();
		}

		ObjectsBeingCustomized.Add(WrapperObject);
		NodesBeingCustomized.Add(CastChecked<URigVMNode>(WrapperObject->GetSubject()));
	}

	if (BlueprintBeingCustomized == nullptr || ObjectsBeingCustomized.IsEmpty() || NodesBeingCustomized.IsEmpty())
	{
		return;
	}

	UClass* WrapperClass = ObjectsBeingCustomized[0]->GetClass();

	// now loop over all of the properties and display them
	TArray<TSharedPtr<IPropertyHandle>> PropertiesToVisit;
	for (TFieldIterator<FProperty> PropertyIt(WrapperClass); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		TSharedPtr<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(Property->GetFName(), WrapperClass);
		if (!PropertyHandle->IsValidHandle())
		{
			continue;
		}
		PropertiesToVisit.Add(PropertyHandle);

		// check if any / all pins are bound to a variable
		int32 PinsBoundToVariable = 0;
		TArray<URigVMPin*> ModelPins;
		for(TWeakObjectPtr<URigVMNode> Node : NodesBeingCustomized)
		{
			if(URigVMPin* ModelPin = Node->FindPin(Property->GetName()))
			{
				ModelPins.Add(ModelPin);
				PinsBoundToVariable += ModelPin->IsBoundToVariable() ? 1 : 0;
			}
		}

		if(PinsBoundToVariable > 0)
		{
			if(PinsBoundToVariable == ModelPins.Num())
			{
				if(IDetailPropertyRow* Row = DetailLayout.EditDefaultProperty(PropertyHandle))
				{
					Row->CustomWidget()
					.NameContent()
					[
						PropertyHandle->CreatePropertyNameWidget()
					]
					.ValueContent()
					[
					SNew(SRigVMGraphVariableBinding)
						.ModelPins(ModelPins)
						.Blueprint(BlueprintBeingCustomized)
					];
				}
			}
			else // in this case some pins are bound, and some are not - we'll hide the input value widget
			{
				if(IDetailPropertyRow* Row = DetailLayout.EditDefaultProperty(PropertyHandle))
				{
					Row->CustomWidget()
					.NameContent()
					[
						PropertyHandle->CreatePropertyNameWidget()
					];
				}
			}

			continue;
		}
		
		if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
        {
        	FString CustomWidgetName = NameProperty->GetMetaData(TEXT("CustomWidget"));
        	if (!CustomWidgetName.IsEmpty())
        	{
        		URigVMEdGraph* GraphBeingCustomized = Cast<URigVMEdGraph>(
        			BlueprintBeingCustomized->GetEdGraph(NodesBeingCustomized[0]->GetGraph()));
        		ensure(GraphBeingCustomized);
        		
        		const TArray<TSharedPtr<FRigVMStringWithTag>>* NameList = GraphBeingCustomized->GetNameListForWidget(CustomWidgetName);
        		if (NameList)
        		{
        			TSharedPtr<SRigVMGraphPinNameListValueWidget> NameListWidget;

        			if(IDetailPropertyRow* Row = DetailLayout.EditDefaultProperty(PropertyHandle))
        			{
        				Row->CustomWidget()
						.NameContent()
						[
							PropertyHandle->CreatePropertyNameWidget()
						]
						.ValueContent()
						[
							SAssignNew(NameListWidget, SRigVMGraphPinNameListValueWidget)
							.OptionsSource(NameList)
							.OnGenerateWidget(this, &FRigVMWrappedNodeDetailCustomization::MakeNameListItemWidget)
							.OnSelectionChanged(this, &FRigVMWrappedNodeDetailCustomization::OnNameListChanged, NameProperty, DetailLayout.GetPropertyUtilities())
							.OnComboBoxOpening(this, &FRigVMWrappedNodeDetailCustomization::OnNameListComboBox, NameProperty, NameList)
							.InitiallySelectedItem(GetCurrentlySelectedItem(NameProperty, NameList))
							.Content()
							[
								SNew(STextBlock)
								.Text(this, &FRigVMWrappedNodeDetailCustomization::GetNameListText, NameProperty)
								.ColorAndOpacity_Lambda([this, NameProperty]() -> FSlateColor
								{
									static FText NoneText = LOCTEXT("None", "None"); 
									if(GetNameListText(NameProperty).EqualToCaseIgnored(NoneText))
									{
										return FSlateColor(FLinearColor::Red);
									}
									return FSlateColor::UseForeground();
								})
							]
        				];
        			}        			
        			NameListWidgets.Add(Property->GetFName(), NameListWidget);
        		}
        		else
        		{
        			if(IDetailPropertyRow* Row = DetailLayout.EditDefaultProperty(PropertyHandle))
        			{
        				Row->CustomWidget()
						.NameContent()
						[
							PropertyHandle->CreatePropertyNameWidget()
						];
        			}
        		}
        		continue;
        	}
        }
	}

	// now loop over all handles and determine expansion states of the corresponding pins
	for (int32 Index = 0; Index < PropertiesToVisit.Num(); Index++)
	{
		TSharedPtr<IPropertyHandle> PropertyHandle = PropertiesToVisit[Index];
		FProperty* Property = PropertyHandle->GetProperty();

		// certain properties we don't look at for expansion states
		if(FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if(StructProperty->Struct == TBaseStructure<FVector>::Get() ||
				StructProperty->Struct == TBaseStructure<FVector2D>::Get() ||
				StructProperty->Struct == TBaseStructure<FRotator>::Get() ||
				StructProperty->Struct == TBaseStructure<FQuat>::Get())
			{
				continue;
			}
		}

		bool bFound = false;
		const FString PinPath = PropertyHandle->GeneratePathToProperty();
		for(TWeakObjectPtr<URigVMNode> Node : NodesBeingCustomized)
		{
			if(URigVMPin* Pin = Node->FindPin(PinPath))
			{
				bFound = true;
				
				if(Pin->IsExpanded())
				{
					if(IDetailPropertyRow* Row = DetailLayout.EditDefaultProperty(PropertyHandle))
					{
						Row->ShouldAutoExpand(true);
					}
					break;
				}
			}
		}

		if(!bFound)
		{
			continue;
		}

		uint32 NumChildren = 0;
		PropertyHandle->GetNumChildren(NumChildren);
		for(uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
		{
			PropertiesToVisit.Add(PropertyHandle->GetChildHandle(ChildIndex));
		}
	}

	CustomizeLiveValues(DetailLayout);
}

TSharedRef<SWidget> FRigVMWrappedNodeDetailCustomization::MakeNameListItemWidget(TSharedPtr<FRigVMStringWithTag> InItem)
{
	//TODO: make this prettier
	return 	SNew(STextBlock).Text(FText::FromString(InItem->GetStringWithTag()));// .Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}

FText FRigVMWrappedNodeDetailCustomization::GetNameListText(FNameProperty* InProperty) const
{
	FText FirstText;
	for(TWeakObjectPtr<URigVMDetailsViewWrapperObject> ObjectBeingCustomized : ObjectsBeingCustomized)
	{
		if (FName* Value = InProperty->ContainerPtrToValuePtr<FName>(ObjectBeingCustomized.Get()))
		{
			FText Text = FText::FromName(*Value);
			if(FirstText.IsEmpty())
			{
				FirstText = Text;
			}
			else if(!FirstText.EqualTo(Text))
			{
				return RigVMGraphDetailCustomizationMultipleValues;
			}
		}
	}
	return FirstText;
}

TSharedPtr<FRigVMStringWithTag> FRigVMWrappedNodeDetailCustomization::GetCurrentlySelectedItem(FNameProperty* InProperty, const TArray<TSharedPtr<FRigVMStringWithTag>>* InNameList) const
{
	FString CurrentItem = GetNameListText(InProperty).ToString();
	for (const TSharedPtr<FRigVMStringWithTag>& Item : *InNameList)
	{
		if (Item->Equals(CurrentItem))
		{
			return Item;
		}
	}
	return TSharedPtr<FRigVMStringWithTag>();
}


void FRigVMWrappedNodeDetailCustomization::SetNameListText(const FText& NewTypeInValue, ETextCommit::Type, FNameProperty* InProperty, TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	URigVMGraph* Graph = NodesBeingCustomized[0]->GetGraph();
	URigVMController* Controller = BlueprintBeingCustomized->GetController(Graph);

	Controller->OpenUndoBracket(FString::Printf(TEXT("Set %s"), *InProperty->GetName()));
	
	for(TWeakObjectPtr<URigVMNode> Node : NodesBeingCustomized)
	{
		if(URigVMPin* Pin = Node->FindPin(InProperty->GetName()))
		{
			Controller->SetPinDefaultValue(Pin->GetPinPath(), NewTypeInValue.ToString(), false, true, false, true);
		}
	}

	Controller->CloseUndoBracket();
}

void FRigVMWrappedNodeDetailCustomization::OnNameListChanged(TSharedPtr<FRigVMStringWithTag> NewSelection, ESelectInfo::Type SelectInfo, FNameProperty* InProperty, TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		const FString& NewValue = NewSelection->GetString();
		SetNameListText(FText::FromString(NewValue), ETextCommit::OnEnter, InProperty, PropertyUtilities);
	}
}

void FRigVMWrappedNodeDetailCustomization::OnNameListComboBox(FNameProperty* InProperty, const TArray<TSharedPtr<FRigVMStringWithTag>>* InNameList)
{
	TSharedPtr<SRigVMGraphPinNameListValueWidget> Widget = NameListWidgets.FindChecked(InProperty->GetFName());
	const TSharedPtr<FRigVMStringWithTag> CurrentlySelected = GetCurrentlySelectedItem(InProperty, InNameList);
	Widget->SetSelectedItem(CurrentlySelected);
}

void FRigVMWrappedNodeDetailCustomization::CustomizeLiveValues(IDetailLayoutBuilder& DetailLayout)
{
	if(ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}
	
	URigVMHost* DebuggedHost = Cast<URigVMHost>(BlueprintBeingCustomized->GetObjectBeingDebugged());
	if(DebuggedHost == nullptr)
	{
		return;
	}

	URigVM* VM = DebuggedHost->GetVM();
	if(VM == nullptr)
	{
		return;
	}

	URigVMDetailsViewWrapperObject* FirstWrapper = ObjectsBeingCustomized[0].Get();
	URigVMNode* FirstNode = NodesBeingCustomized[0].Get();
	if(FirstNode->GetTypedOuter<URigVMFunctionLibrary>())
	{
		return;
	}

	TSharedPtr<FRigVMParserAST> AST = FirstNode->GetGraph()->GetRuntimeAST(BlueprintBeingCustomized->VMCompileSettings.ASTSettings, false);
	if(!AST.IsValid())
	{
		return;
	}

	FRigVMByteCode& ByteCode = VM->GetByteCode();
	if(ByteCode.GetFirstInstructionIndexForSubject(FirstNode) == INDEX_NONE)
	{
		return;
	}

	/*
	IDetailCategoryBuilder& DebugCategory = DetailLayout.EditCategory("DebugLiveValues", LOCTEXT("DebugLiveValues", "Inspect Live Values"), ECategoryPriority::Uncommon);
	DebugCategory.InitiallyCollapsed(true);

	for(URigVMPin* Pin : FirstNode->GetPins())
	{
		if (Pin->IsExecuteContext())
		{
			continue;
		}
		
		// only show hidden pins in debug mode
		if(Pin->GetDirection() == ERigVMPinDirection::Hidden)
		{
			if(!DebuggedHost->IsInDebugMode())
			{
				continue;
			}
		}
		
		URigVMPin* SourcePin = Pin;
		if(BlueprintBeingCustomized->VMCompileSettings.ASTSettings.bFoldAssignments)
		{
			do
			{
				TArray<URigVMPin*> SourcePins = SourcePin->GetLinkedSourcePins(false);
				if(SourcePins.Num() > 0)
				{
					SourcePin = SourcePins[0];
				}
				else
				{
					break;
				}
			}
			while(SourcePin->GetNode()->IsA<URigVMRerouteNode>());
		}
		
		TArray<const FRigVMExprAST*> Expressions = AST->GetExpressionsForSubject(SourcePin);
		if(Expressions.Num() == 0 && SourcePin != Pin)
		{
			SourcePin = Pin;
			Expressions = AST->GetExpressionsForSubject(Pin);
		}

		bool bHasVar = false;
		for(const FRigVMExprAST* Expression : Expressions)
		{
			if(Expression->IsA(FRigVMExprAST::EType::Literal))
			{
				continue;
			}
			else if(Expression->IsA(FRigVMExprAST::EType::Var))
			{
				bHasVar = true;
				break;
			}
		}

		TArray<const FRigVMExprAST*> FilteredExpressions;
		for(const FRigVMExprAST* Expression : Expressions)
		{
			if(Expression->IsA(FRigVMExprAST::EType::Literal))
			{
				if(bHasVar)
				{
					continue;
				}
				FilteredExpressions.Add(Expression);
			}
			else if(Expression->IsA(FRigVMExprAST::EType::Var))
			{
				FilteredExpressions.Add(Expression);
			}
			else if(Expression->IsA(FRigVMExprAST::EType::CachedValue))
			{
				const FRigVMCachedValueExprAST* CachedValueExpr = Expression->To<FRigVMCachedValueExprAST>();
				FilteredExpressions.Add(CachedValueExpr->GetVarExpr());
			}
		}

		bool bAddedProperty = false;
		int32 SuffixIndex = 1;
		FString NameSuffix;

		auto UpdateRow = [&](IDetailPropertyRow* PropertyRow)
		{
			PropertyRow->DisplayName(FText::FromString(FString::Printf(TEXT("%s%s"), *Pin->GetName(), *NameSuffix)));
			PropertyRow->IsEnabled(false);

			SuffixIndex++;
			bAddedProperty = true;
			NameSuffix = FString::Printf(TEXT("_%d"), SuffixIndex);
		};

		static const FAddPropertyParams AddPropertyParams = FAddPropertyParams().ForceShowProperty();

		TArray<FRigVMOperand> KnownOperands;
		for(const FRigVMExprAST* Expression : FilteredExpressions)
		{
			const FRigVMVarExprAST* VarExpr = Expression->To<FRigVMVarExprAST>();

			FString PinHash = URigVMCompiler::GetPinHash(SourcePin, VarExpr, false);
			const FRigVMOperand* Operand = BlueprintBeingCustomized->PinToOperandMap.Find(PinHash);
			if(Operand)
			{
				if(Operand->GetRegisterOffset() != INDEX_NONE)
				{
					continue;
				}
				if(KnownOperands.Contains(*Operand))
				{
					continue;
				}

				const FProperty* Property = nullptr;

				TArray<FRigVMMemoryStorageStruct*> ExternalStructs;
				TArray<UObject*> ExternalObjects;

				if(Operand->GetMemoryType() == ERigVMMemoryType::External)
				{
					if(!VM->GetExternalVariableDefs().IsValidIndex(Operand->GetRegisterIndex()))
					{
						continue;
					}
					ExternalObjects.Add(DebuggedHost);
					Property = VM->GetExternalVariableDefs()[Operand->GetRegisterIndex()].Property; 
				}
				else
				{
					FRigVMMemoryStorageStruct* Memory = DebuggedHost->GetMemoryByType(Operand->GetMemoryType());
					if(Memory == nullptr)
					{
						continue;
					}

					if(!Memory->IsValidIndex(Operand->GetRegisterIndex()))
					{
						continue;
					}
					
					Property = Memory->GetProperty(Operand->GetRegisterIndex());
					if(Property == nullptr)
					{
						continue;
					}

					ExternalStructs.Add(Memory);
				}

				check(ExternalObjects.Num() > 0 || ExternalStructs.Num() > 0);
				check(Property);

				if (!ExternalObjects.IsEmpty())
				{
					if(IDetailPropertyRow* PropertyRow = DebugCategory.AddExternalObjectProperty(ExternalObjects, Property->GetFName(), EPropertyLocation::Default, AddPropertyParams))
					{
						UpdateRow(PropertyRow);
					}
				}

				for (FRigVMMemoryStorageStruct* Memory : ExternalStructs)
				{
					if (IDetailPropertyRow* PropertyRow = DebugCategory.AddExternalStructureProperty(MakeShared<FInstancePropertyBagStructureDataProvider>(*Memory), Property->GetFName(), EPropertyLocation::Default, AddPropertyParams))
					{
						UpdateRow(PropertyRow);
					}
				}

				KnownOperands.Add(*Operand);
			}
		}

		if(!bAddedProperty)
		{
			TSharedPtr<IPropertyHandle> PinHandle = DetailLayout.GetProperty(Pin->GetFName());
			if(PinHandle.IsValid())
			{
				// we'll build a new custom row. adding the same property again
				// causes the property to be marked customized - thus it won't
				// show correctly in the default category.
				DebugCategory.AddCustomRow(FText::FromName(Pin->GetFName()))
				.NameContent()
				[
					PinHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					PinHandle->CreatePropertyValueWidget()
				]
				.IsEnabled(false);
			}
		}
	}
	*/
}

FRigVMGraphEnumDetailCustomization::FRigVMGraphEnumDetailCustomization()
: BlueprintBeingCustomized(nullptr)
, GraphBeingCustomized(nullptr)
{
}

void FRigVMGraphEnumDetailCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> Objects;
	InPropertyHandle->GetOuterObjects(Objects);

	StructsBeingCustomized.Reset();
	InPropertyHandle->GetOuterStructs(StructsBeingCustomized);

	for (UObject* Object : Objects)
	{
		ObjectsBeingCustomized.Add(Object);

		if(BlueprintBeingCustomized == nullptr)
		{
			BlueprintBeingCustomized = Object->GetTypedOuter<URigVMBlueprint>();
		}

		if(GraphBeingCustomized == nullptr)
		{
			GraphBeingCustomized = Object->GetTypedOuter<URigVMGraph>();
		}
	}

	FProperty* Property = InPropertyHandle->GetProperty();
	const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);

	HeaderRow
		.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
			.MinDesiredWidth(375.f)
			.MaxDesiredWidth(375.f)
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
					.MinDesiredWidth(150)
					.MaxDesiredWidth(400)
					[
						SNew(SRigVMEnumPicker)
						.IsEnabled(true)
						.OnEnumChanged(this, &FRigVMGraphEnumDetailCustomization::HandleControlEnumChanged, InPropertyHandle)
						.GetCurrentEnum_Lambda([this, InPropertyHandle]()
						{
							UEnum* Enum = nullptr;
							FEditPropertyChain PropertyChain;
							TArray<int32> PropertyArrayIndices;
							bool bEnabled;
							if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
							{
								return Enum;
							}

							const TArray<uint8*> MemoryBlocks = GetMemoryBeingCustomized();
							for(uint8* MemoryBlock: MemoryBlocks)
							{
								if(MemoryBlock)
								{
									if (UEnum** CurrentEnum = ContainerMemoryBlockToEnumPtr(MemoryBlock, PropertyChain, PropertyArrayIndices))
									{
										Enum = *CurrentEnum;
									}
								}
							}
							return Enum;
						})
					]
				];
}

void FRigVMGraphEnumDetailCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// nothing to do here
}

void FRigVMGraphEnumDetailCustomization::HandleControlEnumChanged(TSharedPtr<FString> InEnumPath, ESelectInfo::Type InSelectType, TSharedRef<IPropertyHandle> InPropertyHandle)
{
	if (ObjectsBeingCustomized.IsEmpty())
	{
		return;
	}
		
	FEditPropertyChain PropertyChain;
	TArray<int32> PropertyArrayIndices;
	bool bEnabled;
	if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
	{
		return;
	}
	
	TArray<UObject*> ObjectsView;
	for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
	{
		const TWeakObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
		if (Object.Get())
		{
			ObjectsView.Add(Object.Get());
		}
	}
	FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), EPropertyChangeType::ValueSet, ObjectsView);
	FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);

	URigVMController* Controller = nullptr;
	if(BlueprintBeingCustomized && GraphBeingCustomized)
	{
		Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
		Controller->OpenUndoBracket(FString::Printf(TEXT("Set %s"), *InPropertyHandle->GetProperty()->GetName()));
	}

	for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
	{
		const TWeakObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
		if(Object.Get() && InPropertyHandle->IsValidHandle())
		{
			UEnum** CurrentEnum = ContainerMemoryBlockToEnumPtr((uint8*)Object.Get(), PropertyChain, PropertyArrayIndices);
			if (CurrentEnum)
			{
				const UEnum* PreviousEnum = *CurrentEnum;
				*CurrentEnum = FindObject<UEnum>(nullptr, **InEnumPath.Get(), false);

				if (PreviousEnum != *CurrentEnum)
				{
					Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
					InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
				}
			}
		}
	}

	if(Controller)
	{
		Controller->CloseUndoBracket();
	}
}

template<typename VectorType, int32 NumberOfComponents>
void FRigVMGraphMathTypeDetailCustomization::MakeVectorHeaderRow(TSharedRef<class IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	typedef typename VectorType::FReal NumericType;
	typedef SNumericVectorInputBox<NumericType, VectorType, NumberOfComponents> SLocalVectorInputBox;

	FEditPropertyChain PropertyChain;
	TArray<int32> PropertyArrayIndices;
	bool bEnabled;
	if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
	{
		return;
	}

	typename SLocalVectorInputBox::FArguments Args;
	Args.Font(IDetailLayoutBuilder::GetDetailFont());
	Args.IsEnabled(bEnabled);
	Args.AllowSpin(true);
	Args.SpinDelta(0.01f);
	Args.bColorAxisLabels(true);
	Args.X_Lambda([this, InPropertyHandle]()
		{
			return GetVectorComponent<VectorType, NumericType>(InPropertyHandle, 0);
		});
	Args.OnXChanged_Lambda([this, InPropertyHandle](NumericType Value)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 0, Value, false);
		});
	Args.OnXCommitted_Lambda([this, InPropertyHandle](NumericType Value, ETextCommit::Type CommitType)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 0, Value, true, CommitType);
		});
	Args.Y_Lambda([this, InPropertyHandle]()
		{
			return GetVectorComponent<VectorType, NumericType>(InPropertyHandle, 1);
		});
	Args.OnYChanged_Lambda([this, InPropertyHandle](NumericType Value)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 1, Value, false);
		});
	Args.OnYCommitted_Lambda([this, InPropertyHandle](NumericType Value, ETextCommit::Type CommitType)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 1, Value, true, CommitType);
		});

	ExtendVectorArgs<VectorType>(InPropertyHandle, &Args);

	HeaderRow
		.IsEnabled(bEnabled)
		.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
	.ValueContent()
		.MinDesiredWidth(375.f)
		.MaxDesiredWidth(375.f)
		.HAlign(HAlign_Left)
		[
			SArgumentNew(Args, SLocalVectorInputBox)
		];
}

template<typename RotationType>
void FRigVMGraphMathTypeDetailCustomization::MakeRotationHeaderRow(TSharedRef<class IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FEditPropertyChain PropertyChain;
	TArray<int32> PropertyArrayIndices;
	bool bEnabled;
	if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
	{
		return;
	}
		
	typedef typename RotationType::FReal NumericType;
	typedef SAdvancedRotationInputBox<NumericType> SLocalRotationInputBox;
	typename SLocalRotationInputBox::FArguments Args;
	Args.Font(IDetailLayoutBuilder::GetDetailFont());
	Args.IsEnabled(bEnabled);
	Args.AllowSpin(true);
	Args.bColorAxisLabels(true);

	ExtendRotationArgs<RotationType>(InPropertyHandle, &Args);

	HeaderRow
		.IsEnabled(bEnabled)
		.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
	.ValueContent()
		.MinDesiredWidth(375.f)
		.MaxDesiredWidth(375.f)
		.HAlign(HAlign_Left)
		[
			SArgumentNew(Args, SLocalRotationInputBox)
		];
}

template <typename TransformType>
void FRigVMGraphMathTypeDetailCustomization::ConfigureTransformWidgetArgs(TSharedRef<IPropertyHandle> InPropertyHandle, typename SAdvancedTransformInputBox<TransformType>::FArguments& WidgetArgs)
{
	FEditPropertyChain PropertyChain;
	TArray<int32> PropertyArrayIndices;
	bool bEnabled;
	if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
	{
		return;
	}
	
	typedef typename TransformType::FReal FReal;
	WidgetArgs.IsEnabled(bEnabled);
	WidgetArgs.AllowEditRotationRepresentation(true);
	WidgetArgs.UseQuaternionForRotation(IsQuaternionBasedRotation<TransformType>());

	static TransformType Identity = TransformType::Identity;

	UObject* DefaultObject = !ObjectsBeingCustomized.IsEmpty() ? ObjectsBeingCustomized[0]->GetClass()->GetDefaultObject() :
	!StructsBeingCustomized.IsEmpty() ? StructsBeingCustomized[0]->GetStruct()->GetClass() : nullptr;

	if (!DefaultObject)
	{
		return;
	}
	
	TransformType DefaultValue = ContainerMemoryBlockToValueRef<TransformType>((uint8*)DefaultObject, Identity, PropertyChain, PropertyArrayIndices);
	
	WidgetArgs.DiffersFromDefault_Lambda([this, InPropertyHandle, DefaultValue](ESlateTransformComponent::Type InTransformComponent) -> bool
	{
		if (ObjectsBeingCustomized.IsEmpty())
		{
			return false;
		}
		
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return false;
		}
		
		for(const TWeakObjectPtr<UObject>& Object : ObjectsBeingCustomized)
		{
			if(Object.Get() && InPropertyHandle->IsValidHandle())
			{
				const TransformType& Transform = ContainerMemoryBlockToValueRef<TransformType>((uint8*)Object.Get(), Identity, PropertyChain, PropertyArrayIndices);

				switch(InTransformComponent)
				{
					case ESlateTransformComponent::Location:
					{
						if(!Transform.GetLocation().Equals(DefaultValue.GetLocation()))
						{
							return true;
						}
						break;
					}
					case ESlateTransformComponent::Rotation:
					{
						if(!Transform.Rotator().Equals(DefaultValue.Rotator()))
						{
							return true;
						}
						break;
					}
					case ESlateTransformComponent::Scale:
					{
						if(!Transform.GetScale3D().Equals(DefaultValue.GetScale3D()))
						{
							return true;
						}
						break;
					}
					default:
					{
						break;
					}
				}
			}
		}
		return false;
	});

	WidgetArgs.OnGetNumericValue_Lambda([this, InPropertyHandle](
		ESlateTransformComponent::Type InTransformComponent,
		ESlateRotationRepresentation::Type InRotationRepresentation,
		ESlateTransformSubComponent::Type InTransformSubComponent) -> TOptional<FReal>
	{
		TOptional<FReal> Result;
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return Result;
		}

		const TArray<uint8*> MemoryBlocks = GetMemoryBeingCustomized();
		for(uint8* MemoryBlock: MemoryBlocks)
		{
			if(MemoryBlock)
			{
				const TransformType& Transform = ContainerMemoryBlockToValueRef<TransformType>(MemoryBlock, Identity, PropertyChain, PropertyArrayIndices);
				
				TOptional<FReal> Value = SAdvancedTransformInputBox<TransformType>::GetNumericValueFromTransform(
					Transform,
					InTransformComponent,
					InRotationRepresentation,
					InTransformSubComponent
					);

				if(Value.IsSet())
				{
					if(Result.IsSet())
					{
						if(!FMath::IsNearlyEqual(Result.GetValue(), Value.GetValue()))
						{
							return TOptional<FReal>();
						}
					}
					else
					{
						Result = Value;
					}
				}
			}
		}
		return Result;
	});
	

	auto OnNumericValueChanged = [this, InPropertyHandle](
		ESlateTransformComponent::Type InTransformComponent, 
		ESlateRotationRepresentation::Type InRotationRepresentation, 
		ESlateTransformSubComponent::Type InSubComponent,
		FReal InValue,
		bool bIsCommit,
		ETextCommit::Type InCommitType = ETextCommit::Default)
	{
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return;
		}
		
		TArray<UObject*> ObjectsView;
		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TWeakObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if (Object.Get())
			{
				ObjectsView.Add(Object.Get());
			}
		}
		FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), bIsCommit ? EPropertyChangeType::ValueSet : EPropertyChangeType::Interactive, ObjectsView);
		FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);

		URigVMController* Controller = nullptr;
		if(BlueprintBeingCustomized && GraphBeingCustomized)
		{
			Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
			if(bIsCommit)
			{
				Controller->OpenUndoBracket(FString::Printf(TEXT("Set %s"), *InPropertyHandle->GetProperty()->GetName()));
			}
		}

		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TWeakObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if(Object.Get() && InPropertyHandle->IsValidHandle())
			{
				TransformType& Transform = ContainerMemoryBlockToValueRef<TransformType>((uint8*)Object.Get(), Identity, PropertyChain, PropertyArrayIndices);
				TransformType PreviousTransform = Transform;
				
				SAdvancedTransformInputBox<TransformType>::ApplyNumericValueChange(
					Transform,
					InValue,
					InTransformComponent,
					InRotationRepresentation,
					InSubComponent);

				if(!PreviousTransform.Equals(Transform))
				{
					Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
					InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
				}
			}
		}

		if(Controller && bIsCommit)
		{
			Controller->CloseUndoBracket();
		}
	};

	WidgetArgs.OnNumericValueChanged_Lambda([OnNumericValueChanged](
		ESlateTransformComponent::Type InTransformComponent, 
		ESlateRotationRepresentation::Type InRotationRepresentation, 
		ESlateTransformSubComponent::Type InSubComponent,
		FReal InValue)
	{
		OnNumericValueChanged(InTransformComponent, InRotationRepresentation, InSubComponent, InValue, false);
	});

	WidgetArgs.OnNumericValueCommitted_Lambda([OnNumericValueChanged](
		ESlateTransformComponent::Type InTransformComponent, 
		ESlateRotationRepresentation::Type InRotationRepresentation, 
		ESlateTransformSubComponent::Type InSubComponent,
		FReal InValue, 
		ETextCommit::Type InCommitType)
	{
		OnNumericValueChanged(InTransformComponent, InRotationRepresentation, InSubComponent, InValue, true, InCommitType);
	});

	WidgetArgs.OnResetToDefault_Lambda([this, DefaultValue, InPropertyHandle](ESlateTransformComponent::Type InTransformComponent)
	{
		if (ObjectsBeingCustomized.IsEmpty())
		{
			return;
		}
		
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return;
		}
		
		URigVMController* Controller = nullptr;
		if(BlueprintBeingCustomized && GraphBeingCustomized)
		{
			Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
			if(Controller)
			{
				Controller->OpenUndoBracket(FString::Printf(TEXT("Reset %s to Default"), *InPropertyHandle->GetProperty()->GetName()));
			}
		}

		TArray<UObject*> ObjectsView;
		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TWeakObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if (Object.Get())
			{
				ObjectsView.Add(Object.Get());
			}
		}
		FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), EPropertyChangeType::ValueSet, ObjectsView);
		FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);
		
		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TWeakObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if(Object.Get() && InPropertyHandle->IsValidHandle())
			{
				TransformType& Transform = ContainerMemoryBlockToValueRef<TransformType>((uint8*)Object.Get(), Identity, PropertyChain, PropertyArrayIndices);
				TransformType PreviousTransform = Transform;

				switch(InTransformComponent)
				{
					case ESlateTransformComponent::Location:
					{
						Transform.SetLocation(DefaultValue.GetLocation());
						break;
					}
					case ESlateTransformComponent::Rotation:
					{
						Transform.SetRotation(DefaultValue.GetRotation());
						break;
					}
					case ESlateTransformComponent::Scale:
					{
						Transform.SetScale3D(DefaultValue.GetScale3D());
						break;
					}
					case ESlateTransformComponent::Max:
					default:
					{
						Transform.SetLocation(DefaultValue.GetLocation());
						break;
					}
				}

				
				if(!PreviousTransform.Equals(Transform))
				{
					Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
					InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
				}
			}
		}

		if(Controller)
		{
			Controller->CloseUndoBracket();
		}
	});

	WidgetArgs.OnCopyToClipboard_Lambda([this, InPropertyHandle](
		ESlateTransformComponent::Type InComponent
		)
	{
		TOptional<FReal> Result;
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return;
		}

		const TArray<uint8*> MemoryBlocks = GetMemoryBeingCustomized();
		for(uint8* MemoryBlock: MemoryBlocks)
		{
			if(MemoryBlock)
			{
				const TransformType& Transform = ContainerMemoryBlockToValueRef<TransformType>(MemoryBlock, Identity, PropertyChain, PropertyArrayIndices);
				FString Content;
				switch(InComponent)
				{
					case ESlateTransformComponent::Location:
					{
						const FVector Data = Transform.GetLocation();
						TBaseStructure<FVector>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
						break;
					}
					case ESlateTransformComponent::Rotation:
					{
						const FRotator Data = Transform.Rotator();
						TBaseStructure<FRotator>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
						break;
					}
					case ESlateTransformComponent::Scale:
					{
						const FVector Data = Transform.GetScale3D();
						TBaseStructure<FVector>::Get()->ExportText(Content, &Data, &Data, nullptr, PPF_None, nullptr);
						break;
					}
					case ESlateTransformComponent::Max:
					default:
					{
						TBaseStructure<TransformType>::Get()->ExportText(Content, &Transform, &Transform, nullptr, PPF_None, nullptr);
						break;
					}
				}

				if(!Content.IsEmpty())
				{
					FPlatformApplicationMisc::ClipboardCopy(*Content);
				}
			}
		}
	});

	WidgetArgs.OnPasteFromClipboard_Lambda([this, InPropertyHandle, OnNumericValueChanged](
		ESlateTransformComponent::Type InComponent
		)
	{
		FString Content;
		FPlatformApplicationMisc::ClipboardPaste(Content);
	
		if(Content.IsEmpty())
		{
			return;
		}

		if (ObjectsBeingCustomized.IsEmpty())
		{
			return;
		}

		TOptional<FReal> Result;
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return;
		}

		TArray<UObject*> ObjectsView;
		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TWeakObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if (Object.Get())
			{
				ObjectsView.Add(Object.Get());
			}
		}
		FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), EPropertyChangeType::ValueSet, ObjectsView);
		FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);

		URigVMController* Controller = nullptr;
		if(BlueprintBeingCustomized && GraphBeingCustomized)
		{
			Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
			Controller->OpenUndoBracket(FString::Printf(TEXT("Set %s"), *InPropertyHandle->GetProperty()->GetName()));
		}
		
		for(const TWeakObjectPtr<UObject>& Object : ObjectsBeingCustomized)
		{
			if(Object.Get() && InPropertyHandle->IsValidHandle())
			{
				TransformType& Transform = ContainerMemoryBlockToValueRef<TransformType>((uint8*)Object.Get(), Identity, PropertyChain, PropertyArrayIndices);
				const TransformType PreviousTransform = Transform;

				// Apply the new value
				{
					class FRigPasteTransformWidgetErrorPipe : public FOutputDevice
					{
					public:
				
						int32 NumErrors;
				
						FRigPasteTransformWidgetErrorPipe()
							: FOutputDevice()
							, NumErrors(0)
						{
						}
				
						virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
						{
							UE_LOG(LogRigVM, Error, TEXT("Error Pasting to Widget: %s"), V);
							NumErrors++;
						}
					};
				
					FRigPasteTransformWidgetErrorPipe ErrorPipe;
					
					switch(InComponent)
					{
						case ESlateTransformComponent::Location:
						{
							FVector Data = Transform.GetLocation();
							TBaseStructure<FVector>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FVector>::Get()->GetName(), true);
							Transform.SetLocation(Data);
							break;
						}
						case ESlateTransformComponent::Rotation:
						{
							FRotator Data = Transform.Rotator();
							TBaseStructure<FRotator>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FRotator>::Get()->GetName(), true);
							FQuat Quat = Data.Quaternion();
							Transform.SetRotation(Quat);
							
							break;
						}
						case ESlateTransformComponent::Scale:
						{
							FVector Data = Transform.GetScale3D();
							TBaseStructure<FVector>::Get()->ImportText(*Content, &Data, nullptr, PPF_None, &ErrorPipe, TBaseStructure<FVector>::Get()->GetName(), true);
							Transform.SetScale3D(Data);
							break;
						}
						case ESlateTransformComponent::Max:
						default:
						{
							TBaseStructure<TransformType>::Get()->ImportText(*Content, &Transform, nullptr, PPF_None, &ErrorPipe, TBaseStructure<TransformType>::Get()->GetName(), true);
							break;
						}
					}
				
					if(ErrorPipe.NumErrors == 0 && !PreviousTransform.Equals(Transform))
					{
						Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
						InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
					}
				}
			}
		}
		
		if (Controller)
		{
			Controller->CloseUndoBracket();
		}
	});
}

template<typename TransformType>
void FRigVMGraphMathTypeDetailCustomization::MakeTransformHeaderRow(TSharedRef<class IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	typename SAdvancedTransformInputBox<TransformType>::FArguments WidgetArgs;
	ConfigureTransformWidgetArgs<TransformType>(InPropertyHandle, WidgetArgs);

	SAdvancedTransformInputBox<TransformType>::ConfigureHeader(HeaderRow, InPropertyHandle->GetPropertyDisplayName(), InPropertyHandle->GetToolTipText(), WidgetArgs);
	SAdvancedTransformInputBox<TransformType>::ConfigureComponentWidgetRow(HeaderRow, ESlateTransformComponent::Max, WidgetArgs);
}
	
template<typename TransformType>
void FRigVMGraphMathTypeDetailCustomization::MakeTransformChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FDetailWidgetRow* LocationRow = nullptr;
	FDetailWidgetRow* RotationRow = nullptr;
	FDetailWidgetRow* ScaleRow = nullptr;

	typename SAdvancedTransformInputBox<TransformType>::FArguments WidgetArgs;
	ConfigureTransformWidgetArgs<TransformType>(InPropertyHandle, WidgetArgs);

	const FString StandaloneWidgetMetadata = InPropertyHandle->GetMetaData(TEXT("SStandaloneCustomizedValueWidget"));
	const bool IsUsingStandaloneWidget = StandaloneWidgetMetadata.Equals(TEXT("True"));
	
	LocationRow = &StructBuilder.AddCustomRow(LOCTEXT("TransformLocation", "Location"));
	RotationRow = &StructBuilder.AddCustomRow(LOCTEXT("TransformRotation", "Rotation"));
	ScaleRow = &StructBuilder.AddCustomRow(LOCTEXT("TransformScale", "Scale"));
	
	SAdvancedTransformInputBox<TransformType>::ConfigureComponentWidgetRow(*LocationRow, ESlateTransformComponent::Location, WidgetArgs);
	SAdvancedTransformInputBox<TransformType>::ConfigureComponentWidgetRow(*RotationRow, ESlateTransformComponent::Rotation, WidgetArgs);
	SAdvancedTransformInputBox<TransformType>::ConfigureComponentWidgetRow(*ScaleRow, ESlateTransformComponent::Scale, WidgetArgs);
}

FRigVMGraphMathTypeDetailCustomization::FRigVMGraphMathTypeDetailCustomization()
: ScriptStruct(nullptr)
, BlueprintBeingCustomized(nullptr)
, GraphBeingCustomized(nullptr)
{
}

void FRigVMGraphMathTypeDetailCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle,
                                                           FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> Objects;
	InPropertyHandle->GetOuterObjects(Objects);

	StructsBeingCustomized.Reset();
	InPropertyHandle->GetOuterStructs(StructsBeingCustomized);

	for (UObject* Object : Objects)
	{
		ObjectsBeingCustomized.Add(Object);

		if(BlueprintBeingCustomized == nullptr)
		{
			BlueprintBeingCustomized = Object->GetTypedOuter<URigVMBlueprint>();
		}

		if(GraphBeingCustomized == nullptr)
		{
			GraphBeingCustomized = Object->GetTypedOuter<URigVMGraph>();
		}
	}

	FProperty* Property = InPropertyHandle->GetProperty();
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	ScriptStruct = StructProperty->Struct;

	if(ScriptStruct == TBaseStructure<FVector>::Get())
	{
		MakeVectorHeaderRow<FVector, 3>(InPropertyHandle, HeaderRow, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FVector2D>::Get())
	{
		MakeVectorHeaderRow<FVector2D, 2>(InPropertyHandle, HeaderRow, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FVector4>::Get())
	{
		MakeVectorHeaderRow<FVector4, 4>(InPropertyHandle, HeaderRow, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FRotator>::Get())
	{
		MakeRotationHeaderRow<FRotator>(InPropertyHandle, HeaderRow, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FQuat>::Get())
	{
		MakeRotationHeaderRow<FQuat>(InPropertyHandle, HeaderRow, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FTransform>::Get())
	{
		MakeTransformHeaderRow<FTransform>(InPropertyHandle, HeaderRow, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FEulerTransform>::Get())
	{
		MakeTransformHeaderRow<FEulerTransform>(InPropertyHandle, HeaderRow, StructCustomizationUtils);
	}
}

void FRigVMGraphMathTypeDetailCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if(!InPropertyHandle->IsValidHandle())
	{
		return;
	}
	
	TArray<UObject*> Objects;
	InPropertyHandle->GetOuterObjects(Objects);

	for (UObject* Object : Objects)
	{
		ObjectsBeingCustomized.Add(Object);

		if(BlueprintBeingCustomized == nullptr)
		{
			BlueprintBeingCustomized = Object->GetTypedOuter<URigVMBlueprint>();
		}

		if(GraphBeingCustomized == nullptr)
		{
			GraphBeingCustomized = Object->GetTypedOuter<URigVMGraph>();
		}
	}

	StructsBeingCustomized.Reset();
	InPropertyHandle->GetOuterStructs(StructsBeingCustomized);

	FProperty* Property = InPropertyHandle->GetProperty();
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	ScriptStruct = StructProperty->Struct;

	if(ScriptStruct == TBaseStructure<FTransform>::Get())
	{
		MakeTransformChildren<FTransform>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FEulerTransform>::Get())
	{
		MakeTransformChildren<FEulerTransform>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
}

#undef LOCTEXT_NAMESPACE
