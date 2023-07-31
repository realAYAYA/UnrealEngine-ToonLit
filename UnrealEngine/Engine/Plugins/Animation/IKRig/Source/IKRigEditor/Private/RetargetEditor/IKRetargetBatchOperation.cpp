// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargetBatchOperation.h"

#include "Animation/AnimSequence.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimPose.h"
#include "AnimPreviewInstance.h"
#include "Animation/AnimSequence.h"
#include "ContentBrowserModule.h"
#include "EditorReimportHandler.h"
#include "IContentBrowserSingleton.h"
#include "ObjectEditorUtils.h"
#include "SSkeletonWidget.h"
#include "PropertyCustomizationHelpers.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "EditorFramework/AssetImportData.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ScopedSlowTask.h"
#include "RetargetEditor/SRetargetAnimAssetsWindow.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Animation/AnimMontage.h"

#define LOCTEXT_NAMESPACE "RetargetBatchOperation"

int32 FIKRetargetBatchOperation::GenerateAssetLists(const FIKRetargetBatchOperationContext& Context)
{
	// re-generate lists of selected and referenced assets
	AnimationAssetsToRetarget.Reset();
	AnimBlueprintsToRetarget.Reset();

	for (TWeakObjectPtr<UObject> AssetPtr : Context.AssetsToRetarget)
	{
		UObject* Asset = AssetPtr.Get();
		if (UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(Asset))
		{
			AnimationAssetsToRetarget.AddUnique(AnimAsset);

			// sequences that are used within the montage need to be added as well to be duplicated. They will then
			// be replaced in UAnimMontage::ReplaceReferredAnimations
			if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimAsset))
			{
				// add segments
				for (const FSlotAnimationTrack& Track: AnimMontage->SlotAnimTracks)
				{
					for (const FAnimSegment& Segment: Track.AnimTrack.AnimSegments)
					{
						if (Segment.IsValid() && Segment.GetAnimReference())
						{
							AnimationAssetsToRetarget.AddUnique(Segment.GetAnimReference());
						}
					}
				}

				// add preview pose
				if (AnimMontage->PreviewBasePose)
				{
					AnimationAssetsToRetarget.AddUnique(AnimMontage->PreviewBasePose);
				}
			}
		}
		else if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset))
		{
			// Add parents blueprint. 
			UAnimBlueprint* ParentBP = Cast<UAnimBlueprint>(AnimBlueprint->ParentClass->ClassGeneratedBy);
			while (ParentBP)
			{
				AnimBlueprintsToRetarget.AddUnique(ParentBP);
				ParentBP = Cast<UAnimBlueprint>(ParentBP->ParentClass->ClassGeneratedBy);
			}
				
			AnimBlueprintsToRetarget.AddUnique(AnimBlueprint);				
		}
	}

	if (Context.bRemapReferencedAssets)
	{
		// Grab assets from the blueprint.
		// Do this first as it can add complex assets to the retarget array which will need to be processed next.
		for (UAnimBlueprint* AnimBlueprint : AnimBlueprintsToRetarget)
		{
			GetAllAnimationSequencesReferredInBlueprint(AnimBlueprint, AnimationAssetsToRetarget);
		}

		int32 AssetIndex = 0;
		while (AssetIndex < AnimationAssetsToRetarget.Num())
		{
			UAnimationAsset* AnimAsset = AnimationAssetsToRetarget[AssetIndex++];
			AnimAsset->HandleAnimReferenceCollection(AnimationAssetsToRetarget, true);
		}
	}

	return AnimationAssetsToRetarget.Num();
}

void FIKRetargetBatchOperation::DuplicateRetargetAssets(
	const FIKRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress)
{
	Progress.EnterProgressFrame(1.f, FText(LOCTEXT("DuplicatingBatchRetarget", "Duplicating animation assets...")));

	UPackage* DestinationPackage = Context.TargetMesh->GetOutermost();

	TArray<UAnimationAsset*> AnimationAssetsToDuplicate = AnimationAssetsToRetarget;
	TArray<UAnimBlueprint*> AnimBlueprintsToDuplicate = AnimBlueprintsToRetarget;

	// We only want to duplicate unmapped assets, so we remove mapped assets from the list we're duplicating
	for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : RemappedAnimAssets)
	{
		AnimationAssetsToDuplicate.Remove(Pair.Key);
	}

	DuplicatedAnimAssets = DuplicateAssets<UAnimationAsset>(AnimationAssetsToDuplicate, DestinationPackage, &Context.NameRule);
	DuplicatedBlueprints = DuplicateAssets<UAnimBlueprint>(AnimBlueprintsToDuplicate, DestinationPackage, &Context.NameRule);

	// If we are moving the new asset to a different directory we need to fixup the reimport path.
	// This should only effect source FBX paths within the project.
	if (!Context.NameRule.FolderPath.IsEmpty())
	{
		for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : DuplicatedAnimAssets)
		{
			UAnimSequence* SourceSequence = Cast<UAnimSequence>(Pair.Key);
			UAnimSequence* DestinationSequence = Cast<UAnimSequence>(Pair.Value);
			if (!(SourceSequence && DestinationSequence))
			{
				continue;
			}
			
			for (int index = 0; index < SourceSequence->AssetImportData->SourceData.SourceFiles.Num(); index++)
			{
				const FString& RelativeFilename = SourceSequence->AssetImportData->SourceData.SourceFiles[index].RelativeFilename;
				const FString OldPackagePath = FPackageName::GetLongPackagePath(SourceSequence->GetPathName()) / TEXT("");
				const FString NewPackagePath = FPackageName::GetLongPackagePath(DestinationSequence->GetPathName()) / TEXT("");
				const FString AbsoluteSrcPath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(OldPackagePath));
				const FString SrcFile = AbsoluteSrcPath / RelativeFilename;
				const bool bSrcFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*SrcFile);
				if (!bSrcFileExists || (NewPackagePath == OldPackagePath))
				{
					continue;
				}

				FString BasePath = FPackageName::LongPackageNameToFilename(OldPackagePath);
				FString OldSourceFilePath = FPaths::ConvertRelativePathToFull(BasePath, RelativeFilename);
				TArray<FString> Paths;
				Paths.Add(OldSourceFilePath);
				
				// update the FBX reimport file path
				FReimportManager::Instance()->UpdateReimportPaths(DestinationSequence, Paths);
			}
		}
	}

	// Remapped assets needs the duplicated ones added
	RemappedAnimAssets.Append(DuplicatedAnimAssets);

	DuplicatedAnimAssets.GenerateValueArray(AnimationAssetsToRetarget);
	DuplicatedBlueprints.GenerateValueArray(AnimBlueprintsToRetarget);
}

void FIKRetargetBatchOperation::RetargetAssets(
	const FIKRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress)
{
	USkeleton* OldSkeleton = Context.SourceMesh->GetSkeleton();
	USkeleton* NewSkeleton = Context.TargetMesh->GetSkeleton();
	
	for (UAnimationAsset* AssetToRetarget : AnimationAssetsToRetarget)
	{
		// synchronize curves between old/new asset
		UAnimSequence* AnimSequenceToRetarget = Cast<UAnimSequence>(AssetToRetarget);
		if (AnimSequenceToRetarget)
		{
			// copy curve data from source asset, preserving data in the target if present.
			UAnimationBlueprintLibrary::CopyAnimationCurveNamesToSkeleton(OldSkeleton, NewSkeleton, AnimSequenceToRetarget, ERawCurveTrackTypes::RCT_Float);	
			// clear transform curves since those curves won't work in new skeleton
			IAnimationDataController& Controller = AnimSequenceToRetarget->GetController();
			const bool ShouldTransactAnimEdits = false;
			Controller.OpenBracket(FText::FromString("Generating Retargeted Animation Data"), ShouldTransactAnimEdits);
			Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform);
			// clear bone tracks to prevent recompression
			Controller.RemoveAllBoneTracks(false);
			// set the retarget source to the target skeletal mesh
			AnimSequenceToRetarget->RetargetSource = NAME_None;
			AnimSequenceToRetarget->RetargetSourceAsset = Context.TargetMesh;
			// done editing sequence data, close bracket
			Controller.CloseBracket(ShouldTransactAnimEdits);
		}

		// replace references to other animation
		AssetToRetarget->ReplaceReferredAnimations(RemappedAnimAssets);
		AssetToRetarget->SetSkeleton(NewSkeleton);
		AssetToRetarget->SetPreviewMesh(Context.TargetMesh);
	}

	// Call PostEditChange after the references of all assets were replaced, to prevent order dependence of post edit
	// change hooks. If PostEditChange is called right after ReplaceReferredAnimations it can access references that are
	// still queued for retarget and follow the current asset in the array.
	static const FName RetargetSourceAssetPropertyName =  GET_MEMBER_NAME_STRING_CHECKED(UAnimSequence, RetargetSourceAsset);
	static FProperty* RetargetAssetProperty = UAnimSequence::StaticClass()->FindPropertyByName(RetargetSourceAssetPropertyName);
	for (UAnimationAsset* AssetToRetarget : AnimationAssetsToRetarget)
	{
		// force updating of the retarget pose, this is normally done on PreSave() but is guarded against procedural saves
		if (UAnimSequence* AnimSequenceToRetarget = Cast<UAnimSequence>(AssetToRetarget))
		{
			FPropertyChangedEvent RetargetAssetPropertyChangedEvent(RetargetAssetProperty);
			AnimSequenceToRetarget->PostEditChangeProperty(RetargetAssetPropertyChangedEvent);
		}
		
		AssetToRetarget->PostEditChange();
		AssetToRetarget->MarkPackageDirty();
	}

	// convert the animation using the IK retargeter
	ConvertAnimation(Context,Progress);

	// convert all Animation Blueprints and compile 
	for (UAnimBlueprint* AnimBlueprint : AnimBlueprintsToRetarget)
	{
		// replace skeleton
		AnimBlueprint->TargetSkeleton = NewSkeleton;

		// if they have parent blueprint, make sure to re-link to the new one also
		UAnimBlueprint* CurrentParentBP = Cast<UAnimBlueprint>(AnimBlueprint->ParentClass->ClassGeneratedBy);
		if (CurrentParentBP)
		{
			UAnimBlueprint* const * ParentBP = DuplicatedBlueprints.Find(CurrentParentBP);
			if (ParentBP)
			{
				AnimBlueprint->ParentClass = (*ParentBP)->GeneratedClass;
			}
		}

		if(RemappedAnimAssets.Num() > 0)
		{
			ReplaceReferredAnimationsInBlueprint(AnimBlueprint, RemappedAnimAssets);
		}

		FBlueprintEditorUtils::RefreshAllNodes(AnimBlueprint);
		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
		AnimBlueprint->PostEditChange();
		AnimBlueprint->MarkPackageDirty();
	}
}

void FIKRetargetBatchOperation::ConvertAnimation(
	const FIKRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress)
{
	// initialize the retargeter
	UObject* TransientOuter = Cast<UObject>(GetTransientPackage());
	UIKRetargetProcessor* Processor = NewObject<UIKRetargetProcessor>(TransientOuter);
	Processor->Initialize(Context.SourceMesh, Context.TargetMesh, Context.IKRetargetAsset);
	if (!Processor->IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to initialize the IK Retargeter. Newly created animations were not retargeted!"));
		return;
	}

	// target skeleton data
	const FTargetSkeleton& TargetSkeleton = Processor->GetTargetSkeleton();
	const TArray<FName>& TargetBoneNames = TargetSkeleton.BoneNames;
	const int32 NumTargetBones = TargetBoneNames.Num();

	// allocate target keyframe data
	TArray<FRawAnimSequenceTrack> BoneTracks;
	BoneTracks.SetNumZeroed(NumTargetBones);

	// source skeleton data
	const FRetargetSkeleton& SourceSkeleton = Processor->GetSourceSkeleton();
	const TArray<FName>& SourceBoneNames = SourceSkeleton.BoneNames;
	const int32 NumSourceBones = SourceBoneNames.Num();

	TArray<FTransform> SourceComponentPose;
	SourceComponentPose.SetNum(NumSourceBones);

	// get names of the curves the retargeter is looking for
	TArray<FName> SpeedCurveNames;
	Context.IKRetargetAsset->GetSpeedCurveNames(SpeedCurveNames);
	// get all curve names in the source skeleton
	const FSmartNameMapping* SourceContainer = Context.SourceMesh->GetSkeleton()->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	TArray<FName> SourceCurveNames;
	SourceContainer->FillNameArray(SourceCurveNames);
	// get list of all the source curve IDs that hold speed values the retargeter is looking for
	TMap<FName, SmartName::UID_Type> SourceSpeedCurveIDs;
	for (int32 CurveIndex = 0; CurveIndex < SourceCurveNames.Num(); ++CurveIndex)
	{
		if (SpeedCurveNames.Contains(SourceCurveNames[CurveIndex]))
		{
			SourceSpeedCurveIDs.Add(SourceCurveNames[CurveIndex],SourceContainer->FindUID(SourceCurveNames[CurveIndex]));
		}
	}
	
	// for each pair of source / target animation sequences
	for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : DuplicatedAnimAssets)
	{
		UAnimSequence* SourceSequence = Cast<UAnimSequence>(Pair.Key);
		UAnimSequence* DestinationSequence = Cast<UAnimSequence>(Pair.Value);
		if (!(SourceSequence && DestinationSequence))
		{
			continue;
		}

		// increment progress bar
		FString AssetName = DestinationSequence->GetName();
		Progress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("RunningBatchRetarget", "Retargeting animation asset: {Asset}"), FText::FromString(AssetName)));

		// remove all keys from the destination animation sequence
		IAnimationDataController& TargetSeqController = DestinationSequence->GetController();
		const bool ShouldTransactAnimEdits = false;
		TargetSeqController.OpenBracket(FText::FromString("Generating Retargeted Animation Data"), ShouldTransactAnimEdits);
		TargetSeqController.RemoveAllBoneTracks();

		// number of frames in this animation
		const int32 NumFrames = SourceSequence->GetNumberOfSampledKeys();

		// BoneTracks arrays allocation
		for (int32 TargetBoneIndex=0; TargetBoneIndex<NumTargetBones; ++TargetBoneIndex)
		{
			BoneTracks[TargetBoneIndex].PosKeys.SetNum(NumFrames);
			BoneTracks[TargetBoneIndex].RotKeys.SetNum(NumFrames);
			BoneTracks[TargetBoneIndex].ScaleKeys.SetNum(NumFrames);
		}

		// ensure we evaluate the source animation using the skeletal mesh proportions that were evaluated in the viewport
		FAnimPoseEvaluationOptions EvaluationOptions = FAnimPoseEvaluationOptions();
		EvaluationOptions.OptionalSkeletalMesh = SourceSkeleton.SkeletalMesh;

		// reset the planting state
		Processor->ResetPlanting();
		
		// retarget each frame's pose from source to target
		for (int32 FrameIndex=0; FrameIndex<NumFrames; ++FrameIndex)
		{
			// get the source global pose
			FAnimPose SourcePoseAtFrame;
			UAnimPoseExtensions::GetAnimPoseAtFrame(SourceSequence, FrameIndex, EvaluationOptions, SourcePoseAtFrame);

			// we don't use UAnimPoseExtensions::GetBoneNames as the sequence can store bones that only exist on the
			// skeleton, but not on the current mesh. This results in indices discrepancy
			for (int32 BoneIndex = 0; BoneIndex < NumSourceBones; BoneIndex++)
			{
				const FName& BoneName = SourceBoneNames[BoneIndex];
				SourceComponentPose[BoneIndex] = UAnimPoseExtensions::GetBonePose(SourcePoseAtFrame, BoneName, EAnimPoseSpaces::World);
			}

			// update goals 
			Processor->ApplySettingsFromAsset();
			
			// calculate the delta time
			const float TimeAtCurrentFrame = SourceSequence->GetTimeAtFrame(FrameIndex);
			float DeltaTime = TimeAtCurrentFrame;
			if (FrameIndex > 0)
			{
				const float TimeAtPrevFrame = SourceSequence->GetTimeAtFrame(FrameIndex-1);
				DeltaTime = TimeAtCurrentFrame - TimeAtPrevFrame;
			}
			
			// get the curve values from the source sequence (for speed-based IK planting)
			TMap<FName, float> SpeedCurveValues;
			for (TPair<FName, SmartName::UID_Type> CurveNameAndID : SourceSpeedCurveIDs)
			{
				SpeedCurveValues.Add(CurveNameAndID.Key, SourceSequence->EvaluateCurveData(CurveNameAndID.Value, TimeAtCurrentFrame));
			}

			// run the retargeter
			const TArray<FTransform>& TargetComponentPose = Processor->RunRetargeter(SourceComponentPose, SpeedCurveValues, DeltaTime);

			// convert to a local-space pose
			TArray<FTransform> TargetLocalPose = TargetComponentPose;
			TargetSkeleton.UpdateLocalTransformsBelowBone(0,TargetLocalPose, TargetComponentPose);

			// store key data for each bone
			for (int32 TargetBoneIndex=0; TargetBoneIndex<NumTargetBones; ++TargetBoneIndex)
			{
				const FTransform& LocalPose = TargetLocalPose[TargetBoneIndex];
				
				FRawAnimSequenceTrack& BoneTrack = BoneTracks[TargetBoneIndex];
				
				BoneTrack.PosKeys[FrameIndex] = FVector3f(LocalPose.GetLocation());
				BoneTrack.RotKeys[FrameIndex] = FQuat4f(LocalPose.GetRotation());
				BoneTrack.ScaleKeys[FrameIndex] = FVector3f(LocalPose.GetScale3D());
			}
		}

		// add keys to bone tracks
		const bool bShouldTransact = false;
		for (int32 TargetBoneIndex=0; TargetBoneIndex<NumTargetBones; ++TargetBoneIndex)
		{
			const FName& TargetBoneName = TargetBoneNames[TargetBoneIndex];

			const FRawAnimSequenceTrack& RawTrack = BoneTracks[TargetBoneIndex];
			TargetSeqController.AddBoneTrack(TargetBoneName, bShouldTransact);
			TargetSeqController.SetBoneTrackKeys(TargetBoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys);
		}

		// done editing sequence data, close bracket
		TargetSeqController.CloseBracket(ShouldTransactAnimEdits);
	}
}

void FIKRetargetBatchOperation::NotifyUserOfResults(
	const FIKRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress) const
{
	Progress.EnterProgressFrame(1.f, FText(LOCTEXT("DoneBatchRetarget", "Duplicate and retarget complete!")));

	// gather newly created objects
	TArray<UObject*> NewAssets;
	GetNewAssets(NewAssets);

	// log details of what assets were created
	for (UObject* NewAsset : NewAssets)
	{
		UE_LOG(LogTemp, Display, TEXT("Duplicate and Retarget - New Asset Created: %s"), *NewAsset->GetName());
	}
	
	// notify user
	FNotificationInfo Notification(FText::GetEmpty());
	Notification.ExpireDuration = 5.f;
	Notification.Text = FText::Format(
		LOCTEXT("MultiNonDuplicatedAsset", "{0} assets were retargeted to new skeleton {1}. See Output for details."),
		FText::AsNumber(NewAssets.Num()),
		FText::FromString(Context.TargetMesh->GetName()));
	FSlateNotificationManager::Get().AddNotification(Notification);
	
	// select all new assets
	TArray<FAssetData> CurrentSelection;
	for(UObject* NewObject : NewAssets)
	{
		CurrentSelection.Add(FAssetData(NewObject));
	}

	// show assets in browser
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(CurrentSelection);
}

void FIKRetargetBatchOperation::GetNewAssets(TArray<UObject*>& NewAssets) const
{
	TArray<UAnimationAsset*> NewAnims;
	DuplicatedAnimAssets.GenerateValueArray(NewAnims);
	for (UAnimationAsset* NewAnim : NewAnims)
	{
		NewAssets.Add(Cast<UObject>(NewAnim));
	}

	TArray<UAnimBlueprint*> NewBlueprints;
	DuplicatedBlueprints.GenerateValueArray(NewBlueprints);
	for (UAnimBlueprint* NewBlueprint : NewBlueprints)
	{
		NewAssets.Add(Cast<UObject>(NewBlueprint));
	}
}



void FIKRetargetBatchOperation::RunRetarget(FIKRetargetBatchOperationContext& Context)
{	
	const int32 NumAssets = GenerateAssetLists(Context);
	
	// show progress bar
	FScopedSlowTask Progress(NumAssets + 2, LOCTEXT("GatheringBatchRetarget", "Gathering animation assets..."));
	Progress.MakeDialog();
	
	DuplicateRetargetAssets(Context, Progress);
	RetargetAssets(Context, Progress);
	NotifyUserOfResults(Context, Progress);
}

/**
* Duplicates the supplied AssetsToDuplicate and returns a map of original asset to duplicate. Templated wrapper that calls DuplicateAssetInternal.
*
* @param	AssetsToDuplicate	The animations to duplicate
* @param	DestinationPackage	The package that the duplicates should be placed in
* @param	NameRule			The rules for how to rename the duplicated assets
*
* @return	TMap of original animation to duplicate
*/
template<class AssetType>
TMap<AssetType*, AssetType*> FIKRetargetBatchOperation::DuplicateAssets(
	const TArray<AssetType*>& AssetsToDuplicate,
	UPackage* DestinationPackage,
	const FNameDuplicationRule* NameRule)
{
	TArray<UObject*> Assets;
	for (AssetType* Asset : AssetsToDuplicate)
	{
		Assets.Add(Asset);
	}

	// duplicate assets
	TMap<UObject*, UObject*> DuplicateAssetsMap = DuplicateAssetsInternal(Assets, DestinationPackage, NameRule);

	// cast to AssetType
	TMap<AssetType*, AssetType*> ReturnMap;
	for (const TTuple<UObject*, UObject*>& DuplicateAsset : DuplicateAssetsMap)
	{
		ReturnMap.Add(Cast<AssetType>(DuplicateAsset.Key), Cast<AssetType>(DuplicateAsset.Value));
	}
	
	return ReturnMap;
}

#undef LOCTEXT_NAMESPACE
