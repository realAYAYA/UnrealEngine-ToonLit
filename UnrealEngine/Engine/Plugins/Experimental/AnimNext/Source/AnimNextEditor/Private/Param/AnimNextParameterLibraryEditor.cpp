// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextParameterLibraryEditor.h"
#include "ParameterLibraryEditorMode.h"
#include "Param/AnimNextParameterLibrary.h"
#include "ExternalPackageHelper.h"
#include "Graph/SActionMenu.h"
#include "UncookedOnlyUtils.h"
#include "Framework/Commands/GenericCommands.h"
#include "ScopedTransaction.h"
#include "Param/AnimNextParameterSettings.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterLibraryEditor"

namespace UE::AnimNext::Editor
{

namespace ParameterLibraryModes
{
	const FName ParameterLibraryEditor("AnimNextParameterLibraryEditorMode");
}

namespace ParameterLibraryTabs
{
	const FName Details("DetailsTab");
	const FName Parameters("ParametersTab");
}

const FName ParameterLibraryAppIdentifier("AnimNextParameterLibraryEditor");

FParameterLibraryEditor::FParameterLibraryEditor()
{
	UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
	Settings->LoadConfig();
}

FParameterLibraryEditor::~FParameterLibraryEditor()
{
	UAnimNextParameterSettings* Settings = GetMutableDefault<UAnimNextParameterSettings>();
	Settings->SaveConfig();
}

void FParameterLibraryEditor::InitEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InInitToolkitHost, UAnimNextParameterLibrary* InAnimNextParameterLibrary)
{
	ParameterLibrary = InAnimNextParameterLibrary;

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(InMode, InInitToolkitHost, ParameterLibraryAppIdentifier, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InAnimNextParameterLibrary);

	BindCommands();

	AddApplicationMode(ParameterLibraryModes::ParameterLibraryEditor, MakeShared<FParameterLibraryEditorMode>(SharedThis(this)));
	SetCurrentMode(ParameterLibraryModes::ParameterLibraryEditor);

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FParameterLibraryEditor::BindCommands()
{
}

void FParameterLibraryEditor::ExtendMenu()
{
	
}

void FParameterLibraryEditor::ExtendToolbar()
{
	
}

FName FParameterLibraryEditor::GetToolkitFName() const
{
	return FName("AnimNextParameterLibraryEditor");
}

FText FParameterLibraryEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "AnimNextParameterLibraryEditor");
}

FString FParameterLibraryEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimNextParameterLibraryEditor ").ToString();
}

FLinearColor FParameterLibraryEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

void FParameterLibraryEditor::InitToolMenuContext(FToolMenuContext& InMenuContext)
{
}

void FParameterLibraryEditor::SetSelectedObjects(TArray<UObject*> InObjects)
{
	if(DetailsView.IsValid())
	{
		DetailsView->SetObjects(InObjects);
	}
}

void FParameterLibraryEditor::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	// Base class will pick up edited object
	FWorkflowCentricApplication::GetSaveableObjects(OutObjects);

	// Get external objects too
	FExternalPackageHelper::GetExternalSaveableObjects(ParameterLibrary, OutObjects);
}

}

#undef LOCTEXT_NAMESPACE