// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_ModifyCurve.h"
#include "Textures/SlateIcon.h"
#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "Kismet2/CompilerResultsLog.h"
#include "AnimationGraphSchema.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "Framework/Commands/UIAction.h"
#include "ToolMenus.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "ModifyCurve"


UAnimGraphNode_ModifyCurve::UAnimGraphNode_ModifyCurve() 
{
}

FText UAnimGraphNode_ModifyCurve::GetMenuCategory() const
{
	return LOCTEXT("AnimGraphNode_ModifyCurve_Category", "Animation|Curves");
}

FText UAnimGraphNode_ModifyCurve::GetTooltipText() const
{
	return GetNodeTitle(ENodeTitleType::ListView);
}

FText UAnimGraphNode_ModifyCurve::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("AnimGraphNode_ModifyCurve_Title", "Modify Curve");
}


TArray<FName> UAnimGraphNode_ModifyCurve::GetCurvesToAdd() const
{
	TArray<FName> CurvesToAdd;

	const FSmartNameMapping* Mapping = GetAnimBlueprint()->TargetSkeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	if (Mapping)
	{
		Mapping->FillNameArray(CurvesToAdd);

		for (FName ExistingCurveName : Node.CurveNames)
		{
			CurvesToAdd.RemoveSingleSwap(ExistingCurveName, false);
		}

		CurvesToAdd.Sort(FNameLexicalLess());
	}

	return CurvesToAdd;
}

void UAnimGraphNode_ModifyCurve::GetAddCurveMenuActions(FMenuBuilder& MenuBuilder) const
{
	TArray<FName> CurvesToAdd = GetCurvesToAdd();
	for (FName CurveName : CurvesToAdd)
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateUObject(const_cast<UAnimGraphNode_ModifyCurve*>(this), &UAnimGraphNode_ModifyCurve::AddCurvePin, CurveName));
		MenuBuilder.AddMenuEntry(FText::FromName(CurveName), FText::GetEmpty(), FSlateIcon(), Action);
	}
}

void UAnimGraphNode_ModifyCurve::GetRemoveCurveMenuActions(FMenuBuilder& MenuBuilder) const
{
	for (FName CurveName : Node.CurveNames)
	{
		FUIAction Action = FUIAction(FExecuteAction::CreateUObject(const_cast<UAnimGraphNode_ModifyCurve*>(this), &UAnimGraphNode_ModifyCurve::RemoveCurvePin, CurveName));
		MenuBuilder.AddMenuEntry(FText::FromName(CurveName), FText::GetEmpty(), FSlateIcon(), Action);
	}
}


void UAnimGraphNode_ModifyCurve::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		FToolMenuSection& Section = Menu->AddSection("AnimGraphNodeModifyCurve", LOCTEXT("ModifyCurve", "Modify Curve"));

		// Clicked pin
		if (Context->Pin != NULL)
		{
			// Get property from pin
			FProperty* AssociatedProperty;
			int32 ArrayIndex;
			GetPinAssociatedProperty(GetFNodeType(), Context->Pin, /*out*/ AssociatedProperty, /*out*/ ArrayIndex);

			if (AssociatedProperty != nullptr)
			{
				FName PinPropertyName = AssociatedProperty->GetFName();

				if (PinPropertyName == GET_MEMBER_NAME_CHECKED(FAnimNode_ModifyCurve, CurveValues) && Context->Pin->Direction == EGPD_Input)
				{
					FString PinName = Context->Pin->PinFriendlyName.ToString();
					FUIAction Action = FUIAction(FExecuteAction::CreateUObject(const_cast<UAnimGraphNode_ModifyCurve*>(this), &UAnimGraphNode_ModifyCurve::RemoveCurvePin, FName(*PinName)));
					FText RemovePinLabelText = FText::Format(LOCTEXT("RemoveThisPin", "Remove This Curve Pin: {0}"), FText::FromString(PinName));
					Section.AddMenuEntry("RemoveThisPin", RemovePinLabelText, LOCTEXT("RemoveThisPinTooltip", "Remove this curve pin from this node"), FSlateIcon(), Action);
				}
			}
		}

		// If we have more curves to add, create submenu to offer them
		if (GetCurvesToAdd().Num() > 0)
		{
			Section.AddSubMenu(
				"AddCurvePin",
				LOCTEXT("AddCurvePin", "Add Curve Pin"),
				LOCTEXT("AddCurvePinTooltip", "Add a new pin to drive a curve"),
				FNewMenuDelegate::CreateUObject(this, &UAnimGraphNode_ModifyCurve::GetAddCurveMenuActions));
		}

		// If we have curves to remove, create submenu to offer them
		if (Node.CurveNames.Num() > 0)
		{
			Section.AddSubMenu(
				"RemoveCurvePin",
				LOCTEXT("RemoveCurvePin", "Remove Curve Pin"),
				LOCTEXT("RemoveCurvePinTooltip", "Remove a pin driving a curve"),
				FNewMenuDelegate::CreateUObject(this, &UAnimGraphNode_ModifyCurve::GetRemoveCurveMenuActions));
		}
	}
}

void UAnimGraphNode_ModifyCurve::RemoveCurvePin(FName CurveName)
{
	// Make sure we have a curve pin with that name
	int32 CurveIndex = Node.CurveNames.Find(CurveName);
	if (CurveIndex != INDEX_NONE)
	{
		FScopedTransaction Transaction( LOCTEXT("RemoveCurvePinTrans", "Remove Curve Pin") );
		Modify();

		Node.RemoveCurve(CurveIndex);
	
		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

void UAnimGraphNode_ModifyCurve::AddCurvePin(FName CurveName)
{
	// Make sure it doesn't already exist
	int32 CurveIndex = Node.CurveNames.Find(CurveName);
	if (CurveIndex == INDEX_NONE)
	{
		FScopedTransaction Transaction(LOCTEXT("AddCurvePinTrans", "Add Curve Pin"));
		Modify();

		Node.AddCurve(CurveName, 0.f);

		ReconstructNode();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}


void UAnimGraphNode_ModifyCurve::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	if (SourcePropertyName == GET_MEMBER_NAME_CHECKED(FAnimNode_ModifyCurve, CurveValues))
	{
		if (Node.CurveNames.IsValidIndex(ArrayIndex))
		{
			Pin->PinFriendlyName = FText::FromName(Node.CurveNames[ArrayIndex]);
		}
	}
}

#undef LOCTEXT_NAMESPACE
