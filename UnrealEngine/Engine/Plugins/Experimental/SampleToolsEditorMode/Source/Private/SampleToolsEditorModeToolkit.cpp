// Copyright Epic Games, Inc. All Rights Reserved.

#include "SampleToolsEditorModeToolkit.h"
#include "SampleToolsEditorMode.h"
#include "Engine/Selection.h"

#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"



#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "FSampleToolsEditorModeToolkit"

FSampleToolsEditorModeToolkit::FSampleToolsEditorModeToolkit()
{
}

void FSampleToolsEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FSampleToolsEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}


FName FSampleToolsEditorModeToolkit::GetToolkitFName() const
{
	return FName("SampleToolsEditorMode");
}

FText FSampleToolsEditorModeToolkit::GetBaseToolkitName() const
{
	return NSLOCTEXT("SampleToolsEditorModeToolkit", "DisplayName", "SampleToolsEditorMode Tool");
}

#undef LOCTEXT_NAMESPACE
