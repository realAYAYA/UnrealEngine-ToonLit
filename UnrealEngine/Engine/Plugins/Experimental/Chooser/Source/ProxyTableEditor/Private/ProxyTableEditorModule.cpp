// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProxyTableEditorModule.h"
#include "ProxyTableEditor.h"
#include "ProxyTableEditorCommands.h"
#include "StructOutputDataCustomization.h"
#include "PropertyEditorModule.h"
#include "ProxyTableEditorStyle.h"

#define LOCTEXT_NAMESPACE "ProxyTableEditorModule"

namespace UE::ProxyTableEditor
{

void FModule::StartupModule()
{
	FProxyTableEditorStyle::Initialize();
	FProxyTableEditor::RegisterWidgets();

	FProxyTableEditorCommands::Register();
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(FProxyStructOutput::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FStructOutputDataCustomization>(); }));
}

void FModule::ShutdownModule()
{
	FProxyTableEditorCommands::Unregister();
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomPropertyTypeLayout(FProxyStructOutput::StaticStruct()->GetFName());
	
	FProxyTableEditorStyle::Shutdown();
}

}

IMPLEMENT_MODULE(UE::ProxyTableEditor::FModule, ProxyTableEditor);

#undef LOCTEXT_NAMESPACE