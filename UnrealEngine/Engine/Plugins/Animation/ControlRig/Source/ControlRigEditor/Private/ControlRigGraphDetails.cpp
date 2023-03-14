// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigGraphDetails.h"
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
#include "Graph/ControlRigGraphNode.h"
#include "ControlRig.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Graph/ControlRigGraphSchema.h"
#include "EditorCategoryUtils.h"
#include "IPropertyUtilities.h"
#include "Graph/SControlRigGraphPinVariableBinding.h"

#define LOCTEXT_NAMESPACE "ControlRigGraphDetails"

static const FText ControlRigGraphDetailsMultipleValues = LOCTEXT("MultipleValues", "Multiple Values");

FControlRigArgumentGroupLayout::FControlRigArgumentGroupLayout(
	URigVMGraph* InGraph, 
	UControlRigBlueprint* InBlueprint, 
	TWeakPtr<IControlRigEditor> InEditor,
	bool bInputs)
	: GraphPtr(InGraph)
	, ControlRigBlueprintPtr(InBlueprint)
	, ControlRigEditorPtr(InEditor)
	, bIsInputGroup(bInputs)
{
	if (ControlRigBlueprintPtr.IsValid())
	{
		ControlRigBlueprintPtr.Get()->OnModified().AddRaw(this, &FControlRigArgumentGroupLayout::HandleModifiedEvent);
	}
}

FControlRigArgumentGroupLayout::~FControlRigArgumentGroupLayout()
{
	if (ControlRigBlueprintPtr.IsValid())
	{
		ControlRigBlueprintPtr.Get()->OnModified().RemoveAll(this);
	}
}

void FControlRigArgumentGroupLayout::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
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
					TSharedRef<class FControlRigArgumentLayout> ControlRigArgumentLayout = MakeShareable(new FControlRigArgumentLayout(
						Pin,
						Graph,
						ControlRigBlueprintPtr.Get(),
						ControlRigEditorPtr
					));
					ChildrenBuilder.AddCustomBuilder(ControlRigArgumentLayout);
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
				.Text(LOCTEXT("NoArgumentsAddedForControlRig", "Please press the + icon above to add parameters"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}
}

void FControlRigArgumentGroupLayout::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
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

class FControlRigArgumentPinTypeSelectorFilter : public IPinTypeSelectorFilter
{
public:
	FControlRigArgumentPinTypeSelectorFilter(TWeakPtr<IControlRigEditor> InControlRigEditor, TWeakObjectPtr<URigVMGraph> InGraph)
		: ControlRigEditorPtr(InControlRigEditor), GraphPtr(InGraph)
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
		
		if (ControlRigEditorPtr.IsValid())
		{
			TArray<TSharedPtr<IPinTypeSelectorFilter>> Filters;
			ControlRigEditorPtr.Pin()->GetPinTypeSelectorFilters(Filters);
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

	TWeakPtr<IControlRigEditor> ControlRigEditorPtr;
	
	TWeakObjectPtr<URigVMGraph> GraphPtr;
};

void FControlRigArgumentLayout::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	const UEdGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();

	ETypeTreeFilter TypeTreeFilter = ETypeTreeFilter::None;
	TypeTreeFilter |= ETypeTreeFilter::AllowExec;

	TArray<TSharedPtr<IPinTypeSelectorFilter>> CustomPinTypeFilters;
	if (ControlRigEditorPtr.IsValid())
	{
		CustomPinTypeFilters.Add(MakeShared<FControlRigArgumentPinTypeSelectorFilter>(ControlRigEditorPtr, GraphPtr));
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
				.Text(this, &FControlRigArgumentLayout::OnGetArgNameText)
				.OnTextCommitted(this, &FControlRigArgumentLayout::OnArgNameTextCommitted)
				.ToolTipText(this, &FControlRigArgumentLayout::OnGetArgToolTipText)
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
				.TargetPinType(this, &FControlRigArgumentLayout::OnGetPinInfo)
				.OnPinTypePreChanged(this, &FControlRigArgumentLayout::OnPrePinInfoChange)
				.OnPinTypeChanged(this, &FControlRigArgumentLayout::PinInfoChanged)
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
				.IsEnabled_Raw(this, &FControlRigArgumentLayout::CanArgumentBeMoved, true)
				.OnClicked(this, &FControlRigArgumentLayout::OnArgMoveUp)
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
				.IsEnabled_Raw(this, &FControlRigArgumentLayout::CanArgumentBeMoved, false)
				.OnClicked(this, &FControlRigArgumentLayout::OnArgMoveDown)
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
				PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FControlRigArgumentLayout::OnRemoveClicked), LOCTEXT("FunctionArgDetailsClearTooltip", "Remove this parameter."), !IsPinEditingReadOnly())
			]
		];
}

void FControlRigArgumentLayout::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	// we don't show defaults here - we rely on a SControlRigGraphNode widget in the top of the details
}

void FControlRigArgumentLayout::OnRemoveClicked()
{
	if (PinPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		URigVMPin* Pin = PinPtr.Get();
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Pin->GetNode()))
		{
			if (URigVMController* Controller = Blueprint->GetController(LibraryNode->GetContainedGraph()))
			{
				Controller->RemoveExposedPin(Pin->GetFName(), true, true);
			}
		}
	}
}

FReply FControlRigArgumentLayout::OnArgMoveUp()
{
	if (PinPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		URigVMPin* Pin = PinPtr.Get();
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

FReply FControlRigArgumentLayout::OnArgMoveDown()
{
	if (PinPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		URigVMPin* Pin = PinPtr.Get();
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

bool FControlRigArgumentLayout::ShouldPinBeReadOnly(bool bIsEditingPinType/* = false*/) const
{
	return IsPinEditingReadOnly(bIsEditingPinType);
}

bool FControlRigArgumentLayout::IsPinEditingReadOnly(bool bIsEditingPinType/* = false*/) const
{
	return false;
}

bool FControlRigArgumentLayout::CanArgumentBeMoved(bool bMoveUp) const
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

FText FControlRigArgumentLayout::OnGetArgNameText() const
{
	if (PinPtr.IsValid())
	{
		return FText::FromName(PinPtr.Get()->GetFName());
	}
	return FText();
}

FText FControlRigArgumentLayout::OnGetArgToolTipText() const
{
	return OnGetArgNameText(); // for now since we don't have tooltips
}

void FControlRigArgumentLayout::OnArgNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	if (InTextCommit == ETextCommit::OnEnter)
	{
		if (!NewText.IsEmpty() && PinPtr.IsValid() && ControlRigBlueprintPtr.IsValid() && !ShouldPinBeReadOnly())
		{
			URigVMPin* Pin = PinPtr.Get();
			UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

FEdGraphPinType FControlRigArgumentLayout::OnGetPinInfo() const
{
	if (PinPtr.IsValid())
	{
		return UControlRigGraphNode::GetPinTypeForModelPin(PinPtr.Get());
	}
	return FEdGraphPinType();
}

void FControlRigArgumentLayout::PinInfoChanged(const FEdGraphPinType& PinType)
{
	if (PinPtr.IsValid() && ControlRigBlueprintPtr.IsValid() && FBlueprintEditorUtils::IsPinTypeValid(PinType))
	{
		URigVMPin* Pin = PinPtr.Get();
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

void FControlRigArgumentLayout::OnPrePinInfoChange(const FEdGraphPinType& PinType)
{
	// not needed for Control Rig
}

FControlRigArgumentDefaultNode::FControlRigArgumentDefaultNode(
	URigVMGraph* InGraph,
	UControlRigBlueprint* InBlueprint
)
	: GraphPtr(InGraph)
	, ControlRigBlueprintPtr(InBlueprint)
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		ControlRigBlueprintPtr.Get()->OnModified().AddRaw(this, &FControlRigArgumentDefaultNode::HandleModifiedEvent);

		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(GraphPtr->GetOuter()))
		{
			if (UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ControlRigBlueprintPtr->GetEdGraph(LibraryNode->GetGraph())))
			{
				EdGraphOuterPtr = RigGraph;
				GraphChangedDelegateHandle = RigGraph->AddOnGraphChangedHandler(
					FOnGraphChanged::FDelegate::CreateRaw(this, &FControlRigArgumentDefaultNode::OnGraphChanged)
				);
			}
		}	

	}
}

FControlRigArgumentDefaultNode::~FControlRigArgumentDefaultNode()
{
	if (ControlRigBlueprintPtr.IsValid())
	{
		ControlRigBlueprintPtr.Get()->OnModified().RemoveAll(this);
	}
	
	if (EdGraphOuterPtr.IsValid())
	{
		if (GraphChangedDelegateHandle.IsValid())
		{
			EdGraphOuterPtr->RemoveOnGraphChangedHandler(GraphChangedDelegateHandle);
		}		
	}
}

void FControlRigArgumentDefaultNode::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	if (!GraphPtr.IsValid() || !ControlRigBlueprintPtr.IsValid())
	{
		return;
	}

	UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
	URigVMGraph* Graph = GraphPtr.Get();
	UControlRigGraphNode* ControlRigGraphNode = nullptr;
	if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Graph->GetOuter()))
	{
		if (UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Blueprint->GetEdGraph(LibraryNode->GetGraph())))
		{
			ControlRigGraphNode = Cast<UControlRigGraphNode>(RigGraph->FindNodeForModelNodeName(LibraryNode->GetFName()));
		}
	}

	if (ControlRigGraphNode == nullptr)
	{
		return;
	}

	ChildrenBuilder.AddCustomRow(FText::GetEmpty())
	.WholeRowContent()
	.MaxDesiredWidth(980.f)
	[
		SAssignNew(OwnedNodeWidget, SControlRigGraphNode).GraphNodeObj(ControlRigGraphNode)
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

void FControlRigArgumentDefaultNode::OnGraphChanged(const FEdGraphEditAction& InAction)
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		OnRebuildChildren.ExecuteIfBound();
	}
}

void FControlRigArgumentDefaultNode::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
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

TSharedPtr<IDetailCustomization> FControlRigGraphDetails::MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor)
{
	const TArray<UObject*>* Objects = (InBlueprintEditor.IsValid() ? InBlueprintEditor->GetObjectsCurrentlyBeingEdited() : nullptr);
	if (Objects && Objects->Num() == 1)
	{
		if (UControlRigBlueprint* ControlRigBlueprint = Cast<UControlRigBlueprint>((*Objects)[0]))
		{
			return MakeShareable(new FControlRigGraphDetails(StaticCastSharedPtr<IControlRigEditor>(InBlueprintEditor), ControlRigBlueprint));
		}
	}

	return nullptr;
}

void FControlRigGraphDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	bIsPickingColor = false;

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailLayout.GetObjectsBeingCustomized(Objects);

	GraphPtr = CastChecked<UControlRigGraph>(Objects[0].Get());
	UControlRigGraph* Graph = GraphPtr.Get();

	UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

	IDetailCategoryBuilder& InputsCategory = DetailLayout.EditCategory("Inputs", LOCTEXT("FunctionDetailsInputs", "Inputs"));
	TSharedRef<FControlRigArgumentGroupLayout> InputArgumentGroup = MakeShareable(new FControlRigArgumentGroupLayout(
		Model, 
		Blueprint,
		ControlRigEditorPtr,
		true));
	InputsCategory.AddCustomBuilder(InputArgumentGroup);

	TSharedRef<SHorizontalBox> InputsHeaderContentWidget = SNew(SHorizontalBox);

	InputsHeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(1, 0))
		.OnClicked(this, &FControlRigGraphDetails::OnAddNewInputClicked)
		.Visibility(this, &FControlRigGraphDetails::GetAddNewInputOutputVisibility)
		.HAlign(HAlign_Right)
		.ToolTipText(LOCTEXT("FunctionNewInputArgTooltip", "Create a new input argument"))
		.VAlign(VAlign_Center)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FunctionNewInputArg")))
		.IsEnabled(this, &FControlRigGraphDetails::IsAddNewInputOutputEnabled)
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
	InputsCategory.HeaderContent(InputsHeaderContentWidget);

	IDetailCategoryBuilder& OutputsCategory = DetailLayout.EditCategory("Outputs", LOCTEXT("FunctionDetailsOutputs", "Outputs"));
	TSharedRef<FControlRigArgumentGroupLayout> OutputArgumentGroup = MakeShareable(new FControlRigArgumentGroupLayout(
		Model, 
		Blueprint,
		ControlRigEditorPtr,
		false));
	OutputsCategory.AddCustomBuilder(OutputArgumentGroup);

	TSharedRef<SHorizontalBox> OutputsHeaderContentWidget = SNew(SHorizontalBox);

	OutputsHeaderContentWidget->AddSlot()
	.HAlign(HAlign_Right)
	[
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ContentPadding(FMargin(1, 0))
		.OnClicked(this, &FControlRigGraphDetails::OnAddNewOutputClicked)
		.Visibility(this, &FControlRigGraphDetails::GetAddNewInputOutputVisibility)
		.HAlign(HAlign_Right)
		.ToolTipText(LOCTEXT("FunctionNewOutputArgTooltip", "Create a new output argument"))
		.VAlign(VAlign_Center)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("FunctionNewOutputArg")))
		.IsEnabled(this, &FControlRigGraphDetails::IsAddNewInputOutputEnabled)
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	];
	OutputsCategory.HeaderContent(OutputsHeaderContentWidget);

	IDetailCategoryBuilder& SettingsCategory = DetailLayout.EditCategory("NodeSettings", LOCTEXT("FunctionDetailsNodeSettings", "Node Settings"));

	bool bIsFunction = false;
	if (Model)
	{
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(Model->GetOuter()))
		{
			bIsFunction = LibraryNode->GetGraph()->IsA<URigVMFunctionLibrary>();
		}
	}

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
			.Text(this, &FControlRigGraphDetails::GetNodeCategory)
			.OnTextCommitted(this, &FControlRigGraphDetails::SetNodeCategory)
			.OnVerifyTextChanged_Lambda([&](const FText& InNewText, FText& OutErrorMessage) -> bool
			{
				const FText NewText = FEditorCategoryUtils::GetCategoryDisplayString(InNewText);
				if (NewText.ToString().Len() >= NAME_SIZE)
				{
					OutErrorMessage = LOCTEXT("CategoryTooLong", "Name of category is too long.");
					return false;
				}
				
				if (ControlRigBlueprintPtr.IsValid())
				{
					if (NewText.EqualTo(FText::FromString(ControlRigBlueprintPtr.Get()->GetName())))
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
			.Text(this, &FControlRigGraphDetails::GetNodeKeywords)
			.OnTextCommitted(this, &FControlRigGraphDetails::SetNodeKeywords)
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
			.Text(this, &FControlRigGraphDetails::GetNodeDescription)
			.OnTextCommitted(this, &FControlRigGraphDetails::SetNodeDescription)
		];

		if(AccessSpecifierStrings.IsEmpty())
		{
			AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("Public"))));
			AccessSpecifierStrings.Add(TSharedPtr<FString>(new FString(TEXT("Private"))));
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
                    .Text(this, &FControlRigGraphDetails::GetCurrentAccessSpecifierName)
                    .Font( IDetailLayoutBuilder::GetDetailFont() )
            ]
            .MenuContent()
            [
                SNew(SListView<TSharedPtr<FString> >)
                    .ListItemsSource( &AccessSpecifierStrings )
                    .OnGenerateRow(this, &FControlRigGraphDetails::HandleGenerateRowAccessSpecifier)
                    .OnSelectionChanged(this, &FControlRigGraphDetails::OnAccessSpecifierSelected)
            ]
        ];
	}

	// node color
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
		.OnClicked(this, &FControlRigGraphDetails::OnNodeColorClicked)
		[
			SAssignNew(ColorBlock, SColorBlock)
			.Color(this, &FControlRigGraphDetails::GetNodeColor)
			.Size(FVector2D(77, 16))
		]
	];

	IDetailCategoryBuilder& DefaultsCategory = DetailLayout.EditCategory("NodeDefaults", LOCTEXT("FunctionDetailsNodeDefaults", "Node Defaults"));
	TSharedRef<FControlRigArgumentDefaultNode> DefaultsArgumentNode = MakeShareable(new FControlRigArgumentDefaultNode(
		Model,
		Blueprint));
	DefaultsCategory.AddCustomBuilder(DefaultsArgumentNode);

}

bool FControlRigGraphDetails::IsAddNewInputOutputEnabled() const
{
	return true;
}

EVisibility FControlRigGraphDetails::GetAddNewInputOutputVisibility() const
{
	return EVisibility::Visible;
}

FReply FControlRigGraphDetails::OnAddNewInputClicked()
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

FReply FControlRigGraphDetails::OnAddNewOutputClicked()
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

FText FControlRigGraphDetails::GetNodeCategory() const
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

void FControlRigGraphDetails::SetNodeCategory(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

FText FControlRigGraphDetails::GetNodeKeywords() const
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

void FControlRigGraphDetails::SetNodeKeywords(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

FText FControlRigGraphDetails::GetNodeDescription() const
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

void FControlRigGraphDetails::SetNodeDescription(const FText& InNewText, ETextCommit::Type InCommitType)
{
	if(InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

FLinearColor FControlRigGraphDetails::GetNodeColor() const
{
	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
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

void FControlRigGraphDetails::SetNodeColor(FLinearColor InColor, bool bSetupUndoRedo)
{
	TargetColor = InColor;

	if (GraphPtr.IsValid() && ControlRigBlueprintPtr.IsValid())
	{
		UControlRigBlueprint* Blueprint = ControlRigBlueprintPtr.Get();
		if (URigVMGraph* Model = Blueprint->GetModel(GraphPtr.Get()))
		{
			if (URigVMCollapseNode* OuterNode = Cast<URigVMCollapseNode>(Model->GetOuter()))
			{
				if (URigVMController* Controller = Blueprint->GetOrCreateController(OuterNode->GetGraph()))
				{
					Controller->SetNodeColor(OuterNode, TargetColor, bSetupUndoRedo, bIsPickingColor, true);
				}
			}
		}
	}
}

void FControlRigGraphDetails::OnNodeColorBegin()
{
	bIsPickingColor = true;
}
void FControlRigGraphDetails::OnNodeColorEnd()
{ 
	bIsPickingColor = false; 
}

void FControlRigGraphDetails::OnNodeColorCancelled(FLinearColor OriginalColor)
{
	SetNodeColor(OriginalColor, true);
}

FReply FControlRigGraphDetails::OnNodeColorClicked()
{
	TargetColor = GetNodeColor();
	TargetColors.Reset();
	TargetColors.Add(&TargetColor);

	FColorPickerArgs PickerArgs;
	PickerArgs.ParentWidget = ColorBlock;
	PickerArgs.bUseAlpha = false;
	PickerArgs.DisplayGamma = false;
	PickerArgs.InitialColorOverride = TargetColor;
	PickerArgs.LinearColorArray = &TargetColors;
	PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP(this, &FControlRigGraphDetails::OnNodeColorBegin);
	PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP(this, &FControlRigGraphDetails::OnNodeColorEnd);
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FControlRigGraphDetails::SetNodeColor, true);
	PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &FControlRigGraphDetails::OnNodeColorCancelled);
	OpenColorPicker(PickerArgs);
	return FReply::Handled();
}

TArray<TSharedPtr<FString>> FControlRigGraphDetails::AccessSpecifierStrings;

FText FControlRigGraphDetails::GetCurrentAccessSpecifierName() const
{
	if(ControlRigBlueprintPtr.IsValid() && GraphPtr.IsValid())
	{
		UControlRigGraph* Graph = GraphPtr.Get();
		UControlRigBlueprint* ControlRigBlueprint = ControlRigBlueprintPtr.Get();

		const FControlRigPublicFunctionData ExpectedFunctionData = Graph->GetPublicFunctionData();
		if(ControlRigBlueprint->IsFunctionPublic(ExpectedFunctionData.Name))
		{
			return FText::FromString(*AccessSpecifierStrings[0].Get()); // public
		}
	}

	return FText::FromString(*AccessSpecifierStrings[1].Get()); // private
}

void FControlRigGraphDetails::OnAccessSpecifierSelected( TSharedPtr<FString> SpecifierName, ESelectInfo::Type SelectInfo )
{
	if(ControlRigBlueprintPtr.IsValid() && GraphPtr.IsValid())
	{
		UControlRigGraph* Graph = GraphPtr.Get();
		UControlRigBlueprint* ControlRigBlueprint = ControlRigBlueprintPtr.Get();
		const FControlRigPublicFunctionData ExpectedFunctionData = Graph->GetPublicFunctionData();
		
		if(SpecifierName->Equals(TEXT("Private")))
		{
			ControlRigBlueprint->MarkFunctionPublic(ExpectedFunctionData.Name, false);
		}
		else
		{
			ControlRigBlueprint->MarkFunctionPublic(ExpectedFunctionData.Name, true);
		}
	}
}

TSharedRef<ITableRow> FControlRigGraphDetails::HandleGenerateRowAccessSpecifier( TSharedPtr<FString> SpecifierName, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
        .Content()
        [
            SNew( STextBlock ) 
                .Text(FText::FromString(*SpecifierName.Get()) )
        ];
}

FControlRigWrappedNodeDetails::FControlRigWrappedNodeDetails()
: BlueprintBeingCustomized(nullptr)
{
}

TSharedRef<IDetailCustomization> FControlRigWrappedNodeDetails::MakeInstance()
{
	return MakeShareable(new FControlRigWrappedNodeDetails);
}

void FControlRigWrappedNodeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	TArray<TWeakObjectPtr<UObject>> DetailObjects;
	DetailLayout.GetObjectsBeingCustomized(DetailObjects);
	if (DetailObjects.Num() == 0)
	{
		return;
	}

	for(TWeakObjectPtr<UObject> DetailObject : DetailObjects)
	{
		UDetailsViewWrapperObject* WrapperObject = CastChecked<UDetailsViewWrapperObject>(DetailObject.Get());
		if(BlueprintBeingCustomized == nullptr)
		{
			BlueprintBeingCustomized = WrapperObject->GetTypedOuter<UControlRigBlueprint>();
		}

		ObjectsBeingCustomized.Add(WrapperObject);
		NodesBeingCustomized.Add(CastChecked<URigVMNode>(WrapperObject->GetOuter()));
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
					SNew(SControlRigVariableBinding)
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
        		UControlRigGraph* GraphBeingCustomized = Cast<UControlRigGraph>(
        			BlueprintBeingCustomized->GetEdGraph(NodesBeingCustomized[0]->GetGraph()));
        		ensure(GraphBeingCustomized);
        		
        		const TArray<TSharedPtr<FString>>* NameList = nullptr;
        		if (CustomWidgetName == TEXT("BoneName"))
        		{
        			NameList = GraphBeingCustomized->GetBoneNameList();
        		}
        		else if (CustomWidgetName == TEXT("ControlName"))
        		{
        			NameList = GraphBeingCustomized->GetControlNameListWithoutAnimationChannels();
        		}
        		else if (CustomWidgetName == TEXT("SpaceName"))
        		{
        			NameList = GraphBeingCustomized->GetNullNameList();
        		}
        		else if (CustomWidgetName == TEXT("CurveName"))
        		{
        			NameList = GraphBeingCustomized->GetCurveNameList();
        		}

        		if (NameList)
        		{
        			TSharedPtr<SControlRigGraphPinNameListValueWidget> NameListWidget;

        			if(IDetailPropertyRow* Row = DetailLayout.EditDefaultProperty(PropertyHandle))
        			{
        				Row->CustomWidget()
						.NameContent()
						[
							PropertyHandle->CreatePropertyNameWidget()
						]
						.ValueContent()
						[
							SAssignNew(NameListWidget, SControlRigGraphPinNameListValueWidget)
							.OptionsSource(NameList)
							.OnGenerateWidget(this, &FControlRigWrappedNodeDetails::MakeNameListItemWidget)
							.OnSelectionChanged(this, &FControlRigWrappedNodeDetails::OnNameListChanged, NameProperty, DetailLayout.GetPropertyUtilities())
							.OnComboBoxOpening(this, &FControlRigWrappedNodeDetails::OnNameListComboBox, NameProperty, NameList)
							.InitiallySelectedItem(GetCurrentlySelectedItem(NameProperty, NameList))
							.Content()
							[
								SNew(STextBlock)
								.Text(this, &FControlRigWrappedNodeDetails::GetNameListText, NameProperty)
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

TSharedRef<SWidget> FControlRigWrappedNodeDetails::MakeNameListItemWidget(TSharedPtr<FString> InItem)
{
	return 	SNew(STextBlock).Text(FText::FromString(*InItem));// .Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}

FText FControlRigWrappedNodeDetails::GetNameListText(FNameProperty* InProperty) const
{
	FText FirstText;
	for(TWeakObjectPtr<UDetailsViewWrapperObject> ObjectBeingCustomized : ObjectsBeingCustomized)
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
				return ControlRigGraphDetailsMultipleValues;
			}
		}
	}
	return FirstText;
}

TSharedPtr<FString> FControlRigWrappedNodeDetails::GetCurrentlySelectedItem(FNameProperty* InProperty, const TArray<TSharedPtr<FString>>* InNameList) const
{
	FString CurrentItem = GetNameListText(InProperty).ToString();
	for (const TSharedPtr<FString>& Item : *InNameList)
	{
		if (Item->Equals(CurrentItem))
		{
			return Item;
		}
	}
	return TSharedPtr<FString>();
}


void FControlRigWrappedNodeDetails::SetNameListText(const FText& NewTypeInValue, ETextCommit::Type, FNameProperty* InProperty, TSharedRef<IPropertyUtilities> PropertyUtilities)
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

void FControlRigWrappedNodeDetails::OnNameListChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo, FNameProperty* InProperty, TSharedRef<IPropertyUtilities> PropertyUtilities)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();
		SetNameListText(FText::FromString(NewValue), ETextCommit::OnEnter, InProperty, PropertyUtilities);
	}
}

void FControlRigWrappedNodeDetails::OnNameListComboBox(FNameProperty* InProperty, const TArray<TSharedPtr<FString>>* InNameList)
{
	TSharedPtr<SControlRigGraphPinNameListValueWidget> Widget = NameListWidgets.FindChecked(InProperty->GetFName());
	const TSharedPtr<FString> CurrentlySelected = GetCurrentlySelectedItem(InProperty, InNameList);
	Widget->SetSelectedItem(CurrentlySelected);
}

void FControlRigWrappedNodeDetails::CustomizeLiveValues(IDetailLayoutBuilder& DetailLayout)
{
	if(ObjectsBeingCustomized.Num() != 1)
	{
		return;
	}
	
	UControlRig* DebuggedRig = Cast<UControlRig>(BlueprintBeingCustomized->GetObjectBeingDebugged());
	if(DebuggedRig == nullptr)
	{
		return;
	}

	URigVM* VM = DebuggedRig->GetVM();
	if(VM == nullptr)
	{
		return;
	}

	UDetailsViewWrapperObject* FirstWrapper = ObjectsBeingCustomized[0].Get();
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
	
	IDetailCategoryBuilder& DebugCategory = DetailLayout.EditCategory("DebugLiveValues", LOCTEXT("DebugLiveValues", "Inspect Live Values"), ECategoryPriority::Uncommon);
	DebugCategory.InitiallyCollapsed(true);

	for(URigVMPin* Pin : FirstNode->GetPins())
	{
		// only show hidden pins in debug mode
		if(Pin->GetDirection() == ERigVMPinDirection::Hidden)
		{
			if(!DebuggedRig->IsInDebugMode())
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
				TArray<UObject*> ExternalObjects;

				if(Operand->GetMemoryType() == ERigVMMemoryType::External)
				{
					if(!VM->GetExternalVariables().IsValidIndex(Operand->GetRegisterIndex()))
					{
						continue;
					}
					ExternalObjects.Add(DebuggedRig);
					Property = VM->GetExternalVariables()[Operand->GetRegisterIndex()].Property; 
				}
				else
				{
					URigVMMemoryStorage* Memory = VM->GetMemoryByType(Operand->GetMemoryType());
					if(Memory == nullptr)
					{
						continue;
					}

					if(Memory->GetOuter() == GetTransientPackage())
					{
						continue;
					}

					// the UClass must be alive for the details view to access it
					// this ensure can fail if VM memory is not updated immediately after compile
					// because of deferred copy
					if(!ensure(IsValidChecked(Memory->GetClass())))
					{
						continue;
					}

					if(Memory->GetClass()->GetOuter() == GetTransientPackage())
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

					ExternalObjects.Add(Memory);
				}

				check(ExternalObjects.Num() > 0);
				check(Property);

				IDetailPropertyRow* PropertyRow = DebugCategory.AddExternalObjectProperty(ExternalObjects, Property->GetFName(), EPropertyLocation::Default, FAddPropertyParams().ForceShowProperty());
				if(PropertyRow)
				{
					PropertyRow->DisplayName(FText::FromString(FString::Printf(TEXT("%s%s"), *Pin->GetName(), *NameSuffix)));
					PropertyRow->IsEnabled(false);

					SuffixIndex++;
					bAddedProperty = true;
					NameSuffix = FString::Printf(TEXT("_%d"), SuffixIndex);
				}

				KnownOperands.Add(*Operand);
			}
		}

		if(!bAddedProperty)
		{
			TSharedPtr<IPropertyHandle> PinHandle = DetailLayout.GetProperty(Pin->GetFName());
			if(PinHandle.IsValid())
			{
				DebugCategory.AddProperty(PinHandle)
				.DisplayName(FText::FromName(Pin->GetDisplayName()))
				.IsEnabled(false);
			}
		}
	}
}

FControlRigGraphMathTypeDetails::FControlRigGraphMathTypeDetails()
: ScriptStruct(nullptr)
, BlueprintBeingCustomized(nullptr)
, GraphBeingCustomized(nullptr)
{
}

void FControlRigGraphMathTypeDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle,
                                                           FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> Objects;
	InPropertyHandle->GetOuterObjects(Objects);

	for (UObject* Object : Objects)
	{
		ObjectsBeingCustomized.Add(Object);

		if(BlueprintBeingCustomized == nullptr)
		{
			BlueprintBeingCustomized = Object->GetTypedOuter<UControlRigBlueprint>();
		}

		if(GraphBeingCustomized == nullptr)
		{
			GraphBeingCustomized = Object->GetTypedOuter<URigVMGraph>();
		}
	}

	FProperty* Property = InPropertyHandle->GetProperty();
	const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
	ScriptStruct = StructProperty->Struct;
}

void FControlRigGraphMathTypeDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	if(!InPropertyHandle->IsValidHandle())
	{
		return;
	}

	if(ScriptStruct == TBaseStructure<FVector>::Get())
	{
		CustomizeVector<FVector, 3>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FVector2D>::Get())
	{
		CustomizeVector<FVector2D, 2>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FVector4>::Get())
	{
		CustomizeVector<FVector4, 4>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FRotator>::Get())
	{
		CustomizeRotation<FRotator>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FQuat>::Get())
	{
		CustomizeRotation<FQuat>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FTransform>::Get())
	{
		CustomizeTransform<FTransform>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
	else if(ScriptStruct == TBaseStructure<FEulerTransform>::Get())
	{
		CustomizeTransform<FEulerTransform>(InPropertyHandle, StructBuilder, StructCustomizationUtils);
	}
}

void FControlRigGraphMathTypeDetails::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ObjectsBeingCustomized);
}

FString FControlRigGraphMathTypeDetails::GetReferencerName() const
{
	return TEXT("FControlRigGraphMathTypeDetails:") + ObjectsBeingCustomized[0]->GetPathName();
}

#undef LOCTEXT_NAMESPACE
