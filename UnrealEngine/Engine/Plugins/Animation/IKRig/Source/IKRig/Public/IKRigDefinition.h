// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "IKRigLogger.h"
#include "UObject/Object.h"
#include "Interfaces/Interface_PreviewMeshProvider.h"
#include "IKRigSkeleton.h"
#if WITH_EDITOR
#include "SAdvancedTransformInputBox.h"
#endif
#include "IKRigDefinition.generated.h"

class IPropertyHandle;
class UIKRigSolver;

#if WITH_EDITORONLY_DATA
UENUM()
enum class EIKRigGoalPreviewMode : uint8
{
	Additive		UMETA(DisplayName = "Additive"),
	Absolute		UMETA(DisplayName = "Absolute"),
};

namespace EIKRigTransformType
{
	enum Type : int8
	{
		Current,
		Reference,
	};
}
#endif

UCLASS()
class IKRIG_API UIKRigEffectorGoal : public UObject
{
	GENERATED_BODY()

public:

	/** The name used to refer to this goal from outside systems.
	 * This is the name to use when referring to this Goal from Blueprint, Anim Graph, Control Rig or IK Retargeter.*/
	UPROPERTY(VisibleAnywhere, Category = "Goal Settings")
	FName GoalName;

	/** The name of the bone that this Goal is located at.*/
	UPROPERTY(VisibleAnywhere, Category = "Goal Settings")
	FName BoneName;

	/** Range 0-1, default is 1. Blend between the input bone position (0.0) and the current goal position (1.0).*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PositionAlpha = 1.0f;
	
	/** Range 0-1, default is 1. Blend between the input bone rotation (0.0) and the current goal rotation (1.0).*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RotationAlpha = 1.0f;

	/** The current transform of this Goal, in the Global Space of the character.*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings")
	FTransform CurrentTransform;

	/** The initial transform of this Goal, as defined by the initial transform of the Goal's bone in the retarget pose.*/
	UPROPERTY(VisibleAnywhere, Category = "Goal Settings")
	FTransform InitialTransform;

	bool operator==(const UIKRigEffectorGoal& Other) const { return GoalName == Other.GoalName; }

#if WITH_EDITORONLY_DATA

	/** Effects how this Goal transform is previewed in the IK Rig editor.
	* "Additive" interprets the Goal transform as being relative to the input pose. Useful for previewing animations.
	* "Absolute" pins the Goal transform to the Gizmo in the viewport.
	*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings")
	EIKRigGoalPreviewMode PreviewMode = EIKRigGoalPreviewMode::Additive;
	
	/**The size of the Goal gizmo drawing in the editor viewport.*/
	UPROPERTY(EditAnywhere, Category = "Viewport Goal Settings", meta = (ClampMin = "0.1", ClampMax = "1000.0", UIMin = "0.1", UIMax = "100.0"))
	float SizeMultiplier = 1.0f;

	/**The thickness of the Goal gizmo drawing in the editor viewport.*/
	UPROPERTY(EditAnywhere, Category = "Viewport Goal Settings",  meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "5.0"))
	float ThicknessMultiplier = 0.7f;

	/** Should position data be exposed in Blueprint */
	UPROPERTY(EditAnywhere, Category = "Exposure")
	bool bExposePosition = false;

	/** Should rotation data be exposed in Blueprint */
	UPROPERTY(EditAnywhere, Category = "Exposure")
	bool bExposeRotation = false;
	
	virtual void PostLoad() override
	{
		Super::PostLoad();
		SetFlags(RF_Transactional);
	}
	
#endif

#if WITH_EDITOR

	TOptional<FTransform::FReal> GetNumericValue(
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		EIKRigTransformType::Type TransformType
	) const;

	TTuple<FTransform, FTransform> PrepareNumericValueChanged(
		ESlateTransformComponent::Type Component,
		ESlateRotationRepresentation::Type Representation,
		ESlateTransformSubComponent::Type SubComponent,
		FTransform::FReal Value,
		EIKRigTransformType::Type TransformType
	) const;

	void SetTransform( const FTransform& InTransform, EIKRigTransformType::Type InTransformType);

	void OnCopyToClipboard(ESlateTransformComponent::Type Component, EIKRigTransformType::Type TransformType) const;
	void OnPasteFromClipboard(ESlateTransformComponent::Type Component, EIKRigTransformType::Type TransformType);

	bool TransformDiffersFromDefault(ESlateTransformComponent::Type Component, TSharedPtr<IPropertyHandle> PropertyHandle) const;
	void ResetTransformToDefault(ESlateTransformComponent::Type Component, TSharedPtr<IPropertyHandle> PropertyHandle);
	
#endif
};

USTRUCT()
struct IKRIG_API FBoneChain
{
	GENERATED_BODY()

	FBoneChain() = default;
	
	FBoneChain(FName InName, const FName& InStartBone, const FName& InEndBone)
		: ChainName(InName)
		, StartBone(InStartBone)
		, EndBone(InEndBone)
		, IKGoalName(NAME_None)
	{}

	UPROPERTY(EditAnywhere, Category = BoneChain)
	FName ChainName;

	UPROPERTY(VisibleAnywhere, Category = BoneChain)
	FBoneReference StartBone;

	UPROPERTY(VisibleAnywhere, Category = BoneChain)
	FBoneReference EndBone;
	
	UPROPERTY(VisibleAnywhere, Category = BoneChain)
	FName IKGoalName;
};

USTRUCT()
struct IKRIG_API FRetargetDefinition
{
	GENERATED_BODY()
	
private:

	UPROPERTY()
	FName RootBone;

	UPROPERTY()
	TArray<FBoneChain> BoneChains;

	/** read/write access restricted to friends only */
	FBoneChain* GetEditableBoneChainByName(FName ChainName);
	
	friend class UIKRigController;
	friend class UIKRigDefinition;
};

UCLASS(Blueprintable)
class IKRIG_API UIKRigDefinition : public UObject, public IInterface_PreviewMeshProvider
{
	GENERATED_BODY()

public:

	/** The skeletal mesh to run the IK solve on (loaded into viewport).
	* NOTE: you can assign ANY Skeletal Mesh to apply the IK Rig to. Compatibility is determined when a new mesh is assigned
	* by comparing it's hierarchy with the goals, solvers and bone settings required by the rig. See output log for details. */
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = PreviewMesh)
	TSoftObjectPtr<USkeletalMesh> PreviewSkeletalMesh;

#if WITH_EDITORONLY_DATA
	
	/**The size of the Bones in the editor viewport. This is set by callbacks from the viewport Character>Bones menu*/
	UPROPERTY()
	float BoneSize = 2.0f;

	/**Draw bones in the viewport.*/
	UPROPERTY(EditAnywhere, Category = "Viewport Goal Settings")
	bool DrawGoals = true;
	
	/**The size of the Goals in the editor viewport.*/
	UPROPERTY(EditAnywhere, Category = "Viewport Goal Settings", meta = (ClampMin = "0.01", UIMin = "0.1", UIMax = "100.0"))
	float GoalSize = 7.0f;

	/** The thickness of the Goals in the editor viewport.*/
	UPROPERTY(EditAnywhere, Category = "Viewport Goal Settings",  meta = (ClampMin = "0.01", UIMin = "0.0", UIMax = "10.0"))
	float GoalThickness = 0.7f;

	/** The controller responsible for managing this asset's data (all editor mutation goes through this) */
	UPROPERTY(Transient)
	TObjectPtr<UObject> Controller;

#endif

	/** UObject */
	virtual void Serialize(FArchive& Ar) override;
	/** END UObject */
	
	/** hierarchy and bone-pose transforms */
	UPROPERTY()
	FIKRigSkeleton Skeleton;
	
	/** read-only access to array of Goals, all modifications must go through UIKRigController */
	const TArray<UIKRigEffectorGoal*>& GetGoalArray() const { return Goals; };

	/** read-only access to array of Solvers, all modifications must go through UIKRigController */
	const TArray<UIKRigSolver*>& GetSolverArray() const { return Solvers; };

	/** runtime, read-only access to Retarget Definition, all modifications must go through UIKRigController */
	const TArray<FBoneChain>& GetRetargetChains() const { return RetargetDefinition.BoneChains; };

	/** runtime, read-only access to a FBoneChain by name. Returns nullptr if not found. */
	const FBoneChain* GetRetargetChainByName(FName ChainName) const;

	/** runtime, read-only access to Retarget Root */
	const FName& GetRetargetRoot() const { return RetargetDefinition.RootBone; };
	
	/** IInterface_PreviewMeshProvider interface */
	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	virtual USkeletalMesh* GetPreviewMesh() const override;
	/** END IInterface_PreviewMeshProvider interface */

#if WITH_EDITOR
	/* Get name of Preview Mesh property */
	static const FName GetPreviewMeshPropertyName();
#endif
	
private:

	/** goals, used as effectors by solvers that support them */
	UPROPERTY()
	TArray<TObjectPtr<UIKRigEffectorGoal>> Goals;
	
	/** polymorphic stack of solvers, executed in serial fashion where output of prior solve is input to the next */
	UPROPERTY(instanced)
	TArray<TObjectPtr<UIKRigSolver>> Solvers;

	/** bone chains for IK Retargeter */
	UPROPERTY()
	FRetargetDefinition RetargetDefinition;

	friend class UIKRigController;
};
