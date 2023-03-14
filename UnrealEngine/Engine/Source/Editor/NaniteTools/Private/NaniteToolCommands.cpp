// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteToolCommands.h"
#include "Engine/StaticMesh.h"
#include "Engine/InstancedStaticMesh.h"
#include "StaticMeshCompiler.h"

#define LOCTEXT_NAMESPACE "NaniteToolCommands"

void FNaniteToolCommands::RegisterCommands()
{
	UI_COMMAND(ShowInContentBrowser, "Show in Content Browser", "Shows the selected static mesh in the Content Browser.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(EnableNanite, "Enable Nanite", "Enables Nanite on selected meshes.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(DisableNanite, "Disable Nanite", "Disables Nanite on selected meshes.", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

void ModifyNaniteEnable(TArray<TWeakObjectPtr<UStaticMesh>>& MeshesToProcess, bool bNaniteEnable)
{
	TArray<UStaticMesh*> Meshes;
	Meshes.Reserve(MeshesToProcess.Num());

	for (TWeakObjectPtr<UStaticMesh>& StaticMeshPtr : MeshesToProcess)
	{
		UStaticMesh* Mesh = StaticMeshPtr.Get();
		if (Mesh && Mesh->NaniteSettings.bEnabled != bNaniteEnable)
		{
			Meshes.Add(Mesh);
		}
	}

	FStaticMeshCompilingManager::Get().FinishCompilation(Meshes);

	for (UStaticMesh* Mesh : Meshes)
	{
		Mesh->NaniteSettings.bEnabled = bNaniteEnable;
	}

	UStaticMesh::BatchBuild(Meshes);

	for (UStaticMesh* Mesh : Meshes)
	{
		Mesh->MarkPackageDirty();
		Mesh->OnMeshChanged.Broadcast();
	}
}