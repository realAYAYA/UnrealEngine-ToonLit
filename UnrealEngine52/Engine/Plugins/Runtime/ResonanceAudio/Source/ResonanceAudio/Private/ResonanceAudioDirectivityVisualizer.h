//
// Copyright (C) Google Inc. 2017. All rights reserved.
//

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "ProceduralMeshComponent.h"
#include "ResonanceAudioSpatializationSourceSettings.h"
#include "ResonanceAudioDirectivityVisualizer.generated.h"

class UResonanceAudioSpatializationSourceSettings;

UCLASS()
class RESONANCEAUDIO_API AResonanceAudioDirectivityVisualizer : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties.
	AResonanceAudioDirectivityVisualizer();

	// Draws given directivity pattern as a 2D mesh based on Settings.
	void DrawPattern();

	void SetSettings(UResonanceAudioSpatializationSourceSettings* InSettings) { Settings = InSettings; };
	UResonanceAudioSpatializationSourceSettings* GetSettings() const { return Settings; };

private:
	// Controls the smoothness of mesh visualization.
	const int CIRCLE_SECTIONS = 128;

	// Arrays required by Procedural Mesh Component.
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FColor> VertexColors;

	// Optional arrays.
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;

#if SUPPORTS_PROCEDURAL_MESH
	UProceduralMeshComponent* Mesh;
#endif

	UPROPERTY()
	TObjectPtr<UMaterial> Material;

	UPROPERTY()
	TObjectPtr<UResonanceAudioSpatializationSourceSettings> Settings;
};
