// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GoogleARCoreAugmentedImageDatabase.h"

#include "ARTrackable.h"

#include "GoogleARCoreAugmentedImage.generated.h"

/**
 * An object representing an augmented image currently in the scene.
 */
UCLASS(BlueprintType)
class GOOGLEARCOREBASE_API UGoogleARCoreAugmentedImage : public UARTrackedImage
{
	GENERATED_BODY()

public:

	/**
	 * Get the center in local space of the augmented image.
	 *
	 * @return The center in the augmented image's local coordinate
	 *         space.
	 */
	UE_DEPRECATED(4.22, "There is no need to use this function since it always returns (0,0,0).")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	FVector GetCenter() const;

	/**
	 * Get the size in local space of the augmented image.
	 *
	 * @return The size in the augmented image's local coordinate
	 *         space.
	 */
	UE_DEPRECATED(4.22, "Please use UARTrackedImage::GetEstimatedSize() instead.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	FVector GetExtent() const;

	/**
	 * Get the index of an augmented image in the augmented image
	 * database.
	 *
	 * @return An integer indicating the index into the Entries array
	 *         in the UGoogleARCoreAugmentedImageDatabase.
	 */
	UE_DEPRECATED(4.22, "Instead of getting the index, you can use UARTrackedImage::GetDetectedImage() to get the UARCandidateImage object.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	int32 GetImageIndex() const;

	/**
	 * Get the name of an augmented image in the augmented image
	 * database.
	 *
	 * @return The name of the image in the image database.
	 */
	UE_DEPRECATED(4.22, "Please use UARCandidateImage::GetFriendlyName() instead.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmentedimage"))
	FString GetImageName() const;

	/**
	 * Draw a box around the image, for debugging purposes.
	 *
	 * @param World				World context object.
	 * @param OutlineColor		Color of the lines.
	 * @param OutlineThickness	Thickness of the lines.
	 * @param PersistForSeconds	Number of seconds to keep the lines on-screen.
	 */
	virtual void DebugDraw(
		UWorld* World, const FLinearColor& OutlineColor,
		float OutlineThickness, float PersistForSeconds = 0.0f) const override;

	/**
	 * Update the tracked object.
	 *
	 * This is called by
	 * FGoogleARCoreAugmentedImageResource::UpdateGeometryData() to
	 * adjust the tracked position of the object.
	 *
	 * @param InTrackingSystem				The AR system to use.
	 * @param FrameNumber					The current frame number.
	 * @param Timestamp						The current time stamp.
	 * @param InLocalToTrackingTransform	Local to tracking space transformation.
	 * @param InAlignmentTransform			Alignment transform.
	 * @param InEstimatedSize				The estimated size of the object.
	 * @param InDetectedImage				The image that was found, in Unreal's cross-platform augmente reality API.
	 * @param InImageIndex					Which image in the augmented image database is being tracked.
	 * @param ImageName						The name of the image being tracked.
	 */
	void UpdateTrackedGeometry(
		const TSharedRef<FARSupportInterface, ESPMode::ThreadSafe>& InTrackingSystem,
		uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform,
		const FTransform& InAlignmentTransform,
		FVector2D InEstimatedSize, UARCandidateImage* InDetectedImage,
		int32 InImageIndex, const FString& ImageName);

private:
	UPROPERTY()
	int32 ImageIndex;

	UPROPERTY()
	FString ImageName;

};

