// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAnimNextInterfaceGraphEditorActionMenu.h"

#include "AnimNextInterfaceGraphEditorSchemaActions.h"
#include "Framework/Application/SlateApplication.h"
#include "IDocumentation.h"
#include "SGraphActionMenu.h"
#include "SSubobjectEditor.h"
#include "AnimNextInterfaceGraph_EdGraphSchema.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "SGraphPalette.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Units/RigUnit.h"
#include "Widgets/SToolTip.h"
#include "AnimNextInterfaceExecuteContext.h"

#define LOCTEXT_NAMESPACE "AnimNextInterfaceEditor"

namespace UE::AnimNext::InterfaceGraphEditor
{

static void CollectAllAnimNextInterfaceActions(FGraphContextMenuBuilder& MenuBuilder)
{
	static const TArray<UScriptStruct*> AllowedExecuteContexts =
	{
		FRigVMExecuteContext::StaticStruct(),
		FAnimNextInterfaceExecuteContext::StaticStruct()
	};

	const TChunkedArray<FRigVMFunction>& Functions = FRigVMRegistry::Get().GetFunctions();
	const int32 NumFunctions = Functions.Num();
	for(int i=0; i < NumFunctions; i++)
	{
		const FRigVMFunction& Function = Functions[i];

		const UScriptStruct* FunctionContext = Function.GetExecuteContextStruct();
		if (FunctionContext == nullptr || !AllowedExecuteContexts.Contains(FunctionContext))
		{
			continue;
		}

		UScriptStruct* Struct = Function.Struct;
		if (Struct == nullptr)
		{
			continue;
		}

		FString CategoryMetadata, DisplayNameMetadata, MenuDescSuffixMetadata;
		Struct->GetStringMetaDataHierarchical(FRigVMStruct::CategoryMetaName, &CategoryMetadata);
		Struct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
		Struct->GetStringMetaDataHierarchical(FRigVMStruct::MenuDescSuffixMetaName, &MenuDescSuffixMetadata);
		if (!MenuDescSuffixMetadata.IsEmpty())
		{
			MenuDescSuffixMetadata = TEXT(" ") + MenuDescSuffixMetadata;
		}
		const FText NodeCategory = FText::FromString(CategoryMetadata);
		const FText MenuDesc = FText::FromString(DisplayNameMetadata + MenuDescSuffixMetadata);
		const FText ToolTip = Struct->GetToolTipText();

		if (MenuDesc.IsEmpty())
		{
			continue;
		}

		MenuBuilder.AddAction(MakeShared<FAnimNextInterfaceGraphSchemaAction_RigUnit>(Struct, NodeCategory, MenuDesc, ToolTip));
	};
}

SActionMenu::~SActionMenu()
{
	OnClosedCallback.ExecuteIfBound();
	OnCloseReasonCallback.ExecuteIfBound(bActionExecuted, false, !DraggedFromPins.IsEmpty());
}

void SActionMenu::Construct(const FArguments& InArgs)
{
	Graph = InArgs._Graph;
	DraggedFromPins = InArgs._DraggedFromPins;
	NewNodePosition = InArgs._NewNodePosition;
	OnClosedCallback = InArgs._OnClosedCallback;
	bAutoExpandActionMenu = InArgs._AutoExpandActionMenu;
	OnCloseReasonCallback = InArgs._OnCloseReason;

	SBorder::Construct(SBorder::FArguments()
		.BorderImage(FAppStyle::Get().GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			.WidthOverride(400)
			.HeightOverride(400)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(2, 2, 2, 5)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ContextText", "All AnimNext Interface Node Classes"))
						.Font(FAppStyle::Get().GetFontStyle("BlueprintEditor.ActionMenu.ContextDescriptionFont"))
						.ToolTip(IDocumentation::Get()->CreateToolTip(
							LOCTEXT("ActionMenuContextTextTooltip", "Describes the current context of the action list"),
							nullptr,
							TEXT("Shared/Editors/AnimNextInterfaceEditor"),
							TEXT("AnimNextInterfaceActionMenuContextText")))
						.WrapTextAt(280)
					]
				]
				+SVerticalBox::Slot()
				[
					SAssignNew(GraphActionMenu, SGraphActionMenu)
						.OnActionSelected(this, &SActionMenu::OnActionSelected)
						.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(this, &SActionMenu::OnCreateWidgetForAction))
						.OnCollectAllActions(this, &SActionMenu::CollectAllActions)
						.DraggedFromPins(DraggedFromPins)
						.GraphObj(Graph)
						.AlphaSortItems(true)
						.bAllowPreselectedItemActivation(true)
				]
			]
		]
	);
}

void SActionMenu::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	if (!Graph)
	{
		return;
	}

	FGraphContextMenuBuilder MenuBuilder(Graph);
	if (!DraggedFromPins.IsEmpty())
	{
		MenuBuilder.FromPin = DraggedFromPins[0];
	}

	// Cannot call GetGraphContextActions() during serialization and GC due to its use of FindObject()
	if(!GIsSavingPackage && !IsGarbageCollecting())
	{
		CollectAllAnimNextInterfaceActions(MenuBuilder);
	}

	OutAllActions.Append(MenuBuilder);
}

TSharedRef<SEditableTextBox> SActionMenu::GetFilterTextBox()
{
	return GraphActionMenu->GetFilterTextBox();
}

TSharedRef<SWidget> SActionMenu::OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData)
{
	check(InCreateData);
	InCreateData->bHandleMouseButtonDown = false;

	const FSlateBrush* IconBrush = nullptr;
	FLinearColor IconColor;
	TSharedPtr<FAnimNextInterfaceGraphSchemaAction> Action = StaticCastSharedPtr<FAnimNextInterfaceGraphSchemaAction>(InCreateData->Action);
	if (Action.IsValid())
	{
		IconBrush = Action->GetIconBrush();
		IconColor = Action->GetIconColor();
	}

	TSharedPtr<SHorizontalBox> WidgetBox = SNew(SHorizontalBox);
	if (IconBrush)
	{
		WidgetBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(5, 0, 0, 0)
			[
				SNew(SImage)
				.ColorAndOpacity(IconColor)
				.Image(IconBrush)
			];
	}

	WidgetBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 0, 0)
		[
			SNew(SGraphPaletteItem, InCreateData)
		];

	return WidgetBox->AsShared();
}

void SActionMenu::OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedAction, ESelectInfo::Type InSelectionType)
{
	if (!Graph)
	{
		return;
	}

	if (InSelectionType != ESelectInfo::OnMouseClick  && InSelectionType != ESelectInfo::OnKeyPress && !SelectedAction.IsEmpty())
	{
		return;
	}

	for (const TSharedPtr<FEdGraphSchemaAction>& Action : SelectedAction)
	{
		if (Action.IsValid() && Graph)
		{
			if (!bActionExecuted && (Action->GetTypeId() != FEdGraphSchemaAction_Dummy::StaticGetTypeId()))
			{
				FSlateApplication::Get().DismissAllMenus();
				bActionExecuted = true;
			}

			Action->PerformAction(Graph, DraggedFromPins, NewNodePosition);
		}
	}
}

} 

#undef LOCTEXT_NAMESPACE
