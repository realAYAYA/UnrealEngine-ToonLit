// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Builders/GLTFBufferBuilder.h"
#include "CoreMinimal.h"

struct FAnalyticsEventAttribute;

class AActor;
class UStaticMesh;
class USkeletalMesh;
class ULevelSequence;
class UAnimSequence;
class UMaterialInterface;
class UTexture;
class UCameraComponent;
class ULightComponent;
class ULandscapeComponent;

class GLTFEXPORTER_API FGLTFAnalyticsBuilder : public FGLTFBufferBuilder
{
public:

	FGLTFAnalyticsBuilder(const FString& FileName, const UGLTFExportOptions* ExportOptions);

	TArray<FAnalyticsEventAttribute> GenerateAnalytics() const;

protected:

	void RecordActor(const AActor* Object);
	void RecordStaticMesh(const UStaticMesh* Object);
	void RecordSkeletalMesh(const USkeletalMesh* Object);
	void RecordSplineStaticMesh(const UStaticMesh* Object);
	void RecordLandscapeComponent(const ULandscapeComponent* Object);
	void RecordLevelSequence(const ULevelSequence* Object);
	void RecordAnimSequence(const UAnimSequence* Object);
	void RecordMaterial(const UMaterialInterface* Object);
	void RecordTexture(const UTexture* Object);
	void RecordCamera(const UCameraComponent* Object);
	void RecordLight(const ULightComponent* Object);

private:

	TSet<const AActor*>					ActorsRecorded;
	TSet<const USceneComponent*>		ComponentsRecorded;
	TSet<const UStaticMesh*>			StaticMeshesRecorded;
	TSet<const USkeletalMesh*>			SkeletalMeshesRecorded;
	TSet<const UStaticMesh*>			SplineStaticMeshesRecorded;
	TSet<const ULandscapeComponent*>	LandscapeComponentsRecorded;
	TSet<const ULevelSequence*>			LevelSequencesRecorded;
	TSet<const UAnimSequence*>			AnimSequencesRecorded;
	TSet<const UMaterialInterface*>		MaterialsRecorded;
	TSet<const UTexture*>				TexturesRecorded;
	
	TSet<const UCameraComponent*>		CamerasRecorded;
	TSet<const ULightComponent*>		LightsRecorded;
};
