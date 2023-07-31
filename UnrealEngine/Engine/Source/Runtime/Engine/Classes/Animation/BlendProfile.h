// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BoneContainer.h"
#include "AnimationRuntime.h"
#include "BlendProfile.generated.h"

struct FAlphaBlend;
struct FCompactPose;
struct FBlendedCurve;
struct FSlotEvaluationPose;
namespace UE { namespace Anim { struct FStackAttributeContainer; } }

/** The mode in which the blend profile should be applied. */
UENUM()
enum class EBlendProfileMode : uint8
{
	// The bone's transition time is a factor based on the transition time. 
	// For example 0.5 means it takes half the time of the transition.
	// Values should be between 0 and 1. They will be clamped if they go out of this range.
	// A bone value of 0 means the bone will instantly transition into the target state.
	TimeFactor = 0,

	// The bone's transition weight is multiplied by this factor.
	// For example 2.0 means the bone's blend weight is twice as high as the transition's blend weight.
	// Values should typically be equal or greater than 1.0.
	// If you want certain bones to instantly transition into the target state
	// the Time Factor based method might be a better choice.
	WeightFactor,

	// Used for blend masks. Per bone alpha
	BlendMask UMETA(Hidden),
};

/** A single entry for a blend scale within a profile, mapping a bone to a blendscale */
USTRUCT()
struct FBlendProfileBoneEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=BoneSettings)
	FBoneReference BoneReference;

	UPROPERTY(EditAnywhere, Category=BoneSettings)
	float BlendScale = 0.f;
};

//////////////////////////////////////////////////////////////////////////

/** A blend profile is a set of per-bone scales that can be used in transitions and blend lists
 *  to tweak the weights of specific bones. The scales are applied to the normal weight for that bone
 */
UCLASS(Within=Skeleton)
class ENGINE_API UBlendProfile : public UObject, public IInterpolationIndexProvider
{
public:

	GENERATED_BODY()

	UBlendProfile();

	/** Get the number of entries in the profile (an entry is any blend scale that isn't 1.0f) */
	int32 GetNumBlendEntries() const { return ProfileEntries.Num(); }

	/** Set the blend scale for a specific bone 
	 *  @param InBoneIdx Index of the bone to set the blend scale of
	 *  @param InScale The scale to set the bone to
	 *  @param bRecurse Whether or not to set the scale on all children of this bone
	 *  @param bCreate Whether or not to create a blend profile entry if one does not exist for the specified bone
	 */
	void SetBoneBlendScale(int32 InBoneIdx, float InScale, bool bRecurse = false, bool bCreate = false);

	/** Set the blend scale for a specific bone 
	 *  @param InBoneName Name of the bone to set the blend scale of
	 *  @param InScale The scale to set the bone to
	 *  @param bRecurse Whether or not to set the scale on all children of this bone
	 *  @param bCreate Whether or not to create a blend profile entry if one does not exist for the specified bone
	 */
	void SetBoneBlendScale(const FName& InBoneName, float InScale, bool bRecurse = false, bool bCreate = false);

	/** Removes the entry for the specified bone index (does nothing if it doesn't exist) 
	*  @param InBoneIdx Index of the bone to remove from this blend profile
	*/
	void RemoveEntry(int32 InBoneIdx);

	/** Get the set blend scale for the specified bone, will return 1.0f if no entry was found (no scale)
	 *  @param InBoneIdx Index of the bone to retrieve
	 */
	float GetBoneBlendScale(int32 InBoneIdx) const;

	/** Get the set blend scale for the specified bone, will return 1.0f if no entry was found (no scale). The term bone factor and entry or bone scale refer to the same thing.
	 *  @param InBoneName Name of the bone to retrieve
	 */
	float GetBoneBlendScale(const FName& InBoneName) const;
	
	UE_DEPRECATED(5.0, "Please use the overload that takes a skeleton bone index")
	int32 GetEntryIndex(const int32 InBoneIdx) const;

	/** Get the index of the entry for the specified bone
	 *  @param InBoneIdx Skeleton index of the bone
	 */
	int32 GetEntryIndex(const FSkeletonPoseBoneIndex InBoneIdx) const;

	/** Get the index of the entry for the specified bone
	 *  @param InBoneIdx Index of the bone
	 */
	int32 GetEntryIndex(const FName& BoneName) const;

	/** Get the blend scale stored in a specific entry. The term bone factor and entry scale refer to the same thing.
	 *  @param InEntryIdx Index of the entry to retrieve
	 */
	float GetEntryBlendScale(const int32 InEntryIdx) const;

	/** Update all the bone weights for some provided FBlendSampleData.
	 *  This internally will iterate over all entries inside the InOutCurrentData::PerBoneBlendData parameter and call CalculateBoneWeight for each entry.
	 *  @param InOutCurrentData This is both input and output. The FBlendSampleData::PerBoneBlendData member will be updated with the weight values as calculated by CalculateBoneWeight for each entry.
	 *  @param BlendInfo Information about the blend. This contains things like the blend duration and current alpha. Not all blend profile modes might use this data.
	 *  @param BlendStartAlpha The linear alpha value, so not sampled from the curve, of where the blend started. This is mostly used when we are in the middle of a blend and suddenly reverse its direction.
	 *  This value basically should contain the alpha value of the blend at the point of reversal.
	 *  @param MainWeight The weight of the blend. This is used in the weight factor based mode, where this weight is multiplied by the bone factors.
	 *  @param bInverse Should we inverse the weights? This can be used for things like transition reversal. In most cases you would want it set to false.
	 */
	void UpdateBoneWeights(FBlendSampleData& InOutCurrentData, const FAlphaBlend& BlendInfo, float BlendStartAlpha, float MainWeight, bool bInverse = false);

	/** Calculate the blend weight for a given bone. This methoid basically defines how each blend profile mode works.
	 *  @param BoneFactor This is the per bone value setup in the blend profile editor. The impact of this value depends on what blend profile mode is used.
	 *  @param Mode The blend profile mode that should be used in combination with this bone factor.
	 *  @param BlendInfo Information about the blend. This contains things like the blend duration and current alpha. Not all blend profile modes might use this data.
	 *  @param BlendStartAlpha The linear alpha value, so not sampled from the curve, of where the blend started. This is mostly used when we are in the middle of a blend and suddenly reverse its direction.
	 *  This value basically should contain the alpha value of the blend at the point of reversal.
	 *  @param MainWeight The weight of the blend. This is used in the weight factor based mode, where this weight is multiplied by the bone factors.
	 *  @param bInverse Should we inverse the weights? This can be used for things like transition reversal. In most cases you would want it set to false.
	 */
	static float CalculateBoneWeight(float BoneFactor, EBlendProfileMode Mode, const FAlphaBlend& BlendInfo, float BlendStartAlpha, float MainWeight, bool bInverse = false);

	/** Resize and fill an array of floats with the bone factor values. One for each bone inside the compact pose.
	 *  @param OutBoneBlendProfileFactors This array will be resized and filled with the factors for each bone in the compact pose, as setup in the blend profile editor.
	 *  @param BoneContainer The bone container which is used to extract how many bones are inside the compact pose and to figure out what factor value to place at what array element.
	 */
	void FillBoneScalesArray(TArray<float>& OutBoneBlendProfileFactors, const FBoneContainer& BoneContainer) const;

	/** Fill an array of floats with the bone duration values. One for each bone in the skeleton pose.
	 * @param OutDurationPerBone Must be sized to the number bones in the skeleton pose. It will be filled with the durations of each bone as setup in the blend profile editor.
	 * @param Duration The duration of the blend.
	 */
	void FillSkeletonBoneDurationsArray(TCustomBoneIndexArrayView<float, FSkeletonPoseBoneIndex> OutDurationPerBone, float Duration) const;

	// IInterpolationIndexProvider
	virtual int32 GetPerBoneInterpolationIndex(const FCompactPoseBoneIndex& InCompactPoseBoneIndex, const FBoneContainer& BoneContainer, const IInterpolationIndexProvider::FPerBoneInterpolationData* Data) const override;
	// End IInterpolationIndexProvider

	// UObject
	virtual bool IsSafeForRootSet() const override {return false;}
	virtual void PostLoad() override;
	// End UObject

	// Default value of entries. Default values are not saved
	virtual float GetDefaultBlendScale() const { return IsBlendMask() ? 0.0f : 1.0f; }

	bool IsBlendMask() const { return Mode == EBlendProfileMode::BlendMask;  }

	EBlendProfileMode GetMode() const { return Mode; }

private:
	/** Sets the skeleton this blend profile is used with */
	void SetSkeleton(USkeleton* InSkeleton);

	/** Set the blend scale for a single bone (ignore children) */
	void SetSingleBoneBlendScale(int32 InBoneIdx, float InScale, bool bCreate = false);

public:
	// The skeleton that owns this profile
	UPROPERTY()
	TObjectPtr<USkeleton> OwningSkeleton;

	// List of blend scale entries
	UPROPERTY()
	TArray<FBlendProfileBoneEntry> ProfileEntries;

	// Blend Profile Mode. Read EBlendProfileMode for more details
	UPROPERTY()
	EBlendProfileMode Mode;
};
