// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/SceneComponent.h"
#include "OculusAudioMaterialComponent.h"

#include "OculusAudioGeometryLandscape.generated.h"


class UWorld;


UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class UOculusAudioGeometryLandscape : public UOculusAudioMaterialComponent
{
	GENERATED_BODY()

	void Serialize(FArchive & Ar) override;

	ovrAudioGeometry ovrGeometryLandscape = nullptr;

	void CreateGeometryFromLandscape(ovrAudioContext Context, UWorld* World, ovrAudioGeometry* Geometry);
};