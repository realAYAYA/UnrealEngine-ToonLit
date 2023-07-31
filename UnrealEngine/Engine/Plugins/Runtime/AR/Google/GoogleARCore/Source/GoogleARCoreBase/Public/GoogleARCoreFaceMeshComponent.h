// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARSystem.h"
#include "ProceduralMeshComponent.h"
#include "ARTrackable.h"
#include "GoogleARCoreFaceMeshComponent.generated.h"


UENUM(BlueprintType, Category="GoogleARCore|AugmentedFace", meta=(Experimental))
enum class EARCoreFaceComponentTransformMixing : uint8
{
	/** Uses the component's transform exclusively. Only setting for non-tracked meshes */
	ComponentOnly,
	/** Use the component's location and apply the rotation from the tracked mesh */
	ComponentLocationTrackedRotation,
	/** Use only the tracked face transform */
	TrackingOnly
};

/**
 * This component is updated by the ARSystem with face data on devices that have support for it.
 * Note that this class is now deprecated, it's replaced by ARFaceComponent which works on all the platforms support face tracking.
 */
UCLASS(hidecategories = (Object, LOD, "GoogleARCore|AugmentedFace"), ClassGroup = "AR", Deprecated)
class GOOGLEARCOREBASE_API UDEPRECATED_GoogleARCoreFaceMeshComponent : public UProceduralMeshComponent
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 *	Create the initial face mesh from raw mesh data
	 *
	 *	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	 *	@param	Triangles			Index buffer indicating which vertices make up each triangle. Length must be a multiple of 3.
	 *	@param	UV0					Optional array of texture co-ordinates for each vertex. If supplied, must be same length as Vertices array.
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|ARFaceMesh", meta = (DisplayName = "Create Face Mesh", AutoCreateRefTerm = "UV0"))
	void CreateMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector2D>& UV0);

	/**
	 *	Updates the face mesh vertices. The topology and UVs do not change post creation so only vertices are updated
	 *
	 *	@param	Vertices			Vertex buffer of all vertex positions to use for this mesh section.
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|ARFaceMesh", meta = (DisplayName = "Update Mesh Section FColor"))
	void UpdateMesh(const TArray<FVector>& Vertices);

	/**
	 * If auto bind is true, then this component will update itself from the local face tracking data each tick.
	 * If auto bind is off, use BindARFaceGeometry function to bind to a particular UARFaceGeometry.
	 *
	 * @param	bAutoBind			true to enable, false to disable
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|ARFaceMesh", meta = (DisplayName = "Modify auto bind to local face tracking"))
	void SetAutoBind(bool bAutoBind);

	/**
	 * Bind this FaceMeshComponent to the given UARFaceGeometry object.
	 *
	 * @param	FaceGeometry		The target UARFaceGeometry pointer. Passing nullptr to unbind the previous UARFaceGeometry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|ARFaceMesh", meta = (DisplayName = "Modify auto bind to local face tracking"))
	void BindARFaceGeometry(UARFaceGeometry* FaceGeometry);

	/** Get the transform that the AR camera has detected */
	UFUNCTION(BlueprintPure, Category = "Components|ARFaceMesh")
	FTransform GetTransform() const;

	/**	Indicates whether collision should be created for this face mesh. This adds significant cost, so only use if you need to trace against the face mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|ARFaceMesh")
	bool bWantsCollision;

	/**	If true, the mesh data will come from the local ARKit face mesh data. The face mesh will update every tick and will handle loss of face tracking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|ARFaceMesh")
	bool bAutoBindToLocalFaceMesh;

	/**	Determines how the transform from tracking data and the component's transform are mixed together */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|ARFaceMesh")
	EARCoreFaceComponentTransformMixing TransformSetting;

	/** Used when rendering the face mesh (mostly debug reasons) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components|ARFaceMesh")
	TObjectPtr<class UMaterialInterface> FaceMaterial;

private:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void InitializeComponent() override;
	virtual FMatrix GetRenderMatrix() const override;
	virtual class UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material) override;
	UARFaceGeometry* FindFaceGeometry();

	UARFaceGeometry* BoundFaceGeometry = nullptr;
	FTransform LocalToWorldTransform;
};
