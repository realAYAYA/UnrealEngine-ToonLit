//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononProbeVolumeDetails.h"

#include "PhononCommon.h"
#include "SteamAudioEditorModule.h"
#include "SteamAudioSettings.h"
#include "PhononReverb.h"
#include "SteamAudioEnvironment.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "PhononProbeVolume.h"
#include "PhononProbeComponent.h"
#include "TickableNotification.h"
#include "Async/Async.h"
#include "PhononScene.h"
#include "LevelEditorViewport.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Editor.h"

namespace SteamAudio
{
	static TSharedPtr<FTickableNotification> GGenerateProbesTickable = MakeShareable(new FTickableNotification());

	static void GenerateProbesProgressCallback(float Progress)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("GenerateProbesProgress"), FText::AsPercent(Progress));
		GGenerateProbesTickable->SetDisplayText(FText::Format(NSLOCTEXT("SteamAudio", "ComputingProbeLocationsText", "Computing probe locations ({GenerateProbesProgress} complete)"), Arguments));
	}

	TSharedRef<IDetailCustomization> FPhononProbeVolumeDetails::MakeInstance()
	{
		return MakeShareable(new FPhononProbeVolumeDetails);
	}

	FText FPhononProbeVolumeDetails::GetTotalDataSize()
	{
		return FText::AsMemory(PhononProbeVolume->ProbeDataSize);
	}

	void FPhononProbeVolumeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();

		for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
		{
			const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
			if (CurrentObject.IsValid())
			{
				APhononProbeVolume* CurrentCaptureActor = Cast<APhononProbeVolume>(CurrentObject.Get());
				if (CurrentCaptureActor)
				{
					PhononProbeVolume = CurrentCaptureActor;
					break;
				}
			}
		}

		DetailLayout.HideCategory("BrushSettings");

		DetailLayout.EditCategory("ProbeGeneration").AddProperty(GET_MEMBER_NAME_CHECKED(APhononProbeVolume, PlacementStrategy));
		DetailLayout.EditCategory("ProbeGeneration").AddProperty(GET_MEMBER_NAME_CHECKED(APhononProbeVolume, HorizontalSpacing));
		DetailLayout.EditCategory("ProbeGeneration").AddProperty(GET_MEMBER_NAME_CHECKED(APhononProbeVolume, HeightAboveFloor));
		DetailLayout.EditCategory("ProbeGeneration").AddCustomRow(NSLOCTEXT("SteamAudio", "GenerateProbes", "Generate Probes"))
			.NameContent()
			[
				SNullWidget::NullWidget
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(2)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.OnClicked(this, &FPhononProbeVolumeDetails::OnGenerateProbes)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SteamAudio", "GenerateProbes", "Generate Probes"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			];

			TSharedPtr<IPropertyHandle> BakedDataProperty = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(APhononProbeVolume, BakedDataInfo));
			TSharedRef<FDetailArrayBuilder> BakedDataBuilder = MakeShareable(new FDetailArrayBuilder(BakedDataProperty.ToSharedRef()));
			BakedDataBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FPhononProbeVolumeDetails::OnGenerateBakedDataInfo));
			DetailLayout.EditCategory("ProbeVolumeStatistics").AddProperty(GET_MEMBER_NAME_CHECKED(APhononProbeVolume, NumProbes));

			auto ProbeDataSize = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(APhononProbeVolume, ProbeDataSize));
			TAttribute<FText> TotalDataSize = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FPhononProbeVolumeDetails::GetTotalDataSize));
		
			DetailLayout.EditCategory("ProbeVolumeStatistics").AddProperty(ProbeDataSize).CustomWidget()
				.NameContent()
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SteamAudio", "ProbeDataSize", "Probe Data Size"))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				.ValueContent()
				[
					SNew(STextBlock)
					.Text(TotalDataSize)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				];
			DetailLayout.EditCategory("ProbeVolumeStatistics").AddCustomBuilder(BakedDataBuilder);
	}

	void FPhononProbeVolumeDetails::OnGenerateBakedDataInfo(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
	{
		auto& BakedDataRow = ChildrenBuilder.AddProperty(PropertyHandle);
		auto& BakedDataInfo = PhononProbeVolume->BakedDataInfo[ArrayIndex];

		BakedDataRow.ShowPropertyButtons(false);
		BakedDataRow.CustomWidget(false)
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromName(BakedDataInfo.Name))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(SBox)
					.MinDesiredWidth(200)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::AsMemory(BakedDataInfo.Size))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
				[
					PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateSP(this, &FPhononProbeVolumeDetails::OnClearBakedDataClicked, ArrayIndex))
				]
			];
	}

	void FPhononProbeVolumeDetails::OnClearBakedDataClicked(const int32 ArrayIndex)
	{
		IPLhandle ProbeBox = nullptr;
		PhononProbeVolume->LoadProbeBoxFromDisk(&ProbeBox);
		
		FIdentifierMap IdentifierMap;
		LoadBakedIdentifierMapFromDisk(PhononProbeVolume->GetWorld(), IdentifierMap);
		
		FString BakedDataString = PhononProbeVolume->BakedDataInfo[ArrayIndex].Name.ToString().ToLower();

		IPLBakedDataIdentifier BakedDataIdentifier;
		BakedDataIdentifier.type = BakedDataString.Equals("__reverb__") ? IPL_BAKEDDATATYPE_REVERB : IPL_BAKEDDATATYPE_STATICSOURCE;
		BakedDataIdentifier.identifier = IdentifierMap.Get(BakedDataString);
		
		iplDeleteBakedDataByIdentifier(ProbeBox, BakedDataIdentifier);
		PhononProbeVolume->BakedDataInfo.RemoveAt(ArrayIndex);
		PhononProbeVolume->UpdateProbeData(ProbeBox);
		iplDestroyProbeBox(&ProbeBox);
	}

	FReply FPhononProbeVolumeDetails::OnGenerateProbes()
	{
		GGenerateProbesTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "GeneratingProbes", "Generating probes..."));
		GGenerateProbesTickable->CreateNotification();

		// Grab a copy of the volume ptr as it will be destroyed if user clicks off of volume in GUI
		auto PhononProbeVolumeHandle = PhononProbeVolume.Get();

		Async(EAsyncExecution::Thread, [PhononProbeVolumeHandle]()
		{
			// Load the scene
			UWorld* World = GEditor->GetLevelViewportClients()[0]->GetWorld();
			IPLhandle PhononScene = nullptr;
			FPhononSceneInfo PhononSceneInfo;

			GGenerateProbesTickable->SetDisplayText(NSLOCTEXT("SteamAudio", "LoadingScene", "Loading scene..."));

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

			// Attempt to load from disk, otherwise export
			if (!LoadSceneFromDisk(World, ComputeDevice, SimulationSettings, &PhononScene, PhononSceneInfo, nullptr))
			{
				IPLhandle PhononStaticMesh = nullptr;

				if (!CreateScene(World, &PhononScene, &PhononStaticMesh, PhononSceneInfo.NumTriangles, PhononSceneInfo.NumDynTriangles, PhononSceneInfo.DynDataSize))
				{
					GGenerateProbesTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
						DisplayText = NSLOCTEXT("SteamAudio", "UnableToCreateScene", "Unable to create scene.");
					}, SNotificationItem::CS_Fail, true));

					if (ComputeDevice)
					{
						iplDestroyComputeDevice(&ComputeDevice);
					}

					return;
				}

				PhononSceneInfo.DataSize = iplSaveScene(PhononScene, nullptr);
				bool SaveSceneSuccessful = SaveFinalizedSceneToDisk(World, PhononScene, PhononSceneInfo);

				if (SaveSceneSuccessful)
				{
					FSteamAudioEditorModule* Module = &FModuleManager::GetModuleChecked<FSteamAudioEditorModule>("SteamAudioEditor");
					if (Module != nullptr)
					{
						Module->SetCurrentPhononSceneInfo(PhononSceneInfo);
					}
				}

				iplDestroyStaticMesh(&PhononStaticMesh);
			}

			// Place probes
			IPLhandle SceneCopy = PhononScene;
			TArray<IPLSphere> ProbeSpheres;
			PhononProbeVolumeHandle->PlaceProbes(PhononScene, GenerateProbesProgressCallback, ProbeSpheres);
			PhononProbeVolumeHandle->BakedDataInfo.Empty();

			// Clean up
			iplDestroyScene(&SceneCopy);

			if (ComputeDevice)
			{
				iplDestroyComputeDevice(&ComputeDevice);
			}

			// Update probe component with new probe locations
			{
				FScopeLock ScopeLock(&PhononProbeVolumeHandle->PhononProbeComponent->ProbeLocationsCriticalSection);
				auto& ProbeLocations = PhononProbeVolumeHandle->GetPhononProbeComponent()->ProbeLocations;
				ProbeLocations.Empty();
				ProbeLocations.SetNumUninitialized(ProbeSpheres.Num());
				for (auto i = 0; i < ProbeSpheres.Num(); ++i)
				{
					ProbeLocations[i] = SteamAudio::PhononToUnrealFVector(SteamAudio::FVectorFromIPLVector3(ProbeSpheres[i].center));
				}
			}
			
			// Notify UI that we're done
			GGenerateProbesTickable->QueueWorkItem(FWorkItem([](FText& DisplayText) {
				DisplayText = NSLOCTEXT("SteamAudio", "ProbePlacementComplete", "Probe placement complete."); 
			}, SNotificationItem::CS_Success, true));
		});

		return FReply::Handled();
	}
}
