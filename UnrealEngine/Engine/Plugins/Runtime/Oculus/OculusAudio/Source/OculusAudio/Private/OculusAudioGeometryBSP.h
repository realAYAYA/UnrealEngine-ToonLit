// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "OculusAudioMaterialComponent.h"

#include "OculusAudioGeometryBSP.generated.h"


class UWorld;


UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class UOculusAudioGeometryBSP : public UOculusAudioMaterialComponent
{
	GENERATED_BODY()

	void Serialize(FArchive & Ar) override;

	ovrAudioGeometry ovrGeometryBSP = nullptr;

	void CreateGeometryFromBSP(ovrAudioContext Context, UWorld* World, ovrAudioGeometry* Geometry);
};