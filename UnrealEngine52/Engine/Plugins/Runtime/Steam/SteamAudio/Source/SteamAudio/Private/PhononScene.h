//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "phonon.h"

#include "Async/Async.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/StaticMeshActor.h"

class AActor;
class AStaticMeshActor;
class UWorld;

namespace SteamAudio
{
	struct FPhononSceneInfo
	{
		FPhononSceneInfo()
			: NumTriangles(0)
			, NumDynTriangles(0)
			, DataSize(0)
			, DynDataSize(0)
		{}

		uint32 NumTriangles;
		uint32 NumDynTriangles;
		uint32 DataSize;
		uint32 DynDataSize;
	};

	bool STEAMAUDIO_API LoadSceneFromDisk(UWorld* World, IPLhandle ComputeDevice, const IPLSimulationSettings& SimulationSettings,
		IPLhandle* PhononScene, FPhononSceneInfo& PhononSceneInfo, FString* CurrentLevelName);

	bool STEAMAUDIO_API LoadSceneInfoFromDisk(UWorld* World, FPhononSceneInfo& PhononSceneInfo);

	bool STEAMAUDIO_API LoadDynamicSceneFromDisk(UWorld* World, IPLhandle ComputeDevice, const IPLSimulationSettings& SimulationSettings,
		IPLhandle* PhononScene, FPhononSceneInfo& PhononSceneInfo, FString DynamicSceneID);

	bool STEAMAUDIO_API LoadDynamicScenesInfoFromDisk(UWorld* World, FPhononSceneInfo& PhononSceneInfo, FString DynamicSceneID);

#if WITH_EDITOR

	/** Creates and populates a Phonon scene. Gathers Phonon Geometry asynchronously on the game thread. */
	bool STEAMAUDIO_API CreateScene(UWorld* World, IPLhandle* PhononScene, IPLhandle* PhononStaticMesh, uint32& NumSceneTriangles, uint32& NumDynSceneTriangles, uint32& DynDataSize, bool CreateDynObjFiles = false);
	void STEAMAUDIO_API CreateDynamicScene(UWorld* World, IPLSimulationSettings SimulationSettings, TArray<IPLMaterial> SceneMaterials, uint32& NumSceneTriangles, uint32& DataSize, bool CreateObjFile = false);
	bool STEAMAUDIO_API SaveFinalizedSceneToDisk(UWorld* World, IPLhandle PhononScene, const FPhononSceneInfo& PhononSceneInfo);
	bool STEAMAUDIO_API SaveDynamicSceneToDisk(UWorld* World, IPLhandle PhononScene, const FPhononSceneInfo& PhononSceneInfo, FString DynamicSceneID, bool CreateObjFile = false);
	void STEAMAUDIO_API AddGeometryComponentsToStaticMeshes(UWorld* World);
	void STEAMAUDIO_API RemoveGeometryComponentsFromStaticMeshes(UWorld* World);

	/** Check a directory if it exists, create it if it doesn't. Return True if the directory exists. */
	bool STEAMAUDIO_API CheckSceneDirectory(FString DirectoryPath);

	/** Delete any scene files in a given path that start with the world/level name. */
	void STEAMAUDIO_API DeleteSceneFiles(UWorld* World, FString FilePath);

#endif // WITH_EDITOR

	uint32 GetNumTrianglesForStaticMesh(AStaticMeshActor* StaticMeshActor);
	uint32 GetNumTrianglesAtRoot(AActor* RootActor);
}
