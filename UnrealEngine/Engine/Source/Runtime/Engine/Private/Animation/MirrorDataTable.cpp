// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MirrorDataTable.h"

#include "AnimationRuntime.h"
#include "Algo/LevenshteinDistance.h"
#include "Animation/AnimationSettings.h"
#include "Animation/Skeleton.h"
#include "Internationalization/Regex.h"
#include "UObject/LinkerLoad.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MirrorDataTable)

#define LOCTEXT_NAMESPACE "MirrorDataTables"


FMirrorTableRow::FMirrorTableRow(const FMirrorTableRow& Other)
{
	*this = Other;
}

FMirrorTableRow& FMirrorTableRow::operator=(FMirrorTableRow const& Other)
{
	if (this == &Other)
	{
		return *this;
	}

	Name = Other.Name;
	MirroredName = Other.MirroredName;
	MirrorEntryType = Other.MirrorEntryType;
	return *this;
}

bool FMirrorTableRow::operator==(FMirrorTableRow const& Other) const
{
	return (Name == Other.Name && MirroredName == Other.MirroredName && MirrorEntryType == Other.MirrorEntryType);
}

bool FMirrorTableRow::operator!=(FMirrorTableRow const& Other) const
{
	return (Name != Other.Name || MirroredName != Other.MirroredName || MirrorEntryType != Other.MirrorEntryType);
}

bool FMirrorTableRow::operator<(FMirrorTableRow const& Other) const
{
	if (MirrorEntryType == Other.MirrorEntryType)
	{
		if (Name != Other.Name)
		{
			return Name.LexicalLess(Other.Name);
		}
		else
		{
			return MirroredName.LexicalLess(Other.MirroredName);
		}
	}
	return MirrorEntryType < Other.MirrorEntryType;
}

UMirrorDataTable::UMirrorDataTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer), MirrorAxis(EAxis::X)
{
#if WITH_EDITORONLY_DATA
	OnDataTableChanged().AddUObject(this, &UMirrorDataTable::FillMirrorArrays);
#endif 
}

void UMirrorDataTable::PostLoad()
{
	Super::PostLoad();

	FillMirrorArrays(); 
}

#if WITH_EDITOR

void UMirrorDataTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FillMirrorArrays(); 
}

#endif // WITH_EDITOR

FName UMirrorDataTable::GetSettingsMirrorName(FName InName)
{
	UAnimationSettings* AnimationSettings = UAnimationSettings::Get();
	FName MirrorName;
	if (AnimationSettings)
	{
		MirrorName = GetMirrorName(InName, AnimationSettings->MirrorFindReplaceExpressions);
	}
	return MirrorName;
}

FName UMirrorDataTable::GetMirrorName(FName InName, const TArray<FMirrorFindReplaceExpression>& MirrorFindReplaceExpressions)
{
	FName ReplacedName;
	FString InNameString = InName.ToString();
	for (const FMirrorFindReplaceExpression& regExStr : MirrorFindReplaceExpressions)
	{
		FString FindString = regExStr.FindExpression.ToString();
		FString ReplaceString = regExStr.ReplaceExpression.ToString();
		if (regExStr.FindReplaceMethod == EMirrorFindReplaceMethod::Prefix)
		{
			// convert prefix expression to regex that matches start of string, prefix, and any number of characters
			FindString = FindString + TEXT("([^}]*)");
			FindString = TEXT("^") + FindString;
			ReplaceString = ReplaceString + TEXT("$1");

		}
		else if (regExStr.FindReplaceMethod == EMirrorFindReplaceMethod::Suffix)
		{
			// convert suffix expression to regex that matches any number of characters start, the suffix, and end of string
			FindString = TEXT("([^}]*)") + FindString + TEXT("$");
			ReplaceString = TEXT("$1") + ReplaceString;
		}

		FRegexPattern MatherPatter(FindString);
		FRegexMatcher Matcher(MatherPatter, InNameString);
		bool bFound = false;
		while (Matcher.FindNext())
		{
			for (int32 CaptureIndex = 1; CaptureIndex < 10; CaptureIndex++)
			{
				FString CaptureResult = Matcher.GetCaptureGroup(CaptureIndex);
				int32 CaptureBegin = Matcher.GetCaptureGroupBeginning(CaptureIndex);
				int32 CaptureEnd = Matcher.GetCaptureGroupEnding(CaptureIndex);
				FString CaptureRegion = CaptureResult.Mid(CaptureBegin, CaptureEnd - CaptureBegin);
				if (CaptureResult.IsEmpty())
				{
					break;
				}
				FString MatchString = FString::Printf(TEXT("$%i"), CaptureIndex);
				ReplaceString = ReplaceString.Replace(*MatchString, *CaptureResult);
			}
			bFound = true;
		}
		if (bFound)
		{
			ReplacedName = *ReplaceString;
			break;
		}
	}
	return ReplacedName;
}

FName UMirrorDataTable::FindReplace(FName InName) const
{
	return GetMirrorName(InName, MirrorFindReplaceExpressions); 
}

FName UMirrorDataTable::FindBestMirroredBone(
	const FName InBoneName,
	const FReferenceSkeleton& InRefSkeleton,
	EAxis::Type InMirrorAxis,
	const float SearchThreshold)
{
	const int32 SourceBoneIndex = InRefSkeleton.FindBoneIndex(InBoneName);
	if (!ensureMsgf(SourceBoneIndex != INDEX_NONE, TEXT("Trying to find a mirror for a bone that isn't in the skeleton.")))
	{
		return NAME_None;
	}
	
	// if the bone with the mirrored name exists in the skeleton, then great just use that...
	const FName MirroredName = GetSettingsMirrorName(InBoneName);
	if (InRefSkeleton.FindBoneIndex(MirroredName) != INDEX_NONE)
	{
		return MirroredName;
	}

	// fallback to closest mirrored bone, breaking ties (coincident bones) with fuzzy string score
	TArray<FTransform> RefPoseGlobal;
	FAnimationRuntime::FillUpComponentSpaceTransforms(InRefSkeleton, InRefSkeleton.GetRefBonePose(), RefPoseGlobal);
	FVector MirroredLocation = RefPoseGlobal[SourceBoneIndex].GetLocation();
	switch (InMirrorAxis)
	{
	case EAxis::X:
		MirroredLocation.X *= -1.f;
		break;
	case EAxis::Y:
		MirroredLocation.Y *= -1.f;
		break;
	case EAxis::Z:
		MirroredLocation.Z *= -1.f;
		break;
	default:
		checkNoEntry();
	}

	// find closest bone and all bones near the mirrored location within our search threshold
	TArray<int32> BonesWithinThreshold;
	int32 ClosestBone = 0;
	int32 CurrentBone = 0;
	float ClosestDistSq = TNumericLimits<float>::Max();
	for (const FTransform& BoneTransform : RefPoseGlobal)
	{
		const float DistSq = (BoneTransform.GetLocation() - MirroredLocation).SizeSquared();
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			ClosestBone = CurrentBone;
		}
		if (DistSq <= FMath::Pow(SearchThreshold, 2.f))
		{
			BonesWithinThreshold.Add(CurrentBone);
		}
		++CurrentBone;
	}

	// no other bones were found near the mirrored location, so return the closest one
	if (BonesWithinThreshold.Num() <= 1)
	{
		return InRefSkeleton.GetBoneName(ClosestBone);
	}
	
	// in the case where we have multiple bones at or near the mirrored location (that are within our search threshold)
	// it would be arbitrary to pick the "closest" one since bones are not always placed with that degree of precision.
	// in this case, we break the tie with a fuzzy string comparison with the source bone name...
	const FString SourceBoneStr = InBoneName.ToString().ToLower();
	float BestScore = 0.f;
	int32 BestBoneIndex = INDEX_NONE;
	for (const int32 BoneToTestIndex : BonesWithinThreshold)
	{
		FString BoneToTestStr = InRefSkeleton.GetBoneName(BoneToTestIndex).ToString().ToLower();
		const float WorstCase = SourceBoneStr.Len() + BoneToTestStr.Len();
		const float Score = 1.0f - (Algo::LevenshteinDistance(BoneToTestStr, SourceBoneStr) / WorstCase);
		if (Score > BestScore)
		{
			BestBoneIndex = BoneToTestIndex;
			BestScore = Score;
		}
	}
	
	return InRefSkeleton.GetBoneName(BestBoneIndex);
}

#if WITH_EDITOR  

void UMirrorDataTable::FindReplaceMirroredNames()
{
	if (!Skeleton)
	{
		return; 
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	TSet<FName> NamesByCategory[4];
	ForeachRow<FMirrorTableRow>(TEXT("UMirrorDataTable::FindReplaceMirroredNames"), [&NamesByCategory] (const FName& Key, const FMirrorTableRow& Value) mutable
		{
			NamesByCategory[Value.MirrorEntryType].Add(Value.Name);
		}
	);

	bool bChangedTable = false;

	auto AddMirrorRow = [this, &bChangedTable, &NamesByCategory](const FName& Name, const FName& MirroredName, EMirrorRowType::Type RowType)
	{
		// directly add rows to avoid using FDataTableEditorUtils, which is not appropriate at this point
		// equivalent to FDataTableEditorUtils::AddRow(DataTable, BoneName);
		static const FString CategoryName[] = { TEXT(":Bone"), TEXT(":Notify"), TEXT(":Curve"), TEXT(":SyncMarker") };
		if (!NamesByCategory[RowType].Contains(Name))
		{
			FName RowName = Name;
			FMirrorTableRow* ExistingRow = FindRow<FMirrorTableRow>(RowName, TEXT("UMirrorDataTable::FindReplaceMirroredNames"), false);
			uint32 RenameAttempts = 0; 
			while (ExistingRow)
			{
				// Row names must be unique - in this case append a category name
				FString RowString = Name.ToString() + CategoryName[RowType];
				if (RenameAttempts > 0)
				{
					RowString.Appendf(TEXT("%d"), RenameAttempts);
				}
				RowName = *RowString;
				ExistingRow = FindRow<FMirrorTableRow>(RowName, TEXT("UMirrorDataTable::FindReplaceMirroredNames"), false);
				RenameAttempts++;
			}
			
			if (RowStruct)
			{
				Modify();
				// Allocate data to store information, using UScriptStruct to know its size
				uint8* RowData = (uint8*)FMemory::Malloc(RowStruct->GetStructureSize());
				RowStruct->InitializeStruct(RowData);
				// Add to row map
				AddRowInternal(RowName, RowData);
				bChangedTable = true;
			}
			FMirrorTableRow* MirrorRow = FindRow<FMirrorTableRow>(RowName, TEXT("UMirrorDataTable::FindReplaceMirroredNames"), false);
			if (MirrorRow)
			{
				MirrorRow->MirroredName = MirroredName;
				MirrorRow->Name = Name;
				MirrorRow->MirrorEntryType = RowType;
			}
		}
	};

	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
	{
		FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
		FName MirroredName = FindReplace(BoneName);
		if (!MirroredName.IsNone() && RefSkeleton.FindBoneIndex(MirroredName) != INDEX_NONE)
		{
			AddMirrorRow(BoneName, MirroredName, EMirrorRowType::Bone);
		}
	}

	for (const FName& Notify : Skeleton->AnimationNotifies)
	{
		FName MirroredName = FindReplace(Notify);
		if (!MirroredName.IsNone() && Skeleton->AnimationNotifies.Contains(MirroredName))
		{
			AddMirrorRow(Notify, MirroredName, EMirrorRowType::AnimationNotify);
		}
	}
	
	for (const FName& SyncMarker : Skeleton->GetExistingMarkerNames())
	{
		FName MirroredName = FindReplace(SyncMarker);
		if (!MirroredName.IsNone() && Skeleton->GetExistingMarkerNames().Contains(MirroredName))
		{
			AddMirrorRow(SyncMarker, MirroredName, EMirrorRowType::SyncMarker);
		}
	}

	TArray<FName> CurveNames;
	Skeleton->GetCurveMetaDataNames(CurveNames);

	// For every source curve, try to find the curve with the same name in the target.
	for (const FName& CurveName : CurveNames)
	{
		FName MirroredName = FindReplace(CurveName);
		if (!MirroredName.IsNone() && CurveNames.Contains(MirroredName))
		{
			AddMirrorRow(CurveName, MirroredName, EMirrorRowType::Curve);
		}
	}

	if (bChangedTable)
	{
		OnDataTableChanged().Broadcast();
	}
	FillMirrorArrays(); 
}

#endif // WITH_EDITOR
void UMirrorDataTable::FillCompactPoseMirrorBones(const FBoneContainer& BoneContainer, const TCustomBoneIndexArray<FSkeletonPoseBoneIndex, FSkeletonPoseBoneIndex>& MirrorBoneIndexes, TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex>& OutCompactPoseMirrorBones)
{
	const int32 NumReqBones = BoneContainer.GetCompactPoseNumBones();
	OutCompactPoseMirrorBones.Reset(NumReqBones);

	if (MirrorBoneIndexes.Num() > 0)
	{
		for (int32 CompactBoneIndex = 0; CompactBoneIndex < NumReqBones; ++CompactBoneIndex)
		{
			FSkeletonPoseBoneIndex SkeletonPoseBoneIndex = BoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(FCompactPoseBoneIndex(CompactBoneIndex));

			//Mirror Bone
			const FSkeletonPoseBoneIndex MirrorIndex = MirrorBoneIndexes.IsValidIndex(SkeletonPoseBoneIndex.GetInt()) ? MirrorBoneIndexes[SkeletonPoseBoneIndex] : FSkeletonPoseBoneIndex(INDEX_NONE);

			OutCompactPoseMirrorBones.Add(BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(MirrorIndex));
		}
	}
	else
	{
		for (int32 CompactBoneIndex = 0; CompactBoneIndex < NumReqBones; ++CompactBoneIndex)
		{
			OutCompactPoseMirrorBones.Add(FCompactPoseBoneIndex(INDEX_NONE));
		}
	}
}

void UMirrorDataTable::FillMirrorBoneIndexes(const USkeleton* InSkeleton, TCustomBoneIndexArray<FSkeletonPoseBoneIndex, FSkeletonPoseBoneIndex>& OutMirrorBoneIndexes) const
{
	const FReferenceSkeleton& ReferenceSkeleton = InSkeleton->GetReferenceSkeleton();

	// Reset the mirror table to defaults (no mirroring)
	OutMirrorBoneIndexes.SetNumUninitialized(ReferenceSkeleton.GetNum());
	FMemory::Memset(OutMirrorBoneIndexes.GetData(), INDEX_NONE, OutMirrorBoneIndexes.Num() * OutMirrorBoneIndexes.GetTypeSize());

	TMap<FName, FName> NameToMirrorNameBoneMap; 
	ForeachRow<FMirrorTableRow>(TEXT("UMirrorDataTable::FillMirrorBoneIndexes"), [&NameToMirrorNameBoneMap](const FName& Key, const FMirrorTableRow& Value) mutable
		{
			if (Value.MirrorEntryType == EMirrorRowType::Bone)
			{
				NameToMirrorNameBoneMap.Add(Value.Name, Value.MirroredName);
			}
		}
	);

	if (MirrorAxis != EAxis::None)
	{
		for (int32 BoneIndex = 0; BoneIndex < OutMirrorBoneIndexes.Num(); ++BoneIndex)
		{
			if (!OutMirrorBoneIndexes[BoneIndex].IsValid())
			{
				// Find the candidate mirror partner for this bone (falling back to mirroring to self)
				FName SourceBoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
				int32 MirrorBoneIndex = INDEX_NONE;

				FName* MirroredBoneName = NameToMirrorNameBoneMap.Find(SourceBoneName);
				if (!SourceBoneName.IsNone() && MirroredBoneName)
				{
					MirrorBoneIndex = ReferenceSkeleton.FindBoneIndex(*MirroredBoneName);
				}

				OutMirrorBoneIndexes[BoneIndex] = FSkeletonPoseBoneIndex(MirrorBoneIndex);
				if (MirrorBoneIndex != INDEX_NONE)
				{
					OutMirrorBoneIndexes[MirrorBoneIndex] = FSkeletonPoseBoneIndex(BoneIndex);
				}
			}
		}
	}
}

void UMirrorDataTable::FillCompactPoseAndComponentRefRotations(
	const FBoneContainer& BoneContainer,
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex>& OutCompactPoseMirrorBones,
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex>& OutComponentSpaceRefRotations) const
{
	TCustomBoneIndexArray<FSkeletonPoseBoneIndex, FSkeletonPoseBoneIndex> MirrorBoneIndexes;
	FillMirrorBoneIndexes(BoneContainer.GetSkeletonAsset(), MirrorBoneIndexes);
	FillCompactPoseMirrorBones(BoneContainer, MirrorBoneIndexes, OutCompactPoseMirrorBones);

	const int32 NumBones = BoneContainer.GetCompactPoseNumBones();
	OutComponentSpaceRefRotations.SetNumUninitialized(NumBones);
	if (NumBones > 0 && BoneContainer.GetRefPoseArray().Num() > 0)
	{
		OutComponentSpaceRefRotations[FCompactPoseBoneIndex(0)] =
			BoneContainer.GetRefPoseTransform(FCompactPoseBoneIndex(0)).GetRotation();
	}
	for (FCompactPoseBoneIndex BoneIndex(1); BoneIndex < NumBones; ++BoneIndex)
	{
		const FCompactPoseBoneIndex ParentBoneIndex = BoneContainer.GetParentBoneIndex(BoneIndex);
		OutComponentSpaceRefRotations[BoneIndex] = 
			OutComponentSpaceRefRotations[ParentBoneIndex] * BoneContainer.GetRefPoseTransform(BoneIndex).GetRotation();
	}
}


void UMirrorDataTable::FillMirrorArrays()
{
	SyncToMirrorSyncMap.Empty();
	AnimNotifyToMirrorAnimNotifyMap.Empty();
	CurveToMirrorCurveMap.Empty();
	if (!Skeleton)
	{
		BoneToMirrorBoneIndex.Empty();
		return; 
	}

	FillMirrorBoneIndexes(Skeleton, BoneToMirrorBoneIndex);
	
	ForeachRow<FMirrorTableRow>(TEXT("UMirrorDataTable::FillMirrorArrays"), [this](const FName& Key, const FMirrorTableRow& Value) mutable
		{
			switch (Value.MirrorEntryType)
			{
			case  EMirrorRowType::Curve:
			{
				// curves swap, so only one entry should exist.  For instance, if the table has (Left, Right) and (Right, Left) only add one item
				const FName* TestMirroredMatch = CurveToMirrorCurveMap.Find(Value.MirroredName);
				if (TestMirroredMatch == nullptr || *TestMirroredMatch != Value.Name)
				{
					CurveToMirrorCurveMap.Add(Value.Name, Value.MirroredName);
				}
				break;
			}
			case EMirrorRowType::SyncMarker:
			{
                SyncToMirrorSyncMap.Add(Value.Name, Value.MirroredName);
				break;
            }
			case EMirrorRowType::AnimationNotify:
			{
                AnimNotifyToMirrorAnimNotifyMap.Add(Value.Name, Value.MirroredName);
				break;
            }
			default:
				break;
			}
		}
	);
}

#undef LOCTEXT_NAMESPACE
