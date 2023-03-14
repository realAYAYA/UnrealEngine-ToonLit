// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SControlRigStackView.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SSearchBox.h"
#include "Editor/ControlRigEditor.h"
#include "ControlRigEditorStyle.h"
#include "ControlRigStackCommands.h"
#include "ControlRig.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "DetailLayoutBuilder.h"
#include "RigVMModel/Nodes/RigVMAggregateNode.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "SControlRigStackView"

TAutoConsoleVariable<bool> CVarControlRigExecutionStackDetailedLabels(TEXT("ControlRig.StackDetailedLabels"), false, TEXT("Set to true to turn on detailed labels for the execution stack widget"));

//////////////////////////////////////////////////////////////
/// FRigStackEntry
///////////////////////////////////////////////////////////
FRigStackEntry::FRigStackEntry(int32 InEntryIndex, ERigStackEntry::Type InEntryType, int32 InInstructionIndex, ERigVMOpCode InOpCode, const FString& InLabel, const FRigVMASTProxy& InProxy)
	: EntryIndex(InEntryIndex)
	, EntryType(InEntryType)
	, InstructionIndex(InInstructionIndex)
	, CallPath(InProxy.GetCallstack().GetCallPath())
	, Callstack(InProxy.GetCallstack())
	, OpCode(InOpCode)
	, Label(InLabel)
{

}

TSharedRef<ITableRow> FRigStackEntry::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FRigStackEntry> InEntry, TSharedRef<FUICommandList> InCommandList, TSharedPtr<SControlRigStackView> InStackView, TWeakObjectPtr<UControlRigBlueprint> InBlueprint)
{
	return SNew(SRigStackItem, InOwnerTable, InEntry, InCommandList, InBlueprint);
}

//////////////////////////////////////////////////////////////
/// SRigStackItem
///////////////////////////////////////////////////////////
void SRigStackItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FRigStackEntry> InStackEntry, TSharedRef<FUICommandList> InCommandList, TWeakObjectPtr<UControlRigBlueprint> InBlueprint)
{
	WeakStackEntry = InStackEntry;
	WeakBlueprint = InBlueprint;
	WeakCommandList = InCommandList;

	TSharedPtr< STextBlock > NumberWidget;
	TSharedPtr< STextBlock > TextWidget;

	const FSlateBrush* Icon = nullptr;
	switch (InStackEntry->EntryType)
	{
		case ERigStackEntry::Operator:
		{
			Icon = FControlRigEditorStyle::Get().GetBrush("ControlRig.RigUnit");
			break;
		}
		case ERigStackEntry::Info:
		{
			Icon = FAppStyle::GetBrush("Icons.Info");
			break;
		}
		case ERigStackEntry::Warning:
		{
			Icon = FAppStyle::GetBrush("Icons.Warning");
			break;
		}
		case ERigStackEntry::Error:
		{
			Icon = FAppStyle::GetBrush("Icons.Error");
			break;
		}
		default:
		{
			break;
		}
	}

	STableRow<TSharedPtr<FRigStackEntry>>::Construct(
		STableRow<TSharedPtr<FRigStackEntry>>::FArguments()
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MaxWidth(35.f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SAssignNew(NumberWidget, STextBlock)
					.Text(this, &SRigStackItem::GetIndexText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.MaxWidth(22.f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image(Icon)
				]
				
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(TextWidget, STextBlock)
					.Text(this, &SRigStackItem::GetLabelText)
					.Font(this, &SRigStackItem::GetLabelFont)
					.Justification(ETextJustify::Left)
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SSpacer)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
	            .HAlign(HAlign_Left)
	            [
	                SNew(STextBlock)
	                .Text(this, &SRigStackItem::GetVisitedCountText)
	                .Font(IDetailLayoutBuilder::GetDetailFont())
	            ]
	            
	            + SHorizontalBox::Slot()
	            .Padding(20, 0, 0, 0)
				.AutoWidth()
	            .VAlign(VAlign_Center)
	            .HAlign(HAlign_Left)
	            [
	                SNew(STextBlock)
	                .Text(this, &SRigStackItem::GetDurationText)
	                .Font(IDetailLayoutBuilder::GetDetailFont())
	            ]
	        ]
        ], OwnerTable);
}

FText SRigStackItem::GetIndexText() const
{
	const FString IndexStr = FString::FromInt(WeakStackEntry.Pin()->EntryIndex) + TEXT(".");
	return FText::FromString(IndexStr);
}

FText SRigStackItem::GetLabelText() const
{
	return (FText::FromString(WeakStackEntry.Pin()->Label));
}

FSlateFontInfo SRigStackItem::GetLabelFont() const
{
	if(GetVisitedCountText().IsEmpty())
	{
		return IDetailLayoutBuilder::GetDetailFont();
	}
	return IDetailLayoutBuilder::GetDetailFontBold();
}

FText SRigStackItem::GetVisitedCountText() const
{
	if(WeakStackEntry.IsValid() && WeakBlueprint.IsValid())
	{
		if(WeakBlueprint->RigGraphDisplaySettings.bShowNodeRunCounts)
		{
			if(WeakStackEntry.Pin()->EntryType == ERigStackEntry::Operator)
			{
				if(UControlRig* ControlRig = Cast<UControlRig>(WeakBlueprint->GetObjectBeingDebugged()))
				{
					if(URigVM* VM = ControlRig->GetVM())
					{
						const int32 Count = VM->GetInstructionVisitedCount(WeakStackEntry.Pin()->InstructionIndex);
						if(Count > 0)
						{
							return FText::FromString(FString::FromInt(Count));
						}
					}
				}
			}
		}
	}
	return FText();
}

FText SRigStackItem::GetDurationText() const
{
	if(WeakStackEntry.IsValid() && WeakBlueprint.IsValid())
	{
		if(WeakBlueprint->VMRuntimeSettings.bEnableProfiling)
		{
			if(WeakStackEntry.Pin()->EntryType == ERigStackEntry::Operator)
			{
				if(UControlRig* ControlRig = Cast<UControlRig>(WeakBlueprint->GetObjectBeingDebugged()))
				{
					if(URigVM* VM = ControlRig->GetVM())
					{
						const double MicroSeconds = VM->GetInstructionMicroSeconds(WeakStackEntry.Pin()->InstructionIndex);
						if(MicroSeconds > 0.0)
						{
							return FText::FromString(FString::Printf(TEXT("%d µs"), (int32)MicroSeconds));
						}
					}
				}
			}
		}
	}
	return FText();
}

//////////////////////////////////////////////////////////////
/// SControlRigStackView
///////////////////////////////////////////////////////////

SControlRigStackView::~SControlRigStackView()
{
	if (ControlRigEditor.IsValid())
	{
		if (OnModelModified.IsValid() && ControlRigEditor.Pin()->GetControlRigBlueprint())
		{
			ControlRigEditor.Pin()->GetControlRigBlueprint()->OnModified().Remove(OnModelModified);
		}
		if (OnControlRigInitializedHandle.IsValid())
		{
			if(ControlRigEditor.Pin()->ControlRig)
			{
				ControlRigEditor.Pin()->ControlRig->OnInitialized_AnyThread().Remove(OnControlRigInitializedHandle);
			}
		}
		if (OnPreviewControlRigUpdatedHandle.IsValid())
		{
			ControlRigEditor.Pin()->OnPreviewControlRigUpdated().Remove(OnPreviewControlRigUpdatedHandle);
		}
	}
	if (ControlRigBlueprint.IsValid() && OnVMCompiledHandle.IsValid())
	{
		ControlRigBlueprint->OnVMCompiled().Remove(OnVMCompiledHandle);
	}
}

void SControlRigStackView::Construct( const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;
	ControlRigBlueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();
	Graph = Cast<UControlRigGraph>(ControlRigBlueprint->GetLastEditedUberGraph());
	CommandList = MakeShared<FUICommandList>();
	bSuspendModelNotifications = false;
	bSuspendControllerSelection = false;
	HaltedAtInstruction = INDEX_NONE;

	BindCommands();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.Padding(0.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					//.Visibility(this, &SRigHierarchy::IsSearchbarVisible)
					+SHorizontalBox::Slot()
					.AutoWidth()
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(3.0f, 1.0f)
					[
						SAssignNew(FilterBox, SSearchBox)
						.OnTextChanged(this, &SControlRigStackView::OnFilterTextChanged)
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		[
			SNew(SBorder)
			.Padding(2.0f)
			.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
			[
				SAssignNew(TreeView, STreeView<TSharedPtr<FRigStackEntry>>)
				.TreeItemsSource(&Operators)
				.SelectionMode(ESelectionMode::Multi)
				.OnGenerateRow(this, &SControlRigStackView::MakeTableRowWidget, ControlRigBlueprint)
				.OnGetChildren(this, &SControlRigStackView::HandleGetChildrenForTree)
				.OnSelectionChanged(this, &SControlRigStackView::OnSelectionChanged)
				.OnContextMenuOpening(this, &SControlRigStackView::CreateContextMenu)
				.OnMouseButtonDoubleClick(this, &SControlRigStackView::HandleItemMouseDoubleClick)
				.ItemHeight(28)
			]
		]
	];

	RefreshTreeView(nullptr);

	if (ControlRigBlueprint.IsValid())
	{
		if (OnVMCompiledHandle.IsValid())
		{
			ControlRigBlueprint->OnVMCompiled().Remove(OnVMCompiledHandle);
		}
		if (OnModelModified.IsValid())
		{
			ControlRigBlueprint->OnModified().Remove(OnModelModified);
		}
		OnVMCompiledHandle = ControlRigBlueprint->OnVMCompiled().AddSP(this, &SControlRigStackView::OnVMCompiled);
		OnModelModified = ControlRigBlueprint->OnModified().AddSP(this, &SControlRigStackView::HandleModifiedEvent);

		if (ControlRigEditor.IsValid())
		{
			if (UControlRig* ControlRig = ControlRigEditor.Pin()->ControlRig)
			{
				OnVMCompiled(ControlRigBlueprint.Get(), ControlRig->VM);
			}
		}
	}

	if (ControlRigEditor.IsValid())
	{
		OnPreviewControlRigUpdatedHandle = ControlRigEditor.Pin()->OnPreviewControlRigUpdated().AddSP(this, &SControlRigStackView::HandlePreviewControlRigUpdated);
	}
}

void SControlRigStackView::OnSelectionChanged(TSharedPtr<FRigStackEntry> Selection, ESelectInfo::Type SelectInfo)
{
	if (bSuspendModelNotifications || bSuspendControllerSelection)
	{
		return;
	}
	TGuardValue<bool> SuspendNotifs(bSuspendModelNotifications, true);

	TArray<TSharedPtr<FRigStackEntry>> SelectedItems = TreeView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		if (!ControlRigBlueprint.IsValid())
		{
			return;
		}

		UControlRigBlueprintGeneratedClass* GeneratedClass = ControlRigBlueprint->GetControlRigBlueprintGeneratedClass();
		if (GeneratedClass == nullptr)
		{
			return;
		}

		UControlRig* ControlRig = ControlRigEditor.Pin()->ControlRig;
		if (ControlRig == nullptr || ControlRig->GetVM() == nullptr)
		{
			return;
		}

		const FRigVMByteCode& ByteCode = ControlRig->GetVM()->GetByteCode();

		TMap<URigVMGraph*, TArray<FName>> SelectedNodesPerGraph;
		for (TSharedPtr<FRigStackEntry>& Entry : SelectedItems)
		{
			for(int32 StackIndex = 0; StackIndex < Entry->Callstack.Num(); StackIndex++)
			{
				const UObject* Subject = Entry->Callstack[StackIndex];
				URigVMGraph* SubjectGraph = nullptr;

				FName NodeName = NAME_None;
				if (const URigVMNode* Node = Cast<URigVMNode>(Subject))
				{
					NodeName = Node->GetFName();
					SubjectGraph = Node->GetGraph();
				}
				else if (const URigVMPin* Pin = Cast<URigVMPin>(Subject))
				{
					NodeName = Pin->GetNode()->GetFName();
					SubjectGraph = Pin->GetGraph();
				}

				if (NodeName.IsNone() || SubjectGraph == nullptr)
				{
					continue;
				}

				SelectedNodesPerGraph.FindOrAdd(SubjectGraph).AddUnique(NodeName);
			}
		}

		for (const TPair< URigVMGraph*, TArray<FName> >& Pair : SelectedNodesPerGraph)
		{
			ControlRigBlueprint->GetOrCreateController(Pair.Key)->SetNodeSelection(Pair.Value);
		}
	}
}

void SControlRigStackView::BindCommands()
{
	// create new command
	const FControlRigStackCommands& Commands = FControlRigStackCommands::Get();
	CommandList->MapAction(Commands.FocusOnSelection, FExecuteAction::CreateSP(this, &SControlRigStackView::HandleFocusOnSelectedGraphNode));
}

TSharedRef<ITableRow> SControlRigStackView::MakeTableRowWidget(TSharedPtr<FRigStackEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable, TWeakObjectPtr<UControlRigBlueprint> InBlueprint)
{
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), CommandList.ToSharedRef(), SharedThis(this), InBlueprint);
}

void SControlRigStackView::HandleGetChildrenForTree(TSharedPtr<FRigStackEntry> InItem, TArray<TSharedPtr<FRigStackEntry>>& OutChildren)
{
	OutChildren = InItem->Children;
}

void SControlRigStackView::PopulateStackView(URigVM* InVM)
{
	if (InVM)
	{
		UControlRigBlueprint* Blueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();

		const FRigVMInstructionArray Instructions = InVM->GetInstructions();
		const FRigVMByteCode& ByteCode = InVM->GetByteCode();

		TArray<URigVMGraph*> RootGraphs;
		RootGraphs.AddZeroed(Instructions.Num());

		const bool bUseSimpleLabels = !CVarControlRigExecutionStackDetailedLabels.GetValueOnAnyThread();
		if(bUseSimpleLabels)
		{
			for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
			{
				URigVMNode* Node = Cast<URigVMNode>(ByteCode.GetSubjectForInstruction(InstructionIndex));

				FString DisplayName;

				if(Node)
				{
					DisplayName = Node->GetName();
					RootGraphs[InstructionIndex] = Node->GetRootGraph();
					
					// only unit nodes among all nodes has StaticExecute() that generates actual instructions
					if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
					{
						DisplayName = UnitNode->GetNodeTitle();
	#if WITH_EDITOR
						UScriptStruct* Struct = UnitNode->GetScriptStruct();
						FString MenuDescSuffixMetadata;
						if (Struct)
						{
							Struct->GetStringMetaDataHierarchical(FRigVMStruct::MenuDescSuffixMetaName, &MenuDescSuffixMetadata);
						}
						if (!MenuDescSuffixMetadata.IsEmpty())
						{
							DisplayName = FString::Printf(TEXT("%s %s"), *UnitNode->GetNodeTitle(), *MenuDescSuffixMetadata);
						}

						if(UnitNode->IsEvent())
						{
							DisplayName = Node->GetEventName().ToString();
							if(!DisplayName.EndsWith(TEXT("Event")))
							{
								DisplayName += TEXT(" Event");
							}
						}
	#endif
					}
				}

				FString Label;

				switch (Instructions[InstructionIndex].OpCode)
				{
					case ERigVMOpCode::Execute_0_Operands:
					case ERigVMOpCode::Execute_1_Operands:
					case ERigVMOpCode::Execute_2_Operands:
					case ERigVMOpCode::Execute_3_Operands:
					case ERigVMOpCode::Execute_4_Operands:
					case ERigVMOpCode::Execute_5_Operands:
					case ERigVMOpCode::Execute_6_Operands:
					case ERigVMOpCode::Execute_7_Operands:
					case ERigVMOpCode::Execute_8_Operands:
					case ERigVMOpCode::Execute_9_Operands:
					case ERigVMOpCode::Execute_10_Operands:
					case ERigVMOpCode::Execute_11_Operands:
					case ERigVMOpCode::Execute_12_Operands:
					case ERigVMOpCode::Execute_13_Operands:
					case ERigVMOpCode::Execute_14_Operands:
					case ERigVMOpCode::Execute_15_Operands:
					case ERigVMOpCode::Execute_16_Operands:
					case ERigVMOpCode::Execute_17_Operands:
					case ERigVMOpCode::Execute_18_Operands:
					case ERigVMOpCode::Execute_19_Operands:
					case ERigVMOpCode::Execute_20_Operands:
					case ERigVMOpCode::Execute_21_Operands:
					case ERigVMOpCode::Execute_22_Operands:
					case ERigVMOpCode::Execute_23_Operands:
					case ERigVMOpCode::Execute_24_Operands:
					case ERigVMOpCode::Execute_25_Operands:
					case ERigVMOpCode::Execute_26_Operands:
					case ERigVMOpCode::Execute_27_Operands:
					case ERigVMOpCode::Execute_28_Operands:
					case ERigVMOpCode::Execute_29_Operands:
					case ERigVMOpCode::Execute_30_Operands:
					case ERigVMOpCode::Execute_31_Operands:
					case ERigVMOpCode::Execute_32_Operands:
					case ERigVMOpCode::Execute_33_Operands:
					case ERigVMOpCode::Execute_34_Operands:
					case ERigVMOpCode::Execute_35_Operands:
					case ERigVMOpCode::Execute_36_Operands:
					case ERigVMOpCode::Execute_37_Operands:
					case ERigVMOpCode::Execute_38_Operands:
					case ERigVMOpCode::Execute_39_Operands:
					case ERigVMOpCode::Execute_40_Operands:
					case ERigVMOpCode::Execute_41_Operands:
					case ERigVMOpCode::Execute_42_Operands:
					case ERigVMOpCode::Execute_43_Operands:
					case ERigVMOpCode::Execute_44_Operands:
					case ERigVMOpCode::Execute_45_Operands:
					case ERigVMOpCode::Execute_46_Operands:
					case ERigVMOpCode::Execute_47_Operands:
					case ERigVMOpCode::Execute_48_Operands:
					case ERigVMOpCode::Execute_49_Operands:
					case ERigVMOpCode::Execute_50_Operands:
					case ERigVMOpCode::Execute_51_Operands:
					case ERigVMOpCode::Execute_52_Operands:
					case ERigVMOpCode::Execute_53_Operands:
					case ERigVMOpCode::Execute_54_Operands:
					case ERigVMOpCode::Execute_55_Operands:
					case ERigVMOpCode::Execute_56_Operands:
					case ERigVMOpCode::Execute_57_Operands:
					case ERigVMOpCode::Execute_58_Operands:
					case ERigVMOpCode::Execute_59_Operands:
					case ERigVMOpCode::Execute_60_Operands:
					case ERigVMOpCode::Execute_61_Operands:
					case ERigVMOpCode::Execute_62_Operands:
					case ERigVMOpCode::Execute_63_Operands:
					case ERigVMOpCode::Execute_64_Operands:
					{
						const FRigVMExecuteOp& Op = ByteCode.GetOpAt<FRigVMExecuteOp>(Instructions[InstructionIndex]);

						if(Node)
						{
							Label = DisplayName;
						}
						else
						{
							Label = InVM->GetRigVMFunctionName(Op.FunctionIndex);
						}
						break;
					}
					case ERigVMOpCode::Copy:
					case ERigVMOpCode::Zero:
					case ERigVMOpCode::BoolFalse:
					case ERigVMOpCode::BoolTrue:
					case ERigVMOpCode::Increment:
					case ERigVMOpCode::Decrement:
					case ERigVMOpCode::Equals:
					case ERigVMOpCode::NotEquals:
					case ERigVMOpCode::JumpAbsolute:
					case ERigVMOpCode::JumpForward:
					case ERigVMOpCode::JumpBackward:
					case ERigVMOpCode::JumpAbsoluteIf:
					case ERigVMOpCode::JumpForwardIf:
					case ERigVMOpCode::JumpBackwardIf:
					case ERigVMOpCode::BeginBlock:
					case ERigVMOpCode::EndBlock:
					case ERigVMOpCode::ChangeType:
					case ERigVMOpCode::ArrayReset:
					case ERigVMOpCode::ArrayGetNum: 
					case ERigVMOpCode::ArraySetNum:
					case ERigVMOpCode::ArrayGetAtIndex:  
					case ERigVMOpCode::ArraySetAtIndex:
					case ERigVMOpCode::ArrayAdd:
					case ERigVMOpCode::ArrayInsert:
					case ERigVMOpCode::ArrayRemove:
					case ERigVMOpCode::ArrayFind:
					case ERigVMOpCode::ArrayAppend:
					case ERigVMOpCode::ArrayClone:
					case ERigVMOpCode::ArrayIterator:
					case ERigVMOpCode::ArrayUnion:
					case ERigVMOpCode::ArrayDifference:
					case ERigVMOpCode::ArrayIntersection:
					case ERigVMOpCode::ArrayReverse:
					{
						const FText OpCodeText = StaticEnum<ERigVMOpCode>()->GetDisplayNameTextByValue((int32)Instructions[InstructionIndex].OpCode);
						if(Node)
						{
							Label = DisplayName + TEXT(" - ") + OpCodeText.ToString();
						}
						else
						{
							Label = OpCodeText.ToString();
						}
						break;
					}
					case ERigVMOpCode::InvokeEntry:
					{
						const FRigVMInvokeEntryOp& Op = ByteCode.GetOpAt<FRigVMInvokeEntryOp>(Instructions[InstructionIndex]);
						Label = FString::Printf(TEXT("Run %s Event"), *Op.EntryName.ToString());
						break;
					}
					case ERigVMOpCode::Exit:
					{
						Label = TEXT("Exit");
						break;
					}
					default:
					{
						ensure(false);
						break;
					}
				}
				
				// add the entry with the new label to the stack view
				const FRigVMInstruction& Instruction = Instructions[InstructionIndex];
				if (FilterText.IsEmpty() || Label.Contains(FilterText.ToString()))
				{
					FRigVMASTProxy Proxy;
					if(const TArray<UObject*>* Callstack = ByteCode.GetCallstackForInstruction(InstructionIndex))
					{
						Proxy = FRigVMASTProxy::MakeFromCallstack(Callstack);
					}
					TSharedPtr<FRigStackEntry> NewEntry = MakeShared<FRigStackEntry>(InstructionIndex, ERigStackEntry::Operator, InstructionIndex, Instruction.OpCode, Label, Proxy);
					Operators.Add(NewEntry);
				}
			}
			return;
		}

		// 1. cache information about instructions/nodes, which will be used later 
		TMap<FString, FString> OperandFormatMap;
		for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
		{
			const FRigVMASTProxy Proxy = FRigVMASTProxy::MakeFromCallPath(ByteCode.GetCallPathForInstruction(InstructionIndex), RootGraphs[InstructionIndex]);
			if(URigVMNode* Node = Proxy.GetSubject<URigVMNode>())
			{
				FString DisplayName = Node->GetName();

				// only unit nodes among all nodes has StaticExecute() that generates actual instructions
				if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(Node))
				{
					DisplayName = UnitNode->GetNodeTitle();
#if WITH_EDITOR
					UScriptStruct* Struct = UnitNode->GetScriptStruct();
					FString MenuDescSuffixMetadata;
					if (Struct)
					{
						Struct->GetStringMetaDataHierarchical(FRigVMStruct::MenuDescSuffixMetaName, &MenuDescSuffixMetadata);
					}
					if (!MenuDescSuffixMetadata.IsEmpty())
					{
						DisplayName = FString::Printf(TEXT("%s %s"), *UnitNode->GetNodeTitle(), *MenuDescSuffixMetadata);
					}
#endif
				}

				// this is needed for name replacement later
				OperandFormatMap.Add(Node->GetName(), DisplayName);
			}
		}

		// 2. replace raw operand names with NodeTitle.PinName/PropertyName.OffsetName
		TArray<FString> Labels = InVM->DumpByteCodeAsTextArray(TArray<int32>(), false, [OperandFormatMap](const FString& RegisterName, const FString& RegisterOffsetName)
		{
			FString NewRegisterName = RegisterName;
			FString NodeName;
			FString PinName;
			if (RegisterName.Split(TEXT("."), &NodeName, &PinName))
			{
				const FString* NodeTitle = OperandFormatMap.Find(NodeName);
				NewRegisterName = FString::Printf(TEXT("%s.%s"), NodeTitle ? **NodeTitle : *NodeName, *PinName);
			}
			FString OperandLabel;
			OperandLabel = NewRegisterName;
			if (!RegisterOffsetName.IsEmpty())
			{
				OperandLabel = FString::Printf(TEXT("%s.%s"), *OperandLabel, *RegisterOffsetName);
			}
			return OperandLabel;
		});

		ensure(Labels.Num() == Instructions.Num());
		
		// 3. replace instruction names with node titles
		for (int32 InstructionIndex = 0; InstructionIndex < Labels.Num(); InstructionIndex++)
		{
			FString Label = Labels[InstructionIndex];
			const FRigVMASTProxy Proxy = FRigVMASTProxy::MakeFromCallPath(ByteCode.GetCallPathForInstruction(InstructionIndex), RootGraphs[InstructionIndex]);

			if(URigVMNode* Node = Proxy.GetSubject<URigVMNode>())
			{
				FString Suffix;
				switch(Instructions[InstructionIndex].OpCode)
				{
					case ERigVMOpCode::Copy:
					case ERigVMOpCode::Zero:
	                case ERigVMOpCode::BoolFalse:
	                case ERigVMOpCode::BoolTrue:
	                case ERigVMOpCode::Increment:
	                case ERigVMOpCode::Decrement:
					case ERigVMOpCode::Equals:
                	case ERigVMOpCode::NotEquals:
					case ERigVMOpCode::JumpAbsolute:
                	case ERigVMOpCode::JumpForward:
                	case ERigVMOpCode::JumpBackward:
					case ERigVMOpCode::JumpAbsoluteIf:
                	case ERigVMOpCode::JumpForwardIf:
                	case ERigVMOpCode::JumpBackwardIf:
					case ERigVMOpCode::BeginBlock:
					case ERigVMOpCode::EndBlock:
					{
						const FText OpCodeText = StaticEnum<ERigVMOpCode>()->GetDisplayNameTextByValue((int32)Instructions[InstructionIndex].OpCode);
						Suffix = FString::Printf(TEXT(" - %s"), *OpCodeText.ToString());
						break;
					}
					default:
					{
						break;
					}
				}
				
				Label =  OperandFormatMap[Node->GetName()] + Suffix;
				// Label = Proxy.GetCallstack().GetCallPath() + Suffix;
			}

			// add the entry with the new label to the stack view
			const FRigVMInstruction& Instruction = Instructions[InstructionIndex];
			if (FilterText.IsEmpty() || Label.Contains(FilterText.ToString()))
			{
				TSharedPtr<FRigStackEntry> NewEntry = MakeShared<FRigStackEntry>(InstructionIndex, ERigStackEntry::Operator, InstructionIndex, Instruction.OpCode, Label, Proxy);
				Operators.Add(NewEntry);
			}
		}
	}
}

void SControlRigStackView::RefreshTreeView(URigVM* InVM)
{
	Operators.Reset();

	// populate the stack with node names/instruction names
	PopulateStackView(InVM);
	
	if (InVM)
	{
		// fill the children from the log
		if (ControlRigEditor.IsValid())
		{
			UControlRig* ControlRig = ControlRigEditor.Pin()->ControlRig;
			if(ControlRig && ControlRig->ControlRigLog)
			{
				const TArray<FControlRigLog::FLogEntry>& LogEntries = ControlRig->ControlRigLog->Entries;
				for (const FControlRigLog::FLogEntry& LogEntry : LogEntries)
				{
					if (Operators.Num() <= LogEntry.InstructionIndex)
					{
						continue;
					}
					int32 ChildIndex = Operators[LogEntry.InstructionIndex]->Children.Num();
					switch (LogEntry.Severity)
					{
						case EMessageSeverity::Info:
						{
							Operators[LogEntry.InstructionIndex]->Children.Add(MakeShared<FRigStackEntry>(ChildIndex, ERigStackEntry::Info, LogEntry.InstructionIndex, ERigVMOpCode::Invalid, LogEntry.Message, FRigVMASTProxy()));
							break;
						}
						case EMessageSeverity::Warning:
						case EMessageSeverity::PerformanceWarning:
						{
							Operators[LogEntry.InstructionIndex]->Children.Add(MakeShared<FRigStackEntry>(ChildIndex, ERigStackEntry::Warning, LogEntry.InstructionIndex, ERigVMOpCode::Invalid, LogEntry.Message, FRigVMASTProxy()));
							break;
						}
						case EMessageSeverity::Error:
						{
							Operators[LogEntry.InstructionIndex]->Children.Add(MakeShared<FRigStackEntry>(ChildIndex, ERigStackEntry::Error, LogEntry.InstructionIndex, ERigVMOpCode::Invalid, LogEntry.Message, FRigVMASTProxy()));
							break;
						}
						default:
						{
							break;
						}
					}
				}

				if (ControlRig->GetVM())
				{
					ControlRig->GetVM()->ExecutionHalted().AddSP(this, &SControlRigStackView::HandleExecutionHalted);
				}
			}
		}
	}

	TreeView->RequestTreeRefresh();
}

TSharedPtr< SWidget > SControlRigStackView::CreateContextMenu()
{
	TArray<TSharedPtr<FRigStackEntry>> SelectedItems = TreeView->GetSelectedItems();
	if(SelectedItems.Num() == 0)
	{
		return nullptr;
	}

	const FControlRigStackCommands& Actions = FControlRigStackCommands::Get();

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder(CloseAfterSelection, CommandList);
	{
		MenuBuilder.BeginSection("RigStackToolsAction", LOCTEXT("ToolsAction", "Tools"));
		MenuBuilder.AddMenuEntry(Actions.FocusOnSelection);
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void SControlRigStackView::HandleFocusOnSelectedGraphNode()
{
	OnSelectionChanged(TSharedPtr<FRigStackEntry>(), ESelectInfo::Direct);

	TArray<TSharedPtr<FRigStackEntry>> SelectedItems = TreeView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		UControlRig* ControlRig = ControlRigEditor.Pin()->ControlRig;
		if (ControlRig == nullptr || ControlRig->GetVM() == nullptr)
		{
			return;
		}

		const FRigVMByteCode& ByteCode = ControlRig->GetVM()->GetByteCode();
		UObject* Subject = ByteCode.GetSubjectForInstruction(SelectedItems[0]->InstructionIndex);
		if (URigVMNode* SelectedNode = Cast<URigVMNode>(Subject))
		{
			URigVMGraph* GraphToFocus = SelectedNode->GetGraph();
			if (GraphToFocus && GraphToFocus->GetTypedOuter<URigVMAggregateNode>())
			{
				if(URigVMGraph* ParentGraph = GraphToFocus->GetParentGraph())
				{
					GraphToFocus = ParentGraph;
				} 
			}
			
			if (UEdGraph* EdGraph = ControlRigBlueprint->GetEdGraph(GraphToFocus))
			{
				ControlRigEditor.Pin()->OpenGraphAndBringToFront(EdGraph, true);
				ControlRigEditor.Pin()->ZoomToSelection_Clicked();
				ControlRigEditor.Pin()->HandleModifiedEvent(ERigVMGraphNotifType::NodeSelected, GraphToFocus, SelectedNode);
			}
		}
	}
}

void SControlRigStackView::OnVMCompiled(UObject* InCompiledObject, URigVM* InCompiledVM)
{
	RefreshTreeView(InCompiledVM);

	if (ControlRigEditor.IsValid() && !OnControlRigInitializedHandle.IsValid())
	{
		if(UControlRig* ControlRig = ControlRigEditor.Pin()->ControlRig)
		{
			OnControlRigInitializedHandle = ControlRig->OnInitialized_AnyThread().AddSP(this, &SControlRigStackView::HandleControlRigInitializedEvent);
		}
	}
}

void SControlRigStackView::HandleExecutionHalted(const int32 InHaltedAtInstruction, UObject* InNode, const FName& InEntryName)
{
	if (HaltedAtInstruction == InHaltedAtInstruction)
	{
		return;
	}

	if (InHaltedAtInstruction == INDEX_NONE && InEntryName == ControlRigEditor.Pin()->ControlRig->GetEventQueue().Last())
	{
		HaltedAtInstruction = InHaltedAtInstruction;
		return;
	}
	
	if (InHaltedAtInstruction != INDEX_NONE && Operators.Num() > InHaltedAtInstruction)
	{
		HaltedAtInstruction = InHaltedAtInstruction;
		TreeView->SetSelection(Operators[InHaltedAtInstruction]);
		TreeView->SetScrollOffset(FMath::Max(InHaltedAtInstruction-5, 0));
	}	
}

void SControlRigStackView::OnFilterTextChanged(const FText& SearchText)
{
	FilterText = SearchText;
	RefreshTreeView(ControlRigEditor.Pin()->ControlRig->GetVM());
}

void SControlRigStackView::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	if (bSuspendModelNotifications)
	{
		return;
	}

	UControlRig* ControlRig = ControlRigEditor.Pin()->ControlRig;
	if (ControlRig == nullptr || ControlRig->GetVM() == nullptr)
	{
		return;
	}

	const FRigVMByteCode& ByteCode = ControlRig->GetVM()->GetByteCode();

	switch (InNotifType)
	{
		case ERigVMGraphNotifType::NodeSelected:
		case ERigVMGraphNotifType::NodeDeselected:
		{
			for (TSharedPtr<FRigStackEntry>& Operator : Operators)
			{
				if(Operator->Callstack.Contains(InSubject))
				{
					const TGuardValue<bool> SuspendNotifs(bSuspendModelNotifications, true);
					TreeView->SetItemSelection(Operator, InNotifType == ERigVMGraphNotifType::NodeSelected, ESelectInfo::Direct);
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void SControlRigStackView::HandleControlRigInitializedEvent(UControlRig* InControlRig, const EControlRigState InState, const FName& InEventName)
{
	TGuardValue<bool> SuspendControllerSelection(bSuspendControllerSelection, true);

	RefreshTreeView(InControlRig->VM);
	OnSelectionChanged(TSharedPtr<FRigStackEntry>(), ESelectInfo::Direct);

	for (TSharedPtr<FRigStackEntry>& Operator : Operators)
	{
		for (TSharedPtr<FRigStackEntry>& Child : Operator->Children)
		{
			if (Child->EntryType == ERigStackEntry::Warning || Child->EntryType == ERigStackEntry::Error)
			{
				TreeView->SetItemExpansion(Operator, true);
				break;
			}
		}
	}
	
	if (ControlRigEditor.IsValid())
	{
		InControlRig->OnInitialized_AnyThread().Remove(OnControlRigInitializedHandle);
		OnControlRigInitializedHandle.Reset();

		if (UControlRigBlueprint* RigBlueprint = ControlRigEditor.Pin()->GetControlRigBlueprint())
		{
			TArray<URigVMGraph*> Models = RigBlueprint->GetAllModels();
			for (URigVMGraph* Model : Models)
			{
				for (URigVMNode* ModelNode : Model->GetNodes())
				{
					if (ModelNode->IsSelected())
					{
						HandleModifiedEvent(ERigVMGraphNotifType::NodeSelected, Model, ModelNode);
					}
				}
			}
		}
	}
}

void SControlRigStackView::HandlePreviewControlRigUpdated(FControlRigEditor* InEditor)
{
}

void SControlRigStackView::HandleItemMouseDoubleClick(TSharedPtr<FRigStackEntry> InItem)
{
	if (!ControlRigEditor.IsValid() || !ControlRigBlueprint.IsValid())
	{
		return;
	}
	
	UControlRig* ControlRig = Cast<UControlRig>(ControlRigBlueprint->GetObjectBeingDebugged());
	if (!ControlRig || !ControlRig->GetVM())
	{
		return;
	}

	const FRigVMByteCode& ByteCode = ControlRig->GetVM()->GetByteCode();
	if (URigVMNode* Subject = Cast<URigVMNode>(ByteCode.GetSubjectForInstruction(InItem->InstructionIndex)))
	{
		URigVMGraph* GraphToFocus = Subject->GetGraph();
		if (GraphToFocus && GraphToFocus->GetTypedOuter<URigVMAggregateNode>())
		{
			if(URigVMGraph* ParentGraph = GraphToFocus->GetParentGraph())
			{
				GraphToFocus = ParentGraph;
			} 
		}
		
		if(UControlRigGraph* EdGraph = Cast<UControlRigGraph>(ControlRigBlueprint->GetEdGraph(GraphToFocus)))
		{
			if(const UEdGraphNode* Node = EdGraph->FindNodeForModelNodeName(Subject->GetFName()))
			{
				ControlRigEditor.Pin()->JumpToHyperlink(Node, false);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
