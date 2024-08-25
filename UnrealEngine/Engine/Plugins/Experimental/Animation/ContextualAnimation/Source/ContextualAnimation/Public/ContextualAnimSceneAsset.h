// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "ContextualAnimTypes.h"
#include "ContextualAnimSceneAsset.generated.h"

class UContextualAnimSceneInstance;
class UContextualAnimSceneAsset;

UENUM(BlueprintType)
enum class EContextualAnimCollisionBehavior : uint8
{
	None,
	IgnoreActorWhenMoving,
	IgnoreChannels
};

USTRUCT(BlueprintType)
struct FContextualAnimIgnoreChannelsParam
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (GetOptions = "GetRoles"))
	FName Role = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Defaults")
	TArray<TEnumAsByte<ECollisionChannel>> Channels;
};

USTRUCT()
struct FContextualAnimAttachmentParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (GetOptions = "GetRoles"))
	FName Role = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Defaults")
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Defaults")
	FTransform RelativeTransform = FTransform::Identity;
};

UCLASS(Blueprintable)
class CONTEXTUALANIMATION_API UContextualAnimRolesAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults")
	TArray<FContextualAnimRoleDefinition> Roles;

	UContextualAnimRolesAsset(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {};

	const FContextualAnimRoleDefinition* FindRoleDefinitionByName(const FName& Name) const
	{
		return Roles.FindByPredicate([Name](const FContextualAnimRoleDefinition& RoleDef) { return RoleDef.Name == Name; });
	}

	FORCEINLINE int32 GetNumRoles() const { return Roles.Num(); }
};

/**
 * Contains AnimTracks for each role in the interaction.
 * Example: An specific set for a interaction with a car would have two tracks, one with the animation for the character and another one with the animation for the car.
 * It is common to have variations of the same action with different animations. We could have one AnimSet with the animations for getting into the car from the driver side and another for getting into the car from the passenger side.
*/
USTRUCT(BlueprintType)
struct CONTEXTUALANIMATION_API FContextualAnimSet
{
	GENERATED_BODY()

	/** List of tracks with animation (and relevant data specific to that animation) for each role */
	UPROPERTY(EditAnywhere, Category = "Defaults")
	TArray<FContextualAnimTrack> Tracks;

	/** Map of WarpTargetNames and Transforms for this set. Generated off line based on warp points defined in the asset */
	UPROPERTY(EditAnywhere, Category = "Defaults")
	TMap<FName, FTransform> WarpPoints;

	/** Used by the selection mechanism to 'break the tie' when multiple Sets can be selected */
	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (ClampMin = "0", UIMin = "0", ClampMax = "1", UIMax = "1"))
	float RandomWeight = 1.f;

	int32 GetNumMandatoryRoles() const;
};

/** Named container with one or more ContextualAnimSet */
USTRUCT(BlueprintType)
struct CONTEXTUALANIMATION_API FContextualAnimSceneSection
{
	GENERATED_BODY()

public:

	const FContextualAnimSet* GetAnimSet(int32 AnimSetIdx) const; 

	const FContextualAnimTrack* GetAnimTrack(int32 AnimSetIdx, const FName& Role) const;

	const FContextualAnimTrack* GetAnimTrack(int32 AnimSetIdx, int32 AnimTrackIdx) const;

	FTransform GetIKTargetTransformForRoleAtTime(int32 AnimSetIdx, FName Role, FName TrackName, float Time) const;

	const FContextualAnimIKTargetDefContainer& GetIKTargetDefsForRole(const FName& Role) const;

	const FContextualAnimTrack* FindFirstAnimTrackForRoleThatPassesSelectionCriteria(const FName& Role, const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier) const;

	FORCEINLINE FName GetName() const { return Name; }
	FORCEINLINE const TArray<FContextualAnimWarpPointDefinition>& GetWarpPointDefinitions() const { return WarpPointDefinitions; }
	FORCEINLINE int32 GetNumAnimSets() const { return AnimSets.Num(); }

protected:

	UPROPERTY(EditAnywhere, Category = "Defaults")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Defaults")
	TArray<FContextualAnimSet> AnimSets;

	UPROPERTY(EditAnywhere, Category = "Defaults")
	TMap<FName, FContextualAnimIKTargetDefContainer> RoleToIKTargetDefsMap;

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (TitleProperty = "WarpTargetName"))
	TArray<FContextualAnimWarpPointDefinition> WarpPointDefinitions;

	void GenerateAlignmentTracks(UContextualAnimSceneAsset& SceneAsset);
	void GenerateIKTargetTracks(UContextualAnimSceneAsset& SceneAsset);

 	friend class UContextualAnimSceneAsset;
 	friend class FContextualAnimViewModel;
};

USTRUCT(BlueprintType)
struct FContextualAnimPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	FName Role = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	FTransform Transform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	float Speed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	int32 SectionIdx = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	int32 AnimSetIdx = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Default")
	int32 AnimTrackIdx = INDEX_NONE;

	FContextualAnimPoint(){}
	FContextualAnimPoint(const FName& InRole, const FTransform& InTransform, float InSpeed, int32 InSectionIdx, int32 InAnimSetIdx, int32 InAnimTrackIdx)
		: Role(InRole), Transform(InTransform), Speed(InSpeed), SectionIdx(InSectionIdx), AnimSetIdx(InAnimSetIdx), AnimTrackIdx(InAnimTrackIdx)
	{}
};

UENUM(BlueprintType)
enum class EContextualAnimPointType : uint8
{
	FirstFrame,
	SyncFrame,
	LastFrame
};

UENUM(BlueprintType)
enum class EContextualAnimCriterionToConsider : uint8
{
	All,
	Spatial,
	Other
};

UENUM(BlueprintType)
enum class EContextualAnimActorPreviewType : uint8
{
	SkeletalMesh,
	StaticMesh,
	Actor,
	None
};

USTRUCT(BlueprintType)
struct FContextualAnimActorPreviewData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (GetOptions = "GetRoles"))
	FName Role;

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (GetOptions = "GetRoles"))
	EContextualAnimActorPreviewType Type = EContextualAnimActorPreviewType::StaticMesh;

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (EditCondition = "Type==EContextualAnimActorPreviewType::SkeletalMesh", EditConditionHides))
	TSoftObjectPtr<class USkeletalMesh> PreviewSkeletalMesh;

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (EditCondition = "Type==EContextualAnimActorPreviewType::SkeletalMesh", EditConditionHides))
	TSoftClassPtr<class UAnimInstance> PreviewAnimInstance;

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (EditCondition = "Type==EContextualAnimActorPreviewType::StaticMesh", EditConditionHides))
	TSoftObjectPtr<class UStaticMesh> PreviewStaticMesh;

	UPROPERTY(EditAnywhere, Category = "Defaults", meta = (EditCondition = "Type==EContextualAnimActorPreviewType::Actor", EditConditionHides))
	TSoftClassPtr<class AActor> PreviewActorClass;
};

UCLASS(Blueprintable)
class CONTEXTUALANIMATION_API UContextualAnimSceneAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	typedef TFunctionRef<UE::ContextualAnim::EForEachResult(const FContextualAnimTrack& AnimTrack)> FForEachAnimTrackFunction;

	UContextualAnimSceneAsset(const FObjectInitializer& ObjectInitializer);

	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	void PrecomputeData();
	
	void ForEachAnimTrack(FForEachAnimTrackFunction Function) const;

	FORCEINLINE const FName& GetPrimaryRole() const { return PrimaryRole; }
	FORCEINLINE EContextualAnimCollisionBehavior GetCollisionBehavior() const { return CollisionBehavior; }
	FORCEINLINE int32 GetSampleRate() const { return SampleRate; }
	FORCEINLINE float GetRadius() const { return Radius; }
	FORCEINLINE bool ShouldPrecomputeAlignmentTracks() const { return bPrecomputeAlignmentTracks; }

	const TArray<TEnumAsByte<ECollisionChannel>>& GetCollisionChannelsToIgnoreForRole(FName Role) const;
	
	const FContextualAnimAttachmentParams* GetAttachmentParamsForRole(FName Role) const
	{
		return AttachmentParams.FindByPredicate([Role](const FContextualAnimAttachmentParams& Item) { return Item.Role == Role; });
	}

	bool HasValidData() const { return RolesAsset != nullptr && Sections.Num() > 0 && Sections[0].AnimSets.Num() > 0; }

	const UContextualAnimRolesAsset* GetRolesAsset() const { return RolesAsset; }

	UFUNCTION()
	TArray<FName> GetRoles() const;

	int32 GetNumRoles() const { return RolesAsset ? RolesAsset->GetNumRoles() : 0; }

	int32 GetNumMandatoryRoles(int32 SectionIdx, int32 AnimSetIdx) const;

	const FTransform& GetMeshToComponentForRole(const FName& Role) const;

	TArray<FName> GetSectionNames() const;

	int32 GetNumSections() const;

	int32 GetNumAnimSetsInSection(int32 SectionIdx) const;

	const FContextualAnimSceneSection* GetSection(int32 SectionIdx) const;

	const FContextualAnimSceneSection* GetSection(const FName& SectionName) const;

	const FContextualAnimSet* GetAnimSet(int32 SectionIdx, int32 AnimSetIdx) const;

	int32 GetSectionIndex(const FName& SectionName) const;
	
 	const FContextualAnimTrack* GetAnimTrack(int32 SectionIdx, int32 AnimSetIdx, const FName& Role) const;
 
 	const FContextualAnimTrack* GetAnimTrack(int32 SectionIdx, int32 AnimSetIdx, int32 AnimTrackIdx) const;

	FTransform GetIKTargetTransform(int32 SectionIdx, int32 AnimSetIdx, int32 AnimTrackIdx, const FName& TrackName, float Time) const;

	FTransform GetAlignmentTransform(int32 SectionIdx, int32 AnimSetIdx, int32 AnimTrackIdx, int32 WarpPointIdx, float Time) const;
	FTransform GetAlignmentTransform(int32 SectionIdx, int32 AnimSetIdx, int32 AnimTrackIdx, const FName& WarpPointName, float Time) const;
	FTransform GetAlignmentTransform(const FContextualAnimTrack& AnimTrack, int32 WarpPointIdx, float Time) const;
	FTransform GetAlignmentTransform(const FContextualAnimTrack& AnimTrack, const FName& WarpPointName, float Time) const;
	FTransform GetAlignmentTransformForRoleRelativeToOtherRole(int32 SectionIdx, int32 AnimSetIdx, FName Role, FName OtherRole, float Time) const;

	const FContextualAnimTrack* FindAnimTrackForRoleWithClosestEntryLocation(int32 SectionIdx, const FName& Role, const FContextualAnimSceneBindingContext& Primary, const FVector& TestLocation) const;

	const FContextualAnimTrack* FindAnimTrackByAnimation(const UAnimSequenceBase* Animation) const;

	const FContextualAnimIKTargetDefContainer& GetIKTargetDefsForRoleInSection(int32 SectionIdx, const FName& Role) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset")
	void GetAlignmentPointsForSecondaryRole(EContextualAnimPointType Type, int32 SectionIdx, const FContextualAnimSceneBindingContext& Primary, TArray<FContextualAnimPoint>& OutResult) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset")
	void GetAlignmentPointsForSecondaryRoleConsideringSelectionCriteria(EContextualAnimPointType Type, int32 SectionIdx, const FContextualAnimSceneBindingContext& Primary, const FContextualAnimSceneBindingContext& Querier, EContextualAnimCriterionToConsider CriterionToConsider, TArray<FContextualAnimPoint>& OutResult) const;

public:

	// Blueprint Interface
	//------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset", meta = (DisplayName = "Find Animation For Role"))
	UAnimSequenceBase* BP_FindAnimationForRole(int32 SectionIdx, int32 AnimSetIdx, FName Role) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset", meta = (DisplayName = "Find AnimSet Index By Animation"))
	int32 BP_FindAnimSetIndexByAnimation(int32 SectionIdx, const UAnimSequenceBase* Animation) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset", meta = (DisplayName = "Get Alignment Transform For Role Relative To WarpPoint"))
	FTransform BP_GetAlignmentTransformForRoleRelativeToWarpPoint(int32 SectionIdx, int32 AnimSetIdx, FName Role, float Time) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset", meta = (DisplayName = "Get IK Target Transform For Role At Time"))
	FTransform BP_GetIKTargetTransformForRoleAtTime(int32 SectionIdx, int32 AnimSetIdx, FName Role, FName TrackName, float Time) const;

	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset", meta = (DisplayName = "Get Start and End Time For Warp Section"))
	void BP_GetStartAndEndTimeForWarpSection(int32 SectionIdx, int32 AnimSetIdx, FName Role, FName WarpSectionName, float& OutStartTime, float& OutEndTime) const;

	//@TODO: Kept around only to do not break existing content. It will go away in the future.
	UFUNCTION(BlueprintCallable, Category = "Contextual Anim|Scene Asset")
	bool Query(FName Role, FContextualAnimQueryResult& OutResult, const FContextualAnimQueryParams& QueryParams, const FTransform& ToWorldTransform) const;
	float FindBestAnimStartTime(const FContextualAnimTrack& AnimTrack, const FVector& LocalLocation) const;

protected:

	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<UContextualAnimRolesAsset> RolesAsset;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (GetOptions = "GetRoles"))
	FName PrimaryRole = NAME_None;

#if WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (TitleProperty = "Role"))
	TArray<FContextualAnimActorPreviewData> OverridePreviewData;

#endif // WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category = "Defaults")
	TArray<FContextualAnimSceneSection> Sections;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Radius = 0.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EContextualAnimCollisionBehavior CollisionBehavior = EContextualAnimCollisionBehavior::None;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (EditCondition = "CollisionBehavior==EContextualAnimCollisionBehavior::IgnoreChannels", EditConditionHides))
	TArray<FContextualAnimIgnoreChannelsParam> CollisionChannelsToIgnoreParams;

	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FContextualAnimAttachmentParams> AttachmentParams;

	/** Whether we should extract and cache alignment tracks off line. */
	UPROPERTY(EditAnywhere, Category = "Settings", AdvancedDisplay)
	bool bPrecomputeAlignmentTracks = false;

	/** Sample rate (frames per second) used when sampling the animations to generate alignment and IK tracks */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "1", ClampMax = "60"), AdvancedDisplay)
	int32 SampleRate = 15;

 	friend class FContextualAnimViewModel;
};
