// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMovieGraphMembersTabContent.h"

#include "EdGraph/EdGraphSchema.h"
#include "Framework/Commands/GenericCommands.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphSchema.h"
#include "Graph/Nodes/MovieGraphVariableNode.h"
#include "MovieEdGraphNode.h"
#include "ScopedTransaction.h"
#include "SGraphActionMenu.h"
#include "Toolkits/AssetEditorToolkit.h"

#define LOCTEXT_NAMESPACE "SMoviePipelineMembersTabContent"

namespace UE::MovieGraph::Private
{
	/** Gets a graph member from a FEdGraphSchemaAction. */
	UMovieGraphMember* GetMemberFromAction(FEdGraphSchemaAction* SchemaAction)
	{
		if (!SchemaAction)
		{
			return nullptr;
		}

		const FMovieGraphSchemaAction* SelectedGraphAction = static_cast<FMovieGraphSchemaAction*>(SchemaAction);

		// Currently the action menu only includes UMovieGraphMember targets
		if (UMovieGraphMember* SelectedMember = Cast<UMovieGraphMember>(SelectedGraphAction->ActionTarget))
		{
			return SelectedMember;
		}

		return nullptr;
	}

	static bool GetIconAndColorFromDataType(const UMovieGraphVariable* InGraphVariable, const FSlateBrush*& OutPrimaryBrush, FSlateColor& OutIconColor, const FSlateBrush*& OutSecondaryBrush, FSlateColor& OutSecondaryColor)
	{
		if (!InGraphVariable)
		{
			return false;
		}
		
		constexpr bool bIsBranch = false;
		const FEdGraphPinType PinType = UMoviePipelineEdGraphNodeBase::GetPinType(InGraphVariable->GetValueType(), bIsBranch, InGraphVariable->GetValueTypeObject());

		OutPrimaryBrush = FAppStyle::GetBrush("Kismet.AllClasses.VariableIcon");
		OutIconColor = UMovieGraphSchema::GetTypeColor(PinType.PinCategory, PinType.PinSubCategory);
		OutSecondaryBrush = nullptr;
		
		return true;
	}
}

const TArray<FText> SMovieGraphMembersTabContent::ActionMenuSectionNames
{
	LOCTEXT("ActionMenuSectionName_Invalid", "INVALID"),
	LOCTEXT("ActionMenuSectionName_Inputs", "Inputs"),
	LOCTEXT("ActionMenuSectionName_Outputs", "Outputs"),
	LOCTEXT("ActionMenuSectionName_Variables", "Variables")
};

void SMovieGraphMembersTabContent::Construct(const FArguments& InArgs)
{
	EditorToolkit = InArgs._Editor;
	CurrentGraph = InArgs._Graph;
	OnActionSelected = InArgs._OnActionSelected;

	// Update the UI whenever the graph adds/updates variables. In this case it's not known which variable is added/updated, so just pass nullptr.
	UMovieGraphMember* UpdatedMember = nullptr;
	CurrentGraph->OnGraphVariablesChangedDelegate.AddSP(this, &SMovieGraphMembersTabContent::RefreshMemberActions, UpdatedMember);
	
	ChildSlot
	[
		SAssignNew(ActionMenu, SGraphActionMenu)
		.OnActionSelected(OnActionSelected)
		.AutoExpandActionMenu(true)
		.AlphaSortItems(false)
		.OnActionDragged(this, &SMovieGraphMembersTabContent::OnActionDragged)
		.OnCreateWidgetForAction(this, &SMovieGraphMembersTabContent::CreateActionWidget)
		.OnCollectStaticSections(this, &SMovieGraphMembersTabContent::CollectStaticSections)
		.OnContextMenuOpening(this, &SMovieGraphMembersTabContent::OnContextMenuOpening)
		.OnGetSectionTitle(this, &SMovieGraphMembersTabContent::GetSectionTitle)
		.OnGetSectionWidget(this, &SMovieGraphMembersTabContent::GetSectionWidget)
		.UseSectionStyling(true)
		.OnCollectAllActions(this, &SMovieGraphMembersTabContent::CollectAllActions)
		.OnActionMatchesName(this, &SMovieGraphMembersTabContent::ActionMatchesName)
	];
}

FReply SMovieGraphMembersTabContent::OnActionDragged(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions, const FPointerEvent& MouseEvent)
{
	const TSharedPtr<FEdGraphSchemaAction> Action(!InActions.IsEmpty() ? InActions[0] : nullptr);
	if (!Action.IsValid())
	{
		return FReply::Unhandled();
	}
	
	const FMovieGraphSchemaAction* GraphSchemaAction = static_cast<FMovieGraphSchemaAction*>(Action.Get());
	const TObjectPtr<UObject> ActionTarget = GraphSchemaAction->ActionTarget;

	if (UMovieGraphVariable* VariableMember = Cast<UMovieGraphVariable>(ActionTarget.Get()))
	{
		return FReply::Handled().BeginDragDrop(FMovieGraphDragAction_Variable::New(Action, VariableMember));
	}
	
	return FReply::Unhandled();
}

TSharedRef<SWidget> SMovieGraphMembersTabContent::CreateActionWidget(FCreateWidgetForActionData* InCreateData) const
{
	// For variables, show an icon w/ the color representing the variable's type (in addition to the variable name)
	if (InCreateData->Action->GetSectionID() == static_cast<uint32>(EActionSection::Variables))
	{
		const FMovieGraphSchemaAction_NewVariableNode* VariableAction =
			static_cast<FMovieGraphSchemaAction_NewVariableNode*>(InCreateData->Action.Get());
		const UMovieGraphVariable* Variable = Cast<UMovieGraphVariable>(VariableAction->ActionTarget);
		const FEdGraphPinType PinType = Variable
			? UMoviePipelineEdGraphNodeBase::GetPinType(Variable->GetValueType(), false, Variable->GetValueTypeObject())
			: FEdGraphPinType();
		const FLinearColor PinColor = CurrentGraph->PipelineEdGraph->GetSchema()->GetPinTypeColor(PinType);
		
		return
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 5, 0)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Kismet.AllClasses.VariableIcon"))
				.ColorAndOpacity(PinColor)
			]
			
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(InCreateData->Action->GetMenuDescription())
			];	
	}
	
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(InCreateData->Action->GetMenuDescription())
		];
}

void SMovieGraphMembersTabContent::ClearSelection() const
{
	if (ActionMenu.IsValid())
	{
		ActionMenu->SelectItemByName(NAME_None);
	}
}

void SMovieGraphMembersTabContent::DeleteSelectedMembers()
{
	if (!ActionMenu.IsValid() || !CurrentGraph)
	{
		return;
	}

	TArray<TSharedPtr<FEdGraphSchemaAction>> SelectedActions;
	ActionMenu->GetSelectedActions(SelectedActions);
	for (TSharedPtr<FEdGraphSchemaAction> SelectedAction : SelectedActions)
	{
		if (UMovieGraphMember* GraphMember = UE::MovieGraph::Private::GetMemberFromAction(SelectedAction.Get()))
		{
			FScopedTransaction Transaction(LOCTEXT("DeleteGraphMember", "Delete Graph Member"));
			
			MemberChangedHandles.Remove(GraphMember);
			CurrentGraph->DeleteMember(GraphMember);
		}
	}

	RefreshMemberActions();
}

bool SMovieGraphMembersTabContent::CanDeleteSelectedMembers() const
{
	if (!ActionMenu.IsValid())
	{
		return false;
	}

	TArray<TSharedPtr<FEdGraphSchemaAction>> SelectedActions;
	ActionMenu->GetSelectedActions(SelectedActions);
	
	for (const TSharedPtr<FEdGraphSchemaAction>& SelectedAction : SelectedActions)
	{
		// Don't allow deletion if the member was explicitly marked as non-deletable
		if (const UMovieGraphMember* Member = UE::MovieGraph::Private::GetMemberFromAction(SelectedAction.Get()))
		{
			if (!Member->IsDeletable())
			{
				return false;
			}
		}
	}

	return true;
}

void SMovieGraphMembersTabContent::PostUndo(bool bSuccess)
{
	// Normally the UI relies on delegates to determine when to refresh. However, undo/redo do not fire those delegates, so refresh whenever there
	// is an undo/redo.
	RefreshMemberActions();
}

void SMovieGraphMembersTabContent::PostRedo(bool bSuccess)
{
	RefreshMemberActions();
}

void SMovieGraphMembersTabContent::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	static const FText UserVariablesCategory = LOCTEXT("UserVariablesCategory", "User Variables");
	static const FText GlobalVariablesCategory = LOCTEXT("GlobalVariablesCategory", "Global Variables");
	static const FText EmptyCategory = FText::GetEmpty();
	
	if (!CurrentGraph)
	{
		return;
	}

	FGraphActionMenuBuilder ActionMenuBuilder;

	// Creates a new action in the action menu under a specific section w/ the provided action target
	auto AddToActionMenu = [&ActionMenuBuilder, this](UMovieGraphMember* ActionTarget, const EActionSection Section, const FText& Category) -> void
	{
		const FText MemberActionDesc = FText::FromString(ActionTarget->GetMemberName());
		const FText MemberActionTooltip;
		const FText MemberActionKeywords;
		const int32 MemberActionSectionID = static_cast<int32>(Section);
		const TSharedPtr<FMovieGraphSchemaAction> MemberAction(new FMovieGraphSchemaAction(Category, MemberActionDesc, MemberActionTooltip, 0, MemberActionKeywords, MemberActionSectionID));
		MemberAction->ActionTarget = ActionTarget;
		ActionMenuBuilder.AddAction(MemberAction);

		// Update actions when a member is updated (renamed, etc). Only subscribe to the delegate once.
		if (!MemberChangedHandles.Contains(ActionTarget))
		{
			FDelegateHandle MemberChangedDelegate;
			if (UMovieGraphInput* InputMember = Cast<UMovieGraphInput>(ActionTarget))
			{
				MemberChangedDelegate = InputMember->OnMovieGraphInputChangedDelegate.AddSP(this, &SMovieGraphMembersTabContent::RefreshMemberActions);
			}
			else if (UMovieGraphOutput* OutputMember = Cast<UMovieGraphOutput>(ActionTarget))
			{
				MemberChangedDelegate = OutputMember->OnMovieGraphOutputChangedDelegate.AddSP(this, &SMovieGraphMembersTabContent::RefreshMemberActions);
			}
			else if (UMovieGraphVariable* VariableMember = Cast<UMovieGraphVariable>(ActionTarget))
			{
				MemberChangedDelegate = VariableMember->OnMovieGraphVariableChangedDelegate.AddSP(this, &SMovieGraphMembersTabContent::RefreshMemberActions);
			}
			else
			{
				checkf(false, TEXT("Found an unsupported member type when adding it to the action menu."));
				return;
			}
			
			MemberChangedHandles.Add(ActionTarget, MemberChangedDelegate);
		}
	};

	for (UMovieGraphInput* Input : CurrentGraph->GetInputs())
	{
		if (Input && Input->IsDeletable())
		{
			AddToActionMenu(Input, EActionSection::Inputs, EmptyCategory);
		}
	}

	for (UMovieGraphOutput* Output : CurrentGraph->GetOutputs())
	{
		if (Output && Output->IsDeletable())
		{
			AddToActionMenu(Output, EActionSection::Outputs, EmptyCategory);
		}
	}

	const bool bIncludeGlobal = true;
	const TArray<UMovieGraphVariable*> AllVariables = CurrentGraph->GetVariables(bIncludeGlobal);

	// Add non-global variables first
	for (UMovieGraphVariable* Variable : AllVariables)
	{
		if (Variable && !Variable->IsGlobal())
		{
			AddToActionMenu(Variable, EActionSection::Variables, UserVariablesCategory);
		}
	}

	// Add global variables after user-declared variables
	for (UMovieGraphVariable* Variable : AllVariables)
	{
		if (Variable && Variable->IsGlobal())
		{
			AddToActionMenu(Variable, EActionSection::Variables, GlobalVariablesCategory);
		}
	}
	
	OutAllActions.Append(ActionMenuBuilder);
}

bool SMovieGraphMembersTabContent::ActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const
{
	if (const UMovieGraphMember* Member = UE::MovieGraph::Private::GetMemberFromAction(InAction))
	{
		return InName == Member->GetMemberName();
	}

	return false;
}

void SMovieGraphMembersTabContent::CollectStaticSections(TArray<int32>& StaticSectionIDs)
{
	// Start at index 1 to skip the invalid section
	for (int32 Index = 1; Index < static_cast<int32>(EActionSection::COUNT); ++Index)
	{
		StaticSectionIDs.Add(Index);
	}
}

FText SMovieGraphMembersTabContent::GetSectionTitle(int32 InSectionID)
{
	if (ensure(ActionMenuSectionNames.IsValidIndex(InSectionID)))
	{
		return ActionMenuSectionNames[InSectionID];
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> SMovieGraphMembersTabContent::GetSectionWidget(TSharedRef<SWidget> RowWidget, int32 InSectionID)
{
	FText ButtonTooltip;

	switch (static_cast<EActionSection>(InSectionID))
	{
	case EActionSection::Inputs:
		ButtonTooltip = LOCTEXT("ButtonTooltip_Inputs", "Add Input");
		break;

	case EActionSection::Outputs:
		ButtonTooltip = LOCTEXT("ButtonTooltip_Outputs", "Add Output");
		break;

	case EActionSection::Variables:
		ButtonTooltip = LOCTEXT("ButtonTooltip_Variables", "Add Variable");
		break;

	default:
		return SNullWidget::NullWidget;
	}
	
	return
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.OnClicked(this, &SMovieGraphMembersTabContent::OnAddButtonClickedOnSection, InSectionID)
		.ContentPadding(FMargin(1, 0))
		.ToolTipText(ButtonTooltip)
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		];
}

TSharedPtr<SWidget> SMovieGraphMembersTabContent::OnContextMenuOpening()
{
	if (!ActionMenu.IsValid())
	{
		return nullptr;
	}

	TArray<TSharedPtr<FEdGraphSchemaAction>> SelectedActions;
	ActionMenu->GetSelectedActions(SelectedActions);
	if (SelectedActions.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr< FAssetEditorToolkit> PinnedToolkit = EditorToolkit.Pin();
	if(!PinnedToolkit.IsValid())
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, PinnedToolkit->GetToolkitCommands());
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
	
	return MenuBuilder.MakeWidget();
}

FReply SMovieGraphMembersTabContent::OnAddButtonClickedOnSection(const int32 InSectionID)
{
	const EActionSection Section = static_cast<EActionSection>(InSectionID);

	if (Section == EActionSection::Inputs)
	{
		FScopedTransaction Transaction(LOCTEXT("AddNewInput", "Add New Input"));
		CurrentGraph->AddInput();
	}
	else if (Section == EActionSection::Outputs)
	{
		FScopedTransaction Transaction(LOCTEXT("AddNewOutput", "Add New Output"));
		CurrentGraph->AddOutput();
	}
	else if (Section == EActionSection::Variables)
	{
		FScopedTransaction Transaction(LOCTEXT("AddNewVariable", "Add New Variable"));
		CurrentGraph->AddVariable();
	}

	RefreshMemberActions();

	return FReply::Handled();
}

void SMovieGraphMembersTabContent::RefreshMemberActions(UMovieGraphMember* UpdatedMember)
{
	// Currently the entire action menu is refreshed rather than a specific action being targeted

	// Cache the currently selected member, so it can be referenced after the refresh
	const UMovieGraphMember* SelectedMember = nullptr;
	TArray<TSharedPtr<FEdGraphSchemaAction>> SelectedActions;
	ActionMenu->GetSelectedActions(SelectedActions);
	if (!SelectedActions.IsEmpty())
	{
		if (const UMovieGraphMember* Member = UE::MovieGraph::Private::GetMemberFromAction(SelectedActions[0].Get()))
		{
			SelectedMember = Member;
		}
	}

	// Do the refresh
	const bool bPreserveExpansion = true;
	ActionMenu->RefreshAllActions(bPreserveExpansion);

	// Re-select the updated member if it was previously selected. The action menu performs a reselect after a refresh
	// automatically based on action name, but the member may have been renamed (so an explicit re-select is needed).
	if (SelectedMember && (SelectedMember == UpdatedMember))
	{
		ActionMenu->SelectItemByName(FName(SelectedMember->GetMemberName()));
	}
}

TSharedRef<FMovieGraphDragAction_Variable> FMovieGraphDragAction_Variable::New(
	TSharedPtr<FEdGraphSchemaAction> InAction, UMovieGraphVariable* InVariable)
{
	TSharedRef<FMovieGraphDragAction_Variable> DragAction = MakeShared<FMovieGraphDragAction_Variable>();
	DragAction->SourceAction = InAction;
	DragAction->WeakVariable = InVariable;
	DragAction->Construct();
	
	return DragAction;
}

void FMovieGraphDragAction_Variable::HoverTargetChanged()
{
	FGraphSchemaActionDragDropAction::HoverTargetChanged();
}

FReply FMovieGraphDragAction_Variable::DroppedOnPanel(
	const TSharedRef<SWidget>& InPanel, FVector2D InScreenPosition, FVector2D InGraphPosition, UEdGraph& InGraph)
{
	const UMovieGraphVariable* VariableMember = WeakVariable.Get();
	if (!VariableMember)
	{
		return FReply::Unhandled();
	}

	// When creating the new action, since it's only being used to create a node, the category, display name, and tooltip can just be empty
	const TSharedPtr<FMovieGraphSchemaAction> NewAction = MakeShared<FMovieGraphSchemaAction_NewVariableNode>(
		FText::GetEmpty(), FText::GetEmpty(), VariableMember->GetGuid(), FText::GetEmpty());
	NewAction->NodeClass = UMovieGraphVariableNode::StaticClass();
	
	constexpr UEdGraphPin* FromPin = nullptr;
	constexpr bool bSelectNewNode = true;
	NewAction->PerformAction(&InGraph, FromPin, InGraphPosition, bSelectNewNode);

	return FReply::Handled();
}

void FMovieGraphDragAction_Variable::GetDefaultStatusSymbol(
	const FSlateBrush*& OutPrimaryBrush, FSlateColor& OutIconColor, const FSlateBrush*& OutSecondaryBrush, FSlateColor& OutSecondaryColor) const
{
	const UMovieGraphVariable* VariableMember = WeakVariable.Get();
	if (!VariableMember ||
		!UE::MovieGraph::Private::GetIconAndColorFromDataType(VariableMember, OutPrimaryBrush, OutIconColor, OutSecondaryBrush, OutSecondaryColor))
	{
		return FGraphSchemaActionDragDropAction::GetDefaultStatusSymbol(OutPrimaryBrush, OutIconColor, OutSecondaryBrush, OutSecondaryColor);
	}
}

#undef LOCTEXT_NAMESPACE