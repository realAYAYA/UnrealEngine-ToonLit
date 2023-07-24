// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "OculusInputFunctionLibrary.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EOculusHandType : uint8
{
	None,
	HandLeft,
	HandRight,
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") ETrackingConfidence : uint8
{
	Low,
	High
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EOculusFinger : uint8
{
	Thumb,
	Index,
	Middle,
	Ring,
	Pinky,
	Invalid
};

// This is deprecated, but explicit deprecation causes issues with Android non-adaptive unity build.
// We can follow-up once we investigate the build issue a bit more.
/**
* EBone is enum representing the Bone Ids that come from the Oculus Runtime. 
*/
UENUM(BlueprintType)
enum class /*UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")*/ EBone : uint8
{
	Wrist_Root UMETA(DisplayName = "Wrist Root"),
	Hand_Start = Wrist_Root UMETA(DisplayName = "Hand Start"),
	Forearm_Stub UMETA(DisplayName = "Forearm Stub"),
	Thumb_0 UMETA(DisplayName = "Thumb0"),
	Thumb_1 UMETA(DisplayName = "Thumb1"),
	Thumb_2 UMETA(DisplayName = "Thumb2"),
	Thumb_3 UMETA(DisplayName = "Thumb3"),
	Index_1 UMETA(DisplayName = "Index1"),
	Index_2 UMETA(DisplayName = "Index2"),
	Index_3 UMETA(DisplayName = "Index3"),
	Middle_1 UMETA(DisplayName = "Middle1"),
	Middle_2 UMETA(DisplayName = "Middle2"),
	Middle_3 UMETA(DisplayName = "Middle3"),
	Ring_1 UMETA(DisplayName = "Ring1"),
	Ring_2 UMETA(DisplayName = "Ring2"),
	Ring_3 UMETA(DisplayName = "Ring3"),
	Pinky_0 UMETA(DisplayName = "Pinky0"),
	Pinky_1 UMETA(DisplayName = "Pinky1"),
	Pinky_2 UMETA(DisplayName = "Pinky2"),
	Pinky_3 UMETA(DisplayName = "Pinky3"),
	Thumb_Tip UMETA(DisplayName = "Thumb Tip"),
	Max_Skinnable = Thumb_Tip UMETA(DisplayName = "Max Skinnable"),
	Index_Tip UMETA(DisplayName = "Index Tip"),
	Middle_Tip UMETA(DisplayName = "Middle Tip"),
	Ring_Tip UMETA(DisplayName = "Ring Tip"),
	Pinky_Tip UMETA(DisplayName = "Pinky Tip"),
	Hand_End UMETA(DisplayName = "Hand End"),
	Bone_Max = Hand_End UMETA(DisplayName = "Hand Max"),
	Invalid UMETA(DisplayName = "Invalid")
};

/**
* FOculusCapsuleCollider is a struct that contains information on the physics/collider capsules created by the runtime for hands.
*
* @var Capsule		The UCapsuleComponent that is the collision capsule on the bone. Use this to register for overlap/collision events
* @var BoneIndex	The Bone that this collision capsule is parented to. Corresponds to the EBone enum.
*
*/
USTRUCT(BlueprintType, meta = (Deprecated = "5.1"))
struct OCULUSINPUT_API FOculusCapsuleCollider
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "OculusLibrary|HandTracking")
	TObjectPtr<UCapsuleComponent> Capsule { nullptr };

	UPROPERTY(BlueprintReadOnly, Category = "OculusLibrary|HandTracking")
	EBone BoneId = EBone::Wrist_Root;
};

UCLASS(deprecated, meta = (DeprecationMessage = "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace."))
class OCULUSINPUT_API UDEPRECATED_UOculusInputFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static EOculusFinger ConvertBoneToFinger(const EBone Bone);

	DECLARE_MULTICAST_DELEGATE_FourParams(FHandMovementFilterDelegate, EControllerHand, FVector*, FRotator*, bool*);
	static FHandMovementFilterDelegate HandMovementFilter; /// Called to modify Hand position and orientation whenever it is queried
	
	/**
	 * Creates a new runtime hand skeletal mesh.
	 *
	 * @param HandSkeletalMesh			(out) Skeletal Mesh object that will be used for the runtime hand mesh
	 * @param SkeletonType				(in) The skeleton type that will be used for generating the hand bones
	 * @param MeshType					(in) The mesh type that will be used for generating the hand mesh
	 * @param WorldTometers				(in) Optional change to the world to meters conversion value
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool GetHandSkeletalMesh(USkeletalMesh* HandSkeletalMesh, EOculusHandType SkeletonType, EOculusHandType MeshType, const float WorldToMeters = 100.0f);

	/**
	 * Initializes physics capsules for collision and physics on the runtime mesh
	 *
	 * @param SkeletonType				(in) The skeleton type that will be used to generated the capsules
	 * @param HandComponent				(in) The skinned mesh component that the capsules will be attached to
	 * @param WorldTometers				(in) Optional change to the world to meters conversion value
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintCallable, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static TArray<FOculusCapsuleCollider> InitializeHandPhysics(EOculusHandType SkeletonType, USkinnedMeshComponent* HandComponent, const float WorldToMeters = 100.0f);

	/**
	 * Get the rotation of a specific bone
	 *
	 * @param DeviceHand				(in) The hand to get the rotations from
	 * @param BoneId					(in) The specific bone to get the rotation from
	 * @param ControllerIndex			(in) Optional different controller index
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static FQuat GetBoneRotation(const EOculusHandType DeviceHand, const EBone BoneId, const int32 ControllerIndex = 0);
	
	/**
	 * Get the pointer pose
	 *
	 * @param DeviceHand				(in) The hand to get the pointer pose from
	 * @param ControllerIndex			(in) Optional different controller index
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static FTransform GetPointerPose(const EOculusHandType DeviceHand, const int32 ControllerIndex = 0);

	/**
	 * Check if the pointer pose is a valid pose
	 *
	 * @param DeviceHand				(in) The hand to get the pointer status from
	 * @param ControllerIndex			(in) Optional different controller index
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsPointerPoseValid(const EOculusHandType DeviceHand, const int32 ControllerIndex = 0);

	/**
	 * Get the tracking confidence of the hand
	 *
	 * @param DeviceHand				(in) The hand to get tracking confidence of
	 * @param ControllerIndex			(in) Optional different controller index
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static ETrackingConfidence GetTrackingConfidence(const EOculusHandType DeviceHand, const int32 ControllerIndex = 0);

	/**
	* Get the tracking confidence of a finger
	*
	* @param DeviceHand				(in) The hand to get tracking confidence of
	* @param ControllerIndex			(in) Optional different controller index
	* @param Finger			(in) The finger to get tracking confidence of
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static ETrackingConfidence GetFingerTrackingConfidence(const EOculusHandType DeviceHand, const EOculusFinger Finger, const int32 ControllerIndex = 0);

	/**
	 * Get the scale of the hand
	 *
	 * @param DeviceHand				(in) The hand to get scale of
	 * @param ControllerIndex			(in) Optional different controller index
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static float GetHandScale(const EOculusHandType DeviceHand, const int32 ControllerIndex = 0);

	/**
	 * Get the user's dominant hand
	 *
	 * @param ControllerIndex			(in) Optional different controller index
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static EOculusHandType GetDominantHand(const int32 ControllerIndex = 0);

	/**
	 * Check if hand tracking is enabled currently
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsHandTrackingEnabled();

	/**
	* Check if the hand position is valid
	*
	* @param DeviceHand				(in) The hand to get the position from
	* @param ControllerIndex			(in) Optional different controller index
	*/
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static bool IsHandPositionValid(const EOculusHandType DeviceHand, const int32 ControllerIndex = 0);

	/**
	 * Get the bone name from the bone index
	 *
	 * @param BoneIndex					(in) Bone index to get the name of
	 */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UFUNCTION(BlueprintPure, Category = "OculusLibrary|HandTracking", meta = (DeprecatedFunction, DeprecationMessage = "The OculusVR plugin is deprecated."))
	static FString GetBoneName(EBone BoneId);
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

