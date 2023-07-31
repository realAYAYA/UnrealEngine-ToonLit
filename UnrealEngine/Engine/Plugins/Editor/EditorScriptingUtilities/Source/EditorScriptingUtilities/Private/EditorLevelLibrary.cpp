// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorLevelLibrary.h"

#include "EditorScriptingUtils.h"

#include "ActorEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/MeshComponent.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "EngineUtils.h"
#include "Engine/Brush.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "IMeshMergeUtilities.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Layers/LayersSubsystem.h"
#include "LevelEditorViewport.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "MeshMergeModule.h"
#include "ScopedTransaction.h"
#include "UnrealEdGlobals.h"
#include "LevelEditor.h"
#include "IAssetViewport.h"
#include "SLevelViewport.h"
#include "AssetRegistry/AssetData.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactories/ActorFactoryBlueprint.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "LevelEditorSubsystem.h"
#include "Subsystems/UnrealEditorSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorLevelLibrary)

#define LOCTEXT_NAMESPACE "EditorLevelLibrary"


namespace InternalEditorLevelLibrary
{
	FJoinStaticMeshActorsOptions ConvertJoinStaticMeshActorsOptions(const FEditorScriptingJoinStaticMeshActorsOptions_Deprecated &Options)
	{
		FJoinStaticMeshActorsOptions JoinOptions;

		JoinOptions.bDestroySourceActors = Options.bDestroySourceActors;
		JoinOptions.NewActorLabel = Options.NewActorLabel;
		JoinOptions.bRenameComponentsFromSource = Options.bRenameComponentsFromSource;

		return JoinOptions;
		}

	FMergeStaticMeshActorsOptions ConvertMergeStaticMeshActorsOptions(const FEditorScriptingMergeStaticMeshActorsOptions_Deprecated& Options)
	{
		FMergeStaticMeshActorsOptions MergeOptions;

		MergeOptions.bDestroySourceActors = Options.bDestroySourceActors;
		MergeOptions.NewActorLabel = Options.NewActorLabel;
		MergeOptions.bRenameComponentsFromSource = Options.bRenameComponentsFromSource;

		MergeOptions.bSpawnMergedActor = Options.bSpawnMergedActor;
		MergeOptions.BasePackageName = Options.BasePackageName;
		MergeOptions.MeshMergingSettings = Options.MeshMergingSettings;

		return MergeOptions;
	}

	FCreateProxyMeshActorOptions ConvertCreateProxyStaticMeshActorsOptions(const FEditorScriptingCreateProxyMeshActorOptions_Deprecated& Options)
	{
		FCreateProxyMeshActorOptions CreateProxyOptions;

		CreateProxyOptions.bDestroySourceActors = Options.bDestroySourceActors;
		CreateProxyOptions.NewActorLabel = Options.NewActorLabel;
		CreateProxyOptions.bRenameComponentsFromSource = Options.bRenameComponentsFromSource;

		CreateProxyOptions.bSpawnMergedActor = Options.bSpawnMergedActor;
		CreateProxyOptions.BasePackageName = Options.BasePackageName;
		CreateProxyOptions.MeshProxySettings = Options.MeshProxySettings;

		return CreateProxyOptions;
	}
}

TArray<AActor*> UEditorLevelLibrary::GetAllLevelActors()
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->GetAllLevelActors() : TArray<AActor*>();
}

TArray<UActorComponent*> UEditorLevelLibrary::GetAllLevelActorsComponents()
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->GetAllLevelActorsComponents() : TArray<UActorComponent*>();
}

TArray<AActor*> UEditorLevelLibrary::GetSelectedLevelActors()
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->GetSelectedLevelActors() : TArray<AActor*>();
}

void UEditorLevelLibrary::SetSelectedLevelActors(const TArray<class AActor*>& ActorsToSelect)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	if (EditorActorSubsystem)
	{
		return EditorActorSubsystem->SetSelectedLevelActors(ActorsToSelect);
		}
}

void UEditorLevelLibrary::PilotLevelActor(AActor* ActorToPilot)
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	if (LevelEditorSubsystem)
	{
		return LevelEditorSubsystem->PilotLevelActor(ActorToPilot);
	}
}

void UEditorLevelLibrary::EjectPilotLevelActor()
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	if (LevelEditorSubsystem)
	{
		return LevelEditorSubsystem->EjectPilotLevelActor();
	}
}


bool UEditorLevelLibrary::GetLevelViewportCameraInfo(FVector& CameraLocation, FRotator& CameraRotation)
{
	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	return UnrealEditorSubsystem ? UnrealEditorSubsystem->GetLevelViewportCameraInfo(CameraLocation, CameraRotation) : false;
}


void UEditorLevelLibrary::SetLevelViewportCameraInfo(FVector CameraLocation, FRotator CameraRotation)
{
	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	if (UnrealEditorSubsystem)
	{
		return UnrealEditorSubsystem->SetLevelViewportCameraInfo(CameraLocation, CameraRotation);
		}
}

void UEditorLevelLibrary::ClearActorSelectionSet()
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	if (EditorActorSubsystem)
	{
		return EditorActorSubsystem->ClearActorSelectionSet();
	}
}

void UEditorLevelLibrary::SelectNothing()
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	if (EditorActorSubsystem)
	{
		return EditorActorSubsystem->SelectNothing();
	}
}

void UEditorLevelLibrary::SetActorSelectionState(AActor* Actor, bool bShouldBeSelected)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	if (EditorActorSubsystem)
	{
		return EditorActorSubsystem->SetActorSelectionState(Actor, bShouldBeSelected);
	}
}

AActor* UEditorLevelLibrary::GetActorReference(FString PathToActor)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->GetActorReference(PathToActor) : nullptr;
}

void UEditorLevelLibrary::EditorSetGameView(bool bGameView)
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	if (LevelEditorSubsystem)
	{
		return LevelEditorSubsystem->EditorSetGameView(bGameView);
	}
}

void UEditorLevelLibrary::EditorPlaySimulate()
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	if (LevelEditorSubsystem)
	{
		return LevelEditorSubsystem->EditorPlaySimulate();
	}
}

void UEditorLevelLibrary::EditorEndPlay()
{
	GUnrealEd->RequestEndPlayMap();
}

void UEditorLevelLibrary::EditorInvalidateViewports()
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	if (LevelEditorSubsystem)
	{
		return LevelEditorSubsystem->EditorInvalidateViewports();
	}
}

void UEditorLevelLibrary::ReplaceSelectedActors(const FString& InAssetPath)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");

	if (InAssetPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("ReplaceSelectedActors: Asset path is empty."));
		return;
	}

	FString OutFailureReason;
	FAssetData AssetData = EditorScriptingUtils::FindAssetDataFromAnyPath(InAssetPath, OutFailureReason);
	if (!AssetData.IsValid())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("ReplaceSelectedActors: Asset path not found: %s. %s"), *InAssetPath, *OutFailureReason);
		return;
	}

	if (UClass* AssetClass = AssetData.GetClass())
	{
		UActorFactory* Factory = nullptr;

		if (AssetClass->IsChildOf(UBlueprint::StaticClass()))
		{
			Factory = GEditor->FindActorFactoryByClass(UActorFactoryBlueprint::StaticClass());
		}
		else
		{
			const TArray<UActorFactory*>& ActorFactories = GEditor->ActorFactories;
			for (int32 FactoryIdx = 0; FactoryIdx < ActorFactories.Num(); FactoryIdx++)
			{
				UActorFactory* ActorFactory = ActorFactories[FactoryIdx];

				// Check if the actor can be created using this factory, making sure to check for an asset to be assigned from the selector
				FText ErrorMessage;
				if (ActorFactory->CanCreateActorFrom(AssetData, ErrorMessage))
				{
					Factory = ActorFactory;
					break;
				}
			}
		}

		if (Factory)
		{
			GEditor->ReplaceSelectedActors(Factory, AssetData);
		}
	}
}

AActor* UEditorLevelLibrary::SpawnActorFromObject(UObject* ObjToUse, FVector Location, FRotator Rotation, bool bTransient)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->SpawnActorFromObject(ObjToUse, Location, Rotation, bTransient) : nullptr;
}

AActor* UEditorLevelLibrary::SpawnActorFromClass(TSubclassOf<class AActor> ActorClass, FVector Location, FRotator Rotation, bool bTransient)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->SpawnActorFromClass(ActorClass, Location, Rotation, bTransient) : nullptr;
}


bool UEditorLevelLibrary::DestroyActor(class AActor* ToDestroyActor)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->DestroyActor(ToDestroyActor) : false;
}

UWorld* UEditorLevelLibrary::GetEditorWorld()
{
	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	return UnrealEditorSubsystem ? UnrealEditorSubsystem->GetEditorWorld() : nullptr;

}

UWorld* UEditorLevelLibrary::GetGameWorld()
{
	UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();

	return UnrealEditorSubsystem ? UnrealEditorSubsystem->GetGameWorld() : nullptr;
}

TArray<UWorld*> UEditorLevelLibrary::GetPIEWorlds(bool bIncludeDedicatedServer)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<UWorld*> PIEWorlds;

	if (GEditor)
	{
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::PIE)
			{
				if (bIncludeDedicatedServer || !WorldContext.RunAsDedicated)
				{
					PIEWorlds.Add(WorldContext.World());
				}
			}
		}
	}

	return PIEWorlds;
}

bool UEditorLevelLibrary::NewLevel(const FString& AssetPath)
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	return LevelEditorSubsystem ? LevelEditorSubsystem->NewLevel(AssetPath) : false;
}

bool UEditorLevelLibrary::NewLevelFromTemplate(const FString& AssetPath, const FString& TemplateAssetPath)
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	return LevelEditorSubsystem ? LevelEditorSubsystem->NewLevelFromTemplate(AssetPath, TemplateAssetPath) : false;
}

bool UEditorLevelLibrary::LoadLevel(const FString& AssetPath)
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	return LevelEditorSubsystem ? LevelEditorSubsystem->LoadLevel(AssetPath) : false;

}

bool UEditorLevelLibrary::SaveCurrentLevel()
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	return LevelEditorSubsystem ? LevelEditorSubsystem->SaveCurrentLevel() : false;
}

bool UEditorLevelLibrary::SaveAllDirtyLevels()
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	return LevelEditorSubsystem ? LevelEditorSubsystem->SaveAllDirtyLevels() : false;
}

bool UEditorLevelLibrary::SetCurrentLevelByName(FName LevelName)
{
	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();

	return LevelEditorSubsystem ? LevelEditorSubsystem->SetCurrentLevelByName(LevelName) : false;
}

void UEditorLevelLibrary::ReplaceMeshComponentsMaterials(const TArray<UMeshComponent*>& MeshComponents, UMaterialInterface* MaterialToBeReplaced, UMaterialInterface* NewMaterial)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		StaticMeshEditorSubsystem->ReplaceMeshComponentsMaterials(MeshComponents, MaterialToBeReplaced, NewMaterial);
	}
}

void UEditorLevelLibrary::ReplaceMeshComponentsMaterialsOnActors(const TArray<AActor*>& Actors, UMaterialInterface* MaterialToBeReplaced, UMaterialInterface* NewMaterial)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		StaticMeshEditorSubsystem->ReplaceMeshComponentsMaterialsOnActors(Actors, MaterialToBeReplaced, NewMaterial);
	}
}

void UEditorLevelLibrary::ReplaceMeshComponentsMeshes(const TArray<UStaticMeshComponent*>& MeshComponents, UStaticMesh* MeshToBeReplaced, UStaticMesh* NewMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		StaticMeshEditorSubsystem->ReplaceMeshComponentsMeshes(MeshComponents, MeshToBeReplaced, NewMesh);
	}
}

void UEditorLevelLibrary::ReplaceMeshComponentsMeshesOnActors(const TArray<AActor*>& Actors, UStaticMesh* MeshToBeReplaced, UStaticMesh* NewMesh)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	if (StaticMeshEditorSubsystem)
	{
		StaticMeshEditorSubsystem->ReplaceMeshComponentsMeshesOnActors(Actors, MeshToBeReplaced, NewMesh);
	}
}

TArray<class AActor*> UEditorLevelLibrary::ConvertActors(const TArray<class AActor*>& Actors, TSubclassOf<class AActor> ActorClass, const FString& StaticMeshPackagePath)
{
	UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

	return EditorActorSubsystem ? EditorActorSubsystem->ConvertActors(Actors, ActorClass, StaticMeshPackagePath) : TArray<class AActor*>();

}

AActor* UEditorLevelLibrary::JoinStaticMeshActors(const TArray<AStaticMeshActor*>& ActorsToMerge, const FEditorScriptingJoinStaticMeshActorsOptions_Deprecated& JoinOptions)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->JoinStaticMeshActors(ActorsToMerge, InternalEditorLevelLibrary::ConvertJoinStaticMeshActorsOptions(JoinOptions)) : nullptr;
}

bool UEditorLevelLibrary::MergeStaticMeshActors(const TArray<AStaticMeshActor*>& ActorsToMerge, const FEditorScriptingMergeStaticMeshActorsOptions_Deprecated& MergeOptions, AStaticMeshActor*& OutMergedActor)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->MergeStaticMeshActors(ActorsToMerge, InternalEditorLevelLibrary::ConvertMergeStaticMeshActorsOptions(MergeOptions), OutMergedActor) : false;

}

bool UEditorLevelLibrary::CreateProxyMeshActor(const TArray<class AStaticMeshActor*>& ActorsToMerge, const FEditorScriptingCreateProxyMeshActorOptions_Deprecated& MergeOptions, class AStaticMeshActor*& OutMergedActor)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->CreateProxyMeshActor(ActorsToMerge, InternalEditorLevelLibrary::ConvertCreateProxyStaticMeshActorsOptions(MergeOptions), OutMergedActor) : false;
}

// The functions below are BP exposed copies of functions that use deprecated structs, updated to the new structs in StaticMeshEditorSubsytem
// The old structs redirect to the new ones, so this makes blueprints that use the old structs still work
// The old functions are still available as an overload, which makes old code that uses them compatible

AActor* UEditorLevelLibrary::JoinStaticMeshActors(const TArray<AStaticMeshActor*>& ActorsToMerge, const FJoinStaticMeshActorsOptions& JoinOptions)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->JoinStaticMeshActors(ActorsToMerge, JoinOptions) : nullptr;
}

bool UEditorLevelLibrary::MergeStaticMeshActors(const TArray<AStaticMeshActor*>& ActorsToMerge, const FMergeStaticMeshActorsOptions& MergeOptions, AStaticMeshActor*& OutMergedActor)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->MergeStaticMeshActors(ActorsToMerge, MergeOptions, OutMergedActor) : false;

}

bool UEditorLevelLibrary::CreateProxyMeshActor(const TArray<class AStaticMeshActor*>& ActorsToMerge, const FCreateProxyMeshActorOptions& MergeOptions, class AStaticMeshActor*& OutMergedActor)
{
	UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

	return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->CreateProxyMeshActor(ActorsToMerge, MergeOptions, OutMergedActor) : false;
}

#undef LOCTEXT_NAMESPACE

