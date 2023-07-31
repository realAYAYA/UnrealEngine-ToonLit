// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VPCameraBlueprintLibrary.generated.h"


class ACameraRig_Rail;

UENUM(BlueprintType)
enum class EVPCameraRigSpawnLinearApproximationMode : uint8
{
	None			UMETA(Display = "No Approximation"),	// We won't do linear approximation, instead using the Spline as constructed initially.
	Density,												// LinearApproximationParam will be used as a density value 
	IntegrationStep,										// LinearApproximationParam will be used as the Integration step in Unreal Units.
};


/** Parameters used to custom the CameraRig that's created. */
USTRUCT(BlueprintType)
struct FVPCameraRigSpawnParams
{
	GENERATED_BODY();

	FVPCameraRigSpawnParams();

public:
	/** Use world space (as opposed to local space) for points. */
	UPROPERTY(Transient, BlueprintReadWrite, Category="Camera Rig")
	bool bUseWorldSpace;

	/**
	 * Use the first vector of input as the spawn transform.
	 * Causes RigTransform to be completely ignored.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Camera Rig")
	bool bUseFirstPointAsSpawnLocation;

	/**
	 * Causes a linear approximation of the spline points to be generated instead
	 * of relying purely on the passed in points / curves.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Camera Rig")
	EVPCameraRigSpawnLinearApproximationMode LinearApproximationMode;

	/**
	 * This is only used if LinearApproximationMode is not None.
	 * When mode is Density:
	 * See FSplinePositionLinearApproximation::Build.
	 *
	 * When mode is IntegrationStep:
	 * Integration step (in CM) between approximation points. Decreasing this value will
	 * increase the number of spline points and will therefore increase the accuracy
	 * (at the cost of increased complexity).
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Camera Rig")
	float LinearApproximationParam;
};


UCLASS()
class VPUTILITIES_API UVPCameraBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static ACameraRig_Rail* SpawnDollyTrackFromPoints(UObject* WorldContextObject, UPARAM(ref) const TArray<FTransform>& Points, ESplinePointType::Type InterpType = ESplinePointType::Linear);

	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static ACameraRig_Rail* SpawnDollyTrackFromPointsSmooth(UObject* WorldContextObject, UPARAM(ref) const TArray<FTransform>& Points, ESplinePointType::Type InterpType = ESplinePointType::Linear);

	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static ACameraRig_Rail* SpawnCameraRigFromActors(UObject* WorldContextObject, const FTransform& RigTransform, const TArray<AActor*>& Actors, const FVPCameraRigSpawnParams& Params);

	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static ACameraRig_Rail* SpawnCameraRigFromPoints(UObject* WorldContextObject, const FTransform& RigTransform, const TArray<FVector>& Points, const FVPCameraRigSpawnParams& Params);

	UFUNCTION(BlueprintCallable, Category = "Virtual Production")
	static ACameraRig_Rail* SpawnCameraRigFromSelectedActors(UObject* WorldContextObject, const FTransform& RigTransform, const FVPCameraRigSpawnParams& Params);
};
