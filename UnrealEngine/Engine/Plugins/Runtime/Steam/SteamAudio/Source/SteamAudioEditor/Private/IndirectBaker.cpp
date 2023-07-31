//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "IndirectBaker.h"
#include "TickableNotification.h"
#include "PhononSourceComponent.h"
#include "PhononProbeVolume.h"
#include "PhononScene.h"
#include "SteamAudioSettings.h"
#include "SteamAudioEditorModule.h"
#include "PhononReverb.h"
#include "SteamAudioEnvironment.h"

#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "LevelEditorViewport.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Components/AudioComponent.h"

namespace SteamAudio
{
	TAtomic<bool> GIsBaking(false);

	static TSharedPtr<FTickableNotification> GBakeTickable = MakeShareable(new FTickableNotification());
	static int32 GCurrentProbeVolume = 0;
	static int32 GNumProbeVolumes = 0;
	static int32 GCurrentBakeTask = 0;
	static int32 GNumBakeTasks = 0;

	static void BakeProgressCallback(float Progress)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("BakeProgress"), FText::AsPercent(Progress));
		Arguments.Add(TEXT("CurrentProbeVolume"), FText::AsNumber(GCurrentProbeVolume));
		Arguments.Add(TEXT("NumProbeVolumes"), FText::AsNumber(GNumProbeVolumes));
		Arguments.Add(TEXT("NumBakeTasks"), FText::AsNumber(GNumBakeTasks));
		Arguments.Add(TEXT("CurrentBakeTask"), FText::AsNumber(GCurrentBakeTask));
		GBakeTickable->SetDisplayText(FText::Format(NSLOCTEXT("SteamAudio", "BakeProgressFmt", "Baking {CurrentBakeTask}/{NumBakeTasks} sources \n {CurrentProbeVolume}/{NumProbeVolumes} probe volumes ({BakeProgress} complete)"), Arguments));
	}

	static void CancelBake()
	{
		iplCancelBake();
		GIsBaking = false;
	}

	/**
	 * Bakes propagation for all sources in PhononSourceComponents. Bakes reverb if BakeReverb is set. Performs baking across all probe volumes
	 * in the scene. Runs baking in an async task so that UI remains responsive.
	 */
	void Bake(const TArray<UPhononSourceComponent*> PhononSourceComponents, const bool BakeReverb, FBakedSourceUpdated BakedSourceUpdated)
	{
		GIsBaking = true;

		GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "Baking", "Baking..."));
		GBakeTickable->CreateNotificationWithCancel(FSimpleDelegate::CreateStatic(CancelBake));

		auto World = GEditor->GetLevelViewportClients()[0]->GetWorld();
		check(World);

		GNumBakeTasks = BakeReverb ? PhononSourceComponents.Num() + 1 : PhononSourceComponents.Num();
		GCurrentBakeTask = 1;

		// Get all probe volumes (cannot do this in the async task - not on game thread)
		TArray<AActor*> PhononProbeVolumes;
		UGameplayStatics::GetAllActorsOfClass(World, APhononProbeVolume::StaticClass(), PhononProbeVolumes);

		Async(EAsyncExecution::Thread, [=]()
		{
			// Ensure we have at least one probe
			bool AtLeastOneProbe = false;

			for (auto PhononProbeVolumeActor : PhononProbeVolumes)
			{
				auto PhononProbeVolume = Cast<APhononProbeVolume>(PhononProbeVolumeActor);
				if (PhononProbeVolume->NumProbes > 0)
				{
					AtLeastOneProbe = true;
					break;
				}
			}

			if (!AtLeastOneProbe)
			{
				UE_LOG(LogSteamAudioEditor, Error, TEXT("Ensure at least one Phonon Probe Volume with probes exists."));
				GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "BakeFailed_NoProbes", "Bake failed. Create at least one Phonon Probe Volume that has probes."));
				GBakeTickable->DestroyNotification(SNotificationItem::CS_Fail);
				GIsBaking = false;
				return;
			}

			IPLBakingSettings BakingSettings;
			BakingSettings.bakeParametric = IPL_FALSE;
			BakingSettings.bakeConvolution = IPL_TRUE;

			IPLhandle ComputeDevice = nullptr;

			IPLComputeDeviceFilter DeviceFilter;
			DeviceFilter.fractionCUsForIRUpdate = GetDefault<USteamAudioSettings>()->GetFractionComputeUnitsForIRUpdate();
			DeviceFilter.maxCUsToReserve = GetDefault<USteamAudioSettings>()->MaxComputeUnits;
			DeviceFilter.type = IPL_COMPUTEDEVICE_GPU;

			IPLSimulationSettings SimulationSettings = GetDefault<USteamAudioSettings>()->GetBakedSimulationSettings();

			// If we are using RadeonRays, attempt to create a compute device.
			if (GetDefault<USteamAudioSettings>()->RayTracer == EIplRayTracerType::RADEONRAYS)
			{
				UE_LOG(LogSteamAudioEditor, Log, TEXT("Using Radeon Rays - creating GPU compute device..."));

				IPLerror Error = iplCreateComputeDevice(GlobalContext, DeviceFilter, &ComputeDevice);

				// If we failed to create a compute device, fall back to Phonon scene.
				if (Error != IPL_STATUS_SUCCESS)
				{
					UE_LOG(LogSteamAudioEditor, Warning, TEXT("Failed to create GPU compute device, falling back to Phonon."));

					SimulationSettings.sceneType = IPL_SCENETYPE_PHONON;
					SimulationSettings.bakingBatchSize = 1;
				}
			}

			IPLhandle PhononScene = nullptr;
			IPLhandle PhononEnvironment = nullptr;
			FPhononSceneInfo PhononSceneInfo;

			GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "LoadingScene", "Loading scene..."));

			// Load the scene
			if (!LoadSceneFromDisk(World, ComputeDevice, SimulationSettings, &PhononScene, PhononSceneInfo, nullptr))
			{
				// If we can't find the scene, then presumably they haven't generated probes either, so just exit
				UE_LOG(LogSteamAudioEditor, Error, TEXT("Unable to create Phonon environment: .phononscene not found. Be sure to export the scene."));

				if (ComputeDevice)
				{
					iplDestroyComputeDevice(&ComputeDevice);
				}

				GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "BakeFailed_NoScene", "Bake failed. Export scene first."));
				GBakeTickable->DestroyNotification(SNotificationItem::CS_Fail);
				GIsBaking = false;
				return;
			}

			iplCreateEnvironment(SteamAudio::GlobalContext, ComputeDevice, SimulationSettings, PhononScene, nullptr, &PhononEnvironment);

			if (BakeReverb)
			{
				GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "Baking", "Baking..."));
				GNumProbeVolumes = PhononProbeVolumes.Num();
				GCurrentProbeVolume = 1;

				for (auto PhononProbeVolumeActor : PhononProbeVolumes)
				{
					auto PhononProbeVolume = Cast<APhononProbeVolume>(PhononProbeVolumeActor);

					IPLhandle ProbeBox = nullptr;
					PhononProbeVolume->LoadProbeBoxFromDisk(&ProbeBox);

					IPLBakedDataIdentifier ReverbIdentifier;
					ReverbIdentifier.identifier = 0;
					ReverbIdentifier.type = IPL_BAKEDDATATYPE_REVERB;

					iplDeleteBakedDataByIdentifier(ProbeBox, ReverbIdentifier);
					iplBakeReverb(PhononEnvironment, ProbeBox, BakingSettings, BakeProgressCallback);

					if (!GIsBaking.Load())
					{
						iplDestroyProbeBox(&ProbeBox);
						break;
					}

					FBakedDataInfo BakedDataInfo;
					BakedDataInfo.Name = "__reverb__";
					BakedDataInfo.Size = iplGetBakedDataSizeByIdentifier(ProbeBox, ReverbIdentifier);

					auto ExistingInfo = PhononProbeVolume->BakedDataInfo.FindByPredicate([=](const FBakedDataInfo& InfoItem)
					{
						return InfoItem.Name == BakedDataInfo.Name;
					});

					if (ExistingInfo)
					{
						ExistingInfo->Size = BakedDataInfo.Size;
					}
					else
					{
						PhononProbeVolume->BakedDataInfo.Add(BakedDataInfo);
						PhononProbeVolume->BakedDataInfo.Sort();
					}

					PhononProbeVolume->UpdateProbeData(ProbeBox);
					iplDestroyProbeBox(&ProbeBox);
					++GCurrentProbeVolume;
				}

				if (!GIsBaking.Load())
				{
					iplDestroyEnvironment(&PhononEnvironment);
					iplDestroyScene(&PhononScene);

					if (ComputeDevice)
					{
						iplDestroyComputeDevice(&ComputeDevice);
					}

					GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "BakeCancelled", "Bake cancelled."));
					GBakeTickable->DestroyNotification(SNotificationItem::CS_Fail);
					return;
				}

				BakedSourceUpdated.ExecuteIfBound("__reverb__");

				++GCurrentBakeTask;
			}

			// IN PROGRESS
			FIdentifierMap BakedIdentifierMap;
			LoadBakedIdentifierMapFromDisk(World, BakedIdentifierMap);

			for (UPhononSourceComponent* PhononSourceComponent : PhononSourceComponents)
			{
				// Check if phonon source is valid
				if (!PhononSourceComponent || !PhononSourceComponent->IsValidLowLevelFast())
				{
					UE_LOG(LogSteamAudioEditor, Warning, TEXT("Phonon Source Component is invalid. It will be skipped."));
					continue;
				}
				else if (!PhononSourceComponent->GetOwner() && !PhononSourceComponent->GetOwner()->IsValidLowLevelFast())
				{
					UE_LOG(LogSteamAudioEditor, Warning, TEXT("Phonon Source Component \"%s\" is not attached to a valid actor. It will be skipped."), *(PhononSourceComponent->UniqueIdentifier.ToString()));
					continue;
				}
				else
				{
					UE_LOG(LogSteamAudioEditor, Log, TEXT("Phonon Source Component \"%s\" was used to trigger this bake task."), *(PhononSourceComponent->UniqueIdentifier.ToString()));
				}

				// Go through all audio components that the owner of this phonon source actor has
				TArray<FString> BakeAudioIdentifiers;
				TArray<UActorComponent*> Components;
				PhononSourceComponent->GetOwner()->GetComponents(UAudioComponent::StaticClass(), Components);

				for (auto ActorComp : Components)
				{
					if (ActorComp && ActorComp->IsValidLowLevel())
					{
						UAudioComponent* AudioComponent = Cast<UAudioComponent>(ActorComp);

						// Set the User ID on the audio component
						if (AudioComponent && AudioComponent->IsValidLowLevelFast())
						{
							// Apply the phonon source id to this audio component
							AudioComponent->AudioComponentUserID = PhononSourceComponent->UniqueIdentifier;
							//UE_LOG(LogTemp, Warning, TEXT("Audio Component [%s] UserID is now set to [%s]"), *AudioComponent->GetFullName(), *AudioComponent->AudioComponentUserID.ToString());

							// Check if there's a phonon source child for this audio component
							for (auto ChildComp : AudioComponent->GetAttachChildren())
							{
								auto* ChildPhononComponent = Cast<UPhononSourceComponent>(ChildComp);

								// If there is one, then apply it's child phonon source id, overriding and phonon source id already applied to it
								if (ChildPhononComponent && ChildPhononComponent->IsValidLowLevelFast())
								{
									AudioComponent->AudioComponentUserID = ChildPhononComponent->UniqueIdentifier;
									//UE_LOG(LogTemp, Warning, TEXT("Audio Component [%s] UserID is now overriden to [%s]"), *AudioComponent->GetFullName(), *AudioComponent->AudioComponentUserID.ToString());
									break;
								}
							}

							// Apply id to bake map if not present
							FString SourceString = AudioComponent->GetAudioComponentUserID().ToString().ToLower();
							if (!BakedIdentifierMap.ContainsKey(SourceString))
							{
								BakedIdentifierMap.Add(SourceString);
							}

							BakeAudioIdentifiers.AddUnique(SourceString);
						}
					}
				}

				// Bake audio
				if (BakeAudioIdentifiers.Num() > 0)
				{
					for (auto BakeAudioIdentifier : BakeAudioIdentifiers)
					{
						IPLBakedDataIdentifier SourceIdentifier;
						SourceIdentifier.type = IPL_BAKEDDATATYPE_STATICSOURCE;
						SourceIdentifier.identifier = BakedIdentifierMap.Get(BakeAudioIdentifier);

						FString BakingText = FString::Printf(TEXT("Baking Audio %s"), *BakeAudioIdentifier);
						GBakeTickable->SetDisplayText(FText::AsCultureInvariant(BakingText));
						GNumProbeVolumes = PhononProbeVolumes.Num();
						GCurrentProbeVolume = 1;

						UE_LOG(LogTemp, Log, TEXT("Baking phonon source component %s"), *BakeAudioIdentifier);

						for (auto PhononProbeVolumeActor : PhononProbeVolumes)
						{
							auto PhononProbeVolume = Cast<APhononProbeVolume>(PhononProbeVolumeActor);

							IPLhandle ProbeBox = nullptr;
							PhononProbeVolume->LoadProbeBoxFromDisk(&ProbeBox);

							IPLSphere SourceInfluence;
							SourceInfluence.radius = PhononSourceComponent->BakingRadius * SteamAudio::SCALEFACTOR;
							SourceInfluence.center = SteamAudio::UnrealToPhononIPLVector3(PhononSourceComponent->GetComponentLocation());

							iplDeleteBakedDataByIdentifier(ProbeBox, SourceIdentifier);
							iplBakePropagation(PhononEnvironment, ProbeBox, SourceInfluence, SourceIdentifier, BakingSettings, BakeProgressCallback);

							if (!GIsBaking.Load())
							{
								iplDestroyProbeBox(&ProbeBox);
								break;
							}

							FBakedDataInfo BakedDataInfo;
							BakedDataInfo.Name = PhononSourceComponent->UniqueIdentifier;
							BakedDataInfo.Size = iplGetBakedDataSizeByIdentifier(ProbeBox, SourceIdentifier);

							auto ExistingInfo = PhononProbeVolume->BakedDataInfo.FindByPredicate([=](const FBakedDataInfo& InfoItem)
							{
								return InfoItem.Name == BakedDataInfo.Name;
							});

							if (ExistingInfo)
							{
								ExistingInfo->Size = BakedDataInfo.Size;
							}
							else
							{
								PhononProbeVolume->BakedDataInfo.Add(BakedDataInfo);
								PhononProbeVolume->BakedDataInfo.Sort();
							}

							PhononProbeVolume->UpdateProbeData(ProbeBox);
							iplDestroyProbeBox(&ProbeBox);
							++GCurrentProbeVolume;
						}

						const bool bIsBaking = GIsBaking.Load();
						if (!bIsBaking)
						{
							break;
						}

						BakedSourceUpdated.ExecuteIfBound(PhononSourceComponent->UniqueIdentifier);

						++GCurrentBakeTask;
					}
				}
				else
				{
					UE_LOG(LogSteamAudioEditor, Warning, TEXT("Actor containing the Phonon source \"%s\" has no Audio Component. It will be skipped."), *(PhononSourceComponent->UniqueIdentifier.ToString()));
				}
			}

			SaveBakedIdentifierMapToDisk(World, BakedIdentifierMap);

			iplDestroyEnvironment(&PhononEnvironment);
			iplDestroyScene(&PhononScene);

			if (ComputeDevice)
			{
				iplDestroyComputeDevice(&ComputeDevice);
			}

			if (!GIsBaking.Load())
			{
				GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "BakeCancelled", "Bake cancelled."));
				GBakeTickable->DestroyNotification(SNotificationItem::CS_Fail);
			}
			else
			{
				GBakeTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "BakePropagationComplete", "Bake propagation complete."));
				GBakeTickable->DestroyNotification();
				GIsBaking = false;
			}
		});
	}
}
