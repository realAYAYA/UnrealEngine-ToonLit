// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeAnimationTrackSetNode.generated.h"

/**
 * Enumeration specifying which properties of a camera, light or scene node can be animated besides transform.
 */
UENUM(BlueprintType)
enum class EInterchangeAnimatedProperty : uint8
{
	None UMETA(DisplayName = "No property.", ToolTip = "The associated animation track will be ignored."),
	Visibility UMETA(DisplayName = "Visibility property.", ToolTip = "The associated animation track is applied to the visibility property of the actor"),
	MAX,
};
/**
 * Enumeration specifying how to handle the state of the animated property at the end of an animation track
 */
enum class EInterchangeAimationCompletionMode : uint8
{
	KeepState UMETA(DisplayName = "Keep State", ToolTip = "Keep the animated property at the state set at the end of the animation track."),
	RestoreState UMETA(DisplayName = "Restore State", ToolTip = "Restore the animated property to its state before the start of the animation track."),
	ProjectDefault UMETA(DisplayName = "Project Default", ToolTip = "Restore the animated property to the state set in the project for such property."),
};

/**
 * Class to represent a set of animation track nodes sharing the same frame rate
 */
UCLASS(BlueprintType)
class INTERCHANGENODES_API UInterchangeAnimationTrackSetNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

	UInterchangeAnimationTrackSetNode();

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("AnimationTrackSet");
	}

	/**
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("AnimationTrackSetNode");
		return TypeName;
	}

	/**
	 * This function allow to retrieve the number of track dependencies for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSet")
	int32 GetCustomAnimationTrackUidCount() const;

	/**
	 * This function allow to retrieve the track dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSet")
	void GetCustomAnimationTrackUids(TArray<FString>& OutAnimationTrackUids) const;

	/**
	 * This function allow to retrieve one track dependency for this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSet")
	void GetCustomAnimationTrackUid(const int32 Index, FString& OutAnimationTrackUid) const;

	/**
	 * Add one track dependency to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSet")
	bool AddCustomAnimationTrackUid(const FString& AnimationTrackUid);

	/**
	 * Remove one track dependency from this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSet")
	bool RemoveCustomAnimationTrackUid(const FString& AnimationTrackUid);

	/**
	 * Set the frame rate for the animations in the level sequence.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSet")
	bool SetCustomFrameRate(const float& AttributeValue);

	/**
	 * Get the frame rate for the animations in the level sequence.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSet")
	bool GetCustomFrameRate(float& AttributeValue) const;

private:
	const UE::Interchange::FAttributeKey Macro_CustomFrameRateKey = UE::Interchange::FAttributeKey(TEXT("FrameRate"));

	UE::Interchange::TArrayAttributeHelper<FString> CustomAnimationTrackUids;
};

/**
 * Abstract class providing the minimal services required for an animation track node
 */
UCLASS(Abstract, Experimental)
class INTERCHANGENODES_API UInterchangeAnimationTrackBaseNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("AnimationTrackBaseNode");
	}

	/**
	* Return the node type name of the class, we use this when reporting errors
	*/
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("AnimationTrackBaseNode");
		return TypeName;
	}

	/**
	 * Set how the actor's animated property should behave once its animation completes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool SetCustomCompletionMode(const int32& AttributeValue);

	/**
	 * Get how the actor's animated property behaves once this animation is complete.
	 * The output value will be clamped to the range of values defined in EInterchangeAimationCompletionMode
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool GetCustomCompletionMode(int32& AttributeValue) const;

private:
	const UE::Interchange::FAttributeKey Macro_CustomCompletionModeKey = UE::Interchange::FAttributeKey(TEXT("CompletionMode"));
};

/**
 * Class to represent an animation which instances another animation track set node
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeAnimationTrackSetInstanceNode : public UInterchangeAnimationTrackBaseNode
{
	GENERATED_BODY()

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("AnimationTrackSetInstanceNode");
	}

	/**
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("AnimationTrackSetInstanceNode");
		return TypeName;
	}

	/**
	 * Set the time scale used for the level sequence instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool SetCustomTimeScale(const float& AttributeValue);

	/**
	 * Get the time scale used for the level sequence instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool GetCustomTimeScale(float& AttributeValue) const;

	/**
	 * Set the level sequence instance duration in number of frames.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool SetCustomDuration(const int32& AttributeValue);

	/**
	 * Get the level sequence instance duration in number of frames.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool GetCustomDuration(int32& AttributeValue) const;

	/**
	 * Set the frame where the level sequence instance starts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool SetCustomStartFrame(const int32& AttributeValue);

	/**
	 * Get the frame where the level sequence instance starts.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool GetCustomStartFrame(int32& AttributeValue) const;

	/**
	 * Set the unique id of the level sequence this instance references.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool SetCustomTrackSetDependencyUid(const FString& AttributeValue);

	/**
	 * Get the unique id of the level sequence this instance references.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrackSetInstance")
	bool GetCustomTrackSetDependencyUid(FString& AttributeValue) const;

private:
	const UE::Interchange::FAttributeKey Macro_CustomStartFrameKey = UE::Interchange::FAttributeKey(TEXT("StartFrame"));
	const UE::Interchange::FAttributeKey Macro_CustomDurationKey = UE::Interchange::FAttributeKey(TEXT("Duration"));
	const UE::Interchange::FAttributeKey Macro_CustomTimeScaleKey = UE::Interchange::FAttributeKey(TEXT("TimeScale"));
	const UE::Interchange::FAttributeKey Macro_CustomTrackSetDependencyUidKey = UE::Interchange::FAttributeKey(TEXT("SequenceDependencyUid"));
};

/**
 * Class to represent an animation on the property of a camera, light or scene node
 * The list of supported properties is enumerated in EInterchangeAnimatedProperty
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeAnimationTrackNode : public UInterchangeAnimationTrackBaseNode
{
	GENERATED_BODY()

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("AnimationTrack");
	}

	/**
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("AnimationTrackNode");
		return TypeName;
	}

	/**
	 * Set the actor dependency to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool SetCustomActorDependencyUid(const FString& DependencyUid);

	/**
	 * Get the actor dependency to this object.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool GetCustomActorDependencyUid(FString& DependencyUid) const;

	/**
	 * Set the property animated by this track.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool SetCustomTargetedProperty(const int32& TargetedProperty);

	/**
	 * Get the property animated by this track.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool GetCustomTargetedProperty(int32& TargetedProperty) const;

	/**
	 * Set the payload key needed to retrieve the animation for this track.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool SetCustomAnimationPayloadKey(const FString& PayloadKey);

	/**
	 * Get the payload key needed to retrieve the animation for this track.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool GetCustomAnimationPayloadKey(FString& PayloadKey) const;

	/**
	 * Set the number of frames for the animation of this track.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool SetCustomFrameCount(const int32& AttributeValue);

	/**
	 * Get the number of frames for the animation of this track.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | AnimationTrack")
	bool GetCustomFrameCount(int32& AttributeValue) const;

private:
	const UE::Interchange::FAttributeKey Macro_CustomActorDependencyKey = UE::Interchange::FAttributeKey(TEXT("ActorDependency"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimationPayloadKey = UE::Interchange::FAttributeKey(TEXT("AnimationPayload"));
	const UE::Interchange::FAttributeKey Macro_CustomFrameCountKey = UE::Interchange::FAttributeKey(TEXT("FrameCount"));
	const UE::Interchange::FAttributeKey Macro_CustomTargetedPropertyKey = UE::Interchange::FAttributeKey(TEXT("TargetedProperty"));
};

/**
 * Class to represent an animation on the transform of a camera, light or scene node
 */
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeTransformAnimationTrackNode : public UInterchangeAnimationTrackNode
{
	GENERATED_BODY()

public:
	static FStringView StaticAssetTypeName()
	{
		return TEXT("TransformAnimationTrack");
	}

	/**
	 * Return the node type name of the class, we use this when reporting errors
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("TransformAnimationTrackNode");
		return TypeName;
	}

	/**
	 * Set which channels of this animation should be used. This is a bitwise mask.
	 * Bits are interpreted as follow:
	 *    None          = 0x000,
	 *    TranslationX  = 0x001,
	 *    TranslationY  = 0x002,
	 *    TranslationZ  = 0x004,
	 *    Translation   = TranslationX | TranslationY | TranslationZ,
	 *    RotationX     = 0x008,
	 *    RotationY     = 0x010,
	 *    RotationZ     = 0x020,
	 *    Rotation      = RotationX | RotationY | RotationZ,
	 *    ScaleX        = 0x040,
	 *    ScaleY        = 0x080,
	 *    ScaleZ        = 0x100,
	 *    Scale         = ScaleX | ScaleY | ScaleZ,
	 *    AllTransform  = Translation | Rotation | Scale,
	 *    Weight        = 0x200,
	 *    All           = Translation | Rotation | Scale | Weight,
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | TransformAnimationTrack")
	bool SetCustomUsedChannels(const int32& AttributeValue);

	/**
	 * Get which channels of this animation should be used. This is a bitmask.
	 * See SetCustomUsedChannels for description of bitmask
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | TransformAnimationTrack")
	bool GetCustomUsedChannels(int32& AttributeValue) const;

private:
	const UE::Interchange::FAttributeKey Macro_CustomUsedChannelsKey = UE::Interchange::FAttributeKey(TEXT("UsedChannels"));
};

/*
* Class to hold onto the relationships between a set of animation tracks and the bones, morph targets of a skeleton.
*/
UCLASS(BlueprintType, Experimental)
class INTERCHANGENODES_API UInterchangeSkeletalAnimationTrackNode : public UInterchangeAnimationTrackBaseNode
{
	GENERATED_BODY()

public:
	UInterchangeSkeletalAnimationTrackNode();

	/**
	 * Override serialize to restore SlotMaterialDependencies on load.
	 */
	virtual void Serialize(FArchive& Ar) override
	{
		Super::Serialize(Ar);

		if (Ar.IsLoading() && bIsInitialized)
		{
			SceneNodeAnimationPayloadKeyMap.RebuildCache();
			MorphTargetPayloadKeyMap.RebuildCache();
		}
	}

	static FStringView StaticAssetTypeName()
	{
		return TEXT("SkeletalAnimationTrack");
	}

	/**
	* Return the node type name of the class, we use this when reporting errors
	*/
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("SkeletalAnimationTrack");
		return TypeName;
	}


public:
	/** Get the skeleton factory node unique id. Return false if the attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetCustomSkeletonNodeUid(FString& AttributeValue) const;

	/** Set the skeleton factory node unique id. Return false if the attribute cannot be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetCustomSkeletonNodeUid(const FString& AttributeValue);

	/** Get the skeletal mesh node unique id. Return false if the attribute is not set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetCustomSkeletalMeshNodeUid(FString& AttributeValue) const;

	/** Set the skeletal mesh node unique id. Return false if the attribute cannot be set. */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetCustomSkeletalMeshNodeUid(const FString& AttributeValue);

	/**
	 * Set the animation sample rate. Return false if the attribute cannot be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetCustomAnimationSampleRate(const double& SampleRate);

	/**
	 * Get the animation sample rate. Return false if the attribute is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetCustomAnimationSampleRate(double& SampleRate) const;

	/**
	 * Set the animation start time. Return false if the attribute cannot be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetCustomAnimationStartTime(const double& StartTime);

	/**
	 * Get the animation start time. Return false if the attribute is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetCustomAnimationStartTime(double& StartTime) const;
	
	/**
	 * Set the animation stop time. Return false if the attribute cannot be set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetCustomAnimationStopTime(const double& StopTime);

	/**
	 * Get the animation stop time. Return false if the attribute is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetCustomAnimationStopTime(double& StopTime) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	void GetSceneNodeAnimationPayloadKeys(TMap<FString, FString>& OutSceneNodeAnimationPayloadKeys) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetAnimationPayloadKeyFromSceneNodeUid(const FString& SceneNodeUid, FString& OutPayloadKey) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetAnimationPayloadKeyForSceneNodeUid(const FString& SceneNodeUid, const FString& PayloadKey);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool RemoveAnimationPayloadKeyForSceneNodeUid(const FString& SceneNodeUid);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	void GetMorphTargetNodeAnimationPayloadKeys(TMap<FString, FString>& OutMorphTargetNodeAnimationPayloads) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool GetAnimationPayloadKeyFromMorphTargetNodeUid(const FString& MorphTargetNodeUid, FString& OutPayloadKey) const;

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool SetAnimationPayloadKeyForMorphTargetNodeUid(const FString& MorphTargetNodeUid, const FString& PayloadKey);

	UFUNCTION(BlueprintCallable, Category = "Interchange | Node | SkeletalAnimationTrack")
	bool RemoveAnimationPayloadKeyForMorphTargetNodeUid(const FString& MorphTargetNodeUid);

private:
	const UE::Interchange::FAttributeKey Macro_CustomSkeletonNodeUidKey = UE::Interchange::FAttributeKey(TEXT("SkeletonNodeUid"));
	const UE::Interchange::FAttributeKey Macro_CustomSkeletalMeshNodeUidKey = UE::Interchange::FAttributeKey(TEXT("SkeletalMeshNodeUid"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimationSampleRateKey = UE::Interchange::FAttributeKey(TEXT("AnimationSampleRate"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimationStartTimeKey = UE::Interchange::FAttributeKey(TEXT("AnimationStartTime"));
	const UE::Interchange::FAttributeKey Macro_CustomAnimationStopTimeKey = UE::Interchange::FAttributeKey(TEXT("AnimationStopTime"));
	
	UE::Interchange::TMapAttributeHelper<FString, FString> SceneNodeAnimationPayloadKeyMap;
	UE::Interchange::TMapAttributeHelper<FString, FString> MorphTargetPayloadKeyMap;
};
