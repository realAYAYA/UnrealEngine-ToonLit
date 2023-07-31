// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMeshEditorModule.h"

#include "AssetTypeActions_NaniteDisplacedMesh.h"
#include "IAssetTools.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleManager.h"
#include "NaniteDisplacedMeshCustomization.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "UObject/Package.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "NaniteDisplacedMeshEditor"


void FNaniteDisplacedMeshEditorModule::StartupModule()
{
	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();

	NaniteDisplacedMeshAssetActions = new FAssetTypeActions_NaniteDisplacedMesh();
	AssetTools.RegisterAssetTypeActions(MakeShareable(NaniteDisplacedMeshAssetActions));

	// The procedural tools flow use this transient package to avoid name collision with other transient object
	NaniteDisplacedMeshTransientPackage = NewObject<UPackage>(nullptr, TEXT("/Engine/Transient/NaniteDisplacedMesh"), RF_Transient);
	NaniteDisplacedMeshTransientPackage->AddToRoot();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(TEXT("NaniteDisplacedMesh"), FOnGetDetailCustomizationInstance::CreateStatic(&FNaniteDisplacedMeshDetails::MakeInstance));
}
	
void FNaniteDisplacedMeshEditorModule::ShutdownModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout(TEXT("NaniteDisplacedMesh"));

	NaniteDisplacedMeshTransientPackage->RemoveFromRoot();
}

FNaniteDisplacedMeshEditorModule& FNaniteDisplacedMeshEditorModule::GetModule()
{
	static const FName ModuleName = "NaniteDisplacedMeshEditor";
	return FModuleManager::LoadModuleChecked<FNaniteDisplacedMeshEditorModule>(ModuleName);
}

UPackage* FNaniteDisplacedMeshEditorModule::GetNaniteDisplacementMeshTransientPackage() const
{
	return NaniteDisplacedMeshTransientPackage;
}

IMPLEMENT_MODULE(FNaniteDisplacedMeshEditorModule, NaniteDisplacedMeshEditor);

#undef LOCTEXT_NAMESPACE
