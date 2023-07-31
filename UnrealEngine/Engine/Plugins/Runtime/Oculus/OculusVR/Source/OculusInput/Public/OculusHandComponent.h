// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusInputFunctionLibrary.h"
#include "Components/PoseableMeshComponent.h"
#include "OculusHandComponent.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") EConfidenceBehavior : uint8
{
	None,
	HideActor
};

UENUM(BlueprintType)
enum class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") ESystemGestureBehavior : uint8
{
	None,
	SwapMaterial
};

OCULUSINPUT_API extern const FQuat HandRootFixupRotation;

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = OculusHand, deprecated, meta = (DeprecationMessage = "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace."))
class OCULUSINPUT_API UDEPRECATED_UOculusHandComponent : public UPoseableMeshComponent
{
	GENERATED_UCLASS_BODY()

public:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The hand skeleton that will be loaded */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	EOculusHandType SkeletonType;

	/** The hand mesh that will be applied to the skeleton */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	EOculusHandType MeshType;

	/** Behavior for when hand tracking loses high confidence tracking */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	EConfidenceBehavior ConfidenceBehavior = EConfidenceBehavior::HideActor;

	/** Behavior for when the system gesture is actived */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	ESystemGestureBehavior SystemGestureBehavior = ESystemGestureBehavior::SwapMaterial;

	/** Material that gets applied to the hands when the system gesture is active */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	TObjectPtr<class UMaterialInterface> SystemGestureMaterial;

	/** Whether or not to initialize physics capsules on the skeletal mesh */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	bool bInitializePhysics;

	/** Whether or not the hand scale should update based on values from the runtime to match the users hand scale */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	bool bUpdateHandScale;

	/** Material override for the runtime skeletal mesh */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "HandProperties", meta = (DeprecatedProperty))
	TObjectPtr<class UMaterialInterface> MaterialOverride;

	/** Bone mapping for custom hand skeletal meshes */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CustomSkeletalMesh", meta = (DeprecatedProperty))
	TMap<EBone, FName> BoneNameMappings;

	/** List of capsule colliders created for the skeletal mesh */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadOnly, Category = "HandProperties", meta = (DeprecatedProperty))
	TArray<FOculusCapsuleCollider> CollisionCapsules;

	/** Whether or not the runtime skeletal mesh has been loaded and initialized */
	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(BlueprintReadOnly, Category = "HandProperties", meta = (DeprecatedProperty))
	bool bSkeletalMeshInitialized = false;

protected:
	virtual void SystemGesturePressed();
	virtual void SystemGestureReleased();

private:
	/** Whether or not this component has authority within the frame */
	bool bHasAuthority;

	/** Whether or not a custom hand mesh is being used */
	bool bCustomHandMesh = false;

	/** Whether or not the physics capsules have been initialized */
	bool bInitializedPhysics = false;

	USkeletalMesh* RuntimeSkeletalMesh;

	UMaterialInterface* CachedBaseMaterial;

	void InitializeSkeletalMesh();

	void UpdateBonePose();
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
