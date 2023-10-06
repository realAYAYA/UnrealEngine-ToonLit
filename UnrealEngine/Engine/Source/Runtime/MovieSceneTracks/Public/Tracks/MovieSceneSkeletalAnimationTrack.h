// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneNameableTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "MovieSceneSkeletalAnimationTrack.generated.h"

enum class ESwapRootBone : uint8;

/**Struct to hold the cached root motion positions based upon how we calculated them.
* Also provides way to get the root motion at a particular time.
*/

USTRUCT()
struct FMovieSceneSkeletalAnimRootMotionTrackParams
{
	GENERATED_BODY()

	FFrameTime FrameTick; 
	FFrameTime StartFrame;
	FFrameTime EndFrame;
	bool bRootMotionsDirty;
	bool bHaveRootMotion;
	FTransform RootMotionStartOffset; // root motion may not be in Mesh Space if we are putting values on a bone that is a child of a root with an offset
	/** Get the Root Motion transform at the specified time.*/
	FMovieSceneSkeletalAnimRootMotionTrackParams() : bRootMotionsDirty(true),bHaveRootMotion(false) {}

#if WITH_EDITORONLY_DATA
	bool bCacheRootTransforms = false;
	TArray<FTransform> RootTransforms;
#endif
};

/**
 * Handles animation of skeletal mesh actors
 */
UCLASS(MinimalAPI)
class UMovieSceneSkeletalAnimationTrack
	: public UMovieSceneNameableTrack
{
	GENERATED_UCLASS_BODY()

public:

	/** Adds a new animation to this track */
	MOVIESCENETRACKS_API virtual UMovieSceneSection* AddNewAnimationOnRow(FFrameNumber KeyTime, class UAnimSequenceBase* AnimSequence, int32 RowIndex);

	/** Adds a new animation to this track on the next available/non-overlapping row */
	virtual UMovieSceneSection* AddNewAnimation(FFrameNumber KeyTime, class UAnimSequenceBase* AnimSequence) { return AddNewAnimationOnRow(KeyTime, AnimSequence, INDEX_NONE); }

	/** Gets the animation sections at a certain time */
	MOVIESCENETRACKS_API TArray<UMovieSceneSection*> GetAnimSectionsAtTime(FFrameNumber Time);

public:

	// UMovieSceneTrack interface

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif

	virtual void RemoveAllAnimationData() override;
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual void RemoveSectionAt(int32 SectionIndex) override;
	virtual bool IsEmpty() const override;
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual bool PopulateEvaluationTree(TMovieSceneEvaluationTree<FMovieSceneTrackEvaluationData>& OutData) const override;
	virtual bool SupportsMultipleRows() const override;
	virtual void UpdateEasing() override;

#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif

#if WITH_EDITOR
		virtual EMovieSceneSectionMovedResult OnSectionMoved(UMovieSceneSection& Section, const FMovieSceneSectionMovedParams& Params) override;
#endif

private:
	void SortSections();

public:

	MOVIESCENETRACKS_API void SetRootMotionsDirty();
	MOVIESCENETRACKS_API void SetUpRootMotions(bool bForce);
	MOVIESCENETRACKS_API TOptional<FTransform>  GetRootMotion(FFrameTime CurrentTime);
	MOVIESCENETRACKS_API void MatchSectionByBoneTransform(bool bMatchWithPrevious, USkeletalMeshComponent* SkelMeshComp, UMovieSceneSkeletalAnimationSection* CurrentSection, FFrameTime CurrentFrame, FFrameRate FrameRate,
		const FName& BoneName, FTransform& SecondSectionRootDiff, FVector& TranslationDiff, FQuat& RotationDiff); //add options for z and for rotation.
#if WITH_EDITORONLY_DATA

	MOVIESCENETRACKS_API void ToggleShowRootMotionTrail();
#endif

private:
	//Not called yet, will be used to automatch a section when it's added to another
	void AutoMatchSectionRoot(UMovieSceneSkeletalAnimationSection* AnimSection);
public:

	/** List of all animation sections */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSection>> AnimationSections;

	UPROPERTY()
	bool bUseLegacySectionIndexBlend;

	UPROPERTY()
	FMovieSceneSkeletalAnimRootMotionTrackParams RootMotionParams;

	/** Whether to blend and adjust the first child node with animation instead of the root, this should be true for blending when the root is static, false if the animations have proper root motion*/
	UPROPERTY(EditAnywhere, Category = "Root Motions")
	bool bBlendFirstChildOfRoot;

	/** If on the root bone transform will be swapped to the specified root*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Setter,Getter, Category = "Root Motions")
	ESwapRootBone SwapRootBone;

	UFUNCTION()
	MOVIESCENETRACKS_API void SetSwapRootBone(ESwapRootBone InValue);
	UFUNCTION()
	MOVIESCENETRACKS_API ESwapRootBone GetSwapRootBone() const;

#if WITH_EDITORONLY_DATA
public:

	/** Whether to show the position of the root for this sections */
	UPROPERTY(EditAnywhere, Category = "Root Motions")
	bool bShowRootMotionTrail;
#endif

};








