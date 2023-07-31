//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononGeometryComponent.h"
#include "PhononScene.h"
#include "StaticMeshResources.h"
#include "PhononPluginManager.h"

UPhononGeometryComponent::UPhononGeometryComponent()
	: ExportAllChildren(false)
	, NumVertices(0)
	, NumTriangles(0)
{
}


void UPhononGeometryComponent::BeginPlay()
{
	Super::BeginPlay();

	// Check if we are parented to a movable mesh
	USceneComponent* Parent = GetAttachParent();

	if (Parent
		&& Parent->IsValidLowLevelFast()
		&& Parent->Mobility == EComponentMobility::Movable
		)
	{
		bHasDynamicParent = true;

		// Register this component to the plugin manager as holding on to dynamic geometry
		FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice();
		if (AudioDevice)
		{
			for (TAudioPluginListenerPtr AudioListener : AudioDevice->PluginListeners)
			{
				auto SteamAudioListener = StaticCastSharedPtr<SteamAudio::FPhononPluginManager>(AudioListener);

				if (SteamAudioListener && SteamAudioListener->PluginID.IsEqual(FName(TEXT("Phonon"))))
				{
					// Add dynamic geometry to runtime cache array
					SteamAudioListener->AddDynamicGeometry(this);
				}
			}
		}
	}
}


void UPhononGeometryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Destroy any instanced meshes created
	if (bHasDynamicParent)
	{	
		// Unregister this component from the plugin manager
		FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice();
		if (AudioDevice)
		{
			for (TAudioPluginListenerPtr AudioListener : AudioDevice->PluginListeners)
			{
				auto SteamAudioListener = StaticCastSharedPtr<SteamAudio::FPhononPluginManager>(AudioListener);

				if (SteamAudioListener && SteamAudioListener->PluginID.IsEqual(FName(TEXT("Phonon"))))
				{
					SteamAudioListener->RemoveDynamicGeometry(this);
				}
			}
		}
	}

}

void UPhononGeometryComponent::UpdateStatistics()
{
#if WITH_EDITOR
	if (ExportAllChildren)
	{
		NumTriangles = SteamAudio::GetNumTrianglesAtRoot(GetOwner());
	}
	else
	{
		NumTriangles = SteamAudio::GetNumTrianglesForStaticMesh(Cast<AStaticMeshActor>(GetOwner()));
	}

	NumVertices = NumTriangles * 3;
#endif // WITH_EDITOR
}

#if WITH_EDITOR
void UPhononGeometryComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UPhononGeometryComponent, ExportAllChildren)))
	{
		UpdateStatistics();
	}
}
#endif

void UPhononGeometryComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

#if WITH_EDITOR
	UpdateStatistics();
#endif // WITH_EDITOR
}
