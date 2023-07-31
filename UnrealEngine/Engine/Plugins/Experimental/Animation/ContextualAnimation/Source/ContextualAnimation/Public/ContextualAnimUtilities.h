// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimSequence.h"
#include "ContextualAnimTypes.h"
#include "ContextualAnimUtilities.generated.h"

class UContextualAnimSceneAsset;
class USkeletalMeshComponent;
class UAnimInstance;
class AActor;
class FPrimitiveDrawInterface;
struct FAnimMontageInstance;
struct FContextualAnimSet;

UCLASS()
class CONTEXTUALANIMATION_API UContextualAnimUtilities : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** 
	 * Helper function to extract local space pose from an animation at a given time.
	 * If the supplied animation is a montage it will extract the pose from the first track
	 * IMPORTANT: This function expects you to add a MemMark (FMemMark Mark(FMemStack::Get());) at the correct scope if you are using it from outside world's tick
	 */
	static void ExtractLocalSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCompactPose& OutPose);

	/**
	 * Helper function to extract component space pose from an animation at a given time
     * If the supplied animation is a montage it will extract the pose from the first track
	 * IMPORTANT: This function expects you to add a MemMark (FMemMark Mark(FMemStack::Get());) at the correct scope if you are using it from outside world's tick
	 */
	static void ExtractComponentSpacePose(const UAnimSequenceBase* Animation, const FBoneContainer& BoneContainer, float Time, bool bExtractRootMotion, FCSPose<FCompactPose>& OutPose);

	/** Extract Root Motion transform from a contiguous position range */
	static FTransform ExtractRootMotionFromAnimation(const UAnimSequenceBase* Animation, float StartTime, float EndTime);

	/** Extract root bone transform at a given time */
	static FTransform ExtractRootTransformFromAnimation(const UAnimSequenceBase* Animation, float Time);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Utilities", meta = (WorldContext = "WorldContextObject", DisplayName = "Draw Debug Pose"))
	static void BP_DrawDebugPose(const UObject* WorldContextObject, const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float LifeTime, float Thickness);

	typedef TFunctionRef<void(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, float LifeTime, float Thickness)> FDrawLineFunction;
	static void DrawPose(const UWorld* World, const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float LifeTime, float Thickness);
	static void DrawPose(FPrimitiveDrawInterface* PDI, const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float Thickness);
	static void DrawPose(const UAnimSequenceBase* Animation, float Time, FTransform LocalToWorldTransform, FLinearColor Color, float LifeTime, float Thickness, FDrawLineFunction DrawFunction);
	
	static void DrawDebugAnimSet(const UWorld* World, const UContextualAnimSceneAsset& SceneAsset, const FContextualAnimSet& AnimSet, float Time, const FTransform& ToWorldTransform, const FColor& Color, float LifeTime, float Thickness);

	static USkeletalMeshComponent* TryGetSkeletalMeshComponent(const AActor* Actor);

	static UAnimInstance* TryGetAnimInstance(const AActor* Actor);

	static FAnimMontageInstance* TryGetActiveAnimMontageInstance(const AActor* Actor);

	static void DrawSector(FPrimitiveDrawInterface& PDI, const FVector& Origin, const FVector& Direction, float MinDistance, float MaxDistance, float MinAngle, float MaxAngle, const FLinearColor& Color, uint8 DepthPriority, float Thickness, bool bDashedLine);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset", meta = (DisplayName = "Create Contextual Anim Scene Bindings"))
	static bool BP_CreateContextualAnimSceneBindings(const UContextualAnimSceneAsset* SceneAsset, const TMap<FName, FContextualAnimSceneBindingContext>& Params, FContextualAnimSceneBindings& OutBindings);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset", meta = (DisplayName = "Create Contextual Anim Scene Bindings For Two Actors"))
	static bool BP_CreateContextualAnimSceneBindingsForTwoActors(const UContextualAnimSceneAsset* SceneAsset, const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Secondary, FContextualAnimSceneBindings& OutBindings);

	// Montage Blueprint Interface
	//------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Utilities", meta = (DisplayName = "GetSectionStartAndEndTime"))
	static void BP_Montage_GetSectionStartAndEndTime(const UAnimMontage* Montage, int32 SectionIndex, float& OutStartTime, float& OutEndTime);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Utilities", meta = (DisplayName = "GetSectionTimeLeftFromPos"))
	static float BP_Montage_GetSectionTimeLeftFromPos(const UAnimMontage* Montage, float Position);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Utilities", meta = (DisplayName = "GetSectionLength"))
	static float BP_Montage_GetSectionLength(const UAnimMontage* Montage, int32 SectionIndex);

	// SceneBindings Blueprint Interface
	//------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Calculate pivots For Bindings"))
	static void BP_SceneBindings_CalculateAnimSetPivots(const FContextualAnimSceneBindings& Bindings, TArray<FContextualAnimSetPivot>& OutPivots);

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Add Or Update Warp Targets For Bindings"))
	static void BP_SceneBindings_AddOrUpdateWarpTargetsForBindings(const FContextualAnimSceneBindings& Bindings);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Bindings"))
	static const TArray<FContextualAnimSceneBinding>& BP_SceneBindings_GetBindings(const FContextualAnimSceneBindings& Bindings) { return Bindings.GetBindings(); }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Binding By Role"))
	static const FContextualAnimSceneBinding& BP_SceneBindings_GetBindingByRole(const FContextualAnimSceneBindings& Bindings, FName Role);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Binding By Actor"))
	static const FContextualAnimSceneBinding& BP_SceneBindings_GetBindingByActor(const FContextualAnimSceneBindings& Bindings, const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Animation From Binding"))
	static const UAnimSequenceBase* BP_SceneBinding_GetAnimationFromBinding(const FContextualAnimSceneBindings& Bindings, const FContextualAnimSceneBinding& Binding);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Role From Binding"))
	static FName BP_SceneBinding_GetRoleFromBinding(const FContextualAnimSceneBindings& Bindings, const FContextualAnimSceneBinding& Binding);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Scene Asset"))
	static const UContextualAnimSceneAsset* BP_SceneBindings_GetSceneAsset(const FContextualAnimSceneBindings& Bindings) { return Bindings.GetSceneAsset(); }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Section And AnimSet Indices"))
	static void BP_SceneBindings_GetSectionAndAnimSetIndices(const FContextualAnimSceneBindings& Bindings, int32& SectionIdx, int32& AnimSetIdx);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Alignment Transform For Role Relative To Other Role"))
	static FTransform BP_SceneBindings_GetAlignmentTransformForRoleRelativeToOtherRole(const FContextualAnimSceneBindings& Bindings, FName Role, FName RelativeToRole, float Time);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Alignment Transform For Role Relative To Pivot"))
	static FTransform BP_SceneBindings_GetAlignmentTransformForRoleRelativeToPivot(const FContextualAnimSceneBindings& Bindings, FName Role, const FContextualAnimSetPivot& Pivot, float Time);

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Bindings", meta = (DisplayName = "Get Alignment Transform From Binding"))
	static FTransform BP_SceneBindings_GetAlignmentTransformFromBinding(const FContextualAnimSceneBindings& Bindings, const FContextualAnimSceneBinding& Binding, const FContextualAnimSetPivot& Pivot);

	// FContextualAnimSceneBindingContext Blueprint Interface
	//------------------------------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Binding Context", meta = (NativeMakeFunc, DisplayName = "Make Contextual Anim Scene Binding Context"))
	static FContextualAnimSceneBindingContext BP_SceneBindingContext_MakeFromActor(AActor* Actor) { return FContextualAnimSceneBindingContext(Actor); }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Binding Context", meta = (DisplayName = "Make Contextual Anim Scene Binding Context With External Transform"))
	static FContextualAnimSceneBindingContext BP_SceneBindingContext_MakeFromActorWithExternalTransform(AActor* Actor, FTransform ExternalTransform) { return FContextualAnimSceneBindingContext(Actor, ExternalTransform); }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Binding Context", meta = (DisplayName = "Get Actor"))
	static AActor* BP_SceneBindingContext_GetActor(const FContextualAnimSceneBindingContext& BindingContext) { return BindingContext.GetActor(); }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Binding Context", meta = (DisplayName = "Get Transform"))
	static FTransform BP_SceneBindingContext_GetTransform(const FContextualAnimSceneBindingContext& BindingContext) { return BindingContext.GetTransform(); }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Binding Context", meta = (DisplayName = "Get Velocity"))
	static FVector BP_SceneBindingContext_GetVelocity(const FContextualAnimSceneBindingContext& BindingContext) { return BindingContext.GetVelocity(); }

	// FContextualAnimSceneBinding Blueprint Interface
	//------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Binding", meta = (DisplayName = "Get Actor"))
	static AActor* BP_SceneBinding_GetActor(const FContextualAnimSceneBinding& Binding) { return Binding.GetActor(); }

	UFUNCTION(BlueprintPure, Category = "Contextual Anim|Scene Binding", meta = (DisplayName = "Get Skeletal Mesh"))
	static USkeletalMeshComponent* BP_SceneBinding_GetSkeletalMesh(const FContextualAnimSceneBinding& Binding) { return Binding.GetSkeletalMeshComponent(); }

};