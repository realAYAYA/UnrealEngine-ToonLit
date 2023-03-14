// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IKRetargetProfile.h"
#include "IKRigDefinition.h"
#include "IKRetargetSettings.h"

#include "IKRetargeter.generated.h"

#if WITH_EDITOR
class FIKRetargetEditorController;
#endif
struct FIKRetargetPose;
class URetargetChainSettings;
class URetargetRootSettings;

struct UE_DEPRECATED(5.1, "Use URetargetChainSettings instead.") FRetargetChainMap;
USTRUCT()
struct IKRIG_API FRetargetChainMap
{
	GENERATED_BODY()

	FRetargetChainMap() = default;
	FRetargetChainMap(const FName& TargetChain) : TargetChain(TargetChain){}
	
	UPROPERTY(EditAnywhere, Category = Offsets)
	FName SourceChain = NAME_None;
	
	UPROPERTY(EditAnywhere, Category = Offsets)
	FName TargetChain = NAME_None;
};

UCLASS()
class IKRIG_API URetargetChainSettings : public UObject
{
	GENERATED_BODY()

public:
	
	URetargetChainSettings() = default;
	
	URetargetChainSettings(const FName& TargetChain) : TargetChain(TargetChain){}
	
	/** UObject */
	virtual void Serialize(FArchive& Ar) override;
	/** END UObject */
	
	/** The chain on the Source IK Rig asset to copy animation FROM. */
	UPROPERTY(VisibleAnywhere, Category = "Chain Mapping")
	FName SourceChain;

	/** The chain on the Target IK Rig asset to copy animation TO. */
	UPROPERTY(VisibleAnywhere, Category = "Chain Mapping")
	FName TargetChain;

	/** The settings used to control the motion on this target chain. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FTargetChainSettings Settings;

	/** Deprecated properties from before FTargetChainSettings / profile refactor  (July 2022)*/
	UPROPERTY()
	bool CopyPoseUsingFK_DEPRECATED = true;
	UPROPERTY()
	ERetargetRotationMode RotationMode_DEPRECATED;
	UPROPERTY()
	float RotationAlpha_DEPRECATED = 1.0f;
	UPROPERTY()
	ERetargetTranslationMode TranslationMode_DEPRECATED;
	UPROPERTY()
	float TranslationAlpha_DEPRECATED = 1.0f;
	UPROPERTY()
	bool DriveIKGoal_DEPRECATED = true;
	UPROPERTY()
	float BlendToSource_DEPRECATED = 0.0f;
	UPROPERTY()
	FVector BlendToSourceWeights_DEPRECATED = FVector::OneVector;
	UPROPERTY()
	FVector StaticOffset_DEPRECATED;
	UPROPERTY()
	FVector StaticLocalOffset_DEPRECATED;
	UPROPERTY()
	FRotator StaticRotationOffset_DEPRECATED;
	UPROPERTY()
	float Extension_DEPRECATED = 1.0f;
	UPROPERTY()
	bool UseSpeedCurveToPlantIK_DEPRECATED = false;
	UPROPERTY()
	FName SpeedCurveName_DEPRECATED;
	UPROPERTY()
	float VelocityThreshold_DEPRECATED = 15.0f;
	UPROPERTY()
	float UnplantStiffness_DEPRECATED = 250.0f;
	UPROPERTY()
	float UnplantCriticalDamping_DEPRECATED = 1.0f;
	/** END deprecated properties */

	// pointer to editor for details customization
	#if WITH_EDITOR
	TSharedPtr<FIKRetargetEditorController> EditorController;
	#endif
};

UCLASS()
class IKRIG_API URetargetRootSettings: public UObject
{
	GENERATED_BODY()
	
public:
	
	/** UObject */
	virtual void Serialize(FArchive& Ar) override;
	/** END UObject */

	/** The settings used to control the motion of the target root bone. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FTargetRootSettings Settings;

	// pointer to editor for details customization
	#if WITH_EDITOR
	TSharedPtr<FIKRetargetEditorController> EditorController;
	#endif

private:
	/** Deprecated properties from before FTargetRootSettings / profile refactor */
	UPROPERTY()
	bool RetargetRootTranslation_DEPRECATED = true;
	UPROPERTY()
	float GlobalScaleHorizontal_DEPRECATED = 1.0f;
	UPROPERTY()
	float GlobalScaleVertical_DEPRECATED = 1.0f;
	UPROPERTY()
	FVector BlendToSource_DEPRECATED = FVector::ZeroVector;
	UPROPERTY()
	FVector StaticOffset_DEPRECATED = FVector::ZeroVector;
	UPROPERTY()
	FRotator StaticRotationOffset_DEPRECATED = FRotator::ZeroRotator;
	/** END deprecated properties */
};


UCLASS()
class IKRIG_API UIKRetargetGlobalSettings: public UObject
{
	GENERATED_BODY()
	
public:

	/** Global retargeter settings. */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FRetargetGlobalSettings Settings;

	// pointer to editor for details customization
	#if WITH_EDITOR
	TSharedPtr<FIKRetargetEditorController> EditorController;
	#endif
};


USTRUCT()
struct IKRIG_API FIKRetargetPose
{
	GENERATED_BODY()
	
public:
	
	FIKRetargetPose() = default;

	FQuat GetDeltaRotationForBone(const FName BoneName) const;
	void SetDeltaRotationForBone(FName BoneName, const FQuat& RotationDelta);
	const TMap<FName, FQuat>& GetAllDeltaRotations() const { return BoneRotationOffsets; };

	FVector GetRootTranslationDelta() const;
	void SetRootTranslationDelta(const FVector& TranslationDelta);
	void AddToRootTranslationDelta(const FVector& TranslationDelta);
	
	void SortHierarchically(const FIKRigSkeleton& Skeleton);

private:
	// a translational delta in GLOBAL space, applied only to the retarget root bone
	UPROPERTY(EditAnywhere, Category = RetargetPose)
	FVector RootTranslationOffset = FVector::ZeroVector;

	// these are LOCAL-space rotation deltas to be applied to a bone to modify it's retarget pose
	UPROPERTY(EditAnywhere, Category = RetargetPose)
	TMap<FName, FQuat> BoneRotationOffsets;

	friend class UIKRetargeterController;
};

// which skeleton are we referring to?
enum class ERetargetSourceOrTarget : uint8
{
	Source,	// the SOURCE skeleton (to copy FROM)
	Target, // the TARGET skeleton (to copy TO)
};

UCLASS(Blueprintable)
class IKRIG_API UIKRetargeter : public UObject
{
	GENERATED_BODY()
	
public:
	
	UIKRetargeter(const FObjectInitializer& ObjectInitializer);

	/** Get read-only access to the source IK Rig asset */
	const UIKRigDefinition* GetSourceIKRig() const { return SourceIKRigAsset.LoadSynchronous(); };
	/** Get read-only access to the target IK Rig asset */
	const UIKRigDefinition* GetTargetIKRig() const { return TargetIKRigAsset.LoadSynchronous(); };
	/** Get read-write access to the source IK Rig asset.
	 * WARNING: do not use for editing the data model. Use Controller class instead. */
	UIKRigDefinition* GetSourceIKRigWriteable() const { return SourceIKRigAsset.LoadSynchronous(); };
	/** Get read-write access to the target IK Rig asset.
	 * WARNING: do not use for editing the data model. Use Controller class instead. */
	UIKRigDefinition* GetTargetIKRigWriteable() const { return TargetIKRigAsset.LoadSynchronous(); };

	/** Get read-only access to the chain mapping */
	const TArray<TObjectPtr<URetargetChainSettings>>& GetAllChainSettings() const { return ChainSettings; };
	/** Get read-only access to the chain map for a given chain (null if chain not in retargeter) */
	const TObjectPtr<URetargetChainSettings> GetChainMapByName(const FName& TargetChainName) const;
	/** Get read-only access to the chain settings for a given chain (null if chain not in retargeter) */
	const FTargetChainSettings* GetChainSettingsByName(const FName& TargetChainName) const;
	/** Get access to the root settings */
	URetargetRootSettings* GetRootSettingsUObject() const { return RootSettings; };
	/** Get access to the global settings uobject*/
	UIKRetargetGlobalSettings* GetGlobalSettingsUObject() const { return GlobalSettings; };
	/** Get access to the global settings itself */
	const FRetargetGlobalSettings& GetGlobalSettings() const { return GlobalSettings->Settings; };

	/** Get read-only access to a retarget pose */
	const FIKRetargetPose* GetCurrentRetargetPose(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get name of the current retarget pose */
	FName GetCurrentRetargetPoseName(const ERetargetSourceOrTarget& SourceOrTarget) const;
	/** Get read-only access to a retarget pose */
	const FIKRetargetPose* GetRetargetPoseByName(const ERetargetSourceOrTarget& SourceOrTarget, const FName PoseName) const;
	/* Get name of default pose */
	static const FName GetDefaultPoseName();
	
	/* Get the current retarget profile (may be null) */
	const FRetargetProfile* GetCurrentProfile() const;
	/* Get the retarget profile by name (may be null) */
	const FRetargetProfile* GetProfileByName(const FName& ProfileName) const;

	/** BLUEPRINT GETTERS */

	/** Returns the chain settings associated with a given Goal in an IK Retargeter Asset using the given profile name (optional) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=RetargetAsset)
	static FTargetChainSettings GetChainUsingGoalFromRetargetAsset(
		const UIKRetargeter* RetargetAsset,
		const FName IKGoalName);
	
	/** Returns the chain settings associated with a given target chain in an IK Retargeter Asset using the given profile name (optional) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=RetargetAsset)
	static FTargetChainSettings GetChainSettingsFromRetargetAsset(
		const UIKRetargeter* RetargetAsset,
		const FName TargetChainName,
		const FName OptionalProfileName);

	/** Returns the chain settings associated with a given target chain in the supplied Retarget Profile. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=RetargetProfile)
	static FTargetChainSettings GetChainSettingsFromRetargetProfile(
		UPARAM(ref) FRetargetProfile& RetargetProfile,
		const FName TargetChainName);

	/** Returns the root settings in an IK Retargeter Asset using the given profile name (optional) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=RetargetAsset)
	static void GetRootSettingsFromRetargetAsset(
		const UIKRetargeter* RetargetAsset,
		const FName OptionalProfileName,
		FTargetRootSettings& OutSettings);

	/** Returns the root settings in the supplied Retarget Profile. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=RetargetProfile)
	static FTargetRootSettings GetRootSettingsFromRetargetProfile(UPARAM(ref) FRetargetProfile& RetargetProfile);

	/** Returns the global settings in an IK Retargeter Asset using the given profile name (optional) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=RetargetAsset)
	static void GetGlobalSettingsFromRetargetAsset(
		const UIKRetargeter* RetargetAsset,
		const FName OptionalProfileName,
		FRetargetGlobalSettings& OutSettings);

	/** Returns the global settings in the supplied Retarget Profile. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=RetargetProfile)
	static FRetargetGlobalSettings GetGlobalSettingsFromRetargetProfile(UPARAM(ref) FRetargetProfile& RetargetProfile);

	/** BLUEPRINT SETTERS */

	/** Set the global settings in a retarget profile (will set bApplyGlobalSettings to true). */
	UFUNCTION(BlueprintCallable, Category=RetargetProfile)
	static void SetGlobalSettingsInRetargetProfile(
		UPARAM(ref) FRetargetProfile& RetargetProfile,
		const FRetargetGlobalSettings& GlobalSettings);

	/** Set the root settings in a retarget profile (will set bApplyRootSettings to true). */
	UFUNCTION(BlueprintCallable, Category=RetargetProfile)
	static void SetRootSettingsInRetargetProfile(
		UPARAM(ref) FRetargetProfile& RetargetProfile,
		const FTargetRootSettings& RootSettings);
	
	/** Set the chain settings in a retarget profile (will set bApplyChainSettings to true). */
	UFUNCTION(BlueprintCallable, Category=RetargetProfile)
	static void SetChainSettingsInRetargetProfile(
		UPARAM(ref) FRetargetProfile& RetargetProfile,
		const FTargetChainSettings& ChainSettings,
		const FName TargetChainName);

	/** Set the chain FK settings in a retarget profile (will set bApplyChainSettings to true). */
	UFUNCTION(BlueprintCallable, Category=RetargetProfile)
	static void SetChainFKSettingsInRetargetProfile(
		UPARAM(ref) FRetargetProfile& RetargetProfile,
		const FTargetChainFKSettings& FKSettings,
		const FName TargetChainName);

	/** Set the chain IK settings in a retarget profile (will set bApplyChainSettings to true). */
	UFUNCTION(BlueprintCallable, Category=RetargetProfile)
	static void SetChainIKSettingsInRetargetProfile(
		UPARAM(ref) FRetargetProfile& RetargetProfile,
		const FTargetChainIKSettings& IKSettings,
		const FName TargetChainName);

	/** Set the chain Speed Plant settings in a retarget profile (will set bApplyChainSettings to true). */
	UFUNCTION(BlueprintCallable, Category=RetargetProfile)
	static void SetChainSpeedPlantSettingsInRetargetProfile(
		UPARAM(ref) FRetargetProfile& RetargetProfile,
		const FTargetChainSpeedPlantSettings& SpeedPlantSettings,
		const FName TargetChainName);

	/** UObject */
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	/** END UObject */

#if WITH_EDITOR
	/* Get name of Source IK Rig property */
	static const FName GetSourceIKRigPropertyName();
	/* Get name of Target IK Rig property */
	static const FName GetTargetIKRigPropertyName();
	/* Get name of Source Preview Mesh property */
	static const FName GetSourcePreviewMeshPropertyName();
	/* Get name of Target Preview Mesh property */
	static const FName GetTargetPreviewMeshPropertyName();
	/** Get the names of the all the speed curves the retargeter will be looking for */
	void GetSpeedCurveNames(TArray<FName>& OutSpeedCurveNames) const;
#endif

private:

	void CleanAndInitialize();

	/** The rig to copy animation FROM.*/
	UPROPERTY(EditAnywhere, Category = Source)
	TSoftObjectPtr<UIKRigDefinition> SourceIKRigAsset = nullptr;

#if WITH_EDITORONLY_DATA
	/** Optional. Override the Skeletal Mesh to copy animation from. Uses the preview mesh from the Source IK Rig asset by default. */
	UPROPERTY(EditAnywhere, Category = Source)
	TSoftObjectPtr<USkeletalMesh> SourcePreviewMesh = nullptr;
#endif
	
	/** The rig to copy animation TO.*/
	UPROPERTY(EditAnywhere, Category = Target)
	TSoftObjectPtr<UIKRigDefinition> TargetIKRigAsset = nullptr;

#if WITH_EDITORONLY_DATA
	/** Optional. Override the Skeletal Mesh to preview the retarget on. Uses the preview mesh from the Target IK Rig asset by default. */
	UPROPERTY(EditAnywhere, Category = Target)
	TSoftObjectPtr<USkeletalMesh> TargetPreviewMesh = nullptr;
#endif
	
public:

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bRetargetRoot_DEPRECATED = true;
	UPROPERTY()
	bool bRetargetFK_DEPRECATED = true;
	UPROPERTY()
	bool bRetargetIK_DEPRECATED = true;
	UPROPERTY()
	float TargetActorOffset_DEPRECATED = 0.0f;
	UPROPERTY()
	float TargetActorScale_DEPRECATED = 0.0f;

	/** The offset applied to the target mesh in the editor viewport. */
	UPROPERTY(EditAnywhere, Category = PreviewSettings)
	FVector TargetMeshOffset;

	/** Scale the target mesh in the viewport for easier visualization next to the source.*/
	UPROPERTY(EditAnywhere, Category = PreviewSettings, meta = (UIMin = "0.01", UIMax = "10.0"))
	float TargetMeshScale = 1.0f;

	/** The offset applied to the source mesh in the editor viewport. */
	UPROPERTY(EditAnywhere, Category = PreviewSettings)
	FVector SourceMeshOffset;

	/** Toggle debug drawing for retargeting in the viewport. */
	UPROPERTY(EditAnywhere, Category = DebugSettings)
	bool bDebugDraw = true;

	/** Draw final IK goal locations. */
	UPROPERTY(EditAnywhere, Category = DebugSettings)
	bool bDrawFinalGoals = true;

	/** Draw goal locations from source skeleton. */
	UPROPERTY(EditAnywhere, Category = DebugSettings)
	bool bDrawSourceLocations = true;
	
	/** The visual size of the IK goals in the viewport. */
	UPROPERTY(EditAnywhere, Category = DebugSettings)
	float ChainDrawSize = 5.0f;

	/** The thickness of lines on the IK goals in the viewport. */
	UPROPERTY(EditAnywhere, Category = DebugSettings)
	float ChainDrawThickness = 0.5f;
	
	/** The visual size of the bones in the viewport (saved between sessions). This is set from the viewport Character>Bones menu*/
	UPROPERTY()
	float BoneDrawSize = 8.0f;
	
private:

	/** The controller responsible for managing this asset's data (all editor mutation goes through this) */
	UPROPERTY(Transient)
	TObjectPtr<UObject> Controller;

	/** only ask to fix the root height once, then warn thereafter (don't nag) */
	UPROPERTY()
	TSet<TObjectPtr<USkeletalMesh>> MeshesAskedToFixRootHeightFor;
#endif
	
private:

	/** (OLD VERSION) Mapping of chains to copy animation between source and target rigs.*/
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	TArray<FRetargetChainMap> ChainMapping_DEPRECATED;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	/** Settings for how to map source chains to target chains.*/
	UPROPERTY()
	TArray<TObjectPtr<URetargetChainSettings>> ChainSettings;
	
	/** the retarget root settings */
	UPROPERTY()
	TObjectPtr<URetargetRootSettings> RootSettings;

	/** the retarget root settings */
	UPROPERTY()
	TObjectPtr<UIKRetargetGlobalSettings> GlobalSettings;

	/** settings profiles stored in this asset */
	UPROPERTY()
	TMap<FName, FRetargetProfile> Profiles;
	UPROPERTY()
	FName CurrentProfile = NAME_None;
	
	/** The set of retarget poses for the SOURCE skeleton.*/
	UPROPERTY()
	TMap<FName, FIKRetargetPose> SourceRetargetPoses;
	/** The set of retarget poses for the TARGET skeleton.*/
	UPROPERTY()
	TMap<FName, FIKRetargetPose> TargetRetargetPoses;
	
	/** The current retarget pose to use for the SOURCE.*/
	UPROPERTY()
	FName CurrentSourceRetargetPose;
	/** The current retarget pose to use for the TARGET.*/
	UPROPERTY()
	FName CurrentTargetRetargetPose;

	/** (OLD VERSION) Before retarget poses were stored for target AND source.*/
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	TMap<FName, FIKRetargetPose> RetargetPoses_DEPRECATED;
	UPROPERTY()
	FName CurrentRetargetPose_DEPRECATED;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	friend class UIKRetargeterController;
};
