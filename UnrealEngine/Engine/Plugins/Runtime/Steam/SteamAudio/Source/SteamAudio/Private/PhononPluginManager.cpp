//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononPluginManager.h"
#include "SteamAudioModule.h"
#include "AudioDevice.h"
#include "HAL/FileManager.h"
#include "DrawDebugHelpers.h"

namespace SteamAudio
{
	const FName FPhononPluginManager::PluginID = FName("Phonon");
	TArray<FDynamicGeometryMap> FPhononPluginManager::DynamicGeometry;
	TMap<FString, IPLhandle> FPhononPluginManager::DynamicSceneMap;
	UWorld* FPhononPluginManager::CurrentLevel;
		
	FPhononPluginManager::FPhononPluginManager()
		: bEnvironmentInitialized(false)
		, ReverbPtr(nullptr)
		, OcclusionPtr(nullptr)
	{
	}

	FPhononPluginManager::~FPhononPluginManager()
	{
		// Perform cleanup here instead of in OnListenerShutdown, because plugins will still be active and may be using them
		if (bEnvironmentInitialized)
		{
			Environment.Shutdown();
			bEnvironmentInitialized = false;
		}
	}

	void FPhononPluginManager::InitializeEnvironment(FAudioDevice* AudioDevice, UWorld* ListenerWorld)
	{
		if (ListenerWorld->WorldType == EWorldType::Editor || bEnvironmentInitialized)
		{
			return;
		}

		UE_LOG(LogSteamAudio, Log, TEXT("Initializing environment for [%s] world type [%i]."), *ListenerWorld->GetMapName(), (unsigned int)ListenerWorld->WorldType.GetValue());

		CurrentLevel = ListenerWorld;
		bool bIsUsingOcclusion = IsUsingSteamAudioPlugin(EAudioPlugin::OCCLUSION);
		bool bIsUsingReverb = IsUsingSteamAudioPlugin(EAudioPlugin::REVERB);

		// Get level name
		FString CurrentLevelName;
		CurrentLevelName = CurrentLevel->GetName();

		if (bIsUsingOcclusion || bIsUsingReverb)
		{
			if (Environment.Initialize(ListenerWorld, AudioDevice, &CurrentLevelName))
			{
				FScopeLock EnvironmentLock(Environment.GetEnvironmentCriticalSectionHandle());

				UE_LOG(LogSteamAudio, Log, TEXT("Environment initialization successful for level %s."), *CurrentLevelName);

				if (bIsUsingReverb)
				{
					ReverbPtr = static_cast<FPhononReverb*>(AudioDevice->ReverbPluginInterface.Get());
					ReverbPtr->SetEnvironment(&Environment);
					ReverbPtr->CreateReverbEffect();
				}

				if (bIsUsingOcclusion)
				{
					OcclusionPtr = static_cast<FPhononOcclusion*>(AudioDevice->OcclusionInterface.Get());
					OcclusionPtr->SetEnvironment(&Environment);
				}
				
				// Load dynamic scenes for the current level (if any)
				TArray<FString> DynamicSceneFiles;
				FString ScenePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*(DynamicRuntimePath / CurrentLevelName + "_*.phononscene"));

				if (FPaths::DirectoryExists(DynamicRuntimePath))
				{
					IFileManager& FileManager = IFileManager::Get();
					FileManager.FindFiles(DynamicSceneFiles, *ScenePath, true, false);

					for (auto DynamicSceneFile : DynamicSceneFiles)
					{
						IPLhandle DynamicPhononScene = nullptr;
						FPhononSceneInfo PhononSceneInfo;
						FString DynamicSceneFileName = FPaths::GetBaseFilename(DynamicSceneFile);

						// Get Dynamic Scene File
						if (!LoadDynamicSceneFromDisk(CurrentLevel, Environment.GetComputeDevice(), Environment.GetSimulationSettings(), &DynamicPhononScene, PhononSceneInfo, DynamicSceneFileName))
						{
							UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load dynamic scene file [%s]. Do a Scene Export and ensure your ray tracer is supported (Embree)."), *DynamicSceneFileName);
							continue;
						}

						// Check if this scene have dynamic meshes to be added
						bool bFoundInMap = false;
						for (int32 i = 0; i < DynamicGeometry.Num(); i++)
						{
							if (DynamicGeometry[i].ID.Equals(DynamicSceneFileName)
								&& DynamicGeometry[i].DynamicGeometryComponent
								&& DynamicGeometry[i].DynamicGeometryComponent->IsValidLowLevelFast()
								)
							{
								// Get 4x4 Matrix (1D array) from world transform
								float Matrix[16];
								auto DynGeoTransform = DynamicGeometry[i].DynamicGeometryComponent->GetAttachParent()->GetComponentTransform();
								
								GetMatrixForTransform(DynGeoTransform, Matrix, true, false);
								IPLMatrix4x4 PhononMatrix = GetIPLMatrix(Matrix);
								//DrawDebugSphere(ListenerWorld, DynamicGeometry[i].DynamicGeometryComponent->GetOwner()->GetRootComponent()->GetComponentTransform().GetTranslation(), 20.f, 4, FColor::Green, 9999.f);

								// Create SteamAudio Instanced Mesh
								IPLhandle InstancedMesh = nullptr;
								IPLerror ErrCreateMesh = iplCreateInstancedMesh(Environment.GetScene(), DynamicPhononScene, PhononMatrix, &InstancedMesh);

								if (ErrCreateMesh == IPL_STATUS_SUCCESS)
								{
									// Add Instanced Mesh to scene
									iplAddInstancedMesh(Environment.GetScene(), InstancedMesh);

									// Add references to map
									DynamicGeometry[i].DynamicScene = DynamicPhononScene;
									DynamicGeometry[i].DynamicGeometry = InstancedMesh;
									UE_LOG(LogSteamAudio, Log, TEXT("Created instanced mesh [%s]"), *DynamicSceneFileName);
								}
								else
								{
									UE_LOG(LogSteamAudio, Warning, TEXT("Unable to create instanced mesh for [%s]"), *DynamicSceneFileName);
								}

								bFoundInMap = true;
								break;
							}
						}

						if (!bFoundInMap)
						{
							DynamicSceneMap.Add(DynamicSceneFileName, DynamicPhononScene);
							UE_LOG(LogSteamAudio, Warning, TEXT("Loaded dynamic scene file [%s] but no dynamic geometry found in this level."), *DynamicSceneFileName);
						}
					}
				}

				bEnvironmentInitialized = true;
			}
			else
			{
				UE_LOG(LogSteamAudio, Warning, TEXT("Environment initialization unsuccessful for Level %s."), *CurrentLevelName);
			}
		}
	}

	void FPhononPluginManager::OnListenerInitialize(FAudioDevice* AudioDevice, UWorld* ListenerWorld)
	{
		InitializeEnvironment(AudioDevice, ListenerWorld);
	}

	void FPhononPluginManager::OnListenerUpdated(FAudioDevice* AudioDevice, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
	{
		if (!bEnvironmentInitialized)
		{
			return;
		}

		FVector Position = ListenerTransform.GetLocation();
		FVector Forward = ListenerTransform.GetUnitAxis(EAxis::X);
		FVector Up = ListenerTransform.GetUnitAxis(EAxis::Z);
		FVector Right = ListenerTransform.GetUnitAxis(EAxis::Y);

		if (OcclusionPtr)
		{
			OcclusionPtr->UpdateDirectSoundSources(Position, Forward, Up, Right);
		}

		if (ReverbPtr)
		{
			ReverbPtr->UpdateListener(Position, Forward, Up, Right);
		}
	}

	void FPhononPluginManager::OnListenerShutdown(FAudioDevice* AudioDevice)
	{
		FSteamAudioModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioModule>("SteamAudio");
		if (Module != nullptr)
		{
			Module->UnregisterAudioDevice(AudioDevice);
		}
	}
	
	void FPhononPluginManager::OnTick(UWorld* InWorld, const int32 ViewportIndex, const FTransform& ListenerTransform, const float InDeltaSeconds)
	{
		// Update any dynamic geometry
		if (DynamicGeometry.Num() > 0)
		{
			// Update the transforms of the dynamic geometry
			for (auto Geometry : DynamicGeometry)
			{
				// Check if we both have a dynamic mesh and dynamic scene
				if (Geometry.DynamicGeometry
					&& Geometry.DynamicScene
					&& Geometry.DynamicGeometryComponent
					&& Geometry.DynamicGeometryComponent->IsValidLowLevelFast()
					)
				{
					// Get 4x4 Matrix (1D array) from world transform
					float Matrix[16];
					FTransform DynGeoTransform = Geometry.DynamicGeometryComponent->GetAttachParent()->GetComponentTransform();

					GetMatrixForTransform(DynGeoTransform, Matrix, true, false);
					IPLMatrix4x4 PhononMatrix = GetIPLMatrix(Matrix);
					//DrawDebugSphere(InWorld, DynGeoTransform.GetTranslation(), 20.f, 4, FColor::Red);

					// Update instanced mesh transform
					iplUpdateInstancedMeshTransform(Geometry.DynamicGeometry, PhononMatrix);

					// Commit changes to the main scene
					iplCommitScene(Environment.GetScene());
				}
			}
		}
	}

	void FPhononPluginManager::OnWorldChanged(FAudioDevice* AudioDevice, UWorld* ListenerWorld)
	{
		UE_LOG(LogSteamAudio, Log, TEXT("World changed to %s. Reinitializing environment."), *ListenerWorld->GetMapName());

		if (bEnvironmentInitialized)
		{
			Environment.Shutdown();
			bEnvironmentInitialized = false;
		}

		InitializeEnvironment(AudioDevice, ListenerWorld);
	}

	bool FPhononPluginManager::IsUsingSteamAudioPlugin(EAudioPlugin PluginType)
	{
		FSteamAudioModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioModule>("SteamAudio");

		// If we can't get the module from the module manager, then we don't have any of these plugins loaded.
		if (Module == nullptr)
		{
			return false;
		}

		FString SteamPluginName = Module->GetPluginFactory(PluginType)->GetDisplayName();
		FString CurrentPluginName = AudioPluginUtilities::GetDesiredPluginName(PluginType);
		return CurrentPluginName.Equals(SteamPluginName);
	}
	
	void FPhononPluginManager::AddDynamicGeometry(UPhononGeometryComponent* PhononGeometryComponent)
	{
		check(PhononGeometryComponent);

		// Check if there's a valid world and the geometry component being registered is valid
		if (PhononGeometryComponent->IsValidLowLevel())
		{
			FString DynamicGeometryID = PhononGeometryComponent->GetWorld()->GetName() + "_" + PhononGeometryComponent->GetOwner()->GetName() + "_" + PhononGeometryComponent->GetAttachParent()->GetName();
			DynamicGeometry.Add({ DynamicGeometryID, PhononGeometryComponent, nullptr, nullptr });
			UE_LOG(LogSteamAudio, Log, TEXT("Dynamic Geometry Added [%s] Current Count: %i"), *DynamicGeometryID, DynamicGeometry.Num());
		}
	}


	void FPhononPluginManager::RemoveDynamicGeometry(UPhononGeometryComponent* PhononGeometryComponent)
	{
		check(PhononGeometryComponent);

		// Check if there's a valid world and the geometry component being registered is valid
		if (PhononGeometryComponent->IsValidLowLevel())
		{
			FString DynamicGeometryID = PhononGeometryComponent->GetWorld()->GetName() + "_" + PhononGeometryComponent->GetOwner()->GetName() + "_" + PhononGeometryComponent->GetAttachParent()->GetName();

			// Check if this scene have dynamic meshes to be added
			bool bFoundInMap = false;
			for (int32 i = 0; i < DynamicGeometry.Num(); i++)
			{
				if (DynamicGeometry[i].ID.Equals(DynamicGeometryID)
					&& DynamicGeometry[i].DynamicGeometryComponent
					&& DynamicGeometry[i].DynamicGeometryComponent->IsValidLowLevelFast()
					)
				{
					// Delete ipl instances
					if (DynamicGeometry[i].DynamicGeometry)
					{
						iplDestroyStaticMesh(&(DynamicGeometry[i].DynamicGeometry));
					}

					if (DynamicGeometry[i].DynamicScene)
					{
						iplDestroyScene(&(DynamicGeometry[i].DynamicScene));
					}

					// Remove map entry
					DynamicGeometry.RemoveAt(i);
				}
			}

		}

	}
	
	FEnvironment& FPhononPluginManager::GetPhononEnvironment()
	{
		return Environment;
	}
	
}
