// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Skeleton.cpp: Skeleton features
=============================================================================*/ 

#include "Animation/Skeleton.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/PackageReload.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSequence.h"
#include "AnimationRuntime.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/Rig.h"
#include "Animation/BlendProfile.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "ComponentReregisterContext.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/PreviewMeshCollection.h"
#include "Misc/ScopedSlowTask.h"
#include "Animation/AnimBlueprint.h"
#include "UObject/AnimObjectVersion.h"
#include "EngineUtils.h"
#include "Misc/ScopeLock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Skeleton)

#define LOCTEXT_NAMESPACE "Skeleton"
#define ROOT_BONE_PARENT	INDEX_NONE

#if WITH_EDITOR
const FName USkeleton::AnimNotifyTag = FName(TEXT("AnimNotifyList"));
const FString USkeleton::AnimNotifyTagDelimiter = TEXT(";");

const FName USkeleton::CurveNameTag = FName(TEXT("CurveNameList"));
const FString USkeleton::CurveTagDelimiter = TEXT(";");

const FName USkeleton::RigTag = FName(TEXT("Rig"));
#endif 

// Names of smartname containers for skeleton properties
const FName USkeleton::AnimCurveMappingName = FName(TEXT("AnimationCurves"));
const FName USkeleton::AnimTrackCurveMappingName = FName(TEXT("AnimationTrackCurves"));

const FName FAnimSlotGroup::DefaultGroupName = FName(TEXT("DefaultGroup"));
const FName FAnimSlotGroup::DefaultSlotName = FName(TEXT("DefaultSlot"));

// Skeleton remapping related.
TArray<USkeleton*> USkeleton::LoadedSkeletons;		// We keep track of a list of loaded skeletons, because we have no thread safe object iterator at the time this code is written.
FCriticalSection USkeleton::LoadedSkeletonsMutex;	// The mutex we use to lock the LoadedSkeletons array.


void SerializeReferencePose(FArchive& Ar, FReferencePose& P, UObject* Outer)
{
	Ar.UsingCustomVersion(FAnimPhysObjectVersion::GUID);

	Ar << P.PoseName;
	Ar << P.ReferencePose;
#if WITH_EDITORONLY_DATA
	//TODO: we should use strip flags but we need to rev the serialization version
	if (!Ar.IsCooking() && (!Ar.IsLoading() || !Outer->GetOutermost()->bIsCookedForEditor))
	{
		if (Ar.IsLoading() && Ar.CustomVer(FAnimPhysObjectVersion::GUID) < FAnimPhysObjectVersion::ChangeRetargetSourceReferenceToSoftObjectPtr)
		{
			USkeletalMesh* SourceMesh = nullptr;
			Ar << SourceMesh;
			P.SourceReferenceMesh = SourceMesh;
		}
		else
		{
			// Scope the soft pointer serialization so we can tag it as editor only
			FName PackageName;
			FName PropertyName;
			ESoftObjectPathCollectType CollectType = ESoftObjectPathCollectType::AlwaysCollect;
			ESoftObjectPathSerializeType SerializeType = ESoftObjectPathSerializeType::AlwaysSerialize;
			FSoftObjectPathThreadContext& ThreadContext = FSoftObjectPathThreadContext::Get();
			ThreadContext.GetSerializationOptions(PackageName, PropertyName, CollectType, SerializeType);
			FSoftObjectPathSerializationScope SerializationScope(PackageName, PropertyName, ESoftObjectPathCollectType::EditorOnlyCollect, SerializeType);
			Ar << P.SourceReferenceMesh;
		}
	}
#endif
}

const TCHAR* SkipPrefix(const FString& InName)
{
	const int32 PrefixLength = VirtualBoneNameHelpers::VirtualBonePrefix.Len();
	check(InName.Len() > PrefixLength);
	return &InName[PrefixLength];
}

namespace VirtualBoneNameHelpers
{
	const FString VirtualBonePrefix(TEXT("VB "));

	FString AddVirtualBonePrefix(const FString& InName)
	{
		return VirtualBonePrefix + InName;
	}

	FName RemoveVirtualBonePrefix(const FString& InName)
	{
		return FName(SkipPrefix(InName));
	}
}

bool USkeleton::IsCompatible(const USkeleton* InSkeleton) const
{
	if (InSkeleton == nullptr)
	{
		return false;
	}

	if (InSkeleton == this)
	{
		return true;
	}

	for (const TSoftObjectPtr<USkeleton>& CompatibleSkeleton : CompatibleSkeletons)
	{
		if (CompatibleSkeleton == InSkeleton->CachedSoftObjectPtr)
		{
			return true;
		}
	}

	return false;
}

bool USkeleton::IsCompatibleSkeletonByAssetString(const FString& SkeletonAssetString) const
{
	// First check against itself.
	const FString SkeletonString = FAssetData(this).GetExportTextName();
	if (SkeletonString == SkeletonAssetString)
	{
		return true;
	}

	// Get the asset registry.
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	const IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Now check against the list of compatible skeletons and see if we're dealing with the same asset.
	for (const TSoftObjectPtr<USkeleton>& CompatibleSkeleton : CompatibleSkeletons)
	{
		const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(CompatibleSkeleton.ToString());
		if (AssetData.IsValid() && AssetData.GetExportTextName() == SkeletonAssetString)
		{
			return true;
		}
	}

	return false;
}

void USkeleton::AddCompatibleSkeleton(const USkeleton* SourceSkeleton)
{
	CompatibleSkeletons.AddUnique(SourceSkeleton);

	ClearSkeletonRemappings();
	BuildSkeletonRemappings(false);
}

void USkeleton::RemoveCompatibleSkeleton(const USkeleton* SourceSkeleton)
{
	CompatibleSkeletons.Remove(SourceSkeleton);

	ClearSkeletonRemappings();
	BuildSkeletonRemappings(false);
}

bool USkeleton::IsCompatibleSkeletonByAssetData(const FAssetData& AssetData, const TCHAR* InTag) const
{
	return IsCompatibleSkeletonByAssetString(AssetData.GetTagValueRef<FString>(InTag));
}

FSkeletonRemapping::FSkeletonRemapping(const USkeleton* InSourceSkeleton, const USkeleton* InTargetSkeleton)
	: SourceSkeleton(InSourceSkeleton)
	, TargetSkeleton(InTargetSkeleton)
{
	if (!GetSourceSkeleton().IsValid() || !GetTargetSkeleton().IsValid())
	{
		return;
	}

	const FReferenceSkeleton& SourceReferenceSkeleton = InSourceSkeleton->GetReferenceSkeleton();
	const FReferenceSkeleton& TargetReferenceSkeleton = InTargetSkeleton->GetReferenceSkeleton();

	const int32 SourceNumBones = SourceReferenceSkeleton.GetNum();
	const int32 TargetNumBones = TargetReferenceSkeleton.GetNum();

	SourceToTargetBoneIndexes.SetNumUninitialized(SourceNumBones);
	TargetToSourceBoneIndexes.SetNumUninitialized(TargetNumBones);
	RetargetingTable.SetNumUninitialized(TargetNumBones);

	TArrayView<const FTransform> SourceLocalTransforms;
	TArrayView<const FTransform> TargetLocalTransforms;
/*
	// Build the mapping from source to target bones through the remapping rig if one exists
	if (true)	// TODO_SKELETON_REMAPPING: Use a remapping rig if it exists
	{
		for (int32 SourceBoneIndex = 0; SourceBoneIndex < SourceNumBones; ++SourceBoneIndex)
		{
			const FName BoneName = SourceReferenceSkeleton.GetBoneName(SourceBoneIndex);
			int32 TargetBoneIndex = TargetReferenceSkeleton.FindBoneIndex(BoneName);
			SourceToTargetBoneIndexes[SourceBoneIndex] = TargetBoneIndex;
		}

		// Get the matched source and target rest poses from the remapping rig
		//
		// TODO_SKELETON_REMAPPING: For now we'll just use the skeleton's rest poses, but we should really be pulling these
		// poses from a remapping rig so that the use can better align the poses.  Note that we'll want the remapping rig
		// to be independent from the USkeleton itself because a skeleton may want to participate in multiple remappings with
		// different other skeletons and may have to pose itself differently to align with the different other skeletons
		//
		SourceLocalTransforms = MakeArrayView(SourceReferenceSkeleton.GetRefBonePose());
		TargetLocalTransforms = MakeArrayView(TargetReferenceSkeleton.GetRefBonePose());
	}
	else // Fall back to simple name matching if there is no rig
*/
	{
		// Match source to target bones by name lookup between source and target skeletons
		for (int32 SourceBoneIndex = 0; SourceBoneIndex < SourceNumBones; ++SourceBoneIndex)
		{
			const FName BoneName = SourceReferenceSkeleton.GetBoneName(SourceBoneIndex);
			const int32 TargetBoneIndex = TargetReferenceSkeleton.FindBoneIndex(BoneName);
			SourceToTargetBoneIndexes[SourceBoneIndex] = TargetBoneIndex;
		}

		// Get the matched (hopefully) source and target rest poses from the source and target skeletons
		SourceLocalTransforms = MakeArrayView(SourceReferenceSkeleton.GetRefBonePose());
		TargetLocalTransforms = MakeArrayView(TargetReferenceSkeleton.GetRefBonePose());
	}

	// Force the roots to map onto each other regardless of their names
	SourceToTargetBoneIndexes[0] = 0;

	// Build the reverse mapping from target back to source bones
	FMemory::Memset(TargetToSourceBoneIndexes.GetData(), static_cast<uint8>(INDEX_NONE), TargetNumBones * TargetToSourceBoneIndexes.GetTypeSize());
	for (int32 SourceBoneIndex = 0; SourceBoneIndex < SourceNumBones; ++SourceBoneIndex)
	{
		const int32 TargetBoneIndex = SourceToTargetBoneIndexes[SourceBoneIndex];
		if (TargetBoneIndex != INDEX_NONE)
		{
			TargetToSourceBoneIndexes[TargetBoneIndex] = SourceBoneIndex;
		}
	}

	TArray<FTransform> SourceComponentTransforms;
	TArray<FTransform> TargetComponentTransforms;
	FAnimationRuntime::FillUpComponentSpaceTransforms(SourceReferenceSkeleton, SourceLocalTransforms, SourceComponentTransforms);
	FAnimationRuntime::FillUpComponentSpaceTransforms(TargetReferenceSkeleton, TargetLocalTransforms, TargetComponentTransforms);

	// Calculate the retargeting constants to map from source skeleton space to target skeleton space
	//
	// Simply remapping joint indices is usually not sufficient to give us the desired result pose if the source and target
	// skeletons are built with different conventions for joint orientations.  We therefore need to compute a remapping
	// between the source and target joint orientations, which we do in terms of delta rotations from the rest pose:
	//
	//		Q = P * D * R
	//
	// where:
	//
	//		Q is the final joint orientation in component space
	//		P is the parent joint orientation in component space
	//		D is the delta rotation that we want to remap
	//		R is the local space rest pose orientation for the joint
	//
	// We want to find a mapping from the source to the target:
	//
	//		Ps * Ds * Rs  -->  Pt * Dt * Rt
	//
	// such that the deltas produce equivalent rotations on the mesh even if their parent rotation frames are different.
	// In other words, we need to find Dt such that its component space rotation is equivalent to Ds.  To convert a rotation
	// from local space (D) to component space (C), given its parent (P), we have:
	//
	//		C * P = P * D
	//		C = P * D * P⁻¹
	//
	// Setting the source and target component space rotations to be equal (Cs = Ct) then gives us:
	//
	//		Ps * Ds * Ps⁻¹ = Pt * Dt * Pt⁻¹
	//
	// which we then solve for Dt to get:
	//
	//		Dt = Pt⁻¹ * Ps * Ds * Ps⁻¹ * Pt
	//
	// However, when we're remapping an animation pose, we will have the local transforms rather than the deltas from the
	// rest pose, so we also need to convert between these local transforms and the equivalent deltas:
	//
	//		L = D * R
	//		D = L * R⁻¹
	//
	// Combining that with our equation for Dt, we get:
	//
	//		Lt * Rt⁻¹ = Pt⁻¹ * Ps * Ls * Rs⁻¹ * Ps⁻¹ * Pt
	//
	// Solving for Lt then gives us:
	//
	//		Lt = Pt⁻¹ * Ps * Ls * Rs⁻¹ * Ps⁻¹ * Pt * Rt
	//
	// Finally, factoring out the constant terms (which we precompute here) gives us:
	//
	//		Lt = Q0 * Ls * Q1
	//		Q0 = Pt⁻¹ * Ps
	//		Q1 = Rs⁻¹ * Ps⁻¹ * Pt * Rt
	//
	// Note that when remapping additive animations, we drop the rest pose terms, but we still need to convert between the
	// source and target rotation frames.  Dropping Rs and Rt from the equations above gives us:
	//
	//		Lt = Pt⁻¹ * Ps * Ls * Ps⁻¹ * Pt			(for additive animations)
	//
	// which is equivalent to the following in terms of our precomputed constants:
	//
	//		Lt = Q0 * Ls * Q0⁻¹						(for additive animations)
	//
	RetargetingTable[0] = MakeTuple(FQuat::Identity, SourceLocalTransforms[0].GetRotation().Inverse() * TargetLocalTransforms[0].GetRotation());
	for (int32 TargetBoneIndex = 1; TargetBoneIndex < TargetNumBones; ++TargetBoneIndex)
	{
		const int32 SourceBoneIndex = TargetToSourceBoneIndexes[TargetBoneIndex];
		if (SourceBoneIndex != INDEX_NONE)
		{
			const int32 SourceParentIndex = SourceReferenceSkeleton.GetParentIndex(SourceBoneIndex);
			const int32 TargetParentIndex = TargetReferenceSkeleton.GetParentIndex(TargetBoneIndex);
			check(SourceParentIndex != INDEX_NONE);
			check(TargetParentIndex != INDEX_NONE);

			const FQuat PS = SourceComponentTransforms[SourceParentIndex].GetRotation();
			const FQuat PT = TargetComponentTransforms[TargetParentIndex].GetRotation();

			const FQuat RS = SourceLocalTransforms[SourceBoneIndex].GetRotation();
			const FQuat RT = TargetLocalTransforms[TargetBoneIndex].GetRotation();

			const FQuat Q0 = PT.Inverse() * PS;
			const FQuat Q1 = RS.Inverse() * PS.Inverse() * PT * RT;

			RetargetingTable[TargetBoneIndex] = MakeTuple(Q0, Q1);
		}
		else
		{
			RetargetingTable[TargetBoneIndex] = MakeTuple(FQuat::Identity, FQuat::Identity);
		}
	}

	GenerateCurveMapping();
}

void FSkeletonRemapping::ComposeWith(const FSkeletonRemapping& OtherSkeletonRemapping)
{
	check(OtherSkeletonRemapping.SourceSkeleton == TargetSkeleton);

	TargetSkeleton = OtherSkeletonRemapping.TargetSkeleton;

	const int32 SourceNumBones = SourceToTargetBoneIndexes.Num();
	const int32 TargetNumBones = OtherSkeletonRemapping.TargetToSourceBoneIndexes.Num();

	TArray<int32> NewTargetToSourceBoneIndexes;
	TArray<TTuple<FQuat, FQuat>> NewRetargetingTable;

	NewTargetToSourceBoneIndexes.SetNumUninitialized(TargetNumBones);
	NewRetargetingTable.SetNumUninitialized(TargetNumBones);

	// Compose the retargeting constants
	for (int32 NewTargetBoneIndex = 0; NewTargetBoneIndex < TargetNumBones; ++NewTargetBoneIndex)
	{
		const int32 OldTargetBoneIndex = OtherSkeletonRemapping.TargetToSourceBoneIndexes[NewTargetBoneIndex];
		const int32 OldSourceBoneIndex = (OldTargetBoneIndex != INDEX_NONE) ? TargetToSourceBoneIndexes[OldTargetBoneIndex] : INDEX_NONE;

		if (OldSourceBoneIndex != INDEX_NONE)
		{
			const TTuple<FQuat, FQuat>& OldQQ = RetargetingTable[OldTargetBoneIndex];
			const TTuple<FQuat, FQuat>& NewQQ = OtherSkeletonRemapping.RetargetingTable[NewTargetBoneIndex];
			const FQuat Q0 = NewQQ.Get<0>() * OldQQ.Get<0>();
			const FQuat Q1 = OldQQ.Get<1>() * NewQQ.Get<1>();

			NewTargetToSourceBoneIndexes[NewTargetBoneIndex] = OldSourceBoneIndex;
			NewRetargetingTable[NewTargetBoneIndex] = MakeTuple(Q0, Q1);
		}
		else
		{
			NewTargetToSourceBoneIndexes[NewTargetBoneIndex] = INDEX_NONE;
			NewRetargetingTable[NewTargetBoneIndex] = MakeTuple(FQuat::Identity, FQuat::Identity);
		}
	}

	TargetToSourceBoneIndexes = MoveTemp(NewTargetToSourceBoneIndexes);
	RetargetingTable = MoveTemp(NewRetargetingTable);

	// Rebuild the mapping from source bones to the target bones
	FMemory::Memset(SourceToTargetBoneIndexes.GetData(), static_cast<uint8>(INDEX_NONE), SourceNumBones * SourceToTargetBoneIndexes.GetTypeSize());
	for (int32 TargetBoneIndex = 0; TargetBoneIndex < TargetNumBones; ++TargetBoneIndex)
	{
		const int32 SourceBoneIndex = TargetToSourceBoneIndexes[TargetBoneIndex];
		if (SourceBoneIndex != INDEX_NONE)
		{
			SourceToTargetBoneIndexes[SourceBoneIndex] = TargetBoneIndex;
		}
	}
}

void FSkeletonRemapping::GenerateCurveMapping()
{
	const USkeleton* Source = GetSourceSkeleton().Get();
	const USkeleton* Target = GetTargetSkeleton().Get();
	check(Source && Target);

	// Get the curve mappings.
	const FSmartNameMapping* SourceMapping = Source->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	const FSmartNameMapping* TargetMapping = Target->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	if (!SourceMapping) // Source has no curves, so there is nothing to add.
	{
		return;
	}

	// Check whether we have curves or not in both source and target.
	bool bHasSourceCurves = false;
	bool bHasTargetCurves = false;
	if (TargetMapping)
	{
		bHasTargetCurves = (TargetMapping->GetMaxUID() != SmartName::MaxUID);
		bHasSourceCurves = (SourceMapping->GetMaxUID() != SmartName::MaxUID);
	}

	// Init the mapping table.
	const SmartName::UID_Type NumItems = SourceMapping->GetMaxUID() + 1;
	if (bHasSourceCurves)
	{
		SourceToTargetCurveMapping.Init(MAX_uint16, NumItems);
	}

	// If we have no source or target curves (while having valid smart name mappings), there is nothing to do.
	if (!bHasTargetCurves && !bHasSourceCurves)
	{
		return;
	}

	// For every source curve, try to find the curve with the same name in the target.
	for (SmartName::UID_Type Index = 0; Index < NumItems; ++Index)
	{
		FName CurveName;
		SourceMapping->GetName(Index, CurveName);

		// Make sure the curve name is valid and we can actually find a curve with the same name in the target.
		FSmartName TargetSmartName;
		if (!CurveName.IsValid() || !TargetMapping->FindSmartName(CurveName, TargetSmartName))
		{
			continue;
		}

		SourceToTargetCurveMapping[Index] = TargetSmartName.UID;
	}
}

USkeleton::USkeleton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Make sure we have somewhere for curve names.
	AnimCurveMapping = SmartNames.AddContainer(AnimCurveMappingName);
	AnimCurveUidVersion = 0;
	CachedSoftObjectPtr = TSoftObjectPtr<USkeleton>(this);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FCoreUObjectDelegates::OnPackageReloaded.AddStatic(&USkeleton::HandlePackageReloaded);
	}
	else
	{
		if (!HasAnyFlags(RF_NeedPostLoad))
		{
			FScopeLock Lock(&LoadedSkeletonsMutex);
			LoadedSkeletons.Add(this);
		}
	}
}

void USkeleton::BeginDestroy()
{
	//  Remove the loaded skeleton from the list.
	{
		FScopeLock Lock(&LoadedSkeletonsMutex);
		LoadedSkeletons.Remove(this);

		// Also unregister this skeleton from other skeletons this one is listening to (events it is listening to).
		for (USkeleton* Skeleton : LoadedSkeletons)
		{
			if (Skeleton->IsCompatible(this))
			{
				Skeleton->OnSkeletonDestructEvent.RemoveAll(this);
				Skeleton->RemoveCompatibleSkeleton(this);
			}
		}
	}

	Super::BeginDestroy();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);
	}

	// Clear our own skeleton mappings.
	ClearSkeletonRemappings();
	OnSkeletonDestructEvent.Broadcast(this); // Inform others about this skeleton being destructed, so we can remove mappings inside those skeletons.
	OnSkeletonDestructEvent.Clear();
	OnSmartNamesChangedEvent.Clear();
}

void USkeleton::PostInitProperties()
{
	Super::PostInitProperties();

	// this gets called after constructor, and this data can get
	// serialized back if this already has Guid
	if (!IsTemplate())
	{
		RegenerateGuid();
	}
}

bool USkeleton::IsPostLoadThreadSafe() const
{
	return true;
}

void USkeleton::PostLoad()
{
	Super::PostLoad();

	if( GetLinker() && (GetLinker()->UEVer() < VER_UE4_REFERENCE_SKELETON_REFACTOR) )
	{
		// Convert RefLocalPoses & BoneTree to FReferenceSkeleton
		ConvertToFReferenceSkeleton();
	}

	// catch any case if guid isn't valid
	check(Guid.IsValid());

	SmartNames.PostLoad();
	
	// Cache smart name uids for animation curve names
	IncreaseAnimCurveUidVersion();

	// refresh linked bone indices
	FSmartNameMapping* CurveMappingTable = SmartNames.GetContainerInternal(USkeleton::AnimCurveMappingName);
	if (CurveMappingTable)
	{
		CurveMappingTable->InitializeCurveMetaData(this);
	}

	// Add this skeleton to the loaded skeletons list and register some event listeners.
	{
		FScopeLock Lock(&LoadedSkeletonsMutex);

		// Make any loaded skeleton that is compatible with this skeleton listen to destruct events.
		// This is done so when this skeleton gets destructed, other skeletons can remove mappings to this skeleton.
		for (USkeleton* Skeleton : LoadedSkeletons)
		{
			if (!Skeleton || Skeleton == this)
			{
				continue;
			}

			// If the already loaded other skeleton is compatible with this newly loaded skeleton, then let the
			// already loaded other skeleton listen to the destruction event of this newly loaded skeleton.
			if (Skeleton->IsCompatible(this))
			{
				Skeleton->CreateSkeletonRemappingIfNeeded(this);
				if (!OnSkeletonDestructEvent.IsBoundToObject(Skeleton))
				{
					OnSkeletonDestructEvent.AddUObject(Skeleton, &USkeleton::HandleSkeletonDestruct);
				}
			}

			// If we are compatible with the already loaded skeleton, we want to start listening to destruct events of that one.
			if (IsCompatible(Skeleton))
			{
				CreateSkeletonRemappingIfNeeded(Skeleton);
				if (!Skeleton->OnSkeletonDestructEvent.IsBoundToObject(this))
				{
					Skeleton->OnSkeletonDestructEvent.AddUObject(this, &USkeleton::HandleSkeletonDestruct);
				}
			}
		}

		LoadedSkeletons.Add(this);
	}

	// Listen to smart name changes so we can regenerate our skeleton mappings.
	if (!OnSmartNamesChangedEvent.IsBoundToObject(this))
	{
		OnSmartNamesChangedEvent.AddUObject(this, &USkeleton::HandleSmartNamesChangedEvent);
	}
	
	// Cleanup CompatibleSkeletons for convenience. This basically removes any soft object pointers that has an invalid soft object name.
	CompatibleSkeletons = CompatibleSkeletons.FilterByPredicate([](const TSoftObjectPtr<USkeleton>& Skeleton)
	{
		return Skeleton.ToSoftObjectPath().IsValid();
	});
}

// Regenerate all required skeleton remappings whenever curves change.
void USkeleton::HandleSmartNamesChangedEvent()
{
	FScopeLock Lock(&LoadedSkeletonsMutex);
	for (USkeleton* Skeleton : LoadedSkeletons)
	{
		if (Skeleton == this)
		{
			continue;
		}

		if (!Skeleton)
		{
			continue;
		}

		if (Skeleton->IsCompatible(this))
		{
			Skeleton->CreateSkeletonRemappingIfNeeded(this, true);
		}

		if (IsCompatible(Skeleton))
		{
			CreateSkeletonRemappingIfNeeded(Skeleton, true);
		}
	}
}

void USkeleton::HandleSkeletonDestruct(const USkeleton* Skeleton)
{
	OnSkeletonDestructEvent.RemoveAll(Skeleton);

	// We aren't interested to hear that we loaded ourselves as we need no mappings to ourselves.
	// Also if this skeleton isn't in the compatible list, we can ignore it.
	if (Skeleton == this || !Skeleton || !IsCompatible(Skeleton))
	{
		return;
	}

	// Check if we already have a mapping. If so, remove the existing one and create a new one.
	// This might only happen when a skeleton asset gets reimported.
	RemoveSkeletonRemapping(Skeleton);
}

void USkeleton::CreateSkeletonRemappingIfNeeded(const USkeleton* SourceSkeleton, bool bRebuildIfExists)
{
	FScopeLock Lock(&SkeletonRemappingMutex);
	check(SourceSkeleton != nullptr && SourceSkeleton != this && IsCompatible(SourceSkeleton));

	// Check if we already have a mapping. If so, remove the existing one and create a new one.
	// This might only happen when a skeleton asset gets reimported.
	FSkeletonRemapping** Mapping = SkeletonRemappings.Find(SourceSkeleton);
	if (Mapping)	
	{
		if (!bRebuildIfExists)
		{
			return;
		}

		delete *Mapping;
		SkeletonRemappings.Remove(SourceSkeleton);
	}

	// Create, add and return the new entry.
	FSkeletonRemapping* NewMapping = new FSkeletonRemapping(SourceSkeleton, this);
	SkeletonRemappings.Add(SourceSkeleton, NewMapping);

	UE_LOG(LogAnimation, Verbose, TEXT("Recreating remapping between %s and %s"), *FAssetData(this).AssetName.ToString(), *FAssetData(SourceSkeleton).AssetName.ToString());
}

void USkeleton::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		// regenerate Guid
		RegenerateGuid();
	}

	// Initialize skeleton remappings.
	ClearSkeletonRemappings();
	BuildSkeletonRemappings(true);
}

void USkeleton::Serialize( FArchive& Ar )
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("USkeleton::Serialize"), STAT_Skeleton_Serialize, STATGROUP_LoadTime );

	Super::Serialize(Ar);

	if( Ar.UEVer() >= VER_UE4_REFERENCE_SKELETON_REFACTOR )
	{
		Ar << ReferenceSkeleton;
	}

	if (Ar.UEVer() >= VER_UE4_FIX_ANIMATIONBASEPOSE_SERIALIZATION)
	{
		// Load Animation RetargetSources
		if (Ar.IsLoading())
		{
			int32 NumOfRetargetSources;
			Ar << NumOfRetargetSources;

			FName RetargetSourceName;
			FReferencePose RetargetSource;
			AnimRetargetSources.Empty();
			for (int32 Index=0; Index<NumOfRetargetSources; ++Index)
			{
				Ar << RetargetSourceName;
				SerializeReferencePose(Ar, RetargetSource, this);

				AnimRetargetSources.Add(RetargetSourceName, RetargetSource);
			}
		}
		else 
		{
			int32 NumOfRetargetSources = AnimRetargetSources.Num();
			Ar << NumOfRetargetSources;

			for (auto Iter = AnimRetargetSources.CreateIterator(); Iter; ++Iter)
			{
				Ar << Iter.Key();
				SerializeReferencePose(Ar, Iter.Value(), this);
			}
		}
	}
	else
	{
		// this is broken, but we have to keep it to not corrupt content. 
		for (auto Iter = AnimRetargetSources.CreateIterator(); Iter; ++Iter)
		{
			Ar << Iter.Key();
			SerializeReferencePose(Ar, Iter.Value(), this);
		}
	}

	if (Ar.UEVer() < VER_UE4_SKELETON_GUID_SERIALIZATION)
	{
		UE_LOG(LogAnimation, Warning, TEXT("Skeleton '%s' has not been saved since version 'VER_UE4_SKELETON_GUID_SERIALIZATION' This asset will not cook deterministically until it is resaved."), *GetPathName());
		RegenerateGuid();
	}
	else
	{
		Ar << Guid;
	}

	// If we should be using smartnames, serialize the mappings
	if(Ar.UEVer() >= VER_UE4_SKELETON_ADD_SMARTNAMES)
	{
		SmartNames.Serialize(Ar, IsTemplate());

		AnimCurveMapping = SmartNames.GetContainerInternal(USkeleton::AnimCurveMappingName);
	}

	// Build look up table between Slot nodes and their Group.
	if(Ar.UEVer() < VER_UE4_FIX_SLOT_NAME_DUPLICATION)
	{
		// In older assets we may have duplicates, remove these while building the map.
		BuildSlotToGroupMap(true);
	}
	else
	{
		BuildSlotToGroupMap();
	}

#if WITH_EDITORONLY_DATA
	if (Ar.UEVer() < VER_UE4_SKELETON_ASSET_PROPERTY_TYPE_CHANGE)
	{
		PreviewAttachedAssetContainer.SaveAttachedObjectsFromDeprecatedProperties();
	}
#endif

	if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::StoreMarkerNamesOnSkeleton)
	{
		FStripDataFlags StripFlags(Ar);
		if (!StripFlags.IsEditorDataStripped())
		{
			Ar << ExistingMarkerNames;
		}
	}

	// This is crashing when live coding in debug - the ObjectReferenceCollector 
	// is multithreaded and so I assume that is causing some subtle problem
	if (!Ar.IsObjectReferenceCollector())
	{
		const bool bRebuildNameMap = false;
		ReferenceSkeleton.RebuildRefSkeleton(this, bRebuildNameMap);
	}
}

#if WITH_EDITOR
void USkeleton::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Rebuild skeleton mappings involving this skeleton when we modified the compatible skeleton list.
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(USkeleton, CompatibleSkeletons))
	{
		ClearSkeletonRemappings();
		BuildSkeletonRemappings(false); // Build it, but not bidirectional, meaning only rebuild mappings to skeletons we are compatible with.
	}
}

void USkeleton::PreEditUndo()
{
	// Undoing so clear cached data as it will now be stale
	ClearCacheData();
}

void USkeleton::PostEditUndo()
{
	Super::PostEditUndo();

	//If we were undoing virtual bone changes then we need to handle stale cache data
	// Cached data is cleared in PreEditUndo to make sure it is done before any object hits their PostEditUndo
	HandleVirtualBoneChanges();
}
#endif // WITH_EDITOR

/** Remove this function when VER_UE4_REFERENCE_SKELETON_REFACTOR is removed. */
void USkeleton::ConvertToFReferenceSkeleton()
{
	FReferenceSkeletonModifier RefSkelModifier(ReferenceSkeleton, this);

	check( BoneTree.Num() == RefLocalPoses_DEPRECATED.Num() );

	const int32 NumRefBones = RefLocalPoses_DEPRECATED.Num();
	ReferenceSkeleton.Empty();

	for(int32 BoneIndex=0; BoneIndex<NumRefBones; BoneIndex++)
	{
		const FBoneNode& BoneNode = BoneTree[BoneIndex];
		FMeshBoneInfo BoneInfo(BoneNode.Name_DEPRECATED, BoneNode.Name_DEPRECATED.ToString(), BoneNode.ParentIndex_DEPRECATED);
		const FTransform& BoneTransform = RefLocalPoses_DEPRECATED[BoneIndex];

		// All should be good. Parents before children, no duplicate bones?
		RefSkelModifier.Add(BoneInfo, BoneTransform);
	}

	// technically here we should call 	RefershAllRetargetSources(); but this is added after 
	// VER_UE4_REFERENCE_SKELETON_REFACTOR, this shouldn't be needed. It shouldn't have any 
	// AnimatedRetargetSources
	ensure (AnimRetargetSources.Num() == 0);
}

void USkeleton::ClearSkeletonRemappings()
{
	FScopeLock Lock(&SkeletonRemappingMutex);
	for (auto& Mapping : SkeletonRemappings)
	{
		delete Mapping.Value;
	}

	SkeletonRemappings.Reset();
}

const FSkeletonRemapping* USkeleton::GetSkeletonRemapping(const USkeleton* SourceSkeleton) const
{
	check(SourceSkeleton);
	if (SourceSkeleton == this)
	{
		return nullptr;
	}

	// Find the existing one.
	FScopeLock Lock(&SkeletonRemappingMutex);
	const FSkeletonRemapping* const* Mapping = SkeletonRemappings.Find(SourceSkeleton);
	if (Mapping)
	{
		return *Mapping;
	}

	return nullptr;
}

void USkeleton::BuildSkeletonRemappings(bool bBidirectional)
{
	for (USkeleton* Skeleton : LoadedSkeletons)
	{
		if (Skeleton == this)
		{
			continue;
		}

		// Update the remapping in the other skeleton.
		if (bBidirectional && Skeleton->IsCompatible(this))
		{
			Skeleton->CreateSkeletonRemappingIfNeeded(this, true);
		}

		// Update the remapping to this skeleton inside our own remapping table.
		if (IsCompatible(Skeleton))
		{
			CreateSkeletonRemappingIfNeeded(Skeleton, true);
		}
	}
}

void USkeleton::RemoveSkeletonRemapping(const USkeleton* SourceSkeleton)
{
	check(SourceSkeleton && SourceSkeleton != this);

	// Locate the mapping and remove if found.
	FScopeLock Lock(&SkeletonRemappingMutex);
	FSkeletonRemapping** Mapping = SkeletonRemappings.Find(SourceSkeleton);
	if (Mapping)
	{
		delete *Mapping;
		SkeletonRemappings.Remove(SourceSkeleton);
	}
}

bool USkeleton::DoesParentChainMatch(int32 StartBoneIndex, const USkinnedAsset* InSkinnedAsset) const
{
	const FReferenceSkeleton& SkeletonRefSkel = ReferenceSkeleton;
	const FReferenceSkeleton& MeshRefSkel = InSkinnedAsset->GetRefSkeleton();

	// if start is root bone
	if ( StartBoneIndex == 0 )
	{
		// verify name of root bone matches
		return (SkeletonRefSkel.GetBoneName(0) == MeshRefSkel.GetBoneName(0));
	}

	int32 SkeletonBoneIndex = StartBoneIndex;
	// If skeleton bone is not found in mesh, fail.
	int32 MeshBoneIndex = MeshRefSkel.FindBoneIndex( SkeletonRefSkel.GetBoneName(SkeletonBoneIndex) );
	if( MeshBoneIndex == INDEX_NONE )
	{
		return false;
	}
	do
	{
		// verify if parent name matches
		int32 ParentSkeletonBoneIndex = SkeletonRefSkel.GetParentIndex(SkeletonBoneIndex);
		int32 ParentMeshBoneIndex = MeshRefSkel.GetParentIndex(MeshBoneIndex);

		// if one of the parents doesn't exist, make sure both end. Otherwise fail.
		if( (ParentSkeletonBoneIndex == INDEX_NONE) || (ParentMeshBoneIndex == INDEX_NONE) )
		{
			return (ParentSkeletonBoneIndex == ParentMeshBoneIndex);
		}

		// If parents are not named the same, fail.
		if( SkeletonRefSkel.GetBoneName(ParentSkeletonBoneIndex) != MeshRefSkel.GetBoneName(ParentMeshBoneIndex) )
		{
			return false;
		}

		// move up
		SkeletonBoneIndex = ParentSkeletonBoneIndex;
		MeshBoneIndex = ParentMeshBoneIndex;
	} while ( true );

	return true;
}

bool USkeleton::IsCompatibleMesh(const USkinnedAsset* InSkinnedAsset, bool bDoParentChainCheck) const
{
	// at least % of bone should match 
	int32 NumOfBoneMatches = 0;

	const FReferenceSkeleton& SkeletonRefSkel = ReferenceSkeleton;
	const FReferenceSkeleton& MeshRefSkel = InSkinnedAsset->GetRefSkeleton();
	const int32 NumBones = MeshRefSkel.GetRawBoneNum();

	// first ensure the parent exists for each bone
	for (int32 MeshBoneIndex=0; MeshBoneIndex<NumBones; MeshBoneIndex++)
	{
		FName MeshBoneName = MeshRefSkel.GetBoneName(MeshBoneIndex);
		// See if Mesh bone exists in Skeleton.
		int32 SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex( MeshBoneName );

		// if found, increase num of bone matches count
		if( SkeletonBoneIndex != INDEX_NONE )
		{
			++NumOfBoneMatches;

			// follow the parent chain to verify the chain is same
			if(bDoParentChainCheck && !DoesParentChainMatch(SkeletonBoneIndex, InSkinnedAsset))
			{
				UE_LOG(LogAnimation, Verbose, TEXT("%s : Hierarchy does not match."), *MeshBoneName.ToString());
				return false;
			}
		}
		else
		{
			int32 CurrentBoneId = MeshBoneIndex;
			// if not look for parents that matches
			while (SkeletonBoneIndex == INDEX_NONE && CurrentBoneId != INDEX_NONE)
			{
				// find Parent one see exists
				const int32 ParentMeshBoneIndex = MeshRefSkel.GetParentIndex(CurrentBoneId);
				if ( ParentMeshBoneIndex != INDEX_NONE )
				{
					// @TODO: make sure RefSkeleton's root ParentIndex < 0 if not, I'll need to fix this by checking TreeBoneIdx
					FName ParentBoneName = MeshRefSkel.GetBoneName(ParentMeshBoneIndex);
					SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex(ParentBoneName);
				}

				// root is reached
				if( ParentMeshBoneIndex == 0 )
				{
					break;
				}
				else
				{
					CurrentBoneId = ParentMeshBoneIndex;
				}
			}

			// still no match, return false, no parent to look for
			if( SkeletonBoneIndex == INDEX_NONE )
			{
				UE_LOG(LogAnimation, Verbose, TEXT("%s : Missing joint on skeleton.  Make sure to assign to the skeleton."), *MeshBoneName.ToString());
				return false;
			}

			// second follow the parent chain to verify the chain is same
			if (bDoParentChainCheck && !DoesParentChainMatch(SkeletonBoneIndex, InSkinnedAsset))
			{
				UE_LOG(LogAnimation, Verbose, TEXT("%s : Hierarchy does not match."), *MeshBoneName.ToString());
				return false;
			}
		}
	}

	// originally we made sure at least matches more than 50% 
	// but then follower components can't play since they're only partial
	// if the hierarchy matches, and if it's more then 1 bone, we allow
	return (NumOfBoneMatches > 0);
}

void USkeleton::ClearCacheData()
{
	FScopeLock ScopeLock(&LinkupCacheLock);
	LinkupCache.Empty();
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	SkelMesh2LinkupCache.Empty();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	SkinnedAsset2LinkupCache.Empty();
}

int32 USkeleton::GetMeshLinkupIndex(const USkinnedAsset* InSkinnedAsset)
{
	int32* IndexPtr = nullptr;
	{
		FScopeLock ScopeLock(&LinkupCacheLock);
		IndexPtr = SkinnedAsset2LinkupCache.Find(MakeWeakObjectPtr(const_cast<USkinnedAsset*>(InSkinnedAsset)));
	}
	int32 LinkupIndex = INDEX_NONE;

	if ( IndexPtr == NULL )
	{
		LinkupIndex = BuildLinkup(InSkinnedAsset);
	}
	else
	{
		LinkupIndex = *IndexPtr;
	}

	{
		FScopeLock ScopeLock(&LinkupCacheLock);
		// make sure it's not out of range
		check(LinkupIndex < LinkupCache.Num());
	}

	return LinkupIndex;
}

void USkeleton::RemoveLinkup(const USkinnedAsset* InSkinnedAsset)
{
	FScopeLock ScopeLock(&LinkupCacheLock);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (const USkeletalMesh* const InSkelMesh = Cast<const USkeletalMesh>(InSkinnedAsset))
	{
		SkelMesh2LinkupCache.Remove(MakeWeakObjectPtr(const_cast<USkeletalMesh*>(InSkelMesh)));
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	SkinnedAsset2LinkupCache.Remove(MakeWeakObjectPtr(const_cast<USkinnedAsset*>(InSkinnedAsset)));
}

int32 USkeleton::BuildLinkup(const USkinnedAsset* InSkinnedAsset)
{
	const FReferenceSkeleton& SkeletonRefSkel = ReferenceSkeleton;
	const FReferenceSkeleton& MeshRefSkel = InSkinnedAsset->GetRefSkeleton();

	// @todoanim : need to refresh NULL SkeletalMeshes from Cache
	// since now they're autoweak pointer, they will go away if not used
	// so whenever map transition happens, this links will need to clear up
	FSkeletonToMeshLinkup NewMeshLinkup;

	// First, make sure the Skeleton has all the bones the SkeletalMesh possesses.
	// This can get out of sync if a mesh was imported on that Skeleton, but the Skeleton was not saved.

	const int32 NumMeshBones = MeshRefSkel.GetNum();
	NewMeshLinkup.MeshToSkeletonTable.Empty(NumMeshBones);
	NewMeshLinkup.MeshToSkeletonTable.AddUninitialized(NumMeshBones);

	for (int32 MeshBoneIndex = 0; MeshBoneIndex < NumMeshBones; MeshBoneIndex++)
	{
		const FName MeshBoneName = MeshRefSkel.GetBoneName(MeshBoneIndex);
		int32 SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex(MeshBoneName);

#if WITH_EDITOR
		// If we're in editor, and skeleton is missing a bone, fix it.
		// not currently supported in-game.
		if (SkeletonBoneIndex == INDEX_NONE)
		{
			static FName NAME_LoadErrors("LoadErrors");
			FMessageLog LoadErrors(NAME_LoadErrors);

			TSharedRef<FTokenizedMessage> Message = LoadErrors.Error();
			Message->AddToken(FTextToken::Create(LOCTEXT("SkeletonBuildLinkupMissingBones1", "The Skeleton ")));
			Message->AddToken(FAssetNameToken::Create(GetPathName(), FText::FromString( GetNameSafe(this) ) ));
			Message->AddToken(FTextToken::Create(LOCTEXT("SkeletonBuildLinkupMissingBones2", " is missing bones that SkeletalMesh ")));
			Message->AddToken(FAssetNameToken::Create(InSkinnedAsset->GetPathName(), FText::FromString( GetNameSafe(InSkinnedAsset) )));
			Message->AddToken(FTextToken::Create(LOCTEXT("SkeletonBuildLinkupMissingBones3", "  needs. They will be added now. Please save the Skeleton!")));
			LoadErrors.Open();

			// Re-add all SkelMesh bones to the Skeleton.
			MergeAllBonesToBoneTree(InSkinnedAsset);

			// Fix missing bone.
			SkeletonBoneIndex = SkeletonRefSkel.FindBoneIndex(MeshBoneName);
		}
#else
		// If we're not in editor, we still want to know which skeleton is missing a bone.
		ensureMsgf(SkeletonBoneIndex != INDEX_NONE, TEXT("USkeleton::BuildLinkup: The Skeleton %s, is missing bones that SkeletalMesh %s needs. MeshBoneName %s"),
				*GetNameSafe(this), *GetNameSafe(InSkinnedAsset), *MeshBoneName.ToString());
#endif

		NewMeshLinkup.MeshToSkeletonTable[MeshBoneIndex] = SkeletonBoneIndex;
	}

	const int32 NumSkeletonBones = SkeletonRefSkel.GetNum();
	NewMeshLinkup.SkeletonToMeshTable.Empty(NumSkeletonBones);
	NewMeshLinkup.SkeletonToMeshTable.AddUninitialized(NumSkeletonBones);
	
	for (int32 SkeletonBoneIndex=0; SkeletonBoneIndex<NumSkeletonBones; SkeletonBoneIndex++)
	{
		const int32 MeshBoneIndex = MeshRefSkel.FindBoneIndex( SkeletonRefSkel.GetBoneName(SkeletonBoneIndex) );
		NewMeshLinkup.SkeletonToMeshTable[SkeletonBoneIndex] = MeshBoneIndex;
	}

	int32 NewIndex = INDEX_NONE;
	//LinkupCache lock scope
	{
		FScopeLock ScopeLock(&LinkupCacheLock);
		NewIndex = LinkupCache.Add(NewMeshLinkup);
		check(NewIndex != INDEX_NONE);
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (const USkeletalMesh* const InSkelMesh = Cast<const USkeletalMesh>(InSkinnedAsset))
		{
			SkelMesh2LinkupCache.Add(MakeWeakObjectPtr(const_cast<USkeletalMesh*>(InSkelMesh)), NewIndex);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		SkinnedAsset2LinkupCache.Add(MakeWeakObjectPtr(const_cast<USkinnedAsset*>(InSkinnedAsset)), NewIndex);
	}
	return NewIndex;
}


void USkeleton::RebuildLinkup(const USkinnedAsset* InSkinnedAsset)
{
	// remove the key
	RemoveLinkup(InSkinnedAsset);
	// build new one
	BuildLinkup(InSkinnedAsset);
}

void USkeleton::UpdateReferencePoseFromMesh(const USkinnedAsset* InSkinnedAsset)
{
	check(InSkinnedAsset);
	
	FReferenceSkeletonModifier RefSkelModifier(ReferenceSkeleton, this);

	for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetRawBoneNum(); BoneIndex++)
	{
		// find index from ref pose array
		const int32 MeshBoneIndex = InSkinnedAsset->GetRefSkeleton().FindRawBoneIndex(ReferenceSkeleton.GetBoneName(BoneIndex));
		if (MeshBoneIndex != INDEX_NONE)
		{
			RefSkelModifier.UpdateRefPoseTransform(BoneIndex, InSkinnedAsset->GetRefSkeleton().GetRefBonePose()[MeshBoneIndex]);
		}
	}

	MarkPackageDirty();
}

bool USkeleton::RecreateBoneTree(USkinnedAsset* InSkinnedAsset)
{
	if( InSkinnedAsset )
	{
		// regenerate Guid
		RegenerateGuid();	
		BoneTree.Empty();
		ReferenceSkeleton.Empty();

		return MergeAllBonesToBoneTree(InSkinnedAsset);
	}

	return false;
}

bool USkeleton::MergeAllBonesToBoneTree(const USkinnedAsset* InSkinnedAsset)
{
	if( InSkinnedAsset )
	{
		TArray<int32> RequiredBoneIndices;

		// for now add all in this case. 
		RequiredBoneIndices.AddUninitialized(InSkinnedAsset->GetRefSkeleton().GetRawBoneNum());
		// gather bone list
		for (int32 I=0; I<InSkinnedAsset->GetRefSkeleton().GetRawBoneNum(); ++I)
		{
			RequiredBoneIndices[I] = I;
		}

		if( RequiredBoneIndices.Num() > 0 )
		{
			// merge bones to the selected skeleton
			return MergeBonesToBoneTree( InSkinnedAsset, RequiredBoneIndices );
		}
	}

	return false;
}

bool USkeleton::CreateReferenceSkeletonFromMesh(const USkinnedAsset* InSkinnedAsset, const TArray<int32> & RequiredRefBones)
{
	// Filter list, we only want bones that have their parents present in this array.
	TArray<int32> FilteredRequiredBones; 
	FAnimationRuntime::ExcludeBonesWithNoParents(RequiredRefBones, InSkinnedAsset->GetRefSkeleton(), FilteredRequiredBones);

	FReferenceSkeletonModifier RefSkelModifier(ReferenceSkeleton, this);

	if( FilteredRequiredBones.Num() > 0 )
	{
		const int32 NumBones = FilteredRequiredBones.Num();
		ReferenceSkeleton.Empty(NumBones);
		BoneTree.Empty(NumBones);
		BoneTree.AddZeroed(NumBones);

		for (int32 Index=0; Index<FilteredRequiredBones.Num(); Index++)
		{
			const int32& BoneIndex = FilteredRequiredBones[Index];

			FMeshBoneInfo NewMeshBoneInfo = InSkinnedAsset->GetRefSkeleton().GetRefBoneInfo()[BoneIndex];
			// Fix up ParentIndex for our new Skeleton.
			if( BoneIndex == 0 )
			{
				NewMeshBoneInfo.ParentIndex = INDEX_NONE; // root
			}
			else
			{
				const int32 ParentIndex = InSkinnedAsset->GetRefSkeleton().GetParentIndex(BoneIndex);
				const FName ParentName = InSkinnedAsset->GetRefSkeleton().GetBoneName(ParentIndex);
				NewMeshBoneInfo.ParentIndex = ReferenceSkeleton.FindRawBoneIndex(ParentName);
			}
			RefSkelModifier.Add(NewMeshBoneInfo, InSkinnedAsset->GetRefSkeleton().GetRefBonePose()[BoneIndex]);
		}

		return true;
	}

	return false;
}


bool USkeleton::MergeBonesToBoneTree(const USkinnedAsset* InSkinnedAsset, const TArray<int32> & RequiredRefBones)
{
	// see if it needs all animation data to remap - only happens when bone structure CHANGED - added
	bool bSuccess = false;
	bool bShouldHandleHierarchyChange = false;
	// clear cache data since it won't work anymore once this is done
	ClearCacheData();

	// if it's first time
	if( BoneTree.Num() == 0 )
	{
		bSuccess = CreateReferenceSkeletonFromMesh(InSkinnedAsset, RequiredRefBones);
		bShouldHandleHierarchyChange = true;
	}
	else
	{
		// can we play? - hierarchy matches
		if( IsCompatibleMesh(InSkinnedAsset) )
		{
			// Exclude bones who do not have a parent.
			TArray<int32> FilteredRequiredBones;
			FAnimationRuntime::ExcludeBonesWithNoParents(RequiredRefBones, InSkinnedAsset->GetRefSkeleton(), FilteredRequiredBones);

			FReferenceSkeletonModifier RefSkelModifier(ReferenceSkeleton, this);

			for (int32 Index=0; Index<FilteredRequiredBones.Num(); Index++)
			{
				const int32& MeshBoneIndex = FilteredRequiredBones[Index];
				const int32& SkeletonBoneIndex = ReferenceSkeleton.FindRawBoneIndex(InSkinnedAsset->GetRefSkeleton().GetBoneName(MeshBoneIndex));
				
				// Bone doesn't already exist. Add it.
				if( SkeletonBoneIndex == INDEX_NONE )
				{
					FMeshBoneInfo NewMeshBoneInfo = InSkinnedAsset->GetRefSkeleton().GetRefBoneInfo()[MeshBoneIndex];
					// Fix up ParentIndex for our new Skeleton.
					if( ReferenceSkeleton.GetRawBoneNum() == 0 )
					{
						NewMeshBoneInfo.ParentIndex = INDEX_NONE; // root
					}
					else
					{
						NewMeshBoneInfo.ParentIndex = ReferenceSkeleton.FindRawBoneIndex(InSkinnedAsset->GetRefSkeleton().GetBoneName(InSkinnedAsset->GetRefSkeleton().GetParentIndex(MeshBoneIndex)));
					}

					RefSkelModifier.Add(NewMeshBoneInfo, InSkinnedAsset->GetRefSkeleton().GetRefBonePose()[MeshBoneIndex]);
					BoneTree.AddZeroed(1);
					bShouldHandleHierarchyChange = true;
				}
			}

			bSuccess = true;
		}
	}

	// if succeed
	if (bShouldHandleHierarchyChange)
	{
#if WITH_EDITOR
		HandleSkeletonHierarchyChange();
#endif
	}

	return bSuccess;
}

void USkeleton::SetBoneTranslationRetargetingMode(const int32 BoneIndex, EBoneTranslationRetargetingMode::Type NewRetargetingMode, bool bChildrenToo)
{
	BoneTree[BoneIndex].TranslationRetargetingMode = NewRetargetingMode;

	if( bChildrenToo )
	{
		// Bones are guaranteed to be sorted in increasing order. So children will be after this bone.
		const int32 NumBones = ReferenceSkeleton.GetRawBoneNum();
		for(int32 ChildIndex=BoneIndex+1; ChildIndex<NumBones; ChildIndex++)
		{
			if( ReferenceSkeleton.BoneIsChildOf(ChildIndex, BoneIndex) )
			{
				BoneTree[ChildIndex].TranslationRetargetingMode = NewRetargetingMode;
			}
		}
	}
}

#if WITH_EDITORONLY_DATA

FName USkeleton::GetRetargetSourceForMesh(USkinnedAsset* InSkinnedAsset) const
{
	FSoftObjectPath MeshPath(InSkinnedAsset);
	for(const TPair<FName, FReferencePose>& AnimRetargetSource : AnimRetargetSources)
	{
		if(AnimRetargetSource.Value.SourceReferenceMesh.ToSoftObjectPath() == MeshPath)
		{
			return AnimRetargetSource.Key;
		}
	}

	return NAME_None;
}

#endif

int32 USkeleton::GetRawAnimationTrackIndex(const int32 InSkeletonBoneIndex, const UAnimSequence* InAnimSeq)
{
	if (InSkeletonBoneIndex != INDEX_NONE)
	{
#if WITH_EDITOR
		return InAnimSeq->GetDataModel()->GetBoneAnimationTracks().IndexOfByPredicate([InSkeletonBoneIndex](const FBoneAnimationTrack& AnimationTrack)
			{
				return AnimationTrack.BoneTreeIndex == InSkeletonBoneIndex;
			});
#else
		return InAnimSeq->GetCompressedTrackToSkeletonMapTable().IndexOfByPredicate([InSkeletonBoneIndex](const FTrackToSkeletonMap& Mapping)
			{
				return Mapping.BoneTreeIndex == InSkeletonBoneIndex;
			});
#endif
	}

	return INDEX_NONE;
}

int32 USkeleton::GetSkeletonBoneIndexFromMeshBoneIndex(const USkinnedAsset* InSkinnedAsset, const int32 MeshBoneIndex)
{
	check(MeshBoneIndex != INDEX_NONE);
	const int32 LinkupCacheIdx = GetMeshLinkupIndex(InSkinnedAsset);

	//LinkupCache lock scope
	{
		FScopeLock ScopeLock(&LinkupCacheLock);
		const FSkeletonToMeshLinkup& LinkupTable = LinkupCache[LinkupCacheIdx];
		return LinkupTable.MeshToSkeletonTable[MeshBoneIndex];
	}
}


int32 USkeleton::GetMeshBoneIndexFromSkeletonBoneIndex(const USkinnedAsset* InSkinnedAsset, const int32 SkeletonBoneIndex)
{
	check(SkeletonBoneIndex != INDEX_NONE);
	const int32 LinkupCacheIdx = GetMeshLinkupIndex(InSkinnedAsset);

	//LinkupCache lock scope
	{
		FScopeLock ScopeLock(&LinkupCacheLock);
		const FSkeletonToMeshLinkup& LinkupTable = LinkupCache[LinkupCacheIdx];
		return LinkupTable.SkeletonToMeshTable[SkeletonBoneIndex];
	}
}


USkeletalMesh* USkeleton::GetPreviewMesh(bool bFindIfNotSet/*=false*/)
{
#if WITH_EDITORONLY_DATA
	USkeletalMesh* PreviewMesh = PreviewSkeletalMesh.LoadSynchronous();

	if(PreviewMesh && !IsCompatible(PreviewMesh->GetSkeleton())) // fix mismatched skeleton
	{
		PreviewSkeletalMesh.Reset();
		PreviewMesh = nullptr;
	}

	// if not existing, and if bFindIfNotExisting is true, then try find one
	if(!PreviewMesh && bFindIfNotSet)
	{
		USkeletalMesh* CompatibleSkeletalMesh = FindCompatibleMesh();
		if(CompatibleSkeletalMesh)
		{
			SetPreviewMesh(CompatibleSkeletalMesh, false);
			// update PreviewMesh
			PreviewMesh = PreviewSkeletalMesh.Get();
		}
	}

	return PreviewMesh;
#else
	return nullptr;
#endif
}

USkeletalMesh* USkeleton::GetPreviewMesh() const
{
#if WITH_EDITORONLY_DATA
	if (!PreviewSkeletalMesh.IsValid())
	{
		PreviewSkeletalMesh.LoadSynchronous();
	}
	return PreviewSkeletalMesh.Get();
#else
	return nullptr;
#endif
}

void USkeleton::SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty/*=true*/)
{
#if WITH_EDITORONLY_DATA
	if (bMarkAsDirty)
	{
		Modify();
	}

	PreviewSkeletalMesh = PreviewMesh;
#endif
}

#if WITH_EDITORONLY_DATA
void USkeleton::UpdateRetargetSource( const FName Name )
{
	FReferencePose * PoseFound = AnimRetargetSources.Find(Name);

	if (PoseFound)
	{
		USkeletalMesh* ReferenceMesh;
		
		if (PoseFound->SourceReferenceMesh.IsValid())
		{
			ReferenceMesh = PoseFound->SourceReferenceMesh.Get();
		}
		else
		{
			PoseFound->SourceReferenceMesh.LoadSynchronous();
			ReferenceMesh = PoseFound->SourceReferenceMesh.Get();
		}

		if (ReferenceMesh)
		{
			FAnimationRuntime::MakeSkeletonRefPoseFromMesh(ReferenceMesh, this, PoseFound->ReferencePose);
		}
		else
		{
			UE_LOG(LogAnimation, Warning, TEXT("Reference Mesh for Retarget Source %s has been removed."), *GetName());
		}
	}
}

void USkeleton::RefreshAllRetargetSources()
{
	for (auto Iter = AnimRetargetSources.CreateConstIterator(); Iter; ++Iter)
	{
		UpdateRetargetSource(Iter.Key());
	}
}

int32 USkeleton::GetChildBones(int32 ParentBoneIndex, TArray<int32> & Children) const
{
	return ReferenceSkeleton.GetDirectChildBones(ParentBoneIndex, Children);
}

void USkeleton::CollectAnimationNotifies()
{
	CollectAnimationNotifies(AnimationNotifies);
}

void USkeleton::CollectAnimationNotifies(TArray<FName>& OutNotifies) const
{
	// first merge in AnimationNotifies
	if(&AnimationNotifies != &OutNotifies)
	{
		for(const FName& NotifyName : AnimationNotifies)
		{
			OutNotifies.AddUnique(NotifyName);
		}
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// @Todo : remove it when we know the asset registry is updated
	// meanwhile if you remove this, this will miss the links
	//AnimationNotifies.Empty();
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetClassPathName(), AssetList, true);

	// do not clear AnimationNotifies. We can't remove old ones yet. 
	FString CurrentSkeletonName = FAssetData(this).GetExportTextName();
	for (auto Iter = AssetList.CreateConstIterator(); Iter; ++Iter)
	{
		const FAssetData& Asset = *Iter;
		const FString SkeletonValue = Asset.GetTagValueRef<FString>(TEXT("Skeleton"));
		if (SkeletonValue == CurrentSkeletonName)
		{
			FString Value;
			if (Asset.GetTagValue(USkeleton::AnimNotifyTag, Value))
			{
				TArray<FString> NotifyList;
				Value.ParseIntoArray(NotifyList, *USkeleton::AnimNotifyTagDelimiter, true);
				for (auto NotifyIter = NotifyList.CreateConstIterator(); NotifyIter; ++NotifyIter)
				{
					FString NotifyName = *NotifyIter;
					OutNotifies.AddUnique(FName(*NotifyName));
				}
			}
		}
	}
}

void USkeleton::AddNewAnimationNotify(FName NewAnimNotifyName)
{
	if (NewAnimNotifyName!=NAME_None)
	{
		AnimationNotifies.AddUnique( NewAnimNotifyName);
	}
}

USkeletalMesh* USkeleton::FindCompatibleMesh() const
{
	FARFilter Filter;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());

	FString SkeletonString = FAssetData(this).GetExportTextName();
	
	Filter.TagsAndValues.Add(USkeletalMesh::GetSkeletonMemberName(), SkeletonString);

	TArray<FAssetData> AssetList;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	if (AssetList.Num() > 0)
	{
		return Cast<USkeletalMesh>(AssetList[0].GetAsset());
	}

	return nullptr;
}

USkeletalMesh* USkeleton::GetAssetPreviewMesh(UObject* InAsset) 
{
	USkeletalMesh* PreviewMesh = nullptr;

	// return asset preview asset
	// if nothing is assigned, return skeleton asset
	if (UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(InAsset))
	{
		PreviewMesh = AnimAsset->GetPreviewMesh();
	}
	else if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(InAsset))
	{
		PreviewMesh = AnimBlueprint->GetPreviewMesh();
	}

	if (!PreviewMesh)
	{
		PreviewMesh = GetPreviewMesh(false);
	}

	return PreviewMesh;
}

void USkeleton::LoadAdditionalPreviewSkeletalMeshes()
{
	AdditionalPreviewSkeletalMeshes.LoadSynchronous();
}

UDataAsset* USkeleton::GetAdditionalPreviewSkeletalMeshes() const
{
	return AdditionalPreviewSkeletalMeshes.Get();
}

void USkeleton::SetAdditionalPreviewSkeletalMeshes(UDataAsset* InPreviewCollectionAsset)
{
	Modify();

	AdditionalPreviewSkeletalMeshes = InPreviewCollectionAsset;
}

int32 USkeleton::ValidatePreviewAttachedObjects()
{
	int32 NumBrokenAssets = PreviewAttachedAssetContainer.ValidatePreviewAttachedObjects();

	if(NumBrokenAssets > 0)
	{
		MarkPackageDirty();
	}
	return NumBrokenAssets;
}

#if WITH_EDITOR

void USkeleton::RemoveBonesFromSkeleton( const TArray<FName>& BonesToRemove, bool bRemoveChildBones )
{
	TArray<int32> BonesRemoved = ReferenceSkeleton.RemoveBonesByName(this, BonesToRemove);
	if(BonesRemoved.Num() > 0)
	{
		BonesRemoved.Sort();
		for(int32 Index = BonesRemoved.Num()-1; Index >=0; --Index)
		{
			BoneTree.RemoveAt(BonesRemoved[Index]);
		}
		HandleSkeletonHierarchyChange();
	}
}

void USkeleton::HandleSkeletonHierarchyChange()
{
	MarkPackageDirty();

	RegenerateGuid();

	// Clear exiting MeshLinkUp tables.
	ClearCacheData();

	for (int i = VirtualBones.Num() - 1; i >= 0; --i)
	{
		FVirtualBone& VB = VirtualBones[i];

		// Note: here virtual bones can have source bound to other virtual bones
		if (ReferenceSkeleton.FindBoneIndex(VB.SourceBoneName) == INDEX_NONE ||
			ReferenceSkeleton.FindBoneIndex(VB.TargetBoneName) == INDEX_NONE)
		{
			//Virtual Bone no longer valid
			VirtualBones.RemoveAt(i);
		}
	}

	// Full rebuild of all compatible with this and with ones we are compatible with.
	BuildSkeletonRemappings(true);

	// Fix up loaded animations (any animations that aren't loaded will be fixed on load)
	int32 NumLoadedAssets = 0;
	for (TObjectIterator<UAnimationAsset> It; It; ++It)
	{
		UAnimationAsset* CurrentAnimation = *It;
		if (CurrentAnimation->GetSkeleton() == this)
		{
			NumLoadedAssets++;
		}
	}

	FScopedSlowTask SlowTask((float)NumLoadedAssets, LOCTEXT("HandleSkeletonHierarchyChange", "Rebuilding animations..."));
	SlowTask.MakeDialog();

	for (TObjectIterator<UAnimationAsset> It; It; ++It)
	{
		UAnimationAsset* CurrentAnimation = *It;
		if (CurrentAnimation->GetSkeleton() == this)
		{
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("HandleSkeletonHierarchyChange_Format", "Rebuilding Animation: {0}"), FText::FromString(CurrentAnimation->GetName())));

			CurrentAnimation->ValidateSkeleton();
		}
	}

#if WITH_EDITORONLY_DATA
	RefreshAllRetargetSources();
#endif

	// refresh curve meta data that contains joint info
	FSmartNameMapping* CurveMappingTable = SmartNames.GetContainerInternal(USkeleton::AnimCurveMappingName);
	if (CurveMappingTable)
	{
		CurveMappingTable->InitializeCurveMetaData(this);
	}

	// Remove entries from Blend Profiles for bones that no longer exists
	for (UBlendProfile* Profile : BlendProfiles)
	{
		for (int32 EntryIndex = Profile->ProfileEntries.Num() - 1; EntryIndex >= 0; --EntryIndex)
		{
			const FBlendProfileBoneEntry& Entry = Profile->ProfileEntries[EntryIndex];
			if (ReferenceSkeleton.FindBoneIndex(Entry.BoneReference.BoneName) == INDEX_NONE)
			{
				Profile->RemoveEntry(Entry.BoneReference.BoneIndex);
			}
		}
	}

	OnSkeletonHierarchyChanged.Broadcast();
}

void USkeleton::RegisterOnSkeletonHierarchyChanged(const FOnSkeletonHierarchyChanged& Delegate)
{
	OnSkeletonHierarchyChanged.Add(Delegate);
}

void USkeleton::UnregisterOnSkeletonHierarchyChanged(void* Unregister)
{
	OnSkeletonHierarchyChanged.RemoveAll(Unregister);
}

#endif

#endif // WITH_EDITORONLY_DATA

const TArray<FAnimSlotGroup>& USkeleton::GetSlotGroups() const
{
	return SlotGroups;
}

void USkeleton::BuildSlotToGroupMap(bool bInRemoveDuplicates)
{
	SlotToGroupNameMap.Empty();

	for (FAnimSlotGroup& SlotGroup : SlotGroups)
	{
		for (const FName& SlotName : SlotGroup.SlotNames)
		{
			SlotToGroupNameMap.Add(SlotName, SlotGroup.GroupName);
		}
	}

	// Use the map we've just build to rebuild the slot groups
	if(bInRemoveDuplicates)
	{
		for(FAnimSlotGroup& SlotGroup : SlotGroups)
		{
			SlotGroup.SlotNames.Empty(SlotGroup.SlotNames.Num());

			for(TPair<FName, FName>& SlotToGroupPair : SlotToGroupNameMap)
			{
				if(SlotToGroupPair.Value == SlotGroup.GroupName)
				{
					SlotGroup.SlotNames.Add(SlotToGroupPair.Key);
				}
			}
		}

	}
}

FAnimSlotGroup* USkeleton::FindAnimSlotGroup(const FName& InGroupName)
{
	return SlotGroups.FindByPredicate([&InGroupName](const FAnimSlotGroup& Item)
	{
		return Item.GroupName == InGroupName;
	});
}

const FAnimSlotGroup* USkeleton::FindAnimSlotGroup(const FName& InGroupName) const
{
	return SlotGroups.FindByPredicate([&InGroupName](const FAnimSlotGroup& Item)
	{
		return Item.GroupName == InGroupName;
	});
}

bool USkeleton::ContainsSlotName(const FName& InSlotName) const
{
	return SlotToGroupNameMap.Contains(InSlotName);
}

bool USkeleton::RegisterSlotNode(const FName& InSlotName)
{
	// verify the slot name exists, if not create it in the default group.
	if (!ContainsSlotName(InSlotName))
	{
		SetSlotGroupName(InSlotName, FAnimSlotGroup::DefaultGroupName);
		return true;
	}

	return false;
}

void USkeleton::SetSlotGroupName(const FName& InSlotName, const FName& InGroupName)
{
// See if Slot already exists and belongs to a group.
	const FName* FoundGroupNamePtr = SlotToGroupNameMap.Find(InSlotName);

	// If slot exists, but is not in the right group, remove it from there
	if (FoundGroupNamePtr && ((*FoundGroupNamePtr) != InGroupName))
	{
		FAnimSlotGroup* OldSlotGroupPtr = FindAnimSlotGroup(*FoundGroupNamePtr);
		if (OldSlotGroupPtr)
		{
			OldSlotGroupPtr->SlotNames.RemoveSingleSwap(InSlotName);
		}
	}

	// Add the slot to the right group if it's not
	if ((FoundGroupNamePtr == NULL) || (*FoundGroupNamePtr != InGroupName))
	{
		// If the SlotGroup does not exist, create it.
		FAnimSlotGroup* SlotGroupPtr = FindAnimSlotGroup(InGroupName);
		if (SlotGroupPtr == NULL)
		{
			SlotGroups.AddZeroed(1);
			SlotGroupPtr = &SlotGroups.Last();
			SlotGroupPtr->GroupName = InGroupName;
		}
		// Add Slot to group.
		SlotGroupPtr->SlotNames.Add(InSlotName);
		// Keep our TMap up to date.
		SlotToGroupNameMap.Add(InSlotName, InGroupName);
	}
}

bool USkeleton::AddSlotGroupName(const FName& InNewGroupName)
{
	FAnimSlotGroup* ExistingSlotGroupPtr = FindAnimSlotGroup(InNewGroupName);
	if (ExistingSlotGroupPtr == NULL)
	{
		// if not found, create a new one.
		SlotGroups.AddZeroed(1);
		ExistingSlotGroupPtr = &SlotGroups.Last();
		ExistingSlotGroupPtr->GroupName = InNewGroupName;
		return true;
	}

	return false;
}

FName USkeleton::GetSlotGroupName(const FName& InSlotName) const
{
	const FName* FoundGroupNamePtr = SlotToGroupNameMap.Find(InSlotName);
	if (FoundGroupNamePtr)
	{
		return *FoundGroupNamePtr;
	}

	// If Group name cannot be found, use DefaultSlotGroupName.
	return FAnimSlotGroup::DefaultGroupName;
}

void USkeleton::RemoveSlotName(const FName& InSlotName)
{
	FName GroupName = GetSlotGroupName(InSlotName);
	
	if(SlotToGroupNameMap.Remove(InSlotName) > 0)
	{
		FAnimSlotGroup* SlotGroup = FindAnimSlotGroup(GroupName);
		SlotGroup->SlotNames.Remove(InSlotName);
	}
}

void USkeleton::RemoveSlotGroup(const FName& InSlotGroupName)
{
	FAnimSlotGroup* SlotGroup = FindAnimSlotGroup(InSlotGroupName);
	// Remove slot mappings
	for(const FName& SlotName : SlotGroup->SlotNames)
	{
		SlotToGroupNameMap.Remove(SlotName);
	}

	// Remove group
	SlotGroups.RemoveAll([&InSlotGroupName](const FAnimSlotGroup& Item)
	{
		return Item.GroupName == InSlotGroupName;
	});
}

void USkeleton::RenameSlotName(const FName& OldName, const FName& NewName)
{
	// Can't rename a name that doesn't exist
	check(ContainsSlotName(OldName))

	FName GroupName = GetSlotGroupName(OldName);
	RemoveSlotName(OldName);
	SetSlotGroupName(NewName, GroupName);
}

#if WITH_EDITOR

bool USkeleton::AddSmartNameAndModify(FName ContainerName, FName NewDisplayName, FSmartName& OutNewName)
{
	if (NewDisplayName != NAME_None)
	{
		OutNewName.DisplayName = NewDisplayName;
		const bool bAdded = VerifySmartNameInternal(ContainerName, OutNewName);

		if (bAdded)
		{
			IncreaseAnimCurveUidVersion();
		}

		return bAdded;
	}

	return false;
}

bool USkeleton::RenameSmartnameAndModify(FName ContainerName, SmartName::UID_Type Uid, FName NewName)
{
	bool Successful = false;
	if (NewName != NAME_None)
	{
		FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
		if (RequestedMapping)
		{
			FName CurrentName;
			RequestedMapping->GetName(Uid, CurrentName);
			if (CurrentName != NewName)
			{
				Modify();
				Successful = RequestedMapping->Rename(Uid, NewName);
				IncreaseAnimCurveUidVersion();
			}
		}
	}
	return Successful;
}

void USkeleton::RemoveSmartnameAndModify(FName ContainerName, SmartName::UID_Type Uid)
{
	FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		Modify();
		if (RequestedMapping->Remove(Uid))
		{
			IncreaseAnimCurveUidVersion();
		}
	}
}

void USkeleton::RemoveSmartnamesAndModify(FName ContainerName, const TArray<FName>& Names)
{
	FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		bool bModified = false;
		for (const FName& CurveName : Names)
		{
			if (RequestedMapping->Exists(CurveName))
			{
				if (!bModified)
				{
					Modify();
				}
				RequestedMapping->Remove(CurveName);
				bModified = true;
			}
		}

		if (bModified)
		{
			IncreaseAnimCurveUidVersion();
		}
	}
}
#endif // WITH_EDITOR

bool USkeleton::GetSmartNameByUID(const FName& ContainerName, SmartName::UID_Type UID, FSmartName& OutSmartName) const
{
	const FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		return (RequestedMapping->FindSmartNameByUID(UID, OutSmartName));
	}

	return false;
}

bool USkeleton::GetSmartNameByName(const FName& ContainerName, const FName& InName, FSmartName& OutSmartName) const
{
	const FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		return (RequestedMapping->FindSmartName(InName, OutSmartName));
	}

	return false;
}

SmartName::UID_Type USkeleton::GetUIDByName(const FName& ContainerName, const FName& Name) const
{
	const FSmartNameMapping* RequestedMapping = SmartNames.GetContainerInternal(ContainerName);
	if (RequestedMapping)
	{
		return RequestedMapping->FindUID(Name);
	}

	return SmartName::MaxUID;
}

// this does very simple thing.
// @todo: this for now prioritize FName because that is main issue right now
// @todo: @fixme: this has to be fixed when we have GUID
void USkeleton::VerifySmartName(const FName& ContainerName, FSmartName& InOutSmartName)
{
	if (VerifySmartNameInternal(ContainerName, InOutSmartName) && ContainerName == USkeleton::AnimCurveMappingName)
	{
		IncreaseAnimCurveUidVersion();
	}
}


bool USkeleton::FillSmartNameByDisplayName(FSmartNameMapping* Mapping, const FName& DisplayName, FSmartName& OutSmartName)
{
	FSmartName SkeletonName;
	if (Mapping->FindSmartName(DisplayName, SkeletonName))
	{
		OutSmartName.DisplayName = DisplayName;

		// if not editor, we assume name is always correct
		OutSmartName.UID = SkeletonName.UID;
		return true;
	}

	return false;
}

bool USkeleton::VerifySmartNameInternal(const FName&  ContainerName, FSmartName& InOutSmartName)
{
	FSmartNameMapping* Mapping = GetOrAddSmartNameContainer(ContainerName);
	if (Mapping != nullptr)
	{
		if (!Mapping->FindSmartName(InOutSmartName.DisplayName, InOutSmartName))
		{
#if WITH_EDITOR
			if (IsInGameThread())
			{
				Modify();
			}
#endif
			InOutSmartName = Mapping->AddName(InOutSmartName.DisplayName);
			return true;
		}
	}

	return false;
}

void USkeleton::VerifySmartNames(const FName&  ContainerName, TArray<FSmartName>& InOutSmartNames)
{
	bool bRefreshCache = false;

	for(auto& SmartName: InOutSmartNames)
	{
		VerifySmartNameInternal(ContainerName, SmartName);
	}

	if (ContainerName == USkeleton::AnimCurveMappingName)
	{
		IncreaseAnimCurveUidVersion();
	}
}

FSmartNameMapping* USkeleton::GetOrAddSmartNameContainer(const FName& ContainerName)
{
	FSmartNameMapping* Mapping = SmartNames.GetContainerInternal(ContainerName);
	if (Mapping == nullptr)
	{
#if WITH_EDITOR
		if (IsInGameThread())
		{
			Modify();
		}
#endif
		IncreaseAnimCurveUidVersion();
		SmartNames.AddContainer(ContainerName);
		AnimCurveMapping = SmartNames.GetContainerInternal(USkeleton::AnimCurveMappingName);
		Mapping = SmartNames.GetContainerInternal(ContainerName);		
	}

	return Mapping;
}

const FSmartNameMapping* USkeleton::GetSmartNameContainer(const FName& ContainerName) const
{
	return SmartNames.GetContainer(ContainerName);
}

void USkeleton::RegenerateGuid()
{
	Guid = FGuid::NewGuid();
	check(Guid.IsValid());
}

void USkeleton::RegenerateVirtualBoneGuid()
{
	VirtualBoneGuid = FGuid::NewGuid();
	check(VirtualBoneGuid.IsValid());
}

void USkeleton::IncreaseAnimCurveUidVersion()
{
	// this will be checked by skeletalmeshcomponent and if it's same, it won't care. If it's different, it will rebuild UID list
	++AnimCurveUidVersion;

	// update default uid list
	const FSmartNameMapping* Mapping = AnimCurveMapping;
	if (ensureAlways(Mapping))
	{
		DefaultCurveUIDList.Reset();
		// this should exactly work with what current index is
		Mapping->FillUidArray(DefaultCurveUIDList);
	}
}

FCurveMetaData* USkeleton::GetCurveMetaData(const FName& CurveName)
{
	FSmartNameMapping* Mapping = AnimCurveMapping;
	if (ensureAlways(Mapping))
	{
		return Mapping->GetCurveMetaData(CurveName);
	}

	return nullptr;
}

const FCurveMetaData* USkeleton::GetCurveMetaData(const FName& CurveName) const
{
	const FSmartNameMapping* Mapping = AnimCurveMapping;
	if (ensureAlways(Mapping))
	{
		return Mapping->GetCurveMetaData(CurveName);
	}

	return nullptr;
}

const FCurveMetaData* USkeleton::GetCurveMetaData(const SmartName::UID_Type CurveUID) const
{
	const FSmartNameMapping* Mapping = AnimCurveMapping;
	if (ensureAlways(Mapping))
	{
#if WITH_EDITOR
		FSmartName SmartName;
		if (Mapping->FindSmartNameByUID(CurveUID, SmartName))
		{
			return Mapping->GetCurveMetaData(SmartName.DisplayName);
		}
#else
		return &Mapping->GetCurveMetaData(CurveUID);
#endif
	}

	return nullptr;
}

FCurveMetaData* USkeleton::GetCurveMetaData(const FSmartName& CurveName)
{
	FSmartNameMapping* Mapping = AnimCurveMapping;
	if (ensureAlways(Mapping))
	{
		// the name might have changed, make sure it's up-to-date
		FName DisplayName;
		Mapping->GetName(CurveName.UID, DisplayName);
		return Mapping->GetCurveMetaData(DisplayName);
	}

	return nullptr;
}

const FCurveMetaData* USkeleton::GetCurveMetaData(const FSmartName& CurveName) const
{
	const FSmartNameMapping* Mapping = AnimCurveMapping;
	if (ensureAlways(Mapping))
	{
		// the name might have changed, make sure it's up-to-date
		FName DisplayName;
		Mapping->GetName(CurveName.UID, DisplayName);
		return Mapping->GetCurveMetaData(DisplayName);
	}

	return nullptr;
}

// this is called when you know both flags - called by post serialize
void USkeleton::AccumulateCurveMetaData(FName CurveName, bool bMaterialSet, bool bMorphtargetSet)
{
	if (bMaterialSet || bMorphtargetSet)
	{
		const FSmartNameMapping* Mapping = AnimCurveMapping;
		if (ensureAlways(Mapping))
		{
			// if we don't have name, add one
			if (Mapping->Exists(CurveName))
			{
				FCurveMetaData* CurveMetaData = GetCurveMetaData(CurveName);
				bool bOldMaterial = CurveMetaData->Type.bMaterial;
				bool bOldMorphtarget = CurveMetaData->Type.bMorphtarget;
				// we don't want to undo previous flags, if it was true, we just alolw more to it. 
				CurveMetaData->Type.bMaterial |= bMaterialSet;
				CurveMetaData->Type.bMorphtarget |= bMorphtargetSet;

				if (IsInGameThread() && (bOldMaterial != CurveMetaData->Type.bMaterial
					|| bOldMorphtarget != CurveMetaData->Type.bMorphtarget))
				{
					MarkPackageDirty();
				}
			}
		}
	}
}

bool USkeleton::AddNewVirtualBone(const FName SourceBoneName, const FName TargetBoneName)
{
	FName Dummy;
	return AddNewVirtualBone(SourceBoneName, TargetBoneName, Dummy);
}

bool USkeleton::AddNewVirtualBone(const FName SourceBoneName, const FName TargetBoneName, FName& NewVirtualBoneName)
{
	for (const FVirtualBone& SSBone : VirtualBones)
	{
		if (SSBone.SourceBoneName == SourceBoneName &&
			SSBone.TargetBoneName == TargetBoneName)
		{
			return false;
		}
	}
	Modify();
	VirtualBones.Add(FVirtualBone(SourceBoneName, TargetBoneName));
	NewVirtualBoneName = VirtualBones.Last().VirtualBoneName;

	RegenerateVirtualBoneGuid();
	HandleVirtualBoneChanges();


	return true;
}

int32 FindBoneByName(const FName& BoneName, TArray<FVirtualBone>& Bones)
{
	for (int32 Idx = 0; Idx < Bones.Num(); ++Idx)
	{
		if (Bones[Idx].VirtualBoneName == BoneName)
		{
			return Idx;
		}
	}
	return INDEX_NONE;
}

void USkeleton::RemoveVirtualBones(const TArray<FName>& BonesToRemove)
{
	Modify();
	for (const FName& BoneName : BonesToRemove)
	{
		int32 Idx = FindBoneByName(BoneName, VirtualBones);
		if (Idx != INDEX_NONE)
		{
			FName Parent = VirtualBones[Idx].SourceBoneName;
			for (FVirtualBone& VB : VirtualBones)
			{
				if (VB.SourceBoneName == BoneName)
				{
					VB.SourceBoneName = Parent;
				}
			}
			VirtualBones.RemoveAt(Idx,1,false);

			// @todo: This might be a slow operation if there's a large amount of blend profiles and entries
			int32 BoneIdx = GetReferenceSkeleton().FindBoneIndex(BoneName);
			if(BoneIdx != INDEX_NONE)
			{
				for (UBlendProfile* Profile : BlendProfiles)
				{
					Profile->RemoveEntry(BoneIdx);
				}
			}
		}
	}

	RegenerateVirtualBoneGuid();
	HandleVirtualBoneChanges();

	// Blend profiles cache bone names and indices, make sure they remain in sync when the indices change
	for (UBlendProfile* Profile : BlendProfiles)
	{
		// TEMPORARY FIX FOR 5.1.1

		for (FBlendProfileBoneEntry& Entry : Profile->ProfileEntries)
		{
			Entry.BoneReference.Initialize(Profile->OwningSkeleton);
		}
	}
}

void USkeleton::RenameVirtualBone(const FName OriginalBoneName, const FName NewBoneName)
{
	bool bModified = false;

	for (FVirtualBone& VB : VirtualBones)
	{
		if (VB.VirtualBoneName == OriginalBoneName)
		{
			if (!bModified)
			{
				bModified = true;
				Modify();
			}

			VB.VirtualBoneName = NewBoneName;
		}

		if (VB.SourceBoneName == OriginalBoneName)
		{
			if (!bModified)
			{
				bModified = true;
				Modify();
			}
			VB.SourceBoneName = NewBoneName;
		}
	}

	if (bModified)
	{
		RegenerateVirtualBoneGuid();
		HandleVirtualBoneChanges();

		// @todo: This might be a slow operation if there's a large amount of blend profiles and entries
		int32 BoneIdx = GetReferenceSkeleton().FindBoneIndex(NewBoneName);
		if (BoneIdx != INDEX_NONE)
		{
			for (UBlendProfile* Profile : BlendProfiles)
			{
				// TEMPORARY FIX FOR 5.1.1

				FBlendProfileBoneEntry* FoundEntry = Profile->ProfileEntries.FindByPredicate([BoneIdx](const FBlendProfileBoneEntry& Entry)
					{
						return Entry.BoneReference.BoneIndex == BoneIdx;
					});

				if (FoundEntry)
				{
					FoundEntry->BoneReference.BoneName = Profile->OwningSkeleton->GetReferenceSkeleton().GetBoneName(BoneIdx);
					FoundEntry->BoneReference.Initialize(Profile->OwningSkeleton);
				}
			}
		}
	}
}

void USkeleton::HandleVirtualBoneChanges()
{
	const bool bRebuildNameMap = false;
	ReferenceSkeleton.RebuildRefSkeleton(this, bRebuildNameMap);

	BuildSkeletonRemappings(true);

	for (TObjectIterator<USkeletalMesh> ItMesh; ItMesh; ++ItMesh)
	{
		USkeletalMesh* SkelMesh = *ItMesh;
		if (SkelMesh->GetSkeleton() == this)
		{
			// also have to update retarget base pose
			SkelMesh->GetRefSkeleton().RebuildRefSkeleton(this, bRebuildNameMap);
			RebuildLinkup(SkelMesh);
#if WITH_EDITOR
			// whole bone count has changed, so it has to recalculate retarget base pose
			SkelMesh->ReallocateRetargetBasePose();
#endif // #if WITH_EDITOR
		}
	}

	// refresh curve meta data that contains joint info
	FSmartNameMapping* CurveMappingTable = SmartNames.GetContainerInternal(USkeleton::AnimCurveMappingName);
	if (CurveMappingTable)
	{
		CurveMappingTable->InitializeCurveMetaData(this);
	}

	for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
	{
		USkinnedMeshComponent* MeshComponent = *It;
		if (MeshComponent &&
			MeshComponent->GetSkinnedAsset() &&
			MeshComponent->GetSkinnedAsset()->GetSkeleton() == this &&
			!MeshComponent->IsTemplate())
		{
			FComponentReregisterContext Context(MeshComponent);
		}
	}

#if WITH_EDITOR
	OnSkeletonHierarchyChanged.Broadcast();
#endif
}

#if WITH_EDITOR
void USkeleton::SetRigConfig(URig * Rig)
{
	if (RigConfig.Rig != Rig)
	{
		RigConfig.Rig = Rig;
		RigConfig.BoneMappingTable.Empty();

		if (Rig)
		{
			const FReferenceSkeleton& RefSkeleton = GetReferenceSkeleton();
			const TArray<FNode> & Nodes = Rig->GetNodes();
			// now add bone mapping table
			for (auto Node: Nodes)
			{
				// if find same bone, use that bone for mapping
				if (RefSkeleton.FindBoneIndex(Node.Name) != INDEX_NONE)
				{
					RigConfig.BoneMappingTable.Add(FNameMapping(Node.Name, Node.Name));
				}
				else
				{
					RigConfig.BoneMappingTable.Add(FNameMapping(Node.Name));
				}
			}
		}
	}
}

int32 USkeleton::FindRigBoneMapping(const FName& NodeName) const
{
	int32 Index=0;
	for(const auto & NameMap : RigConfig.BoneMappingTable)
	{
		if(NameMap.NodeName == NodeName)
		{
			return Index;
		}

		++Index;
	}

	return INDEX_NONE;
}

FName USkeleton::GetRigBoneMapping(const FName& NodeName) const
{
	int32 Index = FindRigBoneMapping(NodeName);

	if (Index != INDEX_NONE)
	{
		return RigConfig.BoneMappingTable[Index].BoneName;
	}

	return NAME_None;
}

FName USkeleton::GetRigNodeNameFromBoneName(const FName& BoneName) const
{
	for(const auto & NameMap : RigConfig.BoneMappingTable)
	{
		if(NameMap.BoneName == BoneName)
		{
			return NameMap.NodeName;
		}
	}

	return NAME_None;
}

int32 USkeleton::GetMappedValidNodes(TArray<FName> &OutValidNodeNames)
{
	OutValidNodeNames.Empty();

	for (auto Entry : RigConfig.BoneMappingTable)
	{
		if (Entry.BoneName != NAME_None)
		{
			OutValidNodeNames.Add(Entry.NodeName);
		}
	}

	return OutValidNodeNames.Num();
}

bool USkeleton::SetRigBoneMapping(const FName& NodeName, FName BoneName)
{
	// make sure it's valid
	int32 BoneIndex = ReferenceSkeleton.FindBoneIndex(BoneName);

	// @todo we need to have validation phase where you can't set same bone for different nodes
	// but it might be annoying to do that right now since the tool is ugly
	// so for now it lets you set everything, but in the future
	// we'll have to add verification
	if ( BoneIndex == INDEX_NONE )
	{
		BoneName = NAME_None;
	}

	int32 Index = FindRigBoneMapping(NodeName);

	if(Index != INDEX_NONE)
	{
		RigConfig.BoneMappingTable[Index].BoneName = BoneName;
		return true;
	}

	return false;
}

void USkeleton::RefreshRigConfig()
{
	if (RigConfig.Rig != NULL)
	{
		if (RigConfig.BoneMappingTable.Num() > 0)
		{
			// verify if any missing bones or anything
			// remove if removed
			for ( int32 TableId=0; TableId<RigConfig.BoneMappingTable.Num(); ++TableId )
			{
				auto & BoneMapping = RigConfig.BoneMappingTable[TableId];

				if ( RigConfig.Rig->FindNode(BoneMapping.NodeName) == INDEX_NONE)
				{
					// if not contains, remove it
					RigConfig.BoneMappingTable.RemoveAt(TableId);
					--TableId;
				}
			}

			// if the count doesn't match, there is missing nodes. 
			if (RigConfig.Rig->GetNodeNum() != RigConfig.BoneMappingTable.Num())
			{
				int32 NodeNum = RigConfig.Rig->GetNodeNum();
				for(int32 NodeId=0; NodeId<NodeNum; ++NodeId)
				{
					const auto* Node = RigConfig.Rig->GetNode(NodeId);

					if (FindRigBoneMapping(Node->Name) == INDEX_NONE)
					{
						RigConfig.BoneMappingTable.Add(FNameMapping(Node->Name));
					}
				}
			}
		}
	}
}

URig * USkeleton::GetRig() const
{
	return RigConfig.Rig;
}

void USkeleton::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
	FString RigFullName = (RigConfig.Rig)? RigConfig.Rig->GetFullName() : TEXT("");

	OutTags.Add(FAssetRegistryTag(USkeleton::RigTag, RigFullName, FAssetRegistryTag::TT_Hidden));
}

#endif //WITH_EDITOR

UBlendProfile* USkeleton::GetBlendProfile(const FName& InProfileName)
{
	TObjectPtr<UBlendProfile>* FoundProfile = BlendProfiles.FindByPredicate([InProfileName](const UBlendProfile* Profile)
	{
		return Profile->GetName() == InProfileName.ToString();
	});

	if(FoundProfile)
	{
		return *FoundProfile;
	}
	return nullptr;
}

UBlendProfile* USkeleton::CreateNewBlendProfile(const FName& InProfileName)
{
	Modify();
	UBlendProfile* NewProfile = NewObject<UBlendProfile>(this, InProfileName, RF_Public | RF_Transactional);
	BlendProfiles.Add(NewProfile);

	return NewProfile;
}

USkeletalMeshSocket* USkeleton::FindSocket(FName InSocketName) const
{
	int32 DummyIndex;
	return FindSocketAndIndex(InSocketName, DummyIndex);
}

USkeletalMeshSocket* USkeleton::FindSocketAndIndex(FName InSocketName, int32& OutIndex) const
{
	OutIndex = INDEX_NONE;
	if (InSocketName == NAME_None)
	{
		return nullptr;
	}

	for (int32 i = 0; i < Sockets.Num(); ++i)
	{
		USkeletalMeshSocket* Socket = Sockets[i];
		if (Socket && Socket->SocketName == InSocketName)
		{
			OutIndex = i;
			return Socket;
		}
	}

	return nullptr;
}


void USkeleton::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* USkeleton::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void USkeleton::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* USkeleton::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}

void USkeleton::HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
{
	if (InPackageReloadPhase == EPackageReloadPhase::PostPackageFixup)
	{
		for (const auto& RepointedObjectPair : InPackageReloadedEvent->GetRepointedObjects())
		{
			if (USkeleton* NewObject = Cast<USkeleton>(RepointedObjectPair.Value))
			{
				NewObject->HandleVirtualBoneChanges(); // Reloading Skeletons can invalidate virtual bones so refresh
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE 

