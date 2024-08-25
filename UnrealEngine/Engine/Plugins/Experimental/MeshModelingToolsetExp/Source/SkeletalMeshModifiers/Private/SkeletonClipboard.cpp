// Copyright Epic Games, Inc. All Rights Reserved.


#include "SkeletonClipboard.h"

#include "SkeletonModifier.h"

#include "HAL/PlatformApplicationMisc.h"

namespace SkeletonClipboard
{

void CopyToClipboard(const USkeletonModifier& InModifier, const TArray<FName>& InBonesToCopy)
{
	FHierarchyClipboardData ClipboardData;
	ClipboardData.Bones.Reserve(InBonesToCopy.Num());

	const FReferenceSkeleton& RefSkeleton = InModifier.GetReferenceSkeleton();

	// ensure ordering
	TArray<FName> BonesToCopy(InBonesToCopy);
	BonesToCopy.StableSort([&](const FName Bone0, const FName Bone1)
	{
		const int32 Index0 = RefSkeleton.FindBoneIndex(Bone0);
		const int32 Index1 = RefSkeleton.FindBoneIndex(Bone1);
		return Index0 < Index1;
	});

	// convert bone names to clipboard data
	for (const FName& BoneName: BonesToCopy)
	{
		const FName ParentName = InModifier.GetParentName(BoneName);

		FBoneClipboardData BoneData;
			BoneData.BoneName = BoneName;
			BoneData.ParentIndex = ParentName != NAME_None ? BonesToCopy.IndexOfByKey(ParentName) : INDEX_NONE;
			BoneData.Global = InModifier.GetBoneTransform(BoneName, true);
		
		ClipboardData.Bones.Add(MoveTemp(BoneData));
	}
	
	// convert data to text
	FString ClipboardText;
	static const FHierarchyClipboardData DefaultData;
	FHierarchyClipboardData::StaticStruct()->ExportText(ClipboardText, &ClipboardData, &DefaultData, nullptr, PPF_None, nullptr);

	// copy to clipboard
	FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
}

class FHierarchyImportErrorContext : public FOutputDevice
{
public:

    int32 NumErrors;

    FHierarchyImportErrorContext(const bool bInNotify)
    	: FOutputDevice()
    	, NumErrors(0)
		, bNotify(bInNotify)
    {}

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
    	if (bNotify)
    	{
    		UE_LOG(LogTemp, Error, TEXT("Error Importing Bones: %s"), V);
    	}
    	NumErrors++;
    }

private:
	bool bNotify = false;
};

TArray<FName> PasteFromClipboard(USkeletonModifier& InOutModifier, const FName& InDefaultParent)
{
	// Get the text from the clipboard.
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);

	FHierarchyClipboardData ClipboardData;
	FHierarchyImportErrorContext ErrorPipe(true);
	
	UScriptStruct* DataStruct = FHierarchyClipboardData::StaticStruct();
	DataStruct->ImportText(*ClipboardText, &ClipboardData, nullptr, PPF_None, &ErrorPipe, DataStruct->GetName(), true);

	if (ErrorPipe.NumErrors > 0 || ClipboardData.Bones.IsEmpty())
	{
		return {};
	}

	// get new names
	TArray<FName> NewBones;
	NewBones.Reserve(ClipboardData.Bones.Num());

	Algo::Transform(ClipboardData.Bones, NewBones, [&](const FBoneClipboardData& BoneData)
	{
		return InOutModifier.GetUniqueName(BoneData.BoneName, NewBones);
	});

	// get parent names & local transforms
	TArray<FName> NewParents;
	NewParents.Reserve(ClipboardData.Bones.Num());

	TArray<FTransform> LocalTransforms;
	LocalTransforms.Reserve(ClipboardData.Bones.Num());

	for (const FBoneClipboardData& BoneData: ClipboardData.Bones)
	{
		const bool bIsParentFromClipboard = NewBones.IsValidIndex(BoneData.ParentIndex);
		if (bIsParentFromClipboard)
		{ // compute from clipboard
			NewParents.Add(NewBones[BoneData.ParentIndex]);

			const FTransform& ParentGlobal = ClipboardData.Bones[BoneData.ParentIndex].Global;
			LocalTransforms.Add(BoneData.Global.GetRelativeTransform(ParentGlobal));
		}
		else
		{ // compute from modifier
			const bool bUseDefaultParent = InDefaultParent != NAME_None && InDefaultParent != BoneData.BoneName;
			const FName ParentName = bUseDefaultParent ? InDefaultParent : InOutModifier.GetParentName(BoneData.BoneName);
			NewParents.Add(ParentName);

			const FTransform& ParentGlobal = InOutModifier.GetBoneTransform(ParentName, true);
			LocalTransforms.Add(BoneData.Global.GetRelativeTransform(ParentGlobal));
		}
	}

	// add bones
	InOutModifier.AddBones(NewBones, NewParents, LocalTransforms);

	return NewBones;
}
	
bool IsClipboardValid()
{
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);

	FHierarchyClipboardData ClipboardData;
	FHierarchyImportErrorContext ErrorPipe(false);
	
	UScriptStruct* DataStruct = FHierarchyClipboardData::StaticStruct();
	DataStruct->ImportText(*ClipboardText, &ClipboardData, nullptr, PPF_None, &ErrorPipe, DataStruct->GetName(), true);

	return ErrorPipe.NumErrors == 0 && !ClipboardData.Bones.IsEmpty();
}
	
}
