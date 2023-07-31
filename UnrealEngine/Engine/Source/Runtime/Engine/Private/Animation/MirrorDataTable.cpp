// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MirrorDataTable.h"
#include "Animation/AnimationSettings.h"
#include "Internationalization/Regex.h"

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
	if (Skeleton)
	{
		Skeleton->ConditionalPostLoad();
		Skeleton->OnSmartNamesChangedEvent.AddUObject(this, &UMirrorDataTable::FillMirrorArrays);
	}
	FillMirrorArrays(); 
}

#if WITH_EDITOR

void UMirrorDataTable::PreEditChange(FProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_STRING_CHECKED(UMirrorDataTable, Skeleton))
	{
		if (Skeleton)
		{
			Skeleton->OnSmartNamesChangedEvent.RemoveAll(this);
		}
	}
}

void UMirrorDataTable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FillMirrorArrays(); 
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UMirrorDataTable, Skeleton))
	{
		if (Skeleton)
		{
			Skeleton->OnSmartNamesChangedEvent.AddUObject(this, &UMirrorDataTable::FillMirrorArrays);
		}
	}
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

#if WITH_EDITOR  

void UMirrorDataTable::FindReplaceMirroredNames()
{
	if (!Skeleton)
	{
		return; 
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	TSet<FName> NamesByCategory[3];
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
		static const FString CategoryName[] = { TEXT(":Bone"), TEXT(":Notify"), TEXT(":Curve") };
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

	const FSmartNameMapping* CurveSmartNames = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	if (CurveSmartNames)
	{
		const SmartName::UID_Type NumItems = CurveSmartNames->GetMaxUID() + 1;
		TSet<FName> CurveNames; 
		// For every source curve, try to find the curve with the same name in the target.
		for (SmartName::UID_Type Index = 0; Index < NumItems; ++Index)
		{
			FName CurveName;
			CurveSmartNames->GetName(Index, CurveName);
			CurveNames.Add(CurveName);
		}

		for (const FName& CurveName : CurveNames)
		{
			FName MirroredName = FindReplace(CurveName);
			if (!MirroredName.IsNone() && CurveNames.Contains(MirroredName))
			{
				AddMirrorRow(CurveName, MirroredName, EMirrorRowType::Curve);
			}
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
	if (!Skeleton)
	{
		BoneToMirrorBoneIndex.Empty();
		CurveMirrorSourceUIDArray.Empty(); 
		CurveMirrorTargetUIDArray.Empty(); 
		return; 
	}

	FillMirrorBoneIndexes(Skeleton, BoneToMirrorBoneIndex);

	TMap<FName, FName> CurveToMirrorCurveMap;
	
	ForeachRow<FMirrorTableRow>(TEXT("UMirrorDataTable::FillMirrorArrays"), [this, &CurveToMirrorCurveMap](const FName& Key, const FMirrorTableRow& Value) mutable
		{
			switch (Value.MirrorEntryType)
			{
			case  EMirrorRowType::Curve:
			{
				CurveToMirrorCurveMap.Add(Value.Name, Value.MirroredName);
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

	// Ensure post load prepares the smart names
	Skeleton->ConditionalPostLoad();

	//ensure that pairs always appear beside each other in the arrays
	TSet<SmartName::UID_Type> AddedSourceUIDs; 
	const FSmartNameMapping* CurveSmartNames = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

	if (CurveSmartNames)
	{
		CurveMirrorSourceUIDArray.Reset(CurveToMirrorCurveMap.Num());
		CurveMirrorTargetUIDArray.Reset(CurveToMirrorCurveMap.Num());
		for (auto& Elem : CurveToMirrorCurveMap)
		{
			SmartName::UID_Type SourceCurveUID = CurveSmartNames->FindUID(Elem.Key);
			SmartName::UID_Type TargetCurveUID = CurveSmartNames->FindUID(Elem.Value);
			if (SourceCurveUID != INDEX_NONE && TargetCurveUID != INDEX_NONE && !AddedSourceUIDs.Contains(SourceCurveUID))
			{
				CurveMirrorSourceUIDArray.Add(SourceCurveUID);
				AddedSourceUIDs.Add(SourceCurveUID); 
				CurveMirrorTargetUIDArray.Add(TargetCurveUID);
				if (CurveToMirrorCurveMap.Contains(Elem.Value) && CurveSmartNames->FindUID(CurveToMirrorCurveMap[Elem.Value]) == SourceCurveUID)
				{
					CurveMirrorSourceUIDArray.Add(TargetCurveUID);
					AddedSourceUIDs.Add(TargetCurveUID);
					CurveMirrorTargetUIDArray.Add(SourceCurveUID);
				}
			}
		}
		CurveMirrorSourceUIDArray.Shrink(); 
		CurveMirrorTargetUIDArray.Shrink();
	}
}

#undef LOCTEXT_NAMESPACE
