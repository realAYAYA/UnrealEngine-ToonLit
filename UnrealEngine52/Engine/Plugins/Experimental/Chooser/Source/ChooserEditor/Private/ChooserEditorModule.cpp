// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChooserEditorModule.h"
#include "IAssetTools.h"
#include "ChooserTableEditor.h"
#include "BoolColumnEditor.h"
#include "EnumColumnEditor.h"
#include "FloatRangeColumnEditor.h"
#include "GameplayTagColumnEditor.h"

#define LOCTEXT_NAMESPACE "ChooserEditorModule"

namespace UE::ChooserEditor
{

void FModule::StartupModule()
{
	FChooserTableEditor::RegisterWidgets();
	RegisterGameplayTagWidgets();
	RegisterFloatRangeWidgets();
	RegisterBoolWidgets();
	RegisterEnumWidgets();
}

void FModule::ShutdownModule()
{
}

}

IMPLEMENT_MODULE(UE::ChooserEditor::FModule, ChooserEditor);

#undef LOCTEXT_NAMESPACE