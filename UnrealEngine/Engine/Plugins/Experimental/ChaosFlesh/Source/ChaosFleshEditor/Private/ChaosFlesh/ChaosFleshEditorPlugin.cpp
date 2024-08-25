// Copyright Epic Games, Inc. All Rights Reserved.


#include "ChaosFlesh/ChaosFleshEditorPlugin.h"

#include "ChaosFlesh/Asset/AssetDefinition_FleshAsset.h"
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

	if (GIsEditor && !IsRunningCommandlet())
	{
		EditorCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("ChaosDeformable.ImportFile"),
			TEXT("Creates a FleshAsset from the input file"),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FChaosFleshCommands::ImportFile),
			ECVF_Default
		));

		EditorCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("ChaosDeformable.FindQualifyingTetrahedra"),
			TEXT("From the selected actor's flesh components, prints indices of tetrahedra matching our search criteria. "
				"Use arg 'MinVol <value>' to specify a minimum tet volume; "
				"use arg 'MaxAR <value>' to specify a maximum aspect ratio; "
				"use 'XCoordGT <value>', 'YCoordGT <value>', 'ZCoordGT <value>' to select tets with all vertices greater than the specified value; "
				"use 'XCoordLT <value>', 'YCoordLT <value>', 'ZCoordLT <value>' to select tets with all vertices less than the specified value; "
				"use 'HideTets' to add indices to the flesh component's list of tets to skip drawing."),
			FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FChaosFleshCommands::FindQualifyingTetrahedra),
			ECVF_Default
		));
	}

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
}

IMPLEMENT_MODULE(IChaosFleshEditorPlugin, ChaosFleshEditor)


#undef LOCTEXT_NAMESPACE
