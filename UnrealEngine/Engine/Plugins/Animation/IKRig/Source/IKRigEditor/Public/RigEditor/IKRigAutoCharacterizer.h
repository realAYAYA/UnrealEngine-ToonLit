// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RetargetEditor/IKRetargetEditorController.h"
#include "Rig/IKRigDefinition.h"

enum class EPBIKLimitType : uint8;

enum class EPreferredAxis
{
	None,
	PositiveX,
	NegativeX,
	PositiveY,
	NegativeY,
	PositiveZ,
	NegativeZ,
};

// the central ground truth for standardized characterization labels used in Unreal
struct FCharacterizationStandard
{
	//
	// Standard Biped Chain Labels:
	//
	//	- these are Unreal's standardized retargeting chain labels
	//	- see the note below for rules on how to extend this convention to non-bipedal creatures
	//
	
	// core
	static const FName Root;
	static const FName Spine;
	static const FName Neck;
	static const FName Head;
	static const FName Tail;
	// legs
	static const FName LeftLeg;
	static const FName RightLeg;
	// arms
	static const FName LeftClavicle;
	static const FName RightClavicle;
	static const FName LeftArm;
	static const FName RightArm;
	// left hand
	static const FName LeftThumbMetacarpal;
	static const FName LeftIndexMetacarpal;
	static const FName LeftMiddleMetacarpal;
	static const FName LeftRingMetacarpal;
	static const FName LeftPinkyMetacarpal;
	static const FName LeftThumb;
	static const FName LeftIndex;
	static const FName LeftMiddle;
	static const FName LeftRing;
	static const FName LeftPinky;
	// right hand
	static const FName RightThumbMetacarpal;
	static const FName RightIndexMetacarpal;
	static const FName RightMiddleMetacarpal;
	static const FName RightRingMetacarpal;
	static const FName RightPinkyMetacarpal;
	static const FName RightThumb;
	static const FName RightIndex;
	static const FName RightMiddle;
	static const FName RightRing;
	static const FName RightPinky;
	// left foot
	static const FName LeftBigToe;
	static const FName LeftIndexToe;
	static const FName LeftMiddleToe;
	static const FName LeftRingToe;
	static const FName LeftPinkyToe;
	// right foot
	static const FName RightBigToe;
	static const FName RightIndexToe;
	static const FName RightMiddleToe;
	static const FName RightRingToe;
	static const FName RightPinkyToe;
	// standard bipedal IK goal names
	static const FName LeftHandIKGoal;
	static const FName LeftFootIKGoal;
	static const FName RightHandIKGoal;
	static const FName RightFootIKGoal;
	
	// standard bone settings for IK
	static constexpr float PelvisRotationStiffness = 0.95f;
	static constexpr float ClavicleRotationStiffness = 0.95f;
	static constexpr float FootRotationStiffness = 0.85f;

	//
	// Non-Standard Chain Labeling Convention
	//
	// Creatures with extra limbs should follow this labeling convention.
	// The goal of this convention is to maximize animation compatibility and sharing
	// This convention ensures that even spiders, quadrupeds and bipeds can share animation!
	//
	// 1. Always include Legs and Arms:
	//  - the rearmost set of limbs should be labeled LeftLeg / RightLeg
	//	- the foremost set of limbs should be labeled LeftArm / RightArm
	//	- these limbs correspond with the "main" limbs in a quadruped (legs in the back, arms in the front)
	//	- this ensures that ALL creatures can take bipedal animation (by far the most common)
	//
	// 2. Additional Limbs:
	//	- creatures with more than 4 limbs require additional labels beyond what the biped convention provides
	//	- starting from the rear of the creature add LeftLeg and RightLeg (these limbs will take bipedal leg animation)
	//	- working forwards, add a lettered suffix to each new set of limbs, ie LeftLegA, LeftLegB, LeftLegC, etc...
	//	- always name the foremost limb an arm, ie "LeftArm"
	//
	// 3. Fingers and Toes:
	//	- body parts belonging to an intermediate limb with an alphabetic suffix should include the same suffix.
	//	- ie, if "LeftLegE" has an index finger, it should be "LeftIndexE"
	//
	// 4. Core Body Parts:
	//	- Spine / Neck and Head should be labeled identically to a biped.
	//	- Additional Spine/Neck/Head chains should be indexed alphabetically, ie "HeadA", "HeadB", "HeadC" etc...
	//
	// 5. Extra Stuff:
	//	- no convention can cover all possible permutations of skeletal morphology
	//	- by adhering to this convention, all mammals, birds and insects can share animation
	//	- fantastical creatures with recursive limb topology (ie, arms hanging off legs) will not fit easily into this convention
	//	  but neither is it likely that there is any meaningful library of animation such a creature would benefit from
	//	- in such cases, conventions should be adhered to, to the extent possible, and extra parts can use custom labels
	//
	// Examples:
	//
	// Typical Quadruped Example:
	//	Quadrupeds are treated identically to bipeds!
	//	- Limbs: Legs in back to front order: LeftLeg, LeftArm (and equivalent on right side)
	//	- Core: Spine / Neck / Head / Tail
	//	- Fingers/toes as normal.
	//
	// Typical Spider Example:
	//	Spiders include all biped limbs, with additional legs.
	//	- Legs in back to front order: LeftLeg, LeftLegA, LeftLegB, LeftArm
	//	- Core: Spine / Neck / Head / Tail (for the abdomen)
	//
	
	// enough extra legs to cover arachnids
	// if you need more, just use a hard coded FName according to the convention above
	static const FName LeftLegA;
	static const FName LeftLegB;
	static const FName LeftLegC;
	static const FName RightLegA;
	static const FName RightLegB;
	static const FName RightLegC;
	//
	static const FName LeftFootAIKGoal;
	static const FName LeftFootBIKGoal;
	static const FName LeftFootCIKGoal;
	static const FName RightFootAIKGoal;
	static const FName RightFootBIKGoal;
	static const FName RightFootCIKGoal;
};

// clean names are used for comparison
// full names are used to resolve a retarget definition onto an actual skeleton (which may have prefixes)
enum class ECleanOrFullName
{
	Full,
	Clean
};

// an abstract representation of a skeleton including just their names and hierarchy
struct FAbstractHierarchy
{
	FAbstractHierarchy(USkeletalMesh* InMesh);
	
	FAbstractHierarchy(
		const TArray<FName>& InBones,
		const TArray<int32>& InParentIndices)
		: FullBoneNames(InBones),	ParentIndices(InParentIndices)
	{
		GenerateCleanBoneNames();
	};

	// get index of parent bone
	int32 GetParentIndex(const FName BoneName, const ECleanOrFullName NameType) const;

	// get the index from the name
	int32 GetBoneIndex(const FName BoneName, const ECleanOrFullName NameType) const;

	// get the name of a bone from it's index
	FName GetBoneName(const int32 BoneIndex, const ECleanOrFullName NameType) const;

	// returns true if PotentialChild is within the branch under Parent
	bool IsChildOf(const FName Parent, const FName PotentialChild, const ECleanOrFullName NameType) const;

	// outputs a list of bone names in order from root to leaf that reside between start and end (inclusive)
	void GetBonesInChain(
		const FName Start,
		const FName End,
		const ECleanOrFullName NameType,
		TArray<FName>& OutBonesInChain) const;

	// read-only access to the full names of the bones
	const TArray<FName>& GetBoneNames(const ECleanOrFullName NameType) const;

	// get a list of the immediate children of BoneName
	void GetImmediateChildren(
		const FName& BoneName,
		const ECleanOrFullName NameType,
		TArray<FName>& OutChildren) const;

	// returns an integer score representing the number of bones in Other matching this
	// OutMissingBones will contain the names of all bones in the template that were not found in the input
	// OutBonesWithDifferentParent will contain the names of all bones in the input that have a different parent in the template
	void Compare(
		const FAbstractHierarchy& OtherHierarchy,
		TArray<FName>& OutMissingBones,
		TArray<FName>& OutBonesWithDifferentParent,
		int32& OutNumMatchingBones,
		float& OutPercentOfTemplateMatched) const;

	// return a list of all bones in this hierarchy that are NOT in OtherHierarchy
	void FindBonesNotInOther(
		const FAbstractHierarchy& OtherHierarchy,
		TArray<FName>& OutMissingBones,
		TArray<FName>& OutBonesWithDifferentParent) const;
	
	// return the total number of bones that are also in OtherHierarchy with the same parent
	int32 GetNumMatchingBones(const FAbstractHierarchy& OtherHierarchy) const;

private:
	// strips extra junk off the bone names to make direct comparisons
	void GenerateCleanBoneNames();

	// checks if bone exists and has the same parent in both hierarchies (expects the clean bone name for apples-apples comparison)
	void CheckBoneExistsAndHasSameParent(
		const FName& CleanBoneName,
		const FAbstractHierarchy& OtherHierarchy,
		bool& OutExists,
		bool& OutSameParent) const;

	// returns the largest prefix that is common to ALL names in the input array
	static FString FindLargestCommonPrefix(const TArray<FName>& ArrayOfNames);

	TArray<FName> FullBoneNames;
	TArray<FName> CleanBoneNames;
	TArray<int32> ParentIndices;
};

struct FBoneSettingsForIK
{
	FBoneSettingsForIK(const FName InBoneName) : BoneToApplyTo(InBoneName) {}
	
	FName BoneToApplyTo;
	float RotationStiffness = 0.f;
	EPreferredAxis PreferredAxis = EPreferredAxis::None;
	bool bIsHinge = false;
	bool bExcluded = false;

	FVector GetPreferredAxisAsAngles() const;
	void LockNonPreferredAxes(EPBIKLimitType& OutX, EPBIKLimitType& OutY, EPBIKLimitType& OutZ) const;
	
private:
	static constexpr float PreferredAngleMagnitude = 90.f;
};

struct FAllBoneSettingsForIK
{
	void SetPreferredAxis(const FName BoneName, const EPreferredAxis PreferredAxis, const bool bTreatAsHinge=true);
	void SetRotationStiffness(const FName BoneName, const float RotationStiffness);
	void SetExcluded(const FName BoneName, const bool bExclude);
	const TArray<FBoneSettingsForIK>& GetBoneSettings() const { return AllBoneSettings; };

private:
	FBoneSettingsForIK& GetOrAddBoneSettings(const FName BoneName);
	TArray<FBoneSettingsForIK> AllBoneSettings;
};

struct FBoneToPin
{
	FBoneToPin( const FName InBoneToPin, const FName InBoneToPinTo, const ERetargetSourceOrTarget InSkeletonToPinTo) :
		BoneToPin(InBoneToPin),
		BoneToPinTo(InBoneToPinTo),
		SkeletonToPinTo(InSkeletonToPinTo) {}

	FName BoneToPin;
	FName BoneToPinTo;
	ERetargetSourceOrTarget SkeletonToPinTo;
};

struct FBonesToPin
{
	void AddBoneToPin(
		const FName BoneToPin,
		const FName BoneToPinTo,
		ERetargetSourceOrTarget SkeletonToPinTo = ERetargetSourceOrTarget::Target)
	{
		AllBonesToPin.Emplace(BoneToPin,BoneToPinTo, SkeletonToPinTo);
	}

	const TArray<FBoneToPin>& GetBonesToPin() const { return AllBonesToPin; };

private:
	TArray<FBoneToPin> AllBonesToPin;
};

struct FAutoRetargetDefinition
{
	FRetargetDefinition RetargetDefinition;
	FAllBoneSettingsForIK BoneSettingsForIK;
	FBonesToPin BonesToPin;
	TArray<FName> BonesToExcludeFromAutoPose;
};

// the results of auto characterizing an input skeletal mesh
struct FAutoCharacterizeResults
{
	// the retarget root and a list of bone chains
	FAutoRetargetDefinition AutoRetargetDefinition;
	// did the auto characterizer use a template or procedurally generate the retarget definition?
	bool bUsedTemplate = false;
	// the template that most closely matched with the input skeleton
	FName BestTemplateName = NAME_None;
	// the number of bones that matched
	int32 BestNumMatchingBones = 0;
	// the score of how closely the template matched the input skeleton (0-1)
	float BestPercentageOfTemplateScore = 0.0f;
	// bones that were in the template, but not found in the input
	TArray<FName> MissingBones;
	// bones that do not have the same parent as the equivalent in the template
	TArray<FName> BonesWithMissingParent;
	// number of bones we extended the spine/neck chains beyond what the template provides
	TMap<FName,int32> ExpandedChains;
};

// a hard coded template representing a "known" hierarchy that is used in the world (ie UE5 Mannequin, Fortnite skeleton etc..)
// contains the recommended retarget definition to use for this template, including the retarget root, retarget chains and bone settings
struct FTemplateHierarchy
{
	FTemplateHierarchy(const FName& InName, const FAbstractHierarchy& InHierarchy) : Name(InName), Hierarchy(InHierarchy){}

	FName Name;
	FAbstractHierarchy Hierarchy;
	FAutoRetargetDefinition AutoRetargetDefinition;
};

// a collection of FTemplateHierarchy to compare against
struct FKnownTemplateHierarchies
{
	FKnownTemplateHierarchies();

	// iterate over all the known hierarchies, find the one that is closest to the given hierarchy
	// outputs the FAutoCharacterizeResults which includes a retarget definition to use
	void GetClosestMatchingKnownHierarchy(const FAbstractHierarchy& InHierarchy, FAutoCharacterizeResults& Results) const;

	// get a pointer to a template hierarchy by name
	const FTemplateHierarchy* GetKnownHierarchyByName(const FName Name) const;

private:

	// add a template hierarchy
	FTemplateHierarchy& AddTemplateHierarchy(const FName& Label, const TArray<FName>& BoneNames, const TArray<int32>& ParentIndices);
	
	TArray<FTemplateHierarchy> KnownHierarchies;
};

// contains all the known hierarchies in popular usage, and provides a function to compare any given Skeletal Mesh against them
// if a template is matched to the input skeleton, then the retarget definition is adapted to the given skeleton
struct FAutoCharacterizer
{
	FAutoCharacterizer() = default;

	// call this function with any skeletal mesh to auto-generate a retarget definition for it.
	// the results includes the retarget definition itself, as well as the scores indicating how closely the skeleton was matched to a known hierarchy
	void GenerateRetargetDefinitionFromMesh(USkeletalMesh* Mesh, FAutoCharacterizeResults& Results) const;

	// get read-only access to a template by name
	const FTemplateHierarchy* GetKnownTemplateHierarchy(const FName& TemplateName) const;

private:

	// adapts the retarget definition of a template to apply to a different hierarchy
	// may require removing chains that are not valid on the target hierarchy and changing start/end bones to better match it
	void AdaptTemplateToHierarchy(
		const FTemplateHierarchy& Template,
		const FAbstractHierarchy& TargetHierarchy,
		FAutoCharacterizeResults& Results) const;

	// expand the given bone chain to include bones beyond the end bone specified by the template (useful for Spines and Necks)
	// returns the number of bones that the chain was expanded by
	int32 ExpandChain(FBoneChain* ChainToExpand, const FAbstractHierarchy& Hierarchy) const;

	// outputs the closest name to NameToMatch in NamesToCheck. OutBestScore ranges from 0-1 as percentage of string that matched NameToMatch.
	void FindClosestNameInArray(
		const FName& NameToMatch,
		const TArray<FName>& NamesToCheck,
		FName& OutClosestName,
		float& OutBestScore) const;

	// all the known templates to compare against
	FKnownTemplateHierarchies KnownHierarchies;
};
