//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononScene.h"

#include "PhononCommon.h"
#include "PhononGeometryComponent.h"
#include "PhononMaterialComponent.h"

#include "EngineUtils.h"
#include "HAL/FileManagerGeneric.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "SteamAudioSettings.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "Async/Async.h"

#if WITH_EDITOR

#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeDataAccess.h"

#endif // WITH_EDITOR

/*

 The scene export functions set up the following material index layout on the Phonon backend:

 <Presets>
 Default static mesh material
 Default BSP material
 Default landscape material
 <Custom static mesh materials>

 Note that it results in the CUSTOM preset being unused, but the code is simpler this way.

*/

namespace SteamAudio
{
	bool LoadSceneFromDisk(UWorld* World, IPLhandle ComputeDevice, const IPLSimulationSettings& SimulationSettings, IPLhandle* PhononScene,
		FPhononSceneInfo& PhononSceneInfo, FString* CurrentLevelName)
	{
		FString MapName;
		
		if (!CurrentLevelName || CurrentLevelName->IsEmpty())
		{
			MapName = World->GetMapName();
		}
		else
		{
			MapName = StrippedMapName(*CurrentLevelName);
		}
		
		FString SceneFileName = RuntimePath + MapName + ".phononscene";
		FString SceneInfoFileName = EditorOnlyPath + MapName + ".phononsceneinfo";
		TArray<uint8> SceneData;

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* SceneFileHandle = PlatformFile.OpenRead(*SceneFileName);
		if (SceneFileHandle)
		{
			SceneData.SetNum(SceneFileHandle->Size());
			SceneFileHandle->Read(SceneData.GetData(), SceneFileHandle->Size());
			delete SceneFileHandle;
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load Phonon scene: error reading file. Be sure to export the scene."));
			return false;
		}

		IPLerror Error = iplLoadScene(GlobalContext, SimulationSettings, SceneData.GetData(), SceneData.Num(), ComputeDevice, nullptr, PhononScene);
		if (Error != IPL_STATUS_SUCCESS)
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load Phonon scene: error [%i] loading scene file [%s] with data size [%i]. \n "), (int32) Error, *SceneFileName, SceneData.Num());
			return false;
		}

		LoadSceneInfoFromDisk(World, PhononSceneInfo);

		return true;
	}

	bool LoadSceneInfoFromDisk(UWorld* World, FPhononSceneInfo& PhononSceneInfo)
	{
		FString MapName = StrippedMapName(World->GetMapName());
		FString SceneInfoFileName = EditorOnlyPath + MapName + ".phononsceneinfo";

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* SceneInfoFileHandle = PlatformFile.OpenRead(*SceneInfoFileName);
		if (SceneInfoFileHandle)
		{
			SceneInfoFileHandle->Read((uint8*)&PhononSceneInfo.NumTriangles, 4);
			SceneInfoFileHandle->Read((uint8*)&PhononSceneInfo.NumDynTriangles, 4);
			SceneInfoFileHandle->Read((uint8*)&PhononSceneInfo.DataSize, 4);
			SceneInfoFileHandle->Read((uint8*)&PhononSceneInfo.DynDataSize, 4);
			delete SceneInfoFileHandle;
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load Phonon scene info: error loading info file."));
			return false;
		}

		return true;
	}

	bool LoadDynamicSceneFromDisk(UWorld* World, IPLhandle ComputeDevice, const IPLSimulationSettings& SimulationSettings, IPLhandle* PhononScene,
		FPhononSceneInfo& PhononSceneInfo, FString DynamicSceneID)
	{
		// Check for supported raytracer
		if (GetDefault<USteamAudioSettings>()->RayTracer != EIplRayTracerType::EMBREE)
		{
			return false;
		}

		FString SceneFileName = DynamicRuntimePath + DynamicSceneID + ".phononscene";
		FString SceneInfoFileName = DynamicEditorOnlyPath + DynamicSceneID + ".phononinfo";
		TArray<uint8> SceneData;

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		IFileHandle* SceneFileHandle = PlatformFile.OpenRead(*SceneFileName);
		if (SceneFileHandle)
		{
			SceneData.SetNum(SceneFileHandle->Size());
			SceneFileHandle->Read(SceneData.GetData(), SceneFileHandle->Size());
			delete SceneFileHandle;
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load Dynamic Phonon scene: error reading file. Be sure to export the scene."));
			return false;
		}

		IPLerror Error = iplLoadScene(GlobalContext, SimulationSettings, SceneData.GetData(), SceneData.Num(), ComputeDevice, nullptr, PhononScene);
		if (Error != IPL_STATUS_SUCCESS)
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load Dynamic Phonon scene: error [%i] loading scene file [%s] with data size [%i]. \n "), (int32)Error, *SceneFileName, SceneData.Num());
			return false;
		}

		LoadDynamicScenesInfoFromDisk(World, PhononSceneInfo, DynamicSceneID);

		return true;
	}

	bool LoadDynamicScenesInfoFromDisk(UWorld* World, FPhononSceneInfo& PhononSceneInfo, FString DynamicSceneID)
	{
		FString SceneInfoFileName = DynamicEditorOnlyPath + DynamicSceneID + ".phononinfo";

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* SceneInfoFileHandle = PlatformFile.OpenRead(*SceneInfoFileName);
		if (SceneInfoFileHandle)
		{
			SceneInfoFileHandle->Read((uint8*)&PhononSceneInfo.NumTriangles, 4);
			SceneInfoFileHandle->Seek(4);
			SceneInfoFileHandle->Read((uint8*)&PhononSceneInfo.DataSize, 4);
			delete SceneInfoFileHandle;
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load Dynamic Phonon scene info: error loading info file."));
			return false;
		}

		return true;
	}

#if WITH_EDITOR

	static void LoadBSPGeometry(UWorld* World, IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices);
	static void LoadStaticMeshActors(UWorld* World, IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices);
	static void LoadLandscapeActors(UWorld* World, IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices);
	static void RegisterStaticMesh(IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices, IPLhandle* PhononStaticMesh);
	static void RegisterStaticMeshes(IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices, TArray<IPLhandle>* PhononStaticMeshes);
	static void GetPhononMaterials(UWorld* World, TArray<IPLMaterial>& IplMaterials);
	static UPhononMaterialComponent* GetPhononMaterialComponent(AActor* Actor);
	static uint32 GetMeshVerts(UStaticMeshComponent* StaticMeshComponent, TArray<IPLVector3>& VertexArray, bool bGetRelativeVertexPos = true);
	static void CalculateVertsTrisMats(AActor* MeshActor, UStaticMeshComponent* MeshComponent, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices);

	//==============================================================================================================================================
	// High level scene export
	//==============================================================================================================================================

	/**
	 * Loads scene geometry, providing handles to the Phonon scene object and Phonon static meshes.
	 */
	bool CreateScene(UWorld* World, IPLhandle* PhononScene, IPLhandle* PhononStaticMesh, uint32& NumSceneTriangles, uint32& NumDynSceneTriangles, uint32& DynDataSize, bool CreateDynObjFiles /* =false*/ )
	{
		check(World);
		check(PhononScene);
		check(PhononStaticMesh);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading Phonon scene."));

		TPromise<bool> Result;

		AsyncTask(ENamedThreads::GameThread, [World, PhononScene, PhononStaticMesh, &NumSceneTriangles, &NumDynSceneTriangles, &DynDataSize, CreateDynObjFiles, &Result]()
		{
			// Setup simulation settings - we really only care about scene type here, but Beta 17 API throws if ambisonics order > max order,
			// so just zero the struct for now.
			IPLSimulationSettings SimulationSettings;
			FMemory::Memset(SimulationSettings, 0);
			SimulationSettings.sceneType = IPLSceneType::IPL_SCENETYPE_PHONON;

			// Setup IPL Material defaults - custom will be unused
			TArray<IPLMaterial> SceneMaterials;
			MaterialPresets.GenerateValueArray(SceneMaterials);

			// Add default materials as fixed slots (3)
			SceneMaterials.Add(GetDefault<USteamAudioSettings>()->GetDefaultStaticMeshMaterial());
			SceneMaterials.Add(GetDefault<USteamAudioSettings>()->GetDefaultBSPMaterial());
			SceneMaterials.Add(GetDefault<USteamAudioSettings>()->GetDefaultLandscapeMaterial());
			
			// Get IPLMaterials in scene
			GetPhononMaterials(World, SceneMaterials);
			UE_LOG(LogSteamAudio, Log, TEXT("Loading Phonon scene found [%i] materials."), SceneMaterials.Num());
			
			// Ensure that the SteamAudio paths exist, create them if they don't
			CheckSceneDirectory(RuntimePath);
			CheckSceneDirectory(EditorOnlyPath);

			// Create dynamic scene
			CreateDynamicScene(World, SimulationSettings, SceneMaterials, NumDynSceneTriangles, DynDataSize, CreateDynObjFiles);
		
			// Create static scene
			IPLerror IplResult = iplCreateScene(GlobalContext, nullptr, SimulationSettings, SceneMaterials.Num(),
				SceneMaterials.GetData(), nullptr, nullptr, nullptr, nullptr, nullptr, PhononScene);

			if (IplResult != IPL_STATUS_SUCCESS)
			{
				UE_LOG(LogSteamAudio, Warning, TEXT("Error creating Phonon scene."));
				Result.SetValue(false);
				return;
			}

			TArray<IPLVector3> IplVertices;
			TArray<IPLTriangle> IplTriangles;
			TArray<IPLint32> IplMaterialIndices;

			LoadStaticMeshActors(World, *PhononScene, IplVertices, IplTriangles, IplMaterialIndices);

			if (GetDefault<USteamAudioSettings>()->ExportLandscapeGeometry)
			{
				LoadLandscapeActors(World, *PhononScene, IplVertices, IplTriangles, IplMaterialIndices);
			}

			if (GetDefault<USteamAudioSettings>()->ExportBSPGeometry)
			{
				LoadBSPGeometry(World, *PhononScene, IplVertices, IplTriangles, IplMaterialIndices);
			}
			
			RegisterStaticMesh(*PhononScene, IplVertices, IplTriangles, IplMaterialIndices, PhononStaticMesh);
			NumSceneTriangles = IplTriangles.Num();

			Result.SetValue(true);
		});

		TFuture<bool> Value = Result.GetFuture();
		Value.Wait();

		return Value.Get();
	}

	void CreateDynamicScene(UWorld* World, IPLSimulationSettings SimulationSettings, TArray<IPLMaterial> SceneMaterials, uint32& NumSceneTriangles, uint32& DataSize, bool CreateObjFiles /* =false */)
	{
		// Wipe old dynamic scene files for this level (if any)
		DeleteSceneFiles(World, DynamicRuntimePath);
		DeleteSceneFiles(World, DynamicEditorOnlyPath);

		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			if (ActorItr && ActorItr->IsValidLowLevelFast())
			{
				// Check if the actor's root component is movable and there's a phonon geometry component and at least one static mesh component attached
				auto PhononGeometryComponent = ActorItr->GetComponentByClass(UPhononGeometryComponent::StaticClass());
				if (ActorItr->GetRootComponent()
					&& ActorItr->GetRootComponent()->Mobility == EComponentMobility::Movable
					&& PhononGeometryComponent
					&& PhononGeometryComponent->IsValidLowLevelFast()
					&& ActorItr->GetComponentByClass(UStaticMeshComponent::StaticClass())
					)
				{
					// Go through all static mesh components in this actor
					TArray<UActorComponent*> Components;
					ActorItr->GetComponents(UStaticMeshComponent::StaticClass(), Components);

					for (auto ActorItrComponent : Components)
					{
						auto DynamicMeshComponent = Cast<UStaticMeshComponent>(ActorItrComponent);

						if (DynamicMeshComponent && DynamicMeshComponent->IsValidLowLevelFast())
						{
							UPhononGeometryComponent* DynamicGeometryComponent = nullptr;
							TArray<USceneComponent*> ChildrenComponents;
							DynamicMeshComponent->GetChildrenComponents(false, ChildrenComponents);

							// Check if this static mesh component has a Phonon Geometry Component
							for (auto ChildComponent : ChildrenComponents)
							{
								auto ChildPhononGeometryComponent = Cast<UPhononGeometryComponent>(ChildComponent);

								if (ChildPhononGeometryComponent)
								{
									DynamicGeometryComponent = ChildPhononGeometryComponent;
									break;
								}
							}

							if (DynamicGeometryComponent)
							{
								// Create an iplScene 
								IPLhandle DynamicPhononScene = nullptr;
								FString SceneID = World->GetName() + "_" + ActorItr->GetName() + "_" + DynamicMeshComponent->GetName();

								IPLerror IplDynResult = iplCreateScene(GlobalContext, nullptr, SimulationSettings, SceneMaterials.Num(),
									SceneMaterials.GetData(), nullptr, nullptr, nullptr, nullptr, nullptr, &DynamicPhononScene);
								if (IplDynResult == IPL_STATUS_SUCCESS)
								{
									UE_LOG(LogSteamAudio, Log, TEXT("Dynamic Phonon scene succesfully created for [%s]"), *SceneID);
								}
								else
								{
									UE_LOG(LogSteamAudio, Warning, TEXT("Error creating Dynamic Phonon scene for [%s]"), *SceneID);
									continue;
								}

								// Create and register dynamic meshes to the dynamic scene
								uint32 DynamicSceneTriangles = 0;
								TArray<IPLhandle> PhononDynamicMeshes;
								TArray<IPLVector3> DynIplVertices;
								TArray<IPLTriangle> DynIplTriangles;
								TArray<IPLint32> DynIplMaterialIndices;


								CalculateVertsTrisMats(*ActorItr, DynamicMeshComponent, DynIplVertices, DynIplTriangles, DynIplMaterialIndices);

								// Register Instanced Mesh to Dynamic Scene
								RegisterStaticMeshes(DynamicPhononScene, DynIplVertices, DynIplTriangles, DynIplMaterialIndices, &PhononDynamicMeshes);

								// Add tris in this mesh to the total scene tris
								DynamicSceneTriangles += DynIplTriangles.Num();

								// Finalize and save to disk
								FPhononSceneInfo DynamicPhononSceneInfo;
								DynamicPhononSceneInfo.NumTriangles = DynamicSceneTriangles;
								DynamicPhononSceneInfo.DataSize = iplSaveScene(DynamicPhononScene, nullptr);
								bool SaveDynSceneSuccessful = SaveDynamicSceneToDisk(World, DynamicPhononScene, DynamicPhononSceneInfo, SceneID, CreateObjFiles);

								// Clean up Dynamic Phonon structures
								for (IPLhandle PhononDynamicMeshItr : PhononDynamicMeshes)
								{
									iplDestroyStaticMesh(&PhononDynamicMeshItr);
								}
								iplDestroyScene(&DynamicPhononScene);

								if (SaveDynSceneSuccessful)
								{
									DataSize += DynamicPhononSceneInfo.DataSize;
									NumSceneTriangles += DynamicPhononSceneInfo.NumTriangles;
									UE_LOG(LogSteamAudio, Log, TEXT("Dynamic scene [%s] with [%i] triangles saved to disk with size [%i]."), *SceneID, DynamicSceneTriangles, DynamicPhononSceneInfo.DataSize);
								}
								else
								{
									UE_LOG(LogSteamAudio, Warning, TEXT("Unable to save Dynamic scene [%s] with [%i] triangles to disk with size [%i]."), *SceneID, DynamicSceneTriangles, DynamicPhononSceneInfo.DataSize);
								}
							}
						}
					}
				}
			}
		}
	}

	bool SaveFinalizedSceneToDisk(UWorld* World, IPLhandle PhononScene, const FPhononSceneInfo& PhononSceneInfo)
	{
		// Write Phonon Scene data to byte array
		TArray<uint8> SceneData;
		SceneData.SetNum(PhononSceneInfo.DataSize);
		iplSaveScene(PhononScene, SceneData.GetData());

		// Serialize byte array to disk
		FString SceneFileName = RuntimePath + World->GetMapName() + ".phononscene";
		FString SceneInfoName = EditorOnlyPath + World->GetMapName() + ".phononsceneinfo";
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* SceneFileHandle = PlatformFile.OpenWrite(*SceneFileName);
		IFileHandle* SceneInfoHandle = PlatformFile.OpenWrite(*SceneInfoName);

		if (SceneFileHandle && SceneInfoHandle)
		{
			SceneFileHandle->Write(SceneData.GetData(), PhononSceneInfo.DataSize);
			delete SceneFileHandle;

			SceneInfoHandle->Write((uint8*)&PhononSceneInfo.NumTriangles, 4);
			SceneInfoHandle->Write((uint8*)&PhononSceneInfo.NumDynTriangles, 4);
			SceneInfoHandle->Write((uint8*)&PhononSceneInfo.DataSize, 4);
			SceneInfoHandle->Write((uint8*)&PhononSceneInfo.DynDataSize, 4);
			delete SceneInfoHandle;
		}
		else
		{
			if (SceneFileHandle)
			{
				delete SceneFileHandle;
			}

			if (SceneInfoHandle)
			{
				delete SceneInfoHandle;
			}

			return false;
		}

		return true;
	}


	bool STEAMAUDIO_API SaveDynamicSceneToDisk(UWorld* World, IPLhandle PhononScene, const FPhononSceneInfo& PhononSceneInfo, FString DynamicSceneID, bool CreateObjFile /* =false */)
	{
		// Write Phonon Scene data to byte array
		TArray<uint8> SceneData;
		SceneData.SetNum(PhononSceneInfo.DataSize);
		iplSaveScene(PhononScene, SceneData.GetData());

		// Serialize byte array to disk
		FString SceneFileName = DynamicRuntimePath + DynamicSceneID + ".phononscene";
		FString SceneInfoName = DynamicEditorOnlyPath + DynamicSceneID + ".phononinfo";
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* SceneFileHandle = PlatformFile.OpenWrite(*SceneFileName);
		IFileHandle* SceneInfoHandle = PlatformFile.OpenWrite(*SceneInfoName);

		if (SceneFileHandle && SceneInfoHandle)
		{
			if (CreateObjFile)
			{
				iplSaveSceneAsObj(PhononScene, TCHAR_TO_ANSI(*(DynamicEditorOnlyPath + DynamicSceneID + ".obj")));
			}

			SceneFileHandle->Write(SceneData.GetData(), PhononSceneInfo.DataSize);
			delete SceneFileHandle;

			SceneInfoHandle->Write((uint8*)&PhononSceneInfo.NumTriangles, 4);
			SceneInfoHandle->Write((uint8*)&PhononSceneInfo.DataSize, 4);
			delete SceneInfoHandle;
		}
		else
		{
			if (SceneFileHandle)
			{
				delete SceneFileHandle;
			}

			if (SceneInfoHandle)
			{
				delete SceneInfoHandle;
			}

			return false;
		}

		return true;
	}

	bool STEAMAUDIO_API CheckSceneDirectory(FString DirectoryPath)
	{
		IFileManager& FileManager = FFileManagerGeneric::Get();
		if (!FileManager.DirectoryExists(*DirectoryPath))
		{
			FileManager.MakeDirectory(*DirectoryPath);
			return false;
		}
		
		return true;
	}

	void STEAMAUDIO_API DeleteSceneFiles(UWorld * World, FString FilePath)
	{
		IFileManager& FileManager = FFileManagerGeneric::Get();
		if (CheckSceneDirectory(FilePath))
		{
			TArray<FString> FoundFiles;
			FileManager.FindFiles(FoundFiles, *FilePath);

			for (auto FoundFile : FoundFiles)
			{
				FString FileToDelete = FilePath + FoundFile;
				//UE_LOG(LogSteamAudio, Warning, TEXT("Checking for file [%s]"), *FileToDelete);

				if (FoundFile.Left(World->GetName().Len()).Equals(World->GetName(), ESearchCase::IgnoreCase) 
					&& FileManager.Delete(*FileToDelete, true, true, true)
					)
				{
					UE_LOG(LogSteamAudio, Log, TEXT("Deleted old scene file [%s]"), *FileToDelete);
				}
			}
		}
	}

	//==============================================================================================================================================
	// Utilities for adding/removing Phonon Geometry components
	//==============================================================================================================================================

	/**
	 * Adds Phonon Geometry components with default settings. Will not add if one already exists.
	 */
	void AddGeometryComponentsToStaticMeshes(UWorld* World)
	{
		check(World);

		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			
			// Go through any static mesh components found in this actor
			TArray<UActorComponent*> Components;
			ActorItr->GetComponents(UStaticMeshComponent::StaticClass(), Components);

			for (auto ActorComp : Components)
			{
				if (ActorComp && ActorComp->IsValidLowLevel())
				{
					auto* StaticMeshComp = Cast<UStaticMeshComponent>(ActorComp);
	
					// Check if this static mesh can affect audio
					if (StaticMeshComp
						&& StaticMeshComp->IsValidLowLevel()
						&& !StaticMeshComp->IsVisualizationComponent()
						)
					{
						// Check if there's already a phonon geometry component attached to this static mesh component
						TArray<USceneComponent*> ChildrenComponents;
						StaticMeshComp->GetChildrenComponents(true, ChildrenComponents);
						bool bAlreadyHavePhononGeomComponent = false;
						
						for (auto ChildComponent : ChildrenComponents)
						{
							auto* PhononGeomComp = Cast<UPhononGeometryComponent>(ChildComponent);
							
							if (PhononGeomComp && PhononGeomComp->IsValidLowLevelFast())
							{
								bAlreadyHavePhononGeomComponent = true;
								break;
							}
						}

						// If there isn't, create and attach a new phonon geometry component to this static mesh
						if (!bAlreadyHavePhononGeomComponent)
						{
							// Add phonon geometry object
							auto PhononGeometryComponent = NewObject<UPhononGeometryComponent>(*ActorItr);
							PhononGeometryComponent->AttachToComponent(StaticMeshComp, FAttachmentTransformRules::KeepRelativeTransform);
							PhononGeometryComponent->RegisterComponent();
							ActorItr->AddInstanceComponent(PhononGeometryComponent);
						}
					}
				}
			}
		}
	}

	/**
	 * Removes all Phonon Geometry components from Static Mesh actors.
	 */
	void RemoveGeometryComponentsFromStaticMeshes(UWorld* World)
	{
		check(World);

		for (TObjectIterator<UPhononGeometryComponent> PhononGeometryObj; PhononGeometryObj; ++PhononGeometryObj)
		{
			if (PhononGeometryObj && PhononGeometryObj->IsValidLowLevel())
			{
				PhononGeometryObj->DestroyComponent();
			}
		}
	}


	//==============================================================================================================================================
	// Static mesh geometry export
	//==============================================================================================================================================

	/**
	 * Populates VertexArray with the given mesh's vertices. Converts from UE coords to Phonon coords. Returns the number of vertices added.
	 */
	static uint32 GetMeshVerts(UStaticMeshComponent* StaticMeshComponent, TArray<IPLVector3>& VertexArray, bool bGetRelativeVertexPos /*= true*/)
	{
		check(StaticMeshComponent->GetStaticMesh()->HasValidRenderData());

		uint32 NumVerts = 0;

		FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->GetRenderData()->LODResources[0];
		auto Indices = LODModel.IndexBuffer.GetArrayView();

		for (const FStaticMeshSection& Section : LODModel.Sections)
		{
			for (auto TriIndex = 0; TriIndex < (int32)Section.NumTriangles; ++TriIndex)
			{
				auto BaseIndex = Section.FirstIndex + TriIndex * 3;
				for (auto v = 2; v > -1; v--)
				{
					auto i = Indices[BaseIndex + v];
					auto vtx = bGetRelativeVertexPos ? FVector3f(StaticMeshComponent->GetComponentTransform().TransformPosition((FVector)LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(i)))
						: LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(i);

					VertexArray.Add(UnrealToPhononIPLVector3((FVector)vtx));
					NumVerts++;
				}
			}
		}

		return NumVerts;
	}

	/**
	 * Walks up the actor attachment chain, checking for a Phonon Geometry component.
	 */
	static bool IsActorPhononGeometry(AActor* Actor)
	{
		auto CurrentActor = Actor;
		while (CurrentActor)
		{
			if (CurrentActor->GetComponentByClass(UPhononGeometryComponent::StaticClass()))
			{
				return true;
			}
			CurrentActor = CurrentActor->GetAttachParentActor();
		}

		return false;
	}

	/**
	 * Walks up the actor attachment chain, checking for a Phonon Material component.
	 */
	static UPhononMaterialComponent* GetPhononMaterialComponent(AActor* Actor)
	{
		auto CurrentActor = Actor;
		while (CurrentActor)
		{
			if (CurrentActor->GetComponentByClass(UPhononMaterialComponent::StaticClass()))
			{
				return static_cast<UPhononMaterialComponent*>(CurrentActor->GetComponentByClass(UPhononMaterialComponent::StaticClass()));
			}
			CurrentActor = CurrentActor->GetAttachParentActor();
		}

		return nullptr;
	}

	/**
	 * Calculates a mesh's vertices, tris and retries phonon material indices for a given actor
	 */
	static void CalculateVertsTrisMats(AActor* MeshActor, UStaticMeshComponent* MeshComponent, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices)
	{
		check(MeshActor);
		check(MeshComponent);

		int32 StartVertexIdx = IplVertices.Num();
		int32 NumMeshVerts = MeshComponent->Mobility == EComponentMobility::Movable ? GetMeshVerts(MeshComponent, IplVertices, false) : GetMeshVerts(MeshComponent, IplVertices);
		int32 NumMeshTriangles = NumMeshVerts / 3;

		for (int32 i = 0; i < NumMeshTriangles; ++i)
		{
			IPLTriangle IplTriangle;
			IplTriangle.indices[0] = StartVertexIdx + i * 3;
			IplTriangle.indices[1] = StartVertexIdx + i * 3 + 2;
			IplTriangle.indices[2] = StartVertexIdx + i * 3 + 1;
			IplTriangles.Add(IplTriangle);
		}

		auto PhononMaterialComponent = GetPhononMaterialComponent(MeshActor);
		auto MaterialIndex = 0;

		if (PhononMaterialComponent)
		{
			MaterialIndex = PhononMaterialComponent->MaterialIndex;
		}
		else
		{
			// The default static mesh material is always registered at size(MaterialPresets)
			MaterialIndex = MaterialPresets.Num();
		}

		for (auto i = 0; i < NumMeshTriangles; ++i)
		{
			IplMaterialIndices.Add(MaterialIndex);
			//UE_LOG(LogSteamAudio, Warning, TEXT("Phonon scene Material Index Added [%i]"), MaterialIndex);
		}
	}

	/**
	 * Loads any static mesh actors.
	 */
	static void LoadStaticMeshActors(UWorld* World, IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices)
	{
		check(World);
		check(PhononScene);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading static mesh actors."));

		for (TActorIterator<AStaticMeshActor> AStaticMeshItr(World); AStaticMeshItr; ++AStaticMeshItr)
		{
			// Only consider static mesh actors that have both an acoustic geometry component attached and valid render data
			if (IsActorPhononGeometry(*AStaticMeshItr) 
				&& AStaticMeshItr->GetStaticMeshComponent()->Mobility != EComponentMobility::Movable
				&& AStaticMeshItr->GetStaticMeshComponent()->GetStaticMesh() 
				&& AStaticMeshItr->GetStaticMeshComponent()->GetStaticMesh()->HasValidRenderData()
				)
			{
				CalculateVertsTrisMats(*AStaticMeshItr, AStaticMeshItr->GetStaticMeshComponent(), IplVertices, IplTriangles, IplMaterialIndices);
			}
		}
	}

	/**
	 * Loads any static mesh actors, adding any Phonon static meshes to the provided array.
	 */
	static uint32 LoadDynamicMeshes(AActor* DynamicActor, IPLhandle PhononScene)
	{
		check(DynamicActor);
		check(PhononScene);

		TArray<IPLVector3> IplVertices;
		TArray<IPLTriangle> IplTriangles;
		TArray<IPLint32> IplMaterialIndices;

		// Go through all static mesh components in this actor
		TArray<UActorComponent*> Components;
		DynamicActor->GetComponents(UStaticMeshComponent::StaticClass(), Components);
		for (auto StaticMeshComp : Components)
		{
			auto StaticMeshComponent = Cast<UStaticMeshComponent>(StaticMeshComp);

			// Only consider dynamic static meshes that have a static mesh already assigned and have valid render data
			if (StaticMeshComponent
				&& StaticMeshComponent->IsValidLowLevelFast()
				&& StaticMeshComponent->Mobility == EComponentMobility::Movable
				&& StaticMeshComponent->GetStaticMesh()
				&& StaticMeshComponent->GetStaticMesh()->HasValidRenderData()
				&& !StaticMeshComponent->IsVisualizationComponent()
				)
			{
				CalculateVertsTrisMats(DynamicActor, StaticMeshComponent, IplVertices, IplTriangles, IplMaterialIndices);
			}
		}

		// Register a new static mesh with Phonon
		//RegisterStaticMesh(PhononScene, IplVertices, IplTriangles, IplMaterialIndices, PhononStaticMeshes);

		return IplTriangles.Num();
	}

	//==============================================================================================================================================
	// BSP geometry export
	//==============================================================================================================================================

	/**
	 * Loads any BSP geometry.
	 */
	static void LoadBSPGeometry(UWorld* World, IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices)
	{
		check(World);
		check(PhononScene);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading BSP geometry."));

		int32 InitialNumVertices = IplVertices.Num();
		int32 InitialNumTriangles = IplTriangles.Num();

		// Gather and convert all world vertices to Phonon coords
		for (auto& WorldVertex : World->GetModel()->Points)
		{
			IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3((FVector)WorldVertex));
		}

		// Gather vertex indices for all faces ("nodes" are faces)
		for (auto& WorldNode : World->GetModel()->Nodes)
		{
			// Ignore degenerate faces
			if (WorldNode.NumVertices <= 2)
			{
				continue;
			}

			// Faces are organized as triangle fans
			int32 Index0 = World->GetModel()->Verts[WorldNode.iVertPool + 0].pVertex;
			int32 Index1 = World->GetModel()->Verts[WorldNode.iVertPool + 1].pVertex;
			int32 Index2;

			for (auto v = 2; v < WorldNode.NumVertices; ++v)
			{
				Index2 = World->GetModel()->Verts[WorldNode.iVertPool + v].pVertex;

				IPLTriangle IplTriangle;
				IplTriangle.indices[0] = Index0 + InitialNumVertices;
				IplTriangle.indices[1] = Index2 + InitialNumVertices;
				IplTriangle.indices[2] = Index1 + InitialNumVertices;
				IplTriangles.Add(IplTriangle);

				Index1 = Index2;
			}
		}

		// The default BSP material is always registered at size(MaterialPresets) + 1
		auto MaterialIdx = MaterialPresets.Num() + 1;
		for (auto i = InitialNumTriangles; i < IplTriangles.Num(); ++i)
		{
			IplMaterialIndices.Add(MaterialIdx);
		}
	}

	//==============================================================================================================================================
	// Landscape geometry export
	//==============================================================================================================================================

	/**
	 * Loads any Landscape actors.
	 */
	static void LoadLandscapeActors(UWorld* World, IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices)
	{
		check(World);
		check(PhononScene);

		UE_LOG(LogSteamAudio, Log, TEXT("Loading landscape actors."));

		int32 InitialNumTriangles = IplTriangles.Num();

		for (TActorIterator<ALandscape> ALandscapeItr(World); ALandscapeItr; ++ALandscapeItr)
		{
			ULandscapeInfo* LandscapeInfo = ALandscapeItr->GetLandscapeInfo();

			for (auto It = LandscapeInfo->XYtoComponentMap.CreateIterator(); It; ++It)
			{
				ULandscapeComponent* Component = It.Value();
				FLandscapeComponentDataInterface CDI(Component);

				for (auto y = 0; y < Component->ComponentSizeQuads; ++y)
				{
					for (auto x = 0; x < Component->ComponentSizeQuads; ++x)
					{
						auto StartIndex = IplVertices.Num();

						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x, y)));
						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x, y + 1)));
						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x + 1, y + 1)));
						IplVertices.Add(SteamAudio::UnrealToPhononIPLVector3(CDI.GetWorldVertex(x + 1, y)));

						IPLTriangle Triangle;

						Triangle.indices[0] = StartIndex;
						Triangle.indices[1] = StartIndex + 2;
						Triangle.indices[2] = StartIndex + 3;
						IplTriangles.Add(Triangle);

						Triangle.indices[0] = StartIndex;
						Triangle.indices[1] = StartIndex + 1;
						Triangle.indices[2] = StartIndex + 2;
						IplTriangles.Add(Triangle);
					}
				}
			}
		}

		// The default landscape material is always registered at size(MaterialPresets) + 2
		auto MaterialIdx = MaterialPresets.Num() + 2;
		for (auto i = InitialNumTriangles; i < IplTriangles.Num(); ++i)
		{
			IplMaterialIndices.Add(MaterialIdx);
		}
	}

	//==============================================================================================================================================
	// Utility functions
	//==============================================================================================================================================

	/**
	 * Registers a new static mesh with Phonon.
	 */
	static void RegisterStaticMesh(IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices, IPLhandle* PhononStaticMesh)
	{
		if (IplVertices.Num() > 0)
		{
			UE_LOG(LogSteamAudio, Log, TEXT("Registering new mesh with %d triangles."), IplTriangles.Num());

			auto IplResult = iplCreateStaticMesh(PhononScene, IplVertices.Num(), IplTriangles.Num(), IplVertices.GetData(), IplTriangles.GetData(),
				IplMaterialIndices.GetData(), PhononStaticMesh);
			if (IplResult != IPL_STATUS_SUCCESS)
			{
				UE_LOG(LogSteamAudio, Warning, TEXT("Error adding a new object to the Phonon scene."));
				return;
			}
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Skipping mesh registration because no vertices were found."));
		}
	}

	/**
	 * Registers a new static mesh with Phonon, adding its handle to the provided array of static meshes.
	 */
	static void RegisterStaticMeshes(IPLhandle PhononScene, TArray<IPLVector3>& IplVertices, TArray<IPLTriangle>& IplTriangles,
		TArray<IPLint32>& IplMaterialIndices, TArray<IPLhandle>* PhononStaticMeshes)
	{
		if (IplVertices.Num() > 0)
		{
			UE_LOG(LogSteamAudio, Log, TEXT("Registering new mesh with %d triangles."), IplTriangles.Num());

			IPLhandle IplStaticMesh = nullptr;
			auto IplResult = iplCreateStaticMesh(PhononScene, IplVertices.Num(), IplTriangles.Num(), IplVertices.GetData(), IplTriangles.GetData(),
				IplMaterialIndices.GetData(), &IplStaticMesh);
			if (IplResult != IPL_STATUS_SUCCESS)
			{
				UE_LOG(LogSteamAudio, Warning, TEXT("Error adding a new object to the acoustic scene."));
				return;
			}

			PhononStaticMeshes->Add(IplStaticMesh);
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Skipping mesh registration because no vertices were found."));
		}
	}

	/**
	 * Calculates the total number of materials that must be registered with Phonon. This includes presets and any custom materials.
	 */
	static void GetPhononMaterials(UWorld* World, TArray<IPLMaterial>& IPLMaterials)
	{
		check(World);

		// There are size(MaterialPresets) + 3 fixed slots.
		uint32 NumMaterials = MaterialPresets.Num() + 3;

		for (TActorIterator<AActor> AActorItr(World); AActorItr; ++AActorItr)
		{
			auto PhononMaterialComponent = static_cast<UPhononMaterialComponent*>(
				AActorItr->GetComponentByClass(UPhononMaterialComponent::StaticClass()));
			if (PhononMaterialComponent)
			{

				if (PhononMaterialComponent->MaterialPreset == EPhononMaterial::CUSTOM)
				{
					PhononMaterialComponent->MaterialIndex = NumMaterials++;
					IPLMaterials.Add(PhononMaterialComponent->GetMaterialPreset());
					UE_LOG(LogSteamAudio, Log, TEXT("Custom Material [%i] added with Absorption[%f][%f][%f] Transmission[%f][%f][%f]"), 
						PhononMaterialComponent->MaterialIndex,
						PhononMaterialComponent->GetMaterialPreset().lowFreqAbsorption,
						PhononMaterialComponent->GetMaterialPreset().midFreqAbsorption,
						PhononMaterialComponent->GetMaterialPreset().highFreqAbsorption,
						PhononMaterialComponent->GetMaterialPreset().lowFreqTransmission,
						PhononMaterialComponent->GetMaterialPreset().midFreqTransmission,
						PhononMaterialComponent->GetMaterialPreset().highFreqTransmission);
				}
				else
				{
					PhononMaterialComponent->MaterialIndex = static_cast<int32>(PhononMaterialComponent->MaterialPreset);
				}
			}
		}
	}

#endif // WITH_EDITOR

	uint32 GetNumTrianglesForStaticMesh(AStaticMeshActor* StaticMeshActor)
	{
		uint32 NumTriangles = 0;

		if (StaticMeshActor == nullptr || StaticMeshActor->GetStaticMeshComponent() == nullptr ||
			StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh() == nullptr ||
			StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh()->GetRenderData() == nullptr)
		{
			return NumTriangles;
		}

		const auto& LODModel = StaticMeshActor->GetStaticMeshComponent()->GetStaticMesh()->GetRenderData()->LODResources[0];
		for (const auto& Section : LODModel.Sections)
		{
			NumTriangles += Section.NumTriangles;
		}

		return NumTriangles;
	}

	uint32 GetNumTrianglesAtRoot(AActor* RootActor)
	{
		uint32 NumTriangles = 0;

		if (RootActor == nullptr)
		{
			return NumTriangles;
		}

		NumTriangles = GetNumTrianglesForStaticMesh(Cast<AStaticMeshActor>(RootActor));

		TArray<AActor*> AttachedActors;
		RootActor->GetAttachedActors(AttachedActors);

		for (auto AttachedActor : AttachedActors)
		{
			NumTriangles += GetNumTrianglesAtRoot(AttachedActor);
		}

		return NumTriangles;
	}
}
