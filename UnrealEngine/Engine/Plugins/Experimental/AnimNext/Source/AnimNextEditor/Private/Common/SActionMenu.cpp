// Copyright Epic Games, Inc. All Rights Reserved.

#include "SActionMenu.h"

#include "GraphEditorSchemaActions.h"
#include "Framework/Application/SlateApplication.h"
#include "IDocumentation.h"
#include "SSubobjectEditor.h"
#include "Graph/AnimNextGraph_EdGraphSchema.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "SGraphPalette.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Units/RigUnit.h"
#include "Widgets/SToolTip.h"
#include "Graph/AnimNextExecuteContext.h"

#define LOCTEXT_NAMESPACE "AnimNextEditor"

namespace UE::AnimNext::Editor
{

void SActionMenu::CollectAllAnimNextGraphActions(FGraphContextMenuBuilder& MenuBuilder) const
{
	for(const FRigVMFunction& Function : FRigVMRegistry::Get().GetFunctions())
	{
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

		MenuBuilder.AddAction(MakeShared<FAnimNextSchemaAction_RigUnit>(Struct, NodeCategory, MenuDesc, ToolTip));
	}

	for (const FRigVMDispatchFactory* Factory : FRigVMRegistry::Get().GetFactories())
	{
		// See if the factory allows our execute contexts
		bool bAllowed = true;
		for(const UScriptStruct* AllowedExecuteContext : AllowedExecuteContexts)
		{
			if (!Factory->SupportsExecuteContextStruct(AllowedExecuteContext))
			{
				bAllowed = false;
				break;
			}
		}

		if(bAllowed)
		{
			continue;
		}

		const FRigVMTemplate* Template = Factory->GetTemplate();
		if (Template == nullptr)
		{
			continue;
		}

		FText NodeCategory = FText::FromString(Factory->GetCategory());
		FText MenuDesc = FText::FromString(Factory->GetNodeTitle(FRigVMTemplateTypeMap()));
		FText ToolTip = Factory->GetNodeTooltip(FRigVMTemplateTypeMap());

		MenuBuilder.AddAction(MakeShared<FAnimNextSchemaAction_DispatchFactory>(Template->GetNotation(), NodeCategory, MenuDesc, ToolTip));
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
	AllowedExecuteContexts = InArgs._AllowedExecuteContexts;

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
				[
					SAssignNew(GraphActionMenu, SGraphActionMenu)
						.OnActionSelected(this, &SActionMenu::OnActionSelected)
						.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(this, &SActionMenu::OnCreateWidgetForAction))
						.OnCollectAllActions(this, &SActionMenu::CollectAllActions)
						.OnCreateCustomRowExpander_Lambda([](const FCustomExpanderData& InActionMenuData)
						{
							// Default table row doesnt indent correctly
							return SNew(SExpanderArrow, InActionMenuData.TableRow);
						})
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
		CollectAllAnimNextGraphActions(MenuBuilder);
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
	TSharedPtr<FAnimNextSchemaAction> Action = StaticCastSharedPtr<FAnimNextSchemaAction>(InCreateData->Action);
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
			.Padding(0, 0, 0, 0)
			[
				SNew(SImage)
				.ColorAndOpacity(IconColor)
				.Image(IconBrush)
			];
	}

	WidgetBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(IconBrush ? 4.0f : 0.0f, 0, 0, 0)
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
