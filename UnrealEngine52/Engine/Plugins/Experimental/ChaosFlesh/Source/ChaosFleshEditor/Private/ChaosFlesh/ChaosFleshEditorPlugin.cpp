// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChaosFlesh/ChaosFleshEditorPlugin.h"

#include "ChaosFlesh/Asset/AssetTypeActions_FleshAsset.h"
#include "ChaosFlesh/Asset/FleshDeformableInterfaceDetails.h"
#include "ChaosFlesh/Asset/FleshAssetThumbnailRenderer.h"
#include "ChaosFlesh/ChaosDeformableCollisionsActor.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "ChaosFlesh/Cmd/ChaosFleshCommands.h"
#include "ChaosFlesh/FleshActor.h"
#include "ChaosFlesh/FleshAsset.h"
#include "ChaosFlesh/FleshComponent.h"
#include "Editor/FleshEditorStyle.h"

#define LOCTEXT_NAMESPACE "FleshEditor"

#define BOX_BRUSH(StyleSet, RelativePath, ...) FSlateBoxBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

void IChaosFleshEditorPlugin::StartupModule()
{
	FChaosFleshEditorStyle::Get();

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

	FleshAssetActions = new FAssetTypeActions_FleshAsset();
	AssetTools.RegisterAssetTypeActions(MakeShareable(FleshAssetActions));

	if (GIsEditor && !IsRunningCommandlet())
	{
		EditorCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("p.Chaos.Flesh.ImportFile"),
			TEXT("Creates a FleshAsset from the input file"),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FChaosFleshCommands::ImportFile),
			ECVF_Default
		));
	}

	UThumbnailManager::Get().RegisterCustomRenderer(UFleshAsset::StaticClass(), UFleshAssetThumbnailRenderer::StaticClass());

	// register details customization
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomClassLayout(
		ADeformableCollisionsActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDeformableInterfaceDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(
		ADeformableSolverActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDeformableInterfaceDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(
		AFleshActor::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDeformableInterfaceDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(
		UDeformablePhysicsComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDeformableInterfaceDetails::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(
		UDeformableSolverComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FDeformableInterfaceDetails::MakeInstance));

}

void IChaosFleshEditorPlugin::ShutdownModule()
{
	if (UObjectInitialized())
	{	
		UThumbnailManager::Get().UnregisterCustomRenderer(UFleshAsset::StaticClass());

		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		AssetTools.UnregisterAssetTypeActions(FleshAssetActions->AsShared());
	}
}

IMPLEMENT_MODULE(IChaosFleshEditorPlugin, FleshAssetEditor)


#undef LOCTEXT_NAMESPACE
