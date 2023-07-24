// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "CameraRig_Rail.h"
#include "CineSplineComponent.h"

#include "CineCameraRigRail.generated.h"

UCLASS(Blueprintable, Category = "VirtualProduction")
class CINECAMERARIGS_API ACineCameraRigRail : public ACameraRig_Rail
{
	GENERATED_BODY()

public:
	ACineCameraRigRail(const FObjectInitializer& ObjectInitializer);

	/* Returns CineSplineComponent*/
	UFUNCTION(BlueprintPure, Category = "Rail Components")
	UCineSplineComponent* GetCineSplineComponent() const { return CineSplineComponent; }

	/* Use AbsolutePosition metadata to parameterize the spline*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Controls")
	bool bUseAbsolutePosition = true;

	/* Custom parameter to drive current position*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Rail Controls", meta=(EditCondition="bUseAbsolutePosition"))
	float AbsolutePositionOnRail = 1.0f;

	/* Use PointRotation metadata for attachment orientation. If false, attachment orientation is based on the spline curvature*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "Rail Controls", meta = (EditCondition = "bLockOrientationToRail"))
	bool bUsePointRotation = true;

	/* Material assigned to spline component mesh*/
	UPROPERTY(EditAnywhere, BlueprintSetter=SetSplineMeshMaterial, Category = "SplineVisualization")
	TObjectPtr<UMaterialInterface> SplineMeshMaterial;

	/* Material Instance Dynamic created for the spline mesh */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "SplineVisualization")
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SplineMeshMIDs;

	/* Texture that can be set to SplineMeshMIDs */
	UPROPERTY(EditAnywhere, BlueprintSetter=SetSplineMeshTexture, Category = "SplineVisualization")
	TObjectPtr<UTexture2D> SplineMeshTexture;

	/* Determines if camera mount inherits LocationX*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Location")
	bool bAttachLocationX = true;

	/* Determines if camera mount inherits LocationY*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Location")
	bool bAttachLocationY = true;

	/* Determines if camera mount inherits LocationZ*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Location")
	bool bAttachLocationZ = true;

	/* Determines if camera mount inherits RotationX*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Rotation")
	bool bAttachRotationX = true;

	/* Determines if camera mount inherits RotationY*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Rotation")
	bool bAttachRotationY = true;

	/* Determines if camera mount inherits RotationZ*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Rotation")
	bool bAttachRotationZ = true;

	/* Determines if it can drive focal length on the attached actors*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Camera")
	bool bInheritFocalLength = true;

	/* Determines if it can drive aperture on the attached actors*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Camera")
	bool bInheritAperture = true;

	/* Determines if it can drive focus distance on the attached actors*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Camera")
	bool bInheritFocusDistance = true;

	/* Set spline mesh material*/
	UFUNCTION(BlueprintSetter)
	void SetSplineMeshMaterial(UMaterialInterface* InMaterial);

	/* Set texture used in the spline mesh material */
	UFUNCTION(BlueprintSetter)
	void SetSplineMeshTexture(UTexture2D* InTexture);

	/* Calculate internal velocity at the given position */
	UFUNCTION(BlueprintCallable, Category = "CineCameraRigRail")
	FVector GetVelocityAtPosition(const float InPosition, const float delta = 0.001) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	class UCineSplineComponent* CineSplineComponent;

	virtual void UpdateRailComponents() override;

	void UpdateSplineMeshMID();
	void SetMIDParameters();

	void OnSplineEdited();
};
