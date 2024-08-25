// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/SAnimNextGraphNode.h"
#include "Graph/AnimNextGraph_EdGraphNode.h"
#include "SGraphPin.h"
#include "RigVMModel/Nodes/RigVMDispatchNode.h"
#include "DecoratorBase/DecoratorHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "SAnimNextGraphNode"

void SAnimNextGraphNode::Construct( const FArguments& InArgs )
{
	SRigVMGraphNode::Construct(SRigVMGraphNode::FArguments().GraphNodeObj(InArgs._GraphNodeObj));
}

void SAnimNextGraphNode::UpdatePinTreeView()
{
	static const float PinWidgetSidePadding = 6.f;
	static const float EmptySidePadding = 60.f;
	static const float TopPadding = 2.f; 
	static const float MaxHeight = 30.f;

	check(GraphNode);

	// remove all existing content in the Left
	LeftNodeBox->ClearChildren();

	URigVMEdGraphNode* RigGraphNode = Cast<URigVMEdGraphNode>(GraphNode);
	URigVMNode* Node = RigGraphNode->GetModelNode();
	const FRigVMTemplate* Template = nullptr;
	if (URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(Node))
	{
		Template = TemplateNode->GetTemplate();
	}

	TMap<UEdGraphPin*, int32> EdGraphPinToInputPin;
	for(int32 InputPinIndex = 0; InputPinIndex < InputPins.Num(); InputPinIndex++)
	{
		EdGraphPinToInputPin.Add(InputPins[InputPinIndex]->GetPinObj(), InputPinIndex);
	}
	TMap<UEdGraphPin*, int32> EdGraphPinToOutputPin;
	for(int32 OutputPinIndex = 0; OutputPinIndex < OutputPins.Num(); OutputPinIndex++)
	{
		EdGraphPinToOutputPin.Add(OutputPins[OutputPinIndex]->GetPinObj(), OutputPinIndex);
	}

	TArray<URigVMPin*> RootModelPins = ModelNode->GetPins();
	// orphaned pins are appended to the end of pin list on each side of the node
	RootModelPins.Append(ModelNode->GetOrphanedPins());
	TArray<URigVMPin*> ModelPins;

	const bool bSupportSubPins = !RigGraphNode->DrawAsCompactNode();

	// sort model pins
	// a) execute IOs, b) IO pins, c) input / visible pins, d) output pins
	struct Local
	{
		static void VisitPinRecursively(URigVMPin* InPin, TArray<URigVMPin*>& OutPins, bool bSupportSubPins)
		{
			OutPins.Add(InPin);
			if(!bSupportSubPins)
			{
				return;
			}

			if(InPin->ShouldHideSubPins())
			{
				return;
			}

			if (InPin->GetCPPType() == TEXT("FRotator"))
			{
				const TArray<URigVMPin*>& SubPins = InPin->GetSubPins();
				if (SubPins.Num() == 3)
				{
					OutPins.Add(SubPins[2]);	
					OutPins.Add(SubPins[0]);	
					OutPins.Add(SubPins[1]);
				}	
			}
			else
			{				
				for (URigVMPin* SubPin : InPin->GetSubPins())
				{
					VisitPinRecursively(SubPin, OutPins, bSupportSubPins);
				}
			}
		}
	};
	
	for(int32 SortPhase = 0; SortPhase < 4; SortPhase++)
	{
		for(URigVMPin* RootPin : RootModelPins)
		{
			switch (SortPhase)
			{
				case 0: // execute IO pins
				{
					if(RootPin->IsExecuteContext() && RootPin->GetDirection() == ERigVMPinDirection::IO)
					{
						Local::VisitPinRecursively(RootPin, ModelPins, bSupportSubPins);
					}
					break;
				}
				case 1: // IO pins
				{
					if(!RootPin->IsExecuteContext() && RootPin->GetDirection() == ERigVMPinDirection::IO)
					{
						Local::VisitPinRecursively(RootPin, ModelPins, bSupportSubPins);
					}
					break;
				}
				case 2: // input / visible pins
				{
					if(RootPin->GetDirection() == ERigVMPinDirection::Input || RootPin->GetDirection() == ERigVMPinDirection::Visible)
					{
						Local::VisitPinRecursively(RootPin, ModelPins, bSupportSubPins);
					}
					break;
				}
				case 3: // output pins
				default:
				{
					if(RootPin->GetDirection() == ERigVMPinDirection::Output)
					{
						Local::VisitPinRecursively(RootPin, ModelPins, bSupportSubPins);
					}
					break;
				}
			}
		}
	}

	const URigVMEdGraphSchema* RigSchema = Cast<URigVMEdGraphSchema>(RigGraphNode->GetSchema());

	FRigVMDispatchContext DispatchContext;
	if(const URigVMDispatchNode* DispatchNode = Cast<URigVMDispatchNode>(ModelNode))
	{
		DispatchContext = DispatchNode->GetDispatchContext();
	}
	
	TMap<URigVMPin*, int32> ModelPinToInfoIndex;
	for(URigVMPin* ModelPin : ModelPins)
	{
		FPinInfo PinInfo;
		PinInfo.Index = PinInfos.Num();
		PinInfo.ParentIndex = INDEX_NONE;
		PinInfo.bHasChildren = (ModelPin->GetSubPins().Num() > 0);
		PinInfo.bIsContainer = ModelPin->IsArray();
		PinInfo.Depth = 0;
		PinInfo.bExpanded = ModelPin->IsExpanded();
		PinInfo.ModelPinPath = ModelPin->GetPinPath();
		PinInfo.bAutoHeight = false;
		PinInfo.bShowOnlySubPins = ModelPin->ShouldOnlyShowSubPins();

		if(!bSupportSubPins)
		{
			PinInfo.bHasChildren = false;
			PinInfo.bIsContainer = false;
		}

		// Decorator handle pins have special packing that are not user editable, hide their innards
		if (ModelPin->GetCPPTypeObject() == FAnimNextDecoratorHandle::StaticStruct())
		{
			PinInfo.bHasChildren = false;
			PinInfo.bIsContainer = false;
		}
		
		const bool bAskSchemaForEdition = RigSchema && ModelPin->IsStruct() && !ModelPin->IsBoundToVariable();
		PinInfo.bHideInputWidget = (!ModelPin->IsBoundToVariable()) && PinInfo.bIsContainer;
		if (!PinInfo.bHideInputWidget)
		{
			if (bAskSchemaForEdition && !PinInfo.bHasChildren)
			{
				const bool bIsStructEditable = RigSchema->IsStructEditable(ModelPin->GetScriptStruct()); 
				PinInfo.bHideInputWidget = !bIsStructEditable;
				PinInfo.bAutoHeight = bIsStructEditable;
			}
			else if(PinInfo.bHasChildren && !ModelPin->IsBoundToVariable())
			{
				PinInfo.bHideInputWidget = true;
			}
		}
		
		if(URigVMPin* ParentPin = ModelPin->GetParentPin())
		{
			const int32* ParentIndexPtr = ModelPinToInfoIndex.Find(ParentPin);
			if(ParentIndexPtr == nullptr)
			{
				continue;
			}
			PinInfo.ParentIndex = *ParentIndexPtr;
			PinInfo.Depth = PinInfos[PinInfo.ParentIndex].Depth + 1;
			if(PinInfos[PinInfo.ParentIndex].bShowOnlySubPins)
			{
				PinInfo.Depth--;
			}
		}

		TAttribute<EVisibility> PinVisibilityAttribute = TAttribute<EVisibility>::CreateSP(this, &SAnimNextGraphNode::GetPinVisibility, PinInfo.Index, false);

		bool bPinWidgetForExpanderLeft = false;
		TSharedPtr<SGraphPin> PinWidgetForExpander;

		bool bPinInfoIsValid = false;
		if(UEdGraphPin* OutputEdGraphPin = RigGraphNode->FindPin(ModelPin->GetPinPath(), EEdGraphPinDirection::EGPD_Output))
		{
			if(const int32* PinIndexPtr = EdGraphPinToOutputPin.Find(OutputEdGraphPin))
			{
				PinInfo.OutputPinWidget = OutputPins[*PinIndexPtr];
				PinInfo.OutputPinWidget->SetVisibility(PinVisibilityAttribute);
				PinWidgetForExpander = PinInfo.OutputPinWidget;
				bPinWidgetForExpanderLeft = false;
				bPinInfoIsValid = true;
				if (Template && !ModelPin->IsExecuteContext())
				{
					if (URigVMPin* RootPin = ModelPin->GetRootPin())
					{
						FLinearColor PinColorAndOpacity = PinInfo.OutputPinWidget->GetColorAndOpacity();
						if (Template->FindArgument(RootPin->GetFName()) == nullptr && Template->FindExecuteArgument(RootPin->GetFName(), DispatchContext) == nullptr)
						{
							PinColorAndOpacity.A = 0.2f;
						}
						else
						{
							PinColorAndOpacity.A = 1.0f;
						}
						PinInfo.OutputPinWidget->SetColorAndOpacity(PinColorAndOpacity);
					}
				}
			}
		}
		
		if(UEdGraphPin* InputEdGraphPin = RigGraphNode->FindPin(ModelPin->GetPinPath(), EEdGraphPinDirection::EGPD_Input))
		{
			// Make sure to handle input pins that we explicitly hide, like decorator handles
			if (!InputEdGraphPin->bHidden)
			{
				if (const int32* PinIndexPtr = EdGraphPinToInputPin.Find(InputEdGraphPin))
				{
					PinInfo.InputPinWidget = InputPins[*PinIndexPtr];
					PinInfo.InputPinWidget->SetVisibility(PinVisibilityAttribute);
					PinWidgetForExpander = PinInfo.InputPinWidget;
					bPinWidgetForExpanderLeft = true;
					bPinInfoIsValid = true;
					if (Template && !ModelPin->IsExecuteContext())
					{
						if (URigVMPin* RootPin = ModelPin->GetRootPin())
						{
							FLinearColor PinColorAndOpacity = PinInfo.InputPinWidget->GetColorAndOpacity();
							if (Template->FindArgument(RootPin->GetFName()) == nullptr && Template->FindExecuteArgument(RootPin->GetFName(), DispatchContext) == nullptr)
							{
								PinColorAndOpacity.A = 0.2f;
							}
							else
							{
								PinColorAndOpacity.A = 1.0f;
							}
							PinInfo.InputPinWidget->SetColorAndOpacity(PinColorAndOpacity);
						}
					}
				}
			}
		}

		if(!bPinInfoIsValid)
		{
			continue;
		}

		ModelPinToInfoIndex.Add(ModelPin, PinInfos.Add(PinInfo));
		
		// check if this pin has sub pins
		TSharedPtr<SHorizontalBox> FullPinHorizontalRowWidget = PinWidgetForExpander->GetFullPinHorizontalRowWidget().Pin();
		if(FullPinHorizontalRowWidget.IsValid())
		{
			// indent the pin by padding
			const float DepthIndentation = 12.f * float(PinInfo.Depth + (PinInfo.bHasChildren ? 0 : 1));
			const float LeftIndentation = bPinWidgetForExpanderLeft ? DepthIndentation : 0.f;
			const float RightIndentation = bPinWidgetForExpanderLeft ? 0.f : DepthIndentation;
			const FMargin LineIndentation = FMargin(RightIndentation, 0, LeftIndentation, 0); 

			static const TSharedRef<FTagMetaData> ExpanderButtonMetadata = MakeShared<FTagMetaData>(TEXT("SRigVMGraphNode.ExpanderButton"));

			// check if this pin widget may already have the expander button 
			int32 ExpanderSlotIndex = INDEX_NONE;
			int32 PaddedSlotIndex = INDEX_NONE;

			for(int32 SlotIndex=0; SlotIndex<FullPinHorizontalRowWidget->NumSlots(); SlotIndex++)
			{
				const TSharedRef<SWidget> Widget = FullPinHorizontalRowWidget->GetSlot(SlotIndex).GetWidget();
				const TSharedPtr<FTagMetaData> Metadata = Widget->GetMetaData<FTagMetaData>();
				if(Metadata.IsValid())
				{
					if(Metadata->Tag == ExpanderButtonMetadata->Tag)
					{
						ExpanderSlotIndex = SlotIndex;
						break;
					}
				}
			}

			// The expander needs to be recreated to adjust for PinInfo.Index changes
			if (FullPinHorizontalRowWidget->IsValidSlotIndex(ExpanderSlotIndex))
			{
				SHorizontalBox::FSlot& Slot = FullPinHorizontalRowWidget->GetSlot(ExpanderSlotIndex);
				const TSharedRef<SWidget> Widget = Slot.GetWidget();
				Widget->RemoveMetaData(ExpanderButtonMetadata);
				FullPinHorizontalRowWidget->RemoveSlot(Widget);
				ExpanderSlotIndex = INDEX_NONE;
			}
	
			if(PinInfo.bHasChildren && ExpanderSlotIndex == INDEX_NONE)
			{
				// only inject the expander arrow for inputs on input / IO
				// or for output pins
				if(
					(
						(
							(ModelPin->GetDirection() == ERigVMPinDirection::Input) ||
							(ModelPin->GetDirection() == ERigVMPinDirection::IO)
						) &&
						bPinWidgetForExpanderLeft
					) ||
					(
						(ModelPin->GetDirection() == ERigVMPinDirection::Output) &&
						(!bPinWidgetForExpanderLeft)
					)
				)
				{
					// Add the expander arrow
					FullPinHorizontalRowWidget->InsertSlot(bPinWidgetForExpanderLeft ? 1 : FullPinHorizontalRowWidget->GetChildren()->Num() - 1)
					.Padding(FMargin(0, 0, 0, 0))
					.AutoWidth()
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
						.ContentPadding(0)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.ClickMethod( EButtonClickMethod::MouseDown )
						.OnClicked(this, &SAnimNextGraphNode::OnExpanderArrowClicked, PinInfo.Index)
						.ToolTipText(LOCTEXT("ExpandSubPin", "Expand Pin"))
						[
							SNew(SImage)
							.Image(this, &SAnimNextGraphNode::GetExpanderImage, PinInfo.Index, bPinWidgetForExpanderLeft, false)
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
						.AddMetaData(ExpanderButtonMetadata)
					];
				}
			}

			// adjust the padding
			{
				const int32 SlotToAdjustIndex = bPinWidgetForExpanderLeft ? 0 : FullPinHorizontalRowWidget->NumSlots() - 1;
				SHorizontalBox::FSlot& Slot = FullPinHorizontalRowWidget->GetSlot(SlotToAdjustIndex);
				Slot.SetPadding(LineIndentation);
			}
		}
	}

	// add spacer widget at the start
	LeftNodeBox->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	.AutoHeight()
	[
		SNew(SSpacer)
		.Size(FVector2D(1.f, 2.f))
	];

	auto AddArrayPlusButtonLambda = [this](URigVMPin* InModelPin, TSharedPtr<SHorizontalBox> InSlotLayout, const float InEmptySidePadding)
	{
		// add array plus button
		InSlotLayout->AddSlot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(PinWidgetSidePadding, TopPadding, InEmptySidePadding, 0.f)
		[
			SNew(SButton)
			.ContentPadding(0.0f)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.OnClicked(this, &SAnimNextGraphNode::HandleAddArrayElement, InModelPin->GetPinPath())
			.IsEnabled(this, &SGraphNode::IsNodeEditable)
			.Cursor(EMouseCursor::Default)
			.Visibility(this, &SAnimNextGraphNode::GetArrayPlusButtonVisibility, InModelPin)
			.ToolTipText(LOCTEXT("AddArrayElement", "Add Array Element"))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Icons.PlusCircle")))
				]
			]
		];
	};

	struct FPinRowInfo
	{
		int32 InputPinInfo;
		int32 OutputPinInfo;
		
		FPinRowInfo()
			: InputPinInfo(INDEX_NONE)
			, OutputPinInfo(INDEX_NONE)
		{}
	};

	TArray<FPinRowInfo> Rows;
	Rows.Reserve(PinInfos.Num());

	// build a tree with the pin infos in order as before
	for(const FPinInfo& PinInfo : PinInfos)
	{
		FPinRowInfo Row;
		if(PinInfo.InputPinWidget.IsValid())
		{
			Row.InputPinInfo = PinInfo.Index;
		}
		if(PinInfo.OutputPinWidget.IsValid())
		{
			Row.OutputPinInfo = PinInfo.Index;
		}
		Rows.Add(Row);
	}

	// compact the rows - re-use input rows for outputs
	// if their sub-pins count matches
	if(RigGraphNode->DrawAsCompactNode())
	{
		for(int32 RowIndex = 0; RowIndex < Rows.Num();)
		{
			if(Rows[RowIndex].OutputPinInfo == INDEX_NONE)
			{
				RowIndex++;
				continue;
			}

			bool bFoundMatch = false;
			for(int32 InputRowIndex = 0; InputRowIndex < RowIndex; InputRowIndex++)
			{
				if(Rows[InputRowIndex].OutputPinInfo == INDEX_NONE)
				{
					const FPinInfo& OutputPinInfo = PinInfos[Rows[RowIndex].OutputPinInfo];
					const FPinInfo& InputPinInfo = PinInfos[Rows[InputRowIndex].InputPinInfo];
					if(OutputPinInfo.bHasChildren == false &&
						InputPinInfo.bHasChildren == false)
					{
						Rows[InputRowIndex].OutputPinInfo = Rows[RowIndex].OutputPinInfo;
						bFoundMatch = true;
						break;
					}
				}
			}

			if(bFoundMatch)
			{
				Rows.RemoveAt(RowIndex);
			}
			else
			{
				RowIndex++;
			}
		}
	}
	
	for(const FPinRowInfo& Row : Rows)
	{
		const FPinInfo& InputPinInfo = PinInfos[Row.InputPinInfo == INDEX_NONE ? Row.OutputPinInfo : Row.InputPinInfo];
		const FPinInfo& OutputPinInfo = PinInfos[Row.OutputPinInfo == INDEX_NONE ? Row.InputPinInfo : Row.OutputPinInfo];

		if(InputPinInfo.InputPinWidget.IsValid())
		{
			if(InputPinInfo.bHideInputWidget)
			{
				if(InputPinInfo.InputPinWidget->GetValueWidget() != SNullWidget::NullWidget)
				{
					InputPinInfo.InputPinWidget->GetValueWidget()->SetVisibility(EVisibility::Collapsed);
				}
			}
				
			// input pins
			if(!OutputPinInfo.OutputPinWidget.IsValid())
			{
				TSharedPtr<SHorizontalBox> SlotLayout;
				SHorizontalBox::FSlot* FirstSlot = nullptr;

				const float MyEmptySidePadding = InputPinInfo.bHideInputWidget ? EmptySidePadding : 0.f; 
				
				LeftNodeBox->AddSlot()
                .HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.AutoHeight()
				.MaxHeight(InputPinInfo.bAutoHeight ? TAttribute<float>() : MaxHeight)
                [
                    SAssignNew(SlotLayout, SHorizontalBox)
                    .Visibility(this, &SAnimNextGraphNode::GetPinVisibility, InputPinInfo.Index, false)
		            
                    +SHorizontalBox::Slot()
                    .Expose(FirstSlot)
                    .FillWidth(1.f)
                    .HAlign(HAlign_Left)
                    .Padding(PinWidgetSidePadding, TopPadding, InputPinInfo.bIsContainer ? 0.f : MyEmptySidePadding, 0.f)
                    [
                        InputPinInfo.InputPinWidget.ToSharedRef()
                    ]
                ];

				if(InputPinInfo.bIsContainer)
				{
					URigVMPin* ModelPin = ModelNode->GetGraph()->FindPin(InputPinInfo.ModelPinPath);
					if(ModelPin)
					{
						// make sure to minimize the width of the label
						FirstSlot->SetAutoWidth();
						AddArrayPlusButtonLambda(ModelPin, SlotLayout, MyEmptySidePadding);
					}
				}
			}
			// io pins
			else
			{
				TSharedPtr<SHorizontalBox> SlotLayout;
				SHorizontalBox::FSlot* FirstSlot = nullptr;

				OutputPinInfo.OutputPinWidget->SetShowLabel(false);
			
				LeftNodeBox->AddSlot()
                .HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
                .AutoHeight()
				.MaxHeight(InputPinInfo.bAutoHeight ? TAttribute<float>() : MaxHeight)
                [
                    SAssignNew(SlotLayout, SHorizontalBox)
                    .Visibility(this, &SAnimNextGraphNode::GetPinVisibility, OutputPinInfo.Index, false)

                    +SHorizontalBox::Slot()
					.Expose(FirstSlot)
                    .FillWidth(1.f)
                    .HAlign(HAlign_Left)
                    .VAlign(VAlign_Center)
                    .Padding(PinWidgetSidePadding, TopPadding, 0.f, 0.f)
                    [
                        InputPinInfo.InputPinWidget.ToSharedRef()
                    ]
                ];

				if(InputPinInfo.bIsContainer)
				{
					URigVMPin* ModelPin = ModelNode->GetGraph()->FindPin(InputPinInfo.ModelPinPath);
					if(ModelPin)
					{
						// make sure to minimize the width of the label
						FirstSlot->SetAutoWidth();
						AddArrayPlusButtonLambda(ModelPin, SlotLayout, EmptySidePadding);
					}
				}

				SlotLayout->AddSlot()
				.FillWidth(1.f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(0.f, TopPadding, PinWidgetSidePadding, 0.f)
				[
					OutputPinInfo.OutputPinWidget.ToSharedRef()
				];
			}
		}
		// output pins
		else if(OutputPinInfo.OutputPinWidget.IsValid())
		{
			LeftNodeBox->AddSlot()
            .HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
            .AutoHeight()
			.MaxHeight(OutputPinInfo.bAutoHeight ? TAttribute<float>() : MaxHeight)
            [
            	SNew(SHorizontalBox)
	            .Visibility(this, &SAnimNextGraphNode::GetPinVisibility, OutputPinInfo.Index, false)
	            
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
	            .Padding(EmptySidePadding, TopPadding, PinWidgetSidePadding, 0.f)
				[
	                OutputPinInfo.OutputPinWidget.ToSharedRef()
				]
            ];
		}
	}

#if 0
	if(URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(ModelNode))
	{
		TWeakObjectPtr<URigVMFunctionReferenceNode> WeakFunctionReferenceNode = FunctionReferenceNode;
		TWeakObjectPtr<URigVMBlueprint> WeakRigVMBlueprint = Blueprint;

		// add the entries for the variable remapping
		for(const TSharedPtr<FRigVMExternalVariable>& ExternalVariable : RigGraphNode->ExternalVariables)	// TODO: FIX THIS, PRIVATE?
		{
			LeftNodeBox->AddSlot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.AutoHeight()
			.MaxHeight(MaxHeight)
			[
				SNew(SHorizontalBox)
	            
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(PinWidgetSidePadding, TopPadding, PinWidgetSidePadding, 0.f)
				[
					SNew(STextBlock)
					.Text(FText::FromName(ExternalVariable->Name))
					.TextStyle(FAppStyle::Get(), NAME_DefaultPinLabelStyle)
					.ColorAndOpacity(this, &SAnimNextGraphNode::GetVariableLabelTextColor, WeakFunctionReferenceNode, ExternalVariable->Name)
					.ToolTipText(this, &SAnimNextGraphNode::GetVariableLabelTooltipText, WeakControlRigBlueprint, ExternalVariable->Name)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(PinWidgetSidePadding, TopPadding, PinWidgetSidePadding, 0.f)
				[
					SNew(SRigVMGraphVariableBinding)
					.Blueprint(Blueprint.Get())
					.FunctionReferenceNode(FunctionReferenceNode)
					.InnerVariableName(ExternalVariable->Name)
				]
			];
		}
	}
#endif

	CreateAddPinButton();
	
	// add spacer widget at the end
	LeftNodeBox->AddSlot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Center)
	.AutoHeight()
	[
		SNew(SSpacer)
		.Size(FVector2D(1.f, 4.f))
	];
}

#undef LOCTEXT_NAMESPACE
