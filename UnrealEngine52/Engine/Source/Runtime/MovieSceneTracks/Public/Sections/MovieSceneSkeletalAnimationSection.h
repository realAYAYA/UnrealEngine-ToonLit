// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Animation/AnimSequenceBase.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "MovieSceneSkeletalAnimationSection.generated.h"

struct FMovieSceneSkeletalAnimRootMotionTrackParams;
struct FAnimationPoseData;
class UMirrorDataTable;
enum class ESwapRootBone : uint8;

USTRUCT(BlueprintType)
struct FMovieSceneSkeletalAnimationParams
{
	GENERATED_BODY()

	FMovieSceneSkeletalAnimationParams();

	/** Gets the animation duration, modified by play rate */
	float GetDuration() const { return FMath::IsNearlyZero(PlayRate) || Animation == nullptr ? 0.f : Animation->GetPlayLength() / PlayRate; }

	/** Gets the animation sequence length, not modified by play rate */
	float GetSequenceLength() const { return Animation != nullptr ? Animation->GetPlayLength() : 0.f; }

	/**
	 * Convert a sequence frame to a time in seconds inside the animation clip, taking into account start/end offsets,
	 * looping, etc.
	 */
	double MapTimeToAnimation(const UMovieSceneSection* InSection, FFrameTime InPosition, FFrameRate InFrameRate) const;

	/**
	 * As above, but with already computed section bounds.
	 */
	double MapTimeToAnimation(FFrameNumber InSectionStartTime, FFrameNumber InSectionEndTime, FFrameTime InPosition, FFrameRate InFrameRate) const;

	/** The animation this section plays */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation", meta=(AllowedClasses = "/Script/Engine.AnimSequence,/Script/Engine.AnimComposite,/Script/Engine.AnimStreamable"))
	TObjectPtr<UAnimSequenceBase> Animation;

	/** The offset into the beginning of the animation clip for the first loop of play. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	FFrameNumber FirstLoopStartFrameOffset;

	/** The offset into the beginning of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	FFrameNumber StartFrameOffset;

	/** The offset into the end of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	FFrameNumber EndFrameOffset;

	/** The playback rate of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	float PlayRate;

	/** Reverse the playback of the animation clip */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Animation")
	uint32 bReverse:1;

	/** The slot name to use for the animation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation" )
	FName SlotName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	TObjectPtr<class UMirrorDataTable> MirrorDataTable;

	/** The weight curve for this animation section */
	UPROPERTY()
	FMovieSceneFloatChannel Weight;

	/** If on will skip sending animation notifies */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	bool bSkipAnimNotifiers;

	/** If on animation sequence will always play when active even if the animation is controlled by a Blueprint or Anim Instance Class*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	bool bForceCustomMode;

	/** If on the root bone transform will be swapped to the specified root*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation")
	ESwapRootBone SwapRootBone;

	UPROPERTY()
	float StartOffset_DEPRECATED;

	UPROPERTY()
	float EndOffset_DEPRECATED;

};

/**
 * Movie scene section that control skeletal animation
 */
UCLASS( MinimalAPI )
class UMovieSceneSkeletalAnimationSection
	: public UMovieSceneSection
	, public IMovieSceneEntityProvider
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Animation", meta=(ShowOnlyInnerProperties))
	FMovieSceneSkeletalAnimationParams Params;

	/** Get Frame Time as Animation Time*/
	MOVIESCENETRACKS_API double MapTimeToAnimation(FFrameTime InPosition, FFrameRate InFrameRate) const;
	
	//~ UMovieSceneSection interface
	virtual void SetRange(const TRange<FFrameNumber>& NewRange) override;
	virtual void SetStartFrame(TRangeBound<FFrameNumber> NewStartFrame) override;
	virtual void SetEndFrame(TRangeBound<FFrameNumber> NewEndFrame)override;

	struct FRootMotionParams
	{
		bool bBlendFirstChildOfRoot;
		int32 ChildBoneIndex;
		TOptional<FTransform> Transform;
		TOptional<FTransform> PreviousTransform;
	};
protected:

	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys) override;
	virtual void GetSnapTimes(TArray<FFrameNumber>& OutSnapTimes, bool bGetSectionBorders) const override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;
	virtual void MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	virtual float GetTotalWeightValue(FFrameTime InTime) const override;

	/** ~UObject interface */
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	/** ~IMovieSceneEntityProvider interface */
	virtual void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& InParams, FImportedEntity* OutImportedEntity) override;

private:

	//~ UObject interface

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	float PreviousPlayRate;
private:
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;

#endif

private:

	UPROPERTY()
	TObjectPtr<class UAnimSequence> AnimSequence_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UAnimSequenceBase> Animation_DEPRECATED;

	UPROPERTY()
	float StartOffset_DEPRECATED;
	
	UPROPERTY()
	float EndOffset_DEPRECATED;
	
	UPROPERTY()
	float PlayRate_DEPRECATED;

	UPROPERTY()
	uint32 bReverse_DEPRECATED:1;

	UPROPERTY()
	FName SlotName_DEPRECATED;

public:
	/* Location offset applied before the matched offset*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root Motions")
	FVector StartLocationOffset;

	/* Location offset applied after the matched offset*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Root Motions")

	FRotator StartRotationOffset;

	UPROPERTY()
	bool bMatchWithPrevious;

	UPROPERTY()
	FName MatchedBoneName;

	/* Location offset determined by matching*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Root Motions")
	FVector MatchedLocationOffset;

	/* Rotation offset determined by matching*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Root Motions")
	FRotator MatchedRotationOffset;

	UPROPERTY()
	bool bMatchTranslation;

	UPROPERTY()
	bool bMatchIncludeZHeight;

	UPROPERTY()
	bool bMatchRotationYaw;

	UPROPERTY()
	bool bMatchRotationPitch;

	UPROPERTY()
	bool bMatchRotationRoll;

#if WITH_EDITORONLY_DATA
	/** Whether to show the underlying skeleton for this section. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Root Motions")
	bool bShowSkeleton;
#endif
	//Previous transform used to specify the global OffsetTransform while calculting the root motions.
	FTransform PreviousTransform;

	//Temporary index used by GetRootMotionTransform and set by SetBoneIndexforRootMotionCalculations
	TOptional<int32> TempRootBoneIndex;
public:

	struct FRootMotionTransformParam
	{
		FFrameTime CurrentTime; //current time
		FFrameRate FrameRate; //scene frame rate
		bool bOutIsAdditive; //if this is additive or not
		FTransform OutTransform; //total transform including current pose plus offset
		FTransform OutParentTransform; //offset transform not including original bone transform
		FTransform OutPoseTransform; //original bone transform
		FTransform OutRootStartTransform;//start of the root
		float OutWeight; //weight at specified time
	};

	// Functions/params related to root motion calcuations.
	MOVIESCENETRACKS_API FMovieSceneSkeletalAnimRootMotionTrackParams* GetRootMotionParams() const;

	MOVIESCENETRACKS_API bool GetRootMotionVelocity(FFrameTime PreviousTime, FFrameTime CurrentTime, FFrameRate FrameRate, FTransform& OutVelocity, float& OutWeight) const;
	MOVIESCENETRACKS_API int32 SetBoneIndexForRootMotionCalculations(bool bBlendFirstChildOfRoot);
	MOVIESCENETRACKS_API bool GetRootMotionTransform(FAnimationPoseData& PoseData, FRootMotionTransformParam& InOutParams) const;
	MOVIESCENETRACKS_API void MatchSectionByBoneTransform(USkeletalMeshComponent* SkelMeshComp, FFrameTime CurrentFrame, FFrameRate FrameRate,
		const FName& BoneName); //add options for z and rotation

	MOVIESCENETRACKS_API void ClearMatchedOffsetTransforms();
	
	MOVIESCENETRACKS_API void GetRootMotion(FFrameTime CurrentTime, FRootMotionParams& OutRootMotionParams) const;

	MOVIESCENETRACKS_API void ToggleMatchTranslation();

	MOVIESCENETRACKS_API void ToggleMatchIncludeZHeight();

	MOVIESCENETRACKS_API void ToggleMatchIncludeYawRotation();

	MOVIESCENETRACKS_API void ToggleMatchIncludePitchRotation();

	MOVIESCENETRACKS_API void ToggleMatchIncludeRollRotation();

#if WITH_EDITORONLY_DATA
	MOVIESCENETRACKS_API void ToggleShowSkeleton();
#endif

	MOVIESCENETRACKS_API FTransform GetRootMotionStartOffset() const;

private:
	void MultiplyOutInverseOnNextClips(FVector PreviousMatchedLocationOffset, FRotator PreviousMatchedRotationOffset);

};
