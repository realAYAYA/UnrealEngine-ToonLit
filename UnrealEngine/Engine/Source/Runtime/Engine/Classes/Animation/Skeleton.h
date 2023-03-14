// Copyright Epic Games, Inc. All Rights Reserved.

/** 
 * This is the definition for a skeleton, used to animate USkeletalMesh
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "HAL/CriticalSection.h"
#include "Misc/Guid.h"
#include "ReferenceSkeleton.h"
#include "Animation/PreviewAssetAttachComponent.h"
#include "Animation/SmartName.h"
#include "Engine/AssetUserData.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/CriticalSection.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "Interfaces/Interface_PreviewMeshProvider.h"
#include "Delegates/DelegateCombinations.h"
#include "Skeleton.generated.h"

class UAnimSequence;
class UBlendProfile;
class URig;
class USkeletalMeshSocket;
class FPackageReloadedEvent;
struct FAssetData;
enum class EPackageReloadPhase : uint8;

/** This is a mapping table between bone in a particular skeletal mesh and bone of this skeleton set. */
USTRUCT()
struct FSkeletonToMeshLinkup
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * Mapping table. Size must be same as size of bone tree (not Mesh Ref Pose). 
	 * No index should be more than the number of bones in this skeleton
	 * -1 indicates no match for this bone - will be ignored.
	 */
	UPROPERTY()
	TArray<int32> SkeletonToMeshTable;

	/** 
	 * Mapping table. Size must be same as size of ref pose (not bone tree). 
	 * No index should be more than the number of bones in this skeletalmesh
	 * -1 indicates no match for this bone - will be ignored.
	 */
	UPROPERTY()
	TArray<int32> MeshToSkeletonTable;

};

/** Bone translation retargeting mode. */
UENUM()
namespace EBoneTranslationRetargetingMode
{
	enum Type
	{
		/** Use translation from animation data. */
		Animation,
		/** Use fixed translation from Skeleton. */
		Skeleton,
		/** Use Translation from animation, but scale length by Skeleton's proportions. */
		AnimationScaled,
		/** Use Translation from animation, but also play the difference from retargeting pose as an additive. */
		AnimationRelative,
		/** Apply delta orientation and scale from ref pose */
		OrientAndScale,
	};
}

/** Max error allowed when considering bone translations for 'Orient And Scale' retargeting. */
#define BONE_TRANS_RT_ORIENT_AND_SCALE_PRECISION (0.001f) 

/** Each Bone node in BoneTree */
USTRUCT()
struct FBoneNode
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone, this is the search criteria to match with mesh bone. This will be NAME_None if deleted. */
	UPROPERTY()
	FName Name_DEPRECATED;

	/** Parent Index. -1 if not used. The root has 0 as its parent. Do not delete the element but set this to -1. If it is revived by other reason, fix up this link. */
	UPROPERTY()
	int32 ParentIndex_DEPRECATED;

	/** Retargeting Mode for Translation Component. */
	UPROPERTY(EditAnywhere, Category=BoneNode)
	TEnumAsByte<EBoneTranslationRetargetingMode::Type> TranslationRetargetingMode;

	FBoneNode()
		: ParentIndex_DEPRECATED(INDEX_NONE)
		, TranslationRetargetingMode(EBoneTranslationRetargetingMode::Animation)
	{
	}

	FBoneNode(FName InBoneName, int32 InParentIndex)
		: Name_DEPRECATED(InBoneName)
		, ParentIndex_DEPRECATED(InParentIndex)
		, TranslationRetargetingMode(EBoneTranslationRetargetingMode::Animation)
	{
	}
};

/** This is a mapping table between bone in a particular skeletal mesh and bone of this skeleton set. */
USTRUCT()
struct FReferencePose
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName	PoseName;

	UPROPERTY()
	TArray<FTransform>	ReferencePose;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TSoftObjectPtr<USkeletalMesh> SourceReferenceMesh;
#endif

	/**
	 * Serializes the bones
	 *
	 * @param Ar - The archive to serialize into.
	 * @param P - The FReferencePose to serialize
	 * @param Outer - The object containing this instance. Used to determine if we're loading cooked data.
	 */
	friend void SerializeReferencePose(FArchive& Ar, FReferencePose& P, UObject* Outer);
};

USTRUCT()
struct FBoneReductionSetting
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FName> BonesToRemove;

	bool Add(FName BoneName)
	{
		if ( BoneName!=NAME_None && !BonesToRemove.Contains(BoneName) )
		{
			BonesToRemove.Add(BoneName);
			return true;
		}

		return false;
	}

	void Remove(FName BoneName)
	{
		BonesToRemove.Remove(BoneName);
	}

	bool Contains(FName BoneName)
	{
		return (BonesToRemove.Contains(BoneName));
	}
};

USTRUCT()
struct FNameMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName NodeName;

	UPROPERTY()
	FName BoneName;

	FNameMapping()
		: NodeName(NAME_None)
		, BoneName(NAME_None)
	{
	}

	FNameMapping(FName InNodeName)
		: NodeName(InNodeName)
		, BoneName(NAME_None)
	{
	}

	FNameMapping(FName InNodeName, FName InBoneName)
		: NodeName(InNodeName)
		, BoneName(InBoneName)
	{
	}
};

USTRUCT()
struct FRigConfiguration
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TObjectPtr<class URig>  Rig = nullptr;

	// @todo in the future we can make this to be run-time data
	UPROPERTY()
	TArray<FNameMapping> BoneMappingTable;
};

USTRUCT()
struct FAnimSlotGroup
{
	GENERATED_USTRUCT_BODY()

public:
	static ENGINE_API const FName DefaultGroupName;
	static ENGINE_API const FName DefaultSlotName;

	UPROPERTY()
	FName GroupName;

	UPROPERTY()
	TArray<FName> SlotNames;

	FAnimSlotGroup()
		: GroupName(DefaultGroupName)
	{
	}

	FAnimSlotGroup(FName InGroupName)
		: GroupName(InGroupName)
	{
	}
};

namespace VirtualBoneNameHelpers
{
	extern ENGINE_API const FString VirtualBonePrefix;

	ENGINE_API FString AddVirtualBonePrefix(const FString& InName);
	ENGINE_API FName RemoveVirtualBonePrefix(const FString& InName);
}

USTRUCT()
struct FVirtualBone
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	FName SourceBoneName;

	UPROPERTY()
	FName TargetBoneName;

	UPROPERTY()
	FName VirtualBoneName;

	FVirtualBone() {}

	FVirtualBone(FName InSource, FName InTarget)
		: SourceBoneName(InSource)
		, TargetBoneName(InTarget)
	{
		FString VBNameString = VirtualBoneNameHelpers::AddVirtualBonePrefix(SourceBoneName.ToString() + TEXT("_") + TargetBoneName.ToString());
		VirtualBoneName = FName(*VBNameString);
	}
};

struct FSkeletonRemapping
{
	FSkeletonRemapping() = default;
	FSkeletonRemapping(const USkeleton* InSourceSkeleton, const USkeleton* InTargetSkeleton);

	const TWeakObjectPtr<const USkeleton>& GetSourceSkeleton() const { return SourceSkeleton; }
	const TWeakObjectPtr<const USkeleton>& GetTargetSkeleton() const { return TargetSkeleton; }

	/**
	 * Compose this remapping with another remapping in place.  The other remapping's source skeleton must match
	 * this remapping's target skeleton.  The result will map from this remapping's source skeleton to the other
	 * remapping's target skeleton
	 *
	 * @param	OtherSkeletonRemapping		Skeleton remapping to compose into this remapping
	 */
	void ComposeWith(const FSkeletonRemapping& OtherSkeletonRemapping);

	/**
	 * Get the target skeleton bone index that corresponds to the specified bone on the source skeleton
	 *
	 * @param	SourceSkeletonBoneIndex		Skeleton bone index on the source skeleton
	 * @return								Skeleton bone index on the target skeleton (or INDEX_NONE)
	 */
	int32 GetTargetSkeletonBoneIndex(int32 SourceSkeletonBoneIndex) const
	{
		return (SourceToTargetBoneIndexes.IsValidIndex(SourceSkeletonBoneIndex))
				   ? SourceToTargetBoneIndexes[SourceSkeletonBoneIndex]
				   : INDEX_NONE;
	}

	/**
	 * Get the source skeleton bone index that corresponds to the specified bone on the target skeleton
	 *
	 * @param	TargetSkeletonBoneIndex		Skeleton bone index on the target skeleton
	 * @return								Skeleton bone index on the source skeleton (or INDEX_NONE)
	 */
	int32 GetSourceSkeletonBoneIndex(int32 TargetSkeletonBoneIndex) const
	{
		return (TargetToSourceBoneIndexes.IsValidIndex(TargetSkeletonBoneIndex))
				   ? TargetToSourceBoneIndexes[TargetSkeletonBoneIndex]
				   : INDEX_NONE;
	}

	/**
	 * Get the specified bone transform retargeted from the source skeleton onto the target skeleton, corrected
	 * for differences between source and target rest poses
	 *
	 * @param	TargetSkeletonBoneIndex		Skeleton bone index on the target skeleton
	 * @param	SourceTransform				Bone transform from the corresponding bone on the source skeleton
	 * @return								Transform mapped onto the target skeleton
	 */
	FTransform RetargetBoneTransformToTargetSkeleton(int32 TargetSkeletonBoneIndex, const FTransform& SourceTransform) const
	{
		return FTransform(
			RetargetBoneRotationToTargetSkeleton(TargetSkeletonBoneIndex, SourceTransform.GetRotation()),
			RetargetBoneTranslationToTargetSkeleton(TargetSkeletonBoneIndex, SourceTransform.GetTranslation()),
			SourceTransform.GetScale3D());
	}

	/**
	 * Get the specified bone translation retargeted from the source skeleton onto the target skeleton, corrected
	 * for differences between source and target rest poses
	 *
	 * @param	TargetSkeletonBoneIndex		Skeleton bone index on the target skeleton
	 * @param	SourceTranslation			Bone translation from the corresponding bone on the source skeleton
	 * @return								Translation mapped onto the target skeleton
	 */
	FVector RetargetBoneTranslationToTargetSkeleton(int32 TargetSkeletonBoneIndex, const FVector& SourceTranslation) const
	{
		// Compute the translation part of FTransform(Q1) * Source * FTransform(Q0)
		const TTuple<FQuat, FQuat>& QQ = RetargetingTable[TargetSkeletonBoneIndex];
		return QQ.Get<0>().RotateVector(SourceTranslation);
	}

	/**
	 * Get the specified bone rotation retargeted from the source skeleton onto the target skeleton, corrected
	 * for differences between source and target rest poses
	 *
	 * @param	TargetSkeletonBoneIndex		Skeleton bone index on the target skeleton
	 * @param	SourceRotation				Bone rotation from the corresponding bone on the source skeleton
	 * @return								Rotation mapped onto the target skeleton
	 */
	FQuat RetargetBoneRotationToTargetSkeleton(int32 TargetSkeletonBoneIndex, const FQuat& SourceRotation) const
	{
		// Compute the rotation part of FTransform(Q1) * Source * FTransform(Q0)
		const TTuple<FQuat, FQuat>& QQ = RetargetingTable[TargetSkeletonBoneIndex];
		return QQ.Get<0>() * SourceRotation * QQ.Get<1>();
	}

	/**
	 * Get the specified additive transform retargeted from the source skeleton onto the target skeleton, corrected
	 * for differences between source and target rest poses
	 *
	 * @param	TargetSkeletonBoneIndex		Skeleton bone index on the target skeleton
	 * @param	SourceTransform				Bone transform from the corresponding bone on the source skeleton
	 * @return								Transform mapped onto the target skeleton
	 */
	FTransform RetargetAdditiveTransformToTargetSkeleton(int32 TargetSkeletonBoneIndex, const FTransform& SourceTransform) const
	{
		return FTransform(
			RetargetAdditiveRotationToTargetSkeleton(TargetSkeletonBoneIndex, SourceTransform.GetRotation()),
			RetargetAdditiveTranslationToTargetSkeleton(TargetSkeletonBoneIndex, SourceTransform.GetTranslation()),
			SourceTransform.GetScale3D());
	}

	/**
	 * Get the specified additive translation retargeted from the source skeleton onto the target skeleton, corrected
	 * for differences between source and target rest poses
	 *
	 * @param	TargetSkeletonBoneIndex		Skeleton bone index on the target skeleton
	 * @param	SourceTranslation			Bone translation from the corresponding bone on the source skeleton
	 * @return								Translation mapped onto the target skeleton
	 */
	FVector RetargetAdditiveTranslationToTargetSkeleton(int32 TargetSkeletonBoneIndex, const FVector& SourceTranslation) const
	{
		// Compute the translation part of FTransform(Q0.Inverse) * Source * FTransform(Q0)
		const TTuple<FQuat, FQuat>& QQ = RetargetingTable[TargetSkeletonBoneIndex];
		return QQ.Get<0>().RotateVector(SourceTranslation);
	}

	/**
	 * Get the specified additive rotation retargeted from the source skeleton onto the target skeleton, corrected
	 * for differences between source and target rest poses
	 *
	 * @param	TargetSkeletonBoneIndex		Skeleton bone index on the target skeleton
	 * @param	SourceRotation				Bone rotation from the corresponding bone on the source skeleton
	 * @return								Rotation mapped onto the target skeleton
	 */
	FQuat RetargetAdditiveRotationToTargetSkeleton(int32 TargetSkeletonBoneIndex, const FQuat& SourceRotation) const
	{
		// Compute the rotation part of FTransform(Q0.Inverse) * Source * FTransform(Q0)
		const TTuple<FQuat, FQuat>& QQ = RetargetingTable[TargetSkeletonBoneIndex];
		return QQ.Get<0>() * SourceRotation * QQ.Get<0>().Inverse();
	}

	/**
	 * Get the curve mapping array, which maps from the source curve UID to the target curve UID.
	 * You can access this array like this:
	 * 
	 * \code{.cpp}
	 * const SmartName::UID_Type TargetCurveUID = GetSourceToTargetCurveMapping()[SourceCurveUID];
	 * \endcode
	 */
	const TArray<SmartName::UID_Type>& GetSourceToTargetCurveMapping() const { return SourceToTargetCurveMapping; }

	/**
	 * Generates the mapping table for the curves, based on curve names.
	 * Basically this will look at the curve names in the smart name table on the source, and tries to find matching ones in the table of 
	 * the target skeleton. It then maps the UID's.
	 */
	void GenerateCurveMapping();

private:
	TWeakObjectPtr<const USkeleton> SourceSkeleton;
	TWeakObjectPtr<const USkeleton> TargetSkeleton;

	// Table of target skeleton bone indexes (indexed by source skeleton bone index)
	TArray<int32> SourceToTargetBoneIndexes;

	// Table of source skeleton bone indexes (indexed by target skeleton bone index)
	TArray<int32> TargetToSourceBoneIndexes;
	
	// Maps curve UIDs between source and target (indexed by source curve UID).
	TArray<SmartName::UID_Type> SourceToTargetCurveMapping;

	// Table of precalculated constants for retargeting from source to target (indexed by target skeleton bone index)
	TArray<TTuple<FQuat, FQuat>> RetargetingTable;
};

/**
 *	USkeleton : that links between mesh and animation
 *		- Bone hierarchy for animations
 *		- Bone/track linkup between mesh and animation
 *		- Retargetting related
 */
UCLASS(hidecategories=Object, MinimalAPI, BlueprintType)
class USkeleton : public UObject, public IInterface_AssetUserData, public IInterface_PreviewMeshProvider
{
	friend class UAnimationBlueprintLibrary;
	friend class FSkeletonDetails;

	GENERATED_UCLASS_BODY()

protected:
	/** Skeleton bone tree - each contains name and parent index**/
	UPROPERTY(VisibleAnywhere, Category=Skeleton)
	TArray<struct FBoneNode> BoneTree;

	/** Reference skeleton poses in local space */
	UPROPERTY()
	TArray<FTransform> RefLocalPoses_DEPRECATED;

	/** Reference Skeleton */
	FReferenceSkeleton ReferenceSkeleton;

	/** Guid for skeleton */
	FGuid Guid;

	/** Guid for virtual bones.
	 *  Separate so that we don't have to dirty the original guid when only changing virtual bones */
	UPROPERTY()
	FGuid VirtualBoneGuid;

	/** Conversion function. Remove when VER_UE4_REFERENCE_SKELETON_REFACTOR is removed. */
	void ConvertToFReferenceSkeleton();

	/**
	*  Array of this skeletons virtual bones. These are new bones are links between two existing bones
	*  and are baked into all the skeletons animations
	*/
	UPROPERTY()
	TArray<FVirtualBone> VirtualBones;

	/**
	 * The list of compatible skeletons.
	 * This is an array of TSoftObjectPtr in order to prevent all skeletons to be loaded, as we only want to load things on demand.
	 * As this is EditAnywhere and an array of TSoftObjectPtr, checking validity of pointers is needed.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CompatibleSkeletons)
	TArray<TSoftObjectPtr<USkeleton>> CompatibleSkeletons;

public:
	//~ Begin UObject Interface.
#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
	ENGINE_API virtual void PreEditUndo() override;
	ENGINE_API virtual void PostEditUndo() override;
#endif
	ENGINE_API virtual void BeginDestroy() override;

	/** Accessor to Reference Skeleton to make data read only */
	const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return ReferenceSkeleton;
	}

	/** Accessor for the array of virtual bones on this skeleton */
	const TArray<FVirtualBone>& GetVirtualBones() const { return VirtualBones; }

	/** 
	 *	Array of named socket locations, set up in editor and used as a shortcut instead of specifying 
	 *	everything explicitly to AttachComponent in the SkeletalMeshComponent.
	 */
	UPROPERTY()
	TArray<TObjectPtr<class USkeletalMeshSocket>> Sockets;

	/** Serializable retarget sources for this skeleton **/
	TMap< FName, FReferencePose > AnimRetargetSources;

	// Typedefs for greater smartname UID readability, add one for each smartname category 
	typedef SmartName::UID_Type AnimCurveUID;

	// Names for smartname mappings, if you're adding a new category of smartnames add a new name here
	static ENGINE_API const FName AnimCurveMappingName;

	// Names for smartname mappings, if you're adding a new category of smartnames add a new name here
	static ENGINE_API const FName AnimTrackCurveMappingName;

	// these return container of curve meta data, if you modify this container, 
	// you'll have to call REfreshCAchedAnimationCurveData to apply
	ENGINE_API FCurveMetaData* GetCurveMetaData(const FName& CurveName);
	ENGINE_API const FCurveMetaData* GetCurveMetaData(const FName& CurveName) const;
	ENGINE_API const FCurveMetaData* GetCurveMetaData(const SmartName::UID_Type CurveUID) const;
	ENGINE_API FCurveMetaData* GetCurveMetaData(const FSmartName& CurveName);
	ENGINE_API const FCurveMetaData* GetCurveMetaData(const FSmartName& CurveName) const;
	// this is called when you know both flags - called by post serialize
	ENGINE_API void AccumulateCurveMetaData(FName CurveName, bool bMaterialSet, bool bMorphtargetSet);

	ENGINE_API bool AddNewVirtualBone(const FName SourceBoneName, const FName TargetBoneName);

	ENGINE_API bool AddNewVirtualBone(const FName SourceBoneName, const FName TargetBoneName, FName& NewVirtualBoneName);

	ENGINE_API void RemoveVirtualBones(const TArray<FName>& BonesToRemove);

	ENGINE_API void RenameVirtualBone(const FName OriginalBoneName, const FName NewBoneName);
	
	void HandleVirtualBoneChanges();

	// return version of AnimCurveUidVersion
	uint16 GetAnimCurveUidVersion() const { return AnimCurveUidVersion;  }
	const TArray<uint16>& GetDefaultCurveUIDList() const { return DefaultCurveUIDList; }

	ENGINE_API const TArray<TSoftObjectPtr<USkeleton>>& GetCompatibleSkeletons() const { return CompatibleSkeletons; }
	ENGINE_API const FSkeletonRemapping* GetSkeletonRemapping(const USkeleton* SourceSkeleton) const;

#if WITH_EDITOR
	// Get existing (seen) sync marker names for this Skeleton
	const TArray<FName>& GetExistingMarkerNames() const { return ExistingMarkerNames; }

	// Register a new sync marker name
	void RegisterMarkerName(FName MarkerName) { ExistingMarkerNames.AddUnique(MarkerName); ExistingMarkerNames.Sort(FNameLexicalLess()); }

	// Remove a sync marker name
	void RemoveMarkerName(FName MarkerName) { ExistingMarkerNames.Remove(MarkerName); }
#endif

protected:
	// Container for smart name mappings
	UPROPERTY()
	FSmartNameContainer SmartNames;

	// Cached ptr to the persistent AnimCurveMapping
	FSmartNameMapping* AnimCurveMapping;

	// this is default curve uid list used like ref pose, as default value
	// don't use this unless you want all curves from the skeleton
	// FBoneContainer contains only list that is used by current LOD
	TArray<uint16> DefaultCurveUIDList;

	//Cached marker sync marker names (stripped for non editor)
	TArray<FName> ExistingMarkerNames;

private:
	/** Increase the AnimCurveUidVersion so that instances can get the latest information */
	void IncreaseAnimCurveUidVersion();
	/** Current  Anim Curve Uid Version. Increase whenever it has to be recalculated */
	uint16 AnimCurveUidVersion;

public:
	//////////////////////////////////////////////////////////////////////////
	// Blend Profiles

	/** List of blend profiles available in this skeleton */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UBlendProfile>> BlendProfiles;

	/** Get the specified blend profile by name */
	UFUNCTION(BlueprintPure, Category = Skeleton)
	ENGINE_API UBlendProfile* GetBlendProfile(const FName& InProfileName);

	/** Create a new blend profile with the specified name */
	ENGINE_API UBlendProfile* CreateNewBlendProfile(const FName& InProfileName);

	//////////////////////////////////////////////////////////////////////////

	/************************************************************************/
	/* Slot Groups */
	/************************************************************************/
private:
	// serialized slot groups and slot names.
	UPROPERTY()
	TArray<FAnimSlotGroup> SlotGroups;

	/** SlotName to GroupName TMap, only at runtime, not serialized. **/
	TMap<FName, FName> SlotToGroupNameMap;

	void BuildSlotToGroupMap(bool bInRemoveDuplicates = false);

public:
	ENGINE_API FAnimSlotGroup* FindAnimSlotGroup(const FName& InGroupName);
	ENGINE_API const FAnimSlotGroup* FindAnimSlotGroup(const FName& InGroupName) const;
	ENGINE_API const TArray<FAnimSlotGroup>& GetSlotGroups() const;
	ENGINE_API bool ContainsSlotName(const FName& InSlotName) const;

	/** Register a slot name. Return true if a slot was registered, false if it was already registered. */
	ENGINE_API bool RegisterSlotNode(const FName& InSlotName);

	ENGINE_API void SetSlotGroupName(const FName& InSlotName, const FName& InGroupName);
	/** Returns true if Group is added, false if it already exists */
	ENGINE_API bool AddSlotGroupName(const FName& InNewGroupName);
	ENGINE_API FName GetSlotGroupName(const FName& InSlotName) const;

	// Edits/removes slot group data
	// WARNING: Does not verify that the names aren't used anywhere - if it isn't checked
	// by the caller the names will be recreated when referencing assets load again.
	ENGINE_API void RemoveSlotName(const FName& InSlotName);
	ENGINE_API void RemoveSlotGroup(const FName& InSlotName);
	ENGINE_API void RenameSlotName(const FName& OldName, const FName& NewName);

	////////////////////////////////////////////////////////////////////////////
	// Smart Name Interfaces
	////////////////////////////////////////////////////////////////////////////
	// Adds a new name to the smart name container and modifies the skeleton so it can be saved
	// return bool - Whether a name was added (false if already present)
#if WITH_EDITOR
	ENGINE_API bool AddSmartNameAndModify(FName ContainerName, FName NewDisplayName, FSmartName& NewName);

	// Renames a smartname in the specified container and modifies the skeleton
	// return bool - Whether the rename was sucessful
	ENGINE_API bool RenameSmartnameAndModify(FName ContainerName, SmartName::UID_Type Uid, FName NewName);

	// Removes a smartname from the specified container and modifies the skeleton
	ENGINE_API void RemoveSmartnameAndModify(FName ContainerName, SmartName::UID_Type Uid);

	// Removes smartnames from the specified container and modifies the skeleton
	ENGINE_API void RemoveSmartnamesAndModify(FName ContainerName, const TArray<FName>& Names);
#endif// WITH_EDITOR

	// quick wrapper function for Find UID by name, if not found, it will return SmartName::MaxUID
	ENGINE_API SmartName::UID_Type GetUIDByName(const FName& ContainerName, const FName& Name) const;
	ENGINE_API bool GetSmartNameByUID(const FName& ContainerName, SmartName::UID_Type UID, FSmartName& OutSmartName) const;
	ENGINE_API bool GetSmartNameByName(const FName& ContainerName, const FName& InName, FSmartName& OutSmartName) const;

	// Get or add a smartname container with the given name
	ENGINE_API const FSmartNameMapping* GetSmartNameContainer(const FName& ContainerName) const;

	// make sure the smart name has valid UID and so on
	ENGINE_API void VerifySmartName(const FName&  ContainerName, FSmartName& InOutSmartName);
	ENGINE_API void VerifySmartNames(const FName&  ContainerName, TArray<FSmartName>& InOutSmartNames);
private:
	// Get or add a smartname container with the given name
	FSmartNameMapping* GetOrAddSmartNameContainer(const FName& ContainerName);
	bool VerifySmartNameInternal(const FName&  ContainerName, FSmartName& InOutSmartName);
	bool FillSmartNameByDisplayName(FSmartNameMapping* Mapping, const FName& DisplayName, FSmartName& OutSmartName);
#if WITH_EDITORONLY_DATA
private:
	/** The default skeletal mesh to use when previewing this skeleton */
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TSoftObjectPtr<class USkeletalMesh> PreviewSkeletalMesh;

	/** The additional skeletal meshes to use when previewing this skeleton */
	UPROPERTY(duplicatetransient, AssetRegistrySearchable)
	TSoftObjectPtr<class UDataAsset> AdditionalPreviewSkeletalMeshes;

	UPROPERTY()
	FRigConfiguration RigConfig;

	/** rig property will be saved separately */
	ENGINE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

public:

	/** AnimNotifiers that has been created. Right now there is no delete step for this, but in the future we'll supply delete**/
	UPROPERTY()
	TArray<FName> AnimationNotifies;

	/* Attached assets component for this skeleton */
	UPROPERTY()
	FPreviewAssetAttachContainer PreviewAttachedAssetContainer;
#endif // WITH_EDITORONLY_DATA

private:
	DECLARE_MULTICAST_DELEGATE( FOnRetargetSourceChangedMulticaster )
	FOnRetargetSourceChangedMulticaster OnRetargetSourceChanged;

public:
	typedef FOnRetargetSourceChangedMulticaster::FDelegate FOnRetargetSourceChanged;

	/** Registers a delegate to be called after the preview animation has been changed */
	FDelegateHandle RegisterOnRetargetSourceChanged(const FOnRetargetSourceChanged& Delegate)
	{
		return OnRetargetSourceChanged.Add(Delegate);
	}

	const FGuid GetGuid() const
	{
		return Guid;
	}

	FGuid GetVirtualBoneGuid() const
	{
		return VirtualBoneGuid;
	}

	/** Unregisters a delegate to be called after the preview animation has been changed */
	void UnregisterOnRetargetSourceChanged(FDelegateHandle Handle)
	{
		OnRetargetSourceChanged.Remove(Handle);
	}

	void CallbackRetargetSourceChanged()
	{
		OnRetargetSourceChanged.Broadcast();
	}

	typedef TArray<FBoneNode> FBoneTreeType;

	//Use this Lock everytime you change or access LinkupCache and SkelMesh2LinkupCache member.
	FCriticalSection LinkupCacheLock;

	/** Non-serialised cache of linkups between different skeletal meshes and this Skeleton. */
	TArray<struct FSkeletonToMeshLinkup> LinkupCache;

private:
	/** Runtime built mapping table between SkinnedAssets, and LinkupCache array indices. */
	TMap<TWeakObjectPtr<USkinnedAsset>, int32> SkinnedAsset2LinkupCache;
public:
	UE_DEPRECATED(5.1, "Public access to this member variable is deprecated.")
	TMap<TWeakObjectPtr<USkeletalMesh>, int32> SkelMesh2LinkupCache;

	/** A cached soft object pointer of this skeleton. This is done for performance reasons when searching for compatible skeletons when using IsCompatible(Skeleton). */
	TSoftObjectPtr<USkeleton> CachedSoftObjectPtr;

	/** IInterface_PreviewMeshProvider interface */
	virtual USkeletalMesh* GetPreviewMesh(bool bFindIfNotSet = false) override;
	virtual USkeletalMesh* GetPreviewMesh() const override;
	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty=true);

#if WITH_EDITORONLY_DATA

	/*
	 * Collect animation notifies that are referenced in all animations that use this skeleton (uses the asset registry).
	 * Updates the cached AnimationNotifies array.
	 */
	ENGINE_API void CollectAnimationNotifies();

	/*
	 * Collect animation notifies that are referenced in all animations that use this skeleton (uses the asset registry).
	 * @param	OutNotifies		All the notifies that were found
	 */
	ENGINE_API void CollectAnimationNotifies(TArray<FName>& OutNotifies) const;

	// Adds a new anim notify to the cached AnimationNotifies array.
	ENGINE_API void AddNewAnimationNotify(FName NewAnimNotifyName);

	ENGINE_API USkeletalMesh* GetAssetPreviewMesh(UObject* InAsset);

	/** Find the first compatible mesh for this skeleton */
	ENGINE_API USkeletalMesh* FindCompatibleMesh() const;

	/** Load any additional meshes we may have */
	ENGINE_API void LoadAdditionalPreviewSkeletalMeshes();

	/** Get the additional skeletal meshes we use when previewing this skeleton */
	ENGINE_API UDataAsset* GetAdditionalPreviewSkeletalMeshes() const;

	/** Set the additional skeletal meshes we use when previewing this skeleton */
	ENGINE_API void SetAdditionalPreviewSkeletalMeshes(UDataAsset* InPreviewCollectionAsset);

	/**
	 * Makes sure all attached objects are valid and removes any that aren't.
	 *
	 * @return		NumberOfBrokenAssets
	 */
	ENGINE_API int32 ValidatePreviewAttachedObjects();

	/**
	 * Get List of Child Bones of the ParentBoneIndex
	 *
	 * @param	Parent Bone Index
	 * @param	(out) List of Direct Children
	 */
	ENGINE_API int32 GetChildBones(int32 ParentBoneIndex, TArray<int32> & Children) const;

#endif
	/**
	 * Check if animation content authored on the supplied skeleton may be played on this skeleton
	 */
	ENGINE_API bool IsCompatible(const USkeleton* InSkeleton) const;

	/**
	 * Check if this skeleton is compatible with a given other asset, if that is a skeleton.
	 */
	ENGINE_API bool IsCompatibleSkeletonByAssetData(const FAssetData& AssetData, const TCHAR* InTag = TEXT("Skeleton")) const;

	/**
	 * Check if this skeleton is compatible with another skeleton asset that is identified by the string returned by AssetData(SkeletonPtr).GetExportTextName().
	 */
	ENGINE_API bool IsCompatibleSkeletonByAssetString(const FString& SkeletonAssetString) const;

	DECLARE_EVENT_OneParam(USkeleton, FSkeletonDestructEvent, const USkeleton*);
	FSkeletonDestructEvent OnSkeletonDestructEvent;

	DECLARE_EVENT(USkeleton, FSmartNamesChangedEvent);
	FSmartNamesChangedEvent OnSmartNamesChangedEvent;

public:
	UFUNCTION(BlueprintCallable, Category=Skeleton)
	ENGINE_API void AddCompatibleSkeleton(const USkeleton* SourceSkeleton);
	ENGINE_API void RemoveCompatibleSkeleton(const USkeleton* SourceSkeleton);
protected:
	mutable FCriticalSection SkeletonRemappingMutex;

	/** The skeleton remappings, which map bones from a source skeleton to this skeleton (the target). */
	TMap<const USkeleton*, FSkeletonRemapping*> SkeletonRemappings;

	/**
 	 * The function that is called when another skeleton is being destructed.
	 * This is used to unregister skeleton remappings that aren't needed anymore.
	 * @param Skeleton The skeleton that is being destructed.
	 */
	void HandleSkeletonDestruct(const USkeleton* Skeleton);

	/**
	 * Handle smart name changes.
	 * This will internally trigger skeleton remappings to be regenerated where needed, for example when editing curves in the animation editor.
	 **/
	void HandleSmartNamesChangedEvent();

	/**
	 * Build or update all skeleton remappings for all loaded skeletons that we are compatible with.
	 * And if bBidirectional is set to true also update all remappings in other skeletons that are compatible with this skeleton.
	 * Keep in mind this only generates mappings to skeletons that are loaded, as otherwise we can't generate the mappings yet, even though the list of compatible skeletons might contain
	 * a larger set of skeletons. We do not load all skeletons upfront.
	 * @param bBidirectional When enabled we update or create all mappings for both skeletons we are compatible with and skeletons that are compatible with ourselves.
	 *        When set to false it only updates or creates mappings to skeletons we are compatible with.
	 */
	void BuildSkeletonRemappings(bool bBidirectional);

	/**
	 * Remove all skeleton remappings stored inside our skeleton.
	 * This frees up memory.
	 */
	void ClearSkeletonRemappings();

	/**
	 * Remove the skeleton remappings for a specific source skeleton.
	 * This only works if the source skeleton is inside the compatible skeleton list of this skeleton.
	 * If a skeleton is passed in that isn't in the compatibility list this method will essentially do nothing.
	 * The skeleton will remain inside the compatibility list, it is just the remapping data that is being removed.
	 * @param SourceSkeleton The source skeleton we are compatible with and want to remove the generated mapping data for.
	 */
	void RemoveSkeletonRemapping(const USkeleton* SourceSkeleton);

	/**
	 * Create or update a skeletal remapping to a given source skeleton.
	 * @param SourceSkeleton The skeleton from which we receive animation data.
	 * @param bRebuildIfExists When set to true this method will delete the old mapping and generate a new one. This is useful when the hierarchy changes for example.
	 */
	void CreateSkeletonRemappingIfNeeded(const USkeleton* SourceSkeleton, bool bRebuildIfExists = false);

	static TArray<USkeleton*> LoadedSkeletons; // The set of skeletons that are currently loaded.
	static FCriticalSection LoadedSkeletonsMutex; // The mutex used for thread safety when registering and removing items from the LoadedSkeletons array.

public:
	/** 
	 * Indexing naming convention
	 * 
	 * Since this code has indexing to very two distinct array but it can be confusing so I am making it consistency for naming
	 * 
	 * First index is SkeletalMesh->RefSkeleton index - I call this RefBoneIndex
	 * Second index is BoneTree index in USkeleton - I call this TreeBoneIndex
	 */

	/**
	 * Verify to see if we can match this skeleton with the provided SkinnedAsset.
	 * 
	 * Returns true 
	 *		- if bone hierarchy matches (at least needs to have matching parent) 
	 *		- and if parent chain matches - meaning if bone tree has A->B->C and if ref pose has A->C, it will fail
	 *		- and if more than 50 % of bones matches
	 *  
	 * @param	InSkinnedAsset	SkinnedAsset to compare the Skeleton against.
	 * @param   bDoParentChainCheck When true (the default) this method also compares if chains match with the parent. 
	 * 
	 * @return				true if animation set can play on supplied SkinnedAsset, false if not.
	 */
	ENGINE_API bool IsCompatibleMesh(const USkinnedAsset* InSkinnedAsset, bool bDoParentChainCheck=true) const;

	/** Clears all cache data **/
	ENGINE_API void ClearCacheData();

	/** 
	 * Find a mesh linkup table (mapping of skeleton bone tree indices to refpose indices) for a particular SkinnedAsset
	 * If one does not already exist, create it now.
	 */
	ENGINE_API int32 GetMeshLinkupIndex(const USkinnedAsset* InSkinnedAsset);

	/** 
	 * Merge Bones (RequiredBones from InSkinnedAsset) to BoneTrees if not exists
	 * 
	 * Note that this bonetree can't ever clear up because doing so will corrupt all animation data that was imported based on this
	 * If nothing exists, it will build new bone tree 
	 * 
	 * @param InSkinnedAsset		: Mesh to build from. 
	 * @param RequiredRefBones		: RequiredBones are subset of list of bones (index to InSkinnedAsset->RefSkeleton)
									Most of cases, you don't like to add all bones to skeleton, so you'll have choice of cull out some
	 * 
	 * @return true if success
	 */
	ENGINE_API bool MergeBonesToBoneTree(const USkinnedAsset* InSkinnedAsset, const TArray<int32> &RequiredRefBones);

	/** 
	 * Merge all Bones to BoneTrees if not exists
	 * 
	 * Note that this bonetree can't ever clear up because doing so will corrupt all animation data that was imported based on this
	 * If nothing exists, it will build new bone tree 
	 * 
	 * @param InSkinnedAsset		: Mesh to build from. 
	 * 
	 * @return true if success
	 */
	ENGINE_API bool MergeAllBonesToBoneTree(const USkinnedAsset* InSkinnedAsset);

	/** 
	 * Merge has failed, then Recreate BoneTree
	 * 
	 * This will invalidate all animations that were linked before, but this is needed 
	 * 
	 * @param InSkinnedAsset		: Mesh to build from. 
	 * 
	 * @return true if success
	 */
	ENGINE_API bool RecreateBoneTree(USkinnedAsset* InSkinnedAsset);

	// @todo document
	const TArray<FTransform>& GetRefLocalPoses( FName RetargetSource = NAME_None ) const 
	{
		if ( RetargetSource != NAME_None ) 
		{
			const FReferencePose* FoundRetargetSource = AnimRetargetSources.Find(RetargetSource);
			if (FoundRetargetSource)
			{
				return FoundRetargetSource->ReferencePose;
			}
		}

		return ReferenceSkeleton.GetRefBonePose();	
	}

#if WITH_EDITORONLY_DATA
	/**
	 * Find a retarget source for a particular mesh.
	 * @param	InSkinnedAsset	The skinned asset mesh to find a source for
	 * @return NAME_None if a retarget source was not found, or a valid name if it was
	 */
	ENGINE_API FName GetRetargetSourceForMesh(USkinnedAsset* InSkinnedAsset) const;
#endif

	/** 
	 * Get Track index of InAnimSeq for the BoneTreeIndex of BoneTree
	 * this is slow, and it's not supposed to be used heavily
	 * @param	InBoneTreeIdx	BoneTree Index
	 * @param	InAnimSeq		Animation Sequence to get track index for 
	 *
	 * @return	Index of Track of Animation Sequence
	 */
	ENGINE_API int32 GetRawAnimationTrackIndex(const int32 InSkeletonBoneIndex, const UAnimSequence* InAnimSeq);

	/** 
	 * Get Bone Tree Index from Reference Bone Index
	 * @param	InSkinnedAsset	SkinnedAsset for the ref bone idx
	 * @param	InRefBoneIdx	Reference Bone Index to look for - index of USkinnedAsset.RefSkeleton
	 * @return	Index of BoneTree Index
	 */
	ENGINE_API int32 GetSkeletonBoneIndexFromMeshBoneIndex(const USkinnedAsset* InSkinnedAsset, const int32 MeshBoneIndex);

	/** 
	 * Get Reference Bone Index from Bone Tree Index
	 * @param	InSkinnedAsset	SkinnedAsset for the ref bone idx
	 * @param	InBoneTreeIdx	Bone Tree Index to look for - index of USkeleton.BoneTree
	 * @return	Index of BoneTree Index
	 */
	ENGINE_API int32 GetMeshBoneIndexFromSkeletonBoneIndex(const USkinnedAsset* InSkinnedAsset, const int32 SkeletonBoneIndex);

	EBoneTranslationRetargetingMode::Type GetBoneTranslationRetargetingMode(const int32 BoneTreeIdx) const
	{
		if (BoneTree.IsValidIndex(BoneTreeIdx))
		{
			return BoneTree[BoneTreeIdx].TranslationRetargetingMode;
		}
		return EBoneTranslationRetargetingMode::Animation;
	}

	/** 
	 * Rebuild Look up between SkelMesh to BoneTree - this should only get called when SkelMesh is re-imported or so, where the mapping may be no longer valid
	 *
	 * @param	InSkinnedAsset	: SkinnedAsset to build look up for
	 */
	ENGINE_API void RebuildLinkup(const USkinnedAsset* InSkinnedAsset);

	/**
	 * Remove Link up cache for the SkelMesh
	 *
	 * @param	InSkinnedAsset	: SkinnedAsset to remove linkup cache for 
	 */
	void RemoveLinkup(const USkinnedAsset* InSkinnedAsset);

	ENGINE_API void SetBoneTranslationRetargetingMode(const int32 BoneIndex, EBoneTranslationRetargetingMode::Type NewRetargetingMode, bool bChildrenToo=false);

	virtual bool IsPostLoadThreadSafe() const override;
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual void PostDuplicate(bool bDuplicateForPIE) override;
	ENGINE_API virtual void PostInitProperties() override;
	ENGINE_API virtual void Serialize(FArchive& Ar) override;

	/** 
	 * Create RefLocalPoses from InSkinnedAsset. Note InSkinnedAsset cannot be null and this function will assert if it is.
	 * 
	 * If bClearAll is false, it will overwrite ref pose of bones that are found in InSkelMesh
	 * If bClearAll is true, it will reset all Reference Poses 
	 * Note that this means it will remove transforms of extra bones that might not be found in this InSkinnedAsset
	 *
	 * @return true if successful. false if InSkinnedAsset wasn't compatible with the bone hierarchy
	 */
	ENGINE_API void UpdateReferencePoseFromMesh(const USkinnedAsset* InSkinnedAsset);

#if WITH_EDITORONLY_DATA
	/**
	 * Update Retarget Source with given name
	 *
	 * @param Name	Name of pose to update
	 */
	ENGINE_API void UpdateRetargetSource( const FName InName );
#endif
protected:
	/** 
	 * Check if Parent Chain Matches between BoneTree, and SkinnedAsset 
	 * Meaning if BoneTree has A->B->C (top to bottom) and if SkinnedAsset has A->C
	 * It will fail since it's missing B
	 * We ensure this chain matches to play animation properly
	 *
	 * @param StartBoneIndex	: BoneTreeIndex to start from in BoneTree 
	 * @param InSkinnedAsset	: InSkinnedAsset to compare
	 *
	 * @return true if matches till root. false if not. 
	 */
	bool DoesParentChainMatch(int32 StartBoneTreeIndex, const USkinnedAsset* InSkinnedAsset) const;

	/**
	 * Build Look up between SkinnedAsset to BoneTree
	 *
	 * @param	InSkinnedAsset	: SkinnedAsset to build look up for
	 * @return	Index of LinkupCache that this SkelMesh is linked to
	 */
	int32 BuildLinkup(const USkinnedAsset* InSkinnedAsset);

#if WITH_EDITORONLY_DATA
	/**
	 * Refresh All Retarget Sources
	 */
	void RefreshAllRetargetSources();
#endif
	/**
	 * Create Reference Skeleton From the given Mesh 
	 * 
	 * @param InSkinnedAsset	SkinnedAsset that this Skeleton is based on
	 * @param RequiredRefBones	List of required bones to create skeleton from
	 *
	 * @return true if successful
	 */
	bool CreateReferenceSkeletonFromMesh(const USkinnedAsset* InSkinnedAsset, const TArray<int32> & RequiredRefBones);

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE( FOnSkeletonHierarchyChangedMulticaster );
	FOnSkeletonHierarchyChangedMulticaster OnSkeletonHierarchyChanged;

	/** Call this when the skeleton has changed to fix dependent assets */
	ENGINE_API void HandleSkeletonHierarchyChange();

public:
	typedef FOnSkeletonHierarchyChangedMulticaster::FDelegate FOnSkeletonHierarchyChanged;

	/** Registers a delegate to be called after notification has changed*/
	ENGINE_API void RegisterOnSkeletonHierarchyChanged(const FOnSkeletonHierarchyChanged& Delegate);
	ENGINE_API void UnregisterOnSkeletonHierarchyChanged(void* Unregister);

	/** Removes the supplied bones from the skeleton */
	ENGINE_API void RemoveBonesFromSkeleton(const TArray<FName>& BonesToRemove, bool bRemoveChildBones);

	// Asset registry information for animation notifies
	ENGINE_API static const FName AnimNotifyTag;
	ENGINE_API static const FString AnimNotifyTagDelimiter;

	// Asset registry information for animation curves
	ENGINE_API static const FName CurveNameTag;
	ENGINE_API static const FString CurveTagDelimiter;

	// rig Configs
	ENGINE_API static const FName RigTag;
	ENGINE_API void SetRigConfig(URig * Rig);
	ENGINE_API FName GetRigBoneMapping(const FName& NodeName) const;
	ENGINE_API bool SetRigBoneMapping(const FName& NodeName, FName BoneName);
	ENGINE_API FName GetRigNodeNameFromBoneName(const FName& BoneName) const;
	// this make sure it stays within the valid range
	ENGINE_API int32 GetMappedValidNodes(TArray<FName> &OutValidNodeNames);
	// verify if it has all latest data
	ENGINE_API void RefreshRigConfig();
	int32 FindRigBoneMapping(const FName& NodeName) const;
	ENGINE_API URig * GetRig() const;

#endif

public:
	ENGINE_API USkeletalMeshSocket* FindSocketAndIndex(FName InSocketName, int32& OutIndex) const;
	ENGINE_API USkeletalMeshSocket* FindSocket(FName InSocketName) const;

private:
	/** Regenerate new Guid */
	void RegenerateGuid();
	void RegenerateVirtualBoneGuid();

	// Handle skeletons being reloaded via the content browser
	static void HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);

public:
	//~ Begin IInterface_AssetUserData Interface
	ENGINE_API virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	ENGINE_API virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	ENGINE_API virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface
protected:
	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = Skeleton)
	TArray<TObjectPtr<UAssetUserData>> AssetUserData;

	friend struct FReferenceSkeletonModifier;
};

