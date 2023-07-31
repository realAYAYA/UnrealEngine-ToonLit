// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "OVR_Audio.h"
#include "OculusAudioGeometryComponent.generated.h"

/*
 * OculusAudio geometry components are used to customize an a static mesh actor's acoustic properties.
 */
UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class UOculusAudioGeometryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOculusAudioGeometryComponent();
	~UOculusAudioGeometryComponent();
	bool UploadGeometry();
	bool IncludesChildren() const	{ return IncludeChildren; }

private:
	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	void Serialize(FArchive & Ar) override;
	void PostLoad() override;
	void BeginDestroy() override;

	ovrAudioContext GetContext(UWorld* World = nullptr);
	void AppendStaticMesh(UStaticMesh* Mesh,
						  const FTransform& Transform,
						  TArray<FVector>& MergedVertices,
						  TArray<uint32>& MergedIndices,
						  TArray<ovrAudioMeshGroup>& ovrMeshGroups,
						  ovrAudioMaterial ovrMaterial);

	void AppendChildMeshes(AActor* CurrentActor,
						   const FTransform& RootTransform,
						   ovrAudioContext Context,
						   TArray<FVector>& MergedVertices,
						   TArray<uint32>& MergedIndices,
						   TArray<ovrAudioMeshGroup>& ovrMeshGroups,
						   ovrAudioMaterial ovrMaterial);

	// Mesh hierarchy optimization for both content editing and runtime performance
	// if IncludeChildren is true, children (attached) meshes will be merged
	UPROPERTY(EditAnywhere, Category = Settings)
	bool IncludeChildren = true;

	ovrAudioGeometry ovrGeometry;
	ovrAudioContext CachedContext;
	FTransform PreviousTransform;
};