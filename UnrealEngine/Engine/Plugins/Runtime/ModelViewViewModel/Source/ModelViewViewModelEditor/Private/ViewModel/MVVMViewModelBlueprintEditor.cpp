// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModel/MVVMViewModelBlueprintEditor.h"
#include "ViewModel/MVVMViewModelBlueprint.h"
#include "ViewModel/MVVMViewModelBlueprintToolMenuContext.h"

#define LOCTEXT_NAMESPACE "MVVMViewModelBlueprintEditor"

void FMVVMViewModelBlueprintEditor::InitViewModelBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode)
{
	Super::InitBlueprintEditor(Mode, InitToolkitHost, InBlueprints, bShouldOpenInDefaultsMode);
}


UMVVMViewModelBlueprint* FMVVMViewModelBlueprintEditor::GetViewModelBlueprintObj() const
{
	return Cast<UMVVMViewModelBlueprint>(GetBlueprintObj());
}


FName FMVVMViewModelBlueprintEditor::GetToolkitFName() const
{
	return FName("ViewModelBlueprintEditor");
}


FText FMVVMViewModelBlueprintEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Viewmodel Editor");
}


FString FMVVMViewModelBlueprintEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Viewmodel Editor ").ToString();
}


FLinearColor FMVVMViewModelBlueprintEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.25f, 0.35f, 0.5f);
}


void FMVVMViewModelBlueprintEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	Super::InitToolMenuContext(MenuContext);

	UMVVMViewModelBlueprintToolMenuContext* Context = NewObject<UMVVMViewModelBlueprintToolMenuContext>();
	Context->ViewModelBlueprintEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}


void FMVVMViewModelBlueprintEditor::RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated/* = false*/)
{
	Super::RegisterApplicationModes(InBlueprints, bShouldOpenInDefaultsMode);
}


FGraphAppearanceInfo FMVVMViewModelBlueprintEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = Super::GetGraphAppearance(InGraph);

	if (GetBlueprintObj()->IsA(UMVVMViewModelBlueprint::StaticClass()))
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "VIEWMODEL BLUEPRINT");
	}

	return AppearanceInfo;
}


bool FMVVMViewModelBlueprintEditor::NewDocument_IsVisibleForType(ECreatedDocumentType InGraphType) const
{
	switch (InGraphType)
	{
	case CGT_NewVariable:
	case CGT_NewFunctionGraph:
		return true;
	default:
		break;
	}
	return false;
}


bool FMVVMViewModelBlueprintEditor::IsSectionVisible(NodeSectionID::Type InSectionID) const
{
	switch (InSectionID)
	{
	case NodeSectionID::VARIABLE:
	case NodeSectionID::FUNCTION:
	case NodeSectionID::LOCAL_VARIABLE:
		return true;
	default:
		break;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
