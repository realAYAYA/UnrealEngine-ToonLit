// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeGenericAnimationPipeline.h"

#include "Animation/AnimationSettings.h"
#include "CoreMinimal.h"
#include "InterchangeLevelSequenceFactoryNode.h"
#include "InterchangeAnimationTrackSetNode.h"
#include "InterchangeAnimSequenceFactoryNode.h"
#include "InterchangeMeshNode.h"
#include "InterchangePipelineLog.h"
#include "InterchangeSceneNode.h"
#include "InterchangeSkeletalMeshFactoryNode.h"
#include "InterchangeSkeletalMeshLodDataNode.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSkeletonHelper.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeSourceNode.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGenericAnimationPipeline)

#if WITH_EDITOR
bool UInterchangeGenericAnimationPipeline::CanEditChange(const FProperty* InProperty) const
{
	// If other logic prevents editing, we want to respect that
	const bool ParentVal = Super::CanEditChange(InProperty);

	// Can we edit flower color?
	if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UInterchangeGenericAnimationPipeline, FrameImportRange))
	{
		return ParentVal && bImportAnimations && bImportBoneTracks && AnimationRange == EInterchangeAnimationRange::SetRange;
	}
	return ParentVal;
}
#endif

void UInterchangeGenericAnimationPipeline::AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset)
{
	Super::AdjustSettingsForContext(ImportType, ReimportAsset);

#if WITH_EDITOR
	check(CommonSkeletalMeshesAndAnimationsProperties.IsValid());
	
	bSceneImport = ImportType == EInterchangePipelineContext::SceneImport
				|| ImportType == EInterchangePipelineContext::SceneReimport;

	if (ImportType == EInterchangePipelineContext::AssetCustomLODImport
		|| ImportType == EInterchangePipelineContext::AssetCustomLODReimport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningImport
		|| ImportType == EInterchangePipelineContext::AssetAlternateSkinningReimport)
	{
		bImportAnimations = false;
		CommonSkeletalMeshesAndAnimationsProperties->Skeleton = nullptr;
		CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations = false;
	}
	
	TArray<FString> HideCategories;
	if (ImportType == EInterchangePipelineContext::AssetReimport)
	{
		if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(ReimportAsset))
		{
			//Set the skeleton to the current asset skeleton and re-import only the animation
			CommonSkeletalMeshesAndAnimationsProperties->Skeleton = AnimSequence->GetSkeleton();
			CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations = true;
		}
		else
		{
			HideCategories.Add(TEXT("Animations"));
		}
	}

	if (UInterchangePipelineBase* OuterMostPipeline = GetMostPipelineOuter())
	{
		for (const FString& HideCategoryName : HideCategories)
		{
			HidePropertiesOfCategory(OuterMostPipeline, this, HideCategoryName);
		}
	}
#endif //WITH_EDITOR
}

void UInterchangeGenericAnimationPipeline::ExecutePipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas, const FString& ContentBasePath)
{
	if (!InBaseNodeContainer)
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericAnimationPipeline: Cannot execute pre-import pipeline because InBaseNodeContainer is null."));
		return;
	}

	BaseNodeContainer = InBaseNodeContainer;

	if (!bImportAnimations)
	{
		//Nothing to import
		return;
	}

	TArray<UInterchangeAnimationTrackSetNode*> TrackSetNodes;
	BaseNodeContainer->IterateNodesOfType<UInterchangeAnimationTrackSetNode>([&](const FString& NodeUid, UInterchangeAnimationTrackSetNode* Node)
		{
			TrackSetNodes.Add(Node);
		});


	//Create AnimSequences(UInterchangeSkeletalAnimationTrackNode) for Mesh Instances having MorphTargetCurveWeights:
	{
		TArray<UInterchangeSceneNode*> SceneNodesWithMorphTargetCurveWeights;
		BaseNodeContainer->IterateNodesOfType<UInterchangeSceneNode>([&SceneNodesWithMorphTargetCurveWeights](const FString& NodeUid, UInterchangeSceneNode* SceneNode)
			{
				TMap<FString, float> MorphTargetCurveWeights;
				SceneNode->GetMorphTargetCurveWeights(MorphTargetCurveWeights);

				bool CreateAnimSequence = false;
				for (const TTuple<FString, float>& MorphTargetCurveWeight : MorphTargetCurveWeights)
				{
					if (MorphTargetCurveWeight.Value != 0)
					{
						CreateAnimSequence = true;
						break;
					}
				}

				if (CreateAnimSequence)
				{
					SceneNodesWithMorphTargetCurveWeights.Add(SceneNode);
				}
			});

		for (UInterchangeSceneNode* SceneNode : SceneNodesWithMorphTargetCurveWeights)
		{
			UInterchangeSkeletalAnimationTrackNode* SkeletalAnimationNode = NewObject< UInterchangeSkeletalAnimationTrackNode >(BaseNodeContainer);
			FString SkeletalAnimationNodeUid = "\\SkeletalAnimation\\MorphTargetCurveWeightInstantation\\" + SceneNode->GetUniqueID();
			SkeletalAnimationNode->InitializeNode(SkeletalAnimationNodeUid, SceneNode->GetDisplayLabel(), EInterchangeNodeContainerType::TranslatedAsset);

			SkeletalAnimationNode->SetCustomAnimationSampleRate(30.f);
			SkeletalAnimationNode->SetCustomAnimationStartTime(0);
			SkeletalAnimationNode->SetCustomAnimationStopTime(1.f / 30.f); //we want a single frame

			SkeletalAnimationNode->SetCustomSkeletonNodeUid(SceneNode->GetUniqueID());

			TMap<FString, float> MorphTargetCurveWeights;
			SceneNode->GetMorphTargetCurveWeights(MorphTargetCurveWeights);

			for (const TPair<FString, float>& MorphTargetCurveWeight : MorphTargetCurveWeights)
			{
				FString PayloadUid = MorphTargetCurveWeight.Key + TEXT(":") + LexToString(MorphTargetCurveWeight.Value);

				//add the payload key:
				SkeletalAnimationNode->SetAnimationPayloadKeyForMorphTargetNodeUid(MorphTargetCurveWeight.Key, PayloadUid, EInterchangeAnimationPayLoadType::MORPHTARGETCURVEWEIGHTINSTANCE);
			}

			BaseNodeContainer->AddNode(SkeletalAnimationNode);

			SceneNode->SetCustomAnimationAssetUidToPlay(SkeletalAnimationNodeUid);
		}
	}

	if (!bSceneImport)
	{
		//Extract any skeleton node use by the skeletal mesh animation track
		TArray<FString> SceneNodesUsedBySkeleton;
		BaseNodeContainer->IterateNodesOfType<UInterchangeSkeletalAnimationTrackNode>([&](const FString& NodeUid, UInterchangeSkeletalAnimationTrackNode* Node)
			{
				if (Node)
				{
					FString SkeletonNodeUid;
					if (Node->GetCustomSkeletonNodeUid(SkeletonNodeUid))
					{
						SceneNodesUsedBySkeleton.Add(SkeletonNodeUid);
						BaseNodeContainer->IterateNodeChildren(SkeletonNodeUid, [&SceneNodesUsedBySkeleton](const UInterchangeBaseNode* Node)
							{
								if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
								{
									SceneNodesUsedBySkeleton.Add(Node->GetUniqueID());
								}
							});
					}
				}
			});

		auto IsTrackOverrideBySkeletalMeshAnimation = [this, &SceneNodesUsedBySkeleton](TArray<FString>& AnimationTrackUids)
			{
				//Skip track node that is using one or more scene node used by any skeletal mesh skeleton.
				bool bSomeTrackNodeUsedBySkeletalMesh = false;
				for (const FString& AnimationTrackUid : AnimationTrackUids)
				{
					if (const UInterchangeTransformAnimationTrackNode* TransformTrackNode = Cast<UInterchangeTransformAnimationTrackNode>(BaseNodeContainer->GetNode(AnimationTrackUid)))
					{
						FString ActorNodeUid;
						if (TransformTrackNode->GetCustomActorDependencyUid(ActorNodeUid))
						{
							if (SceneNodesUsedBySkeleton.Contains(ActorNodeUid))
							{
								bSomeTrackNodeUsedBySkeletalMesh = true;
								break;
							}
						}
					}
				}
				return bSomeTrackNodeUsedBySkeletalMesh;
			};
		//Support rigid mesh animation animation data  (UAnimSequence for rigid mesh)
		for (UInterchangeAnimationTrackSetNode* TrackSetNode : TrackSetNodes)
		{
			if (!TrackSetNode)
			{
				continue;
			}

			TArray<FString> AnimationTrackUids;
			TrackSetNode->GetCustomAnimationTrackUids(AnimationTrackUids);
			
			if (IsTrackOverrideBySkeletalMeshAnimation(AnimationTrackUids))
			{
				continue;
			}

			UInterchangeSkeletalAnimationTrackNode* SkeletalAnimationNode = NewObject< UInterchangeSkeletalAnimationTrackNode >(BaseNodeContainer);
			FString SkeletalAnimationNodeUid = "\\SkeletalAnimation\\ConvertedFromRigidAnimation\\" + TrackSetNode->GetUniqueID();
			SkeletalAnimationNode->InitializeNode(SkeletalAnimationNodeUid, TrackSetNode->GetDisplayLabel(), EInterchangeNodeContainerType::TranslatedAsset);

			bool bCustomSkeletonNodeUidSet = false;

			
			float CustomFrameRate;
			if (!TrackSetNode->GetCustomFrameRate(CustomFrameRate))
			{
				CustomFrameRate = 30.0f;
			}
			SkeletalAnimationNode->SetCustomAnimationSampleRate(CustomFrameRate);
			SkeletalAnimationNode->SetCustomAnimationStartTime(0);
			//stop time will be calculated once we get the curves from the Translators.
			SkeletalAnimationNode->SetCustomAnimationStopTime(0);

			for (const FString& AnimationTrackUid : AnimationTrackUids)
			{
				if (const UInterchangeTransformAnimationTrackNode* TransformTrackNode = Cast<UInterchangeTransformAnimationTrackNode>(BaseNodeContainer->GetNode(AnimationTrackUid)))
				{
					FString ActorNodeUid;
					if (TransformTrackNode->GetCustomActorDependencyUid(ActorNodeUid))
					{
						if (const UInterchangeSceneNode* ActorNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ActorNodeUid)))
						{
							FInterchangeAnimationPayLoadKey AnimationPayLoadKey;
							if (TransformTrackNode->GetCustomAnimationPayloadKey(AnimationPayLoadKey))
							{
								if (!bCustomSkeletonNodeUidSet)
								{
									FString SkeletonRootUid;
									FString LastSceneNode = ActorNodeUid;
									if (const UInterchangeSceneNode* SceneNode = ActorNode)
									{
										FString ParentUid = SceneNode->GetParentUid();
										while (!ParentUid.Equals(UInterchangeBaseNode::InvalidNodeUid()))
										{
											if (const UInterchangeSceneNode* ParentNode = Cast<UInterchangeSceneNode>(BaseNodeContainer->GetNode(ParentUid)))
											{
												if(ParentNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
												{
													SkeletonRootUid = ParentUid;
												}
												LastSceneNode = ParentUid;
												ParentUid = ParentNode->GetParentUid();
											}
										}
										if (SkeletonRootUid.IsEmpty())
										{
											SkeletonRootUid = LastSceneNode;
										}
									}

									SkeletalAnimationNode->SetCustomSkeletonNodeUid(SkeletonRootUid);
									bCustomSkeletonNodeUidSet = true;
								}

								//add the payload key:
								SkeletalAnimationNode->SetAnimationPayloadKeyForSceneNodeUid(ActorNode->GetUniqueID(), AnimationPayLoadKey.UniqueId, AnimationPayLoadKey.Type);
							}
						}
					}
				}
			}

			if (bCustomSkeletonNodeUidSet)
			{
				//if no SkeletonNodeUid can be set, then the conversion failed and should not be added to the BaseNodeContainer.
				BaseNodeContainer->AddNode(SkeletalAnimationNode);
			}
		}
	}
	else
	{
		//Support scene node animation (ULevelSequence, only supported when doing scene import)
		for (UInterchangeAnimationTrackSetNode* TrackSetNode : TrackSetNodes)
		{
			if (TrackSetNode)
			{
				CreateLevelSequenceFactoryNode(*TrackSetNode);
			}
		}
	}

	if (!CommonSkeletalMeshesAndAnimationsProperties.IsValid())
	{
		return;
	}

	if (CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations && !CommonSkeletalMeshesAndAnimationsProperties->Skeleton.IsValid())
	{
		UE_LOG(LogInterchangePipeline, Warning, TEXT("UInterchangeGenericAnimationPipeline: Cannot execute pre-import pipeline because importing animation only requires a valid skeleton."));
		return;
	}
	SourceDatas.Empty(InSourceDatas.Num());
	for (const UInterchangeSourceData* SourceData : InSourceDatas)
	{
		SourceDatas.Add(SourceData);
	}

	TArray<UInterchangeSkeletalAnimationTrackNode*> TrackNodes;
	BaseNodeContainer->IterateNodesOfType<UInterchangeSkeletalAnimationTrackNode>([&](const FString& NodeUid, UInterchangeSkeletalAnimationTrackNode* Node)
		{
			TrackNodes.Add(Node);
		});

	//Support skeletal mesh animation (UAnimSequence)
	for (UInterchangeSkeletalAnimationTrackNode* TrackNode : TrackNodes)
	{
		if (TrackNode)
		{
			CreateAnimSequenceFactoryNode(*TrackNode);
		}
	}
}

void UInterchangeGenericAnimationPipeline::CreateLevelSequenceFactoryNode(UInterchangeAnimationTrackSetNode& TranslatedNode)
{
	const FString FactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(TranslatedNode.GetUniqueID());

	UInterchangeLevelSequenceFactoryNode* FactoryNode = NewObject<UInterchangeLevelSequenceFactoryNode>(BaseNodeContainer, NAME_None);

	FactoryNode->InitializeNode(FactoryNodeUid, TranslatedNode.GetDisplayLabel(), EInterchangeNodeContainerType::FactoryData);
	FactoryNode->SetEnabled(true);

	TArray<FString> AnimationTrackUids;
	TranslatedNode.GetCustomAnimationTrackUids(AnimationTrackUids);

	for (const FString& AnimationTrackUid : AnimationTrackUids)
	{
		FactoryNode->AddCustomAnimationTrackUid(AnimationTrackUid);

		// Update factory's dependencies
		if (const UInterchangeAnimationTrackBaseNode* TrackNode = Cast<UInterchangeAnimationTrackBaseNode>(BaseNodeContainer->GetNode(AnimationTrackUid)))
		{
			if (const UInterchangeTransformAnimationTrackNode* TransformTrackNode = Cast<UInterchangeTransformAnimationTrackNode>(TrackNode))
			{
				FString ActorNodeUid;
				if (TransformTrackNode->GetCustomActorDependencyUid(ActorNodeUid))
				{
					const FString ActorFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(ActorNodeUid);
					FactoryNode->AddFactoryDependencyUid(ActorFactoryNodeUid);
				}
			}
			else if (const UInterchangeAnimationTrackSetInstanceNode* InstanceTrackNode = Cast<UInterchangeAnimationTrackSetInstanceNode>(TrackNode))
			{
				FString TrackSetNodeUid;
				if (InstanceTrackNode->GetCustomTrackSetDependencyUid(TrackSetNodeUid))
				{
					const FString TrackSetFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(TrackSetNodeUid);
					FactoryNode->AddFactoryDependencyUid(TrackSetFactoryNodeUid);
				}

			}
		}
	}

	float FrameRate;
	if (TranslatedNode.GetCustomFrameRate(FrameRate))
	{
		FactoryNode->SetCustomFrameRate(FrameRate);
	}

	UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(&TranslatedNode, FactoryNode, false);

	FactoryNode->AddTargetNodeUid(TranslatedNode.GetUniqueID());
	TranslatedNode.AddTargetNodeUid(FactoryNode->GetUniqueID());

	BaseNodeContainer->AddNode(FactoryNode);
}

void UInterchangeGenericAnimationPipeline::CreateAnimSequenceFactoryNode(UInterchangeSkeletalAnimationTrackNode& TrackNode)
{
	FString SkeletonNodeUid;
	if (!ensure(TrackNode.GetCustomSkeletonNodeUid(SkeletonNodeUid)))
	{
		// TODO: Warn something wrong happened
		return;
	}
	const bool bImportOnlyAnimation = CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations;

	const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = nullptr;
	const UInterchangeSkeletalMeshFactoryNode* SkeletalMeshFactoryNode = nullptr;

	const FString SkeletonFactoryNodeUid = UInterchangeFactoryBaseNode::BuildFactoryNodeUid(SkeletonNodeUid);
	SkeletonFactoryNode = Cast<const UInterchangeSkeletonFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletonFactoryNodeUid));

	//If we import anim only and we do not have meshes and skeleton. We need to create a skeleton factory node
	//base on the specified skeleton
	if(bImportOnlyAnimation && !SkeletonFactoryNode && CommonSkeletalMeshesAndAnimationsProperties->Skeleton.IsValid())
	{
		const FReferenceSkeleton& ReferenceSkeleton = CommonSkeletalMeshesAndAnimationsProperties->Skeleton->GetReferenceSkeleton();
		TArray<FString> SkeletonRootNodeUids;
		BaseNodeContainer->IterateNodesOfType<UInterchangeSceneNode>([&SkeletonRootNodeUids, BaseNodeContainerClosure = BaseNodeContainer, &ReferenceSkeleton](const FString& NodeUid, UInterchangeSceneNode* Node)
		{
			if (Node->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
			{
				if(ReferenceSkeleton.FindBoneIndex(FName(*Node->GetDisplayLabel())) != INDEX_NONE)
				{
					const FString ParentUid = Node->GetParentUid();
					if (const UInterchangeSceneNode* ParentNode = Cast<UInterchangeSceneNode>(BaseNodeContainerClosure->GetNode(ParentUid)))
					{
						if (!ParentNode->IsSpecializedTypeContains(UE::Interchange::FSceneNodeStaticData::GetJointSpecializeTypeString()))
						{
							SkeletonRootNodeUids.Add(NodeUid);
						}
					}
				}
			}
		});
		FString SkeletonRootUid;
		if (SkeletonRootNodeUids.Num() > 0)
		{
			SkeletonRootUid = SkeletonRootNodeUids[0];
		}
		if(!SkeletonRootUid.IsEmpty())
		{
			//Create a skeleton node from all the joint in the translated nodes
			SkeletonFactoryNode = CommonSkeletalMeshesAndAnimationsProperties->CreateSkeletonFactoryNode(BaseNodeContainer, SkeletonRootUid);
		}
	}

	if (!SkeletonFactoryNode)
	{
		// It can happen if we force static mesh import, in that case no skeleton will be create
		return;
	}

	FString SkeletalMeshFactoryNodeUid;

	if (SkeletonFactoryNode->GetCustomSkeletalMeshFactoryNodeUid(SkeletalMeshFactoryNodeUid))
	{
		SkeletalMeshFactoryNode = Cast<const UInterchangeSkeletalMeshFactoryNode>(BaseNodeContainer->GetFactoryNode(SkeletalMeshFactoryNodeUid));
	}

	double SampleRate = 30.;
	double StartTime = 0.;
	double StopTime = 0.;
	bool bTimeRangeIsValid = false;

	if (bImportBoneTracks)
	{
		if (const UInterchangeSourceNode* SourceNode = UInterchangeSourceNode::GetUniqueInstance(BaseNodeContainer))
		{
			int32 Numerator, Denominator;
			if (!bUse30HzToBakeBoneAnimation && CustomBoneAnimationSampleRate == 0 && SourceNode->GetCustomSourceFrameRateNumerator(Numerator))
			{
				if (SourceNode->GetCustomSourceFrameRateDenominator(Denominator) && Denominator > 0 && Numerator > 0)
				{
					SampleRate = static_cast<double>(Numerator) / static_cast<double>(Denominator);
				}
			}

			if (AnimationRange == EInterchangeAnimationRange::Timeline)
			{
				if (SourceNode->GetCustomSourceTimelineStart(StartTime))
				{
					if (SourceNode->GetCustomSourceTimelineEnd(StopTime))
					{
						bTimeRangeIsValid = true;
					}
				}
			}
			else if (AnimationRange == EInterchangeAnimationRange::Animated)
			{
				if (SourceNode->GetCustomAnimatedTimeStart(StartTime))
				{
					if (SourceNode->GetCustomAnimatedTimeEnd(StopTime))
					{
						bTimeRangeIsValid = true;
					}
				}
			}
			else if (AnimationRange == EInterchangeAnimationRange::SetRange)
			{
				StartTime = static_cast<double>(FrameImportRange.Min) / SampleRate;
				StopTime = static_cast<double>(FrameImportRange.Max) / SampleRate;
				bTimeRangeIsValid = true;
			}
		}
		else
		{
			bTimeRangeIsValid = TrackNode.GetCustomAnimationSampleRate(SampleRate);
			bTimeRangeIsValid &= TrackNode.GetCustomAnimationStartTime(StartTime);
			bTimeRangeIsValid &= TrackNode.GetCustomAnimationStopTime(StopTime);
		}

		if (!bUse30HzToBakeBoneAnimation && CustomBoneAnimationSampleRate > 0)
		{
			SampleRate = static_cast<double>(CustomBoneAnimationSampleRate);
		}

		FFrameRate FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(SampleRate);

		const double SequenceLength = FMath::Max<double>(StopTime - StartTime, MINIMUM_ANIMATION_LENGTH);

		const float SubFrame = FrameRate.AsFrameTime(SequenceLength).GetSubFrame();

		if (!FMath::IsNearlyZero(SubFrame, KINDA_SMALL_NUMBER) && !FMath::IsNearlyEqual(SubFrame, 1.0f, KINDA_SMALL_NUMBER))
		{
			if (bSnapToClosestFrameBoundary)
			{
				// Figure out whether start or stop has to be adjusted
				const FFrameTime StartFrameTime = FrameRate.AsFrameTime(StartTime);
				const FFrameTime StopFrameTime = FrameRate.AsFrameTime(StopTime);
				FFrameNumber StartFrameNumber = StartFrameTime.GetFrame().Value, StopFrameNumber = StopFrameTime.GetFrame().Value;
				double NewStartTime = StartTime, NewStopTime = StopTime;

				if (!FMath::IsNearlyZero(StartFrameTime.GetSubFrame()))
				{
					StartFrameNumber = StartFrameTime.RoundToFrame();
					NewStartTime = FrameRate.AsSeconds(StartFrameNumber);
				}

				if (!FMath::IsNearlyZero(StopFrameTime.GetSubFrame()))
				{
					StopFrameNumber = StopFrameTime.RoundToFrame();
					NewStopTime = FrameRate.AsSeconds(StopFrameNumber);
				}
			
				UInterchangeResultWarning_Generic* Message = AddMessage<UInterchangeResultWarning_Generic>();
				Message->SourceAssetName = SourceDatas[0]->GetFilename();
				Message->DestinationAssetName = TrackNode.GetDisplayLabel();
				Message->AssetType = UAnimSequence::StaticClass();
				Message->Text = FText::Format(NSLOCTEXT("UInterchangeGenericAnimationPipeline", "Info_ImportLengthSnap", "Animation length has been adjusted to align with frame borders using import frame-rate {0}.\n\nOriginal timings:\n\t\tStart: {1} ({2})\n\t\tStop: {3} ({4})\nAligned timings:\n\t\tStart: {5} ({6})\n\t\tStop: {7} ({8})"),
					FrameRate.ToPrettyText(),
					FText::AsNumber(StartTime),
					FText::AsNumber(StartFrameTime.AsDecimal()),
					FText::AsNumber(StopTime),
					FText::AsNumber(StopFrameTime.AsDecimal()),
					FText::AsNumber(NewStartTime),
					FText::AsNumber(StartFrameNumber.Value),
					FText::AsNumber(NewStopTime),
					FText::AsNumber(StopFrameNumber.Value));

				StartTime = NewStartTime;
				StopTime = NewStopTime;
			}
			else
			{
				UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
				Message->SourceAssetName = SourceDatas[0]->GetFilename();
				Message->DestinationAssetName = TrackNode.GetDisplayLabel();
				Message->AssetType = UAnimSequence::StaticClass();
				Message->Text = FText::Format(NSLOCTEXT("UInterchangeGenericAnimationPipeline", "WrongSequenceLength", "Animation length {0} is not compatible with import frame-rate {1} (sub frame {2}). The animation has to be frame-border aligned."),
					FText::AsNumber(SequenceLength), FrameRate.ToPrettyText(), FText::AsNumber(SubFrame));
				//Skip this anim sequence factory node
				return;
			}
		}
	}

	const FString AnimSequenceUid = TEXT("\\AnimSequence") + TrackNode.GetUniqueID();

	UInterchangeAnimSequenceFactoryNode* AnimSequenceFactoryNode = NewObject<UInterchangeAnimSequenceFactoryNode>(BaseNodeContainer, NAME_None);
	AnimSequenceFactoryNode->InitializeAnimSequenceNode(AnimSequenceUid, TrackNode.GetDisplayLabel());

	if (SkeletonFactoryNode)
	{
		AnimSequenceFactoryNode->SetCustomSkeletonFactoryNodeUid(SkeletonFactoryNode->GetUniqueID());
	}
	if (SkeletalMeshFactoryNode)
	{
		AnimSequenceFactoryNode->AddFactoryDependencyUid(SkeletalMeshFactoryNode->GetUniqueID());
	}

	AnimSequenceFactoryNode->SetCustomImportBoneTracks(bImportBoneTracks);
	AnimSequenceFactoryNode->SetCustomImportBoneTracksSampleRate(SampleRate);
	if (bTimeRangeIsValid)
	{
		AnimSequenceFactoryNode->SetCustomImportBoneTracksRangeStart(StartTime);
		AnimSequenceFactoryNode->SetCustomImportBoneTracksRangeStop(StopTime);
	}

	AnimSequenceFactoryNode->SetCustomImportAttributeCurves(bImportCustomAttribute);
	AnimSequenceFactoryNode->SetCustomAddCurveMetadataToSkeleton(bAddCurveMetadataToSkeleton);
	AnimSequenceFactoryNode->SetCustomDoNotImportCurveWithZero(bDoNotImportCurveWithZero);
	AnimSequenceFactoryNode->SetCustomRemoveCurveRedundantKeys(bRemoveCurveRedundantKeys);
	AnimSequenceFactoryNode->SetCustomDeleteExistingMorphTargetCurves(bDeleteExistingMorphTargetCurves);
	AnimSequenceFactoryNode->SetCustomDeleteExistingCustomAttributeCurves(bDeleteExistingCustomAttributeCurves);
	AnimSequenceFactoryNode->SetCustomDeleteExistingNonCurveCustomAttributes(bDeleteExistingNonCurveCustomAttributes);

	AnimSequenceFactoryNode->SetCustomMaterialDriveParameterOnCustomAttribute(bSetMaterialDriveParameterOnCustomAttribute);
	for (const FString& MaterialSuffixe : MaterialCurveSuffixes)
	{
		AnimSequenceFactoryNode->SetAnimatedMaterialCurveSuffixe(MaterialSuffixe);
	}

	//USkeleton cannot be created without a valid skeletal mesh
	FString SkeletonUid;
	if(SkeletonFactoryNode)
	{
		SkeletonUid = SkeletonFactoryNode->GetUniqueID();
		AnimSequenceFactoryNode->AddFactoryDependencyUid(SkeletonUid);
	}

	FString RootJointUid;
	if (SkeletonFactoryNode && SkeletonFactoryNode->GetCustomRootJointUid(RootJointUid))
	{
		// NOTE: Could this be added as an array of FString attributes on the UInterchangeSkeletalAnimationTrackNode
#if WITH_EDITOR
		//Iterate all joints to set the meta data value in the anim sequence factory node
		UE::Interchange::Private::FSkeletonHelper::RecursiveAddSkeletonMetaDataValues(BaseNodeContainer, AnimSequenceFactoryNode, RootJointUid);
#endif //WITH_EDITOR

		const TArray<FString> CustomAttributeNamesToImport = UAnimationSettings::Get()->GetBoneCustomAttributeNamesToImport();

		BaseNodeContainer->IterateNodeChildren(RootJointUid, [&AnimSequenceFactoryNode, &CustomAttributeNamesToImport](const UInterchangeBaseNode* Node)
			{
				if (const UInterchangeSceneNode* SceneNode = Cast<UInterchangeSceneNode>(Node))
				{
					using namespace UE::Interchange;

					FString BoneName = SceneNode->GetDisplayLabel();
					bool bImportAllAttributesOnBone = UAnimationSettings::Get()->BoneNamesWithCustomAttributes.Contains(BoneName);

					TArray<FInterchangeUserDefinedAttributeInfo> AttributeInfos;
					UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttributeInfos(SceneNode, AttributeInfos);
					for (const FInterchangeUserDefinedAttributeInfo& AttributeInfo : AttributeInfos)
					{
						if (AttributeInfo.PayloadKey.IsSet())
						{
							bool bDecimalType = AttributeInfo.Type == EAttributeTypes::Float ||
								AttributeInfo.Type == EAttributeTypes::Float16 ||
								AttributeInfo.Type == EAttributeTypes::Double;

							const bool bForceImportBoneCustomAttribute = CustomAttributeNamesToImport.Contains(AttributeInfo.Name);
							//Material attribute curve
							if (!bImportAllAttributesOnBone && bDecimalType && !bForceImportBoneCustomAttribute)
							{
								AnimSequenceFactoryNode->SetAnimatedAttributeCurveName(AttributeInfo.Name);
							}
							else if (bForceImportBoneCustomAttribute || bImportAllAttributesOnBone)
							{
								AnimSequenceFactoryNode->SetAnimatedAttributeStepCurveName(AttributeInfo.Name);
							}
						}
					}
				}
			});
	}

	//Iterate dependencies
	{
		TArray<FString> SkeletalMeshNodeUids;
		BaseNodeContainer->GetNodes(UInterchangeSkeletalMeshFactoryNode::StaticClass(), SkeletalMeshNodeUids);
		for (const FString& SkelMeshFactoryNodeUid : SkeletalMeshNodeUids)
		{
			if (const UInterchangeSkeletalMeshFactoryNode* ConstSkeletalMeshFactoryNode = Cast<const UInterchangeSkeletalMeshFactoryNode>(BaseNodeContainer->GetFactoryNode(SkelMeshFactoryNodeUid)))
			{
				TArray<FString> SkeletalMeshDependencies;
				ConstSkeletalMeshFactoryNode->GetFactoryDependencies(SkeletalMeshDependencies);
				for (const FString& SkeletalMeshDependencyUid : SkeletalMeshDependencies)
				{
					if (SkeletonUid.Equals(SkeletalMeshDependencyUid))
					{
						AnimSequenceFactoryNode->AddFactoryDependencyUid(SkelMeshFactoryNodeUid);
						break;
					}
				}
			}
		}
	}

	if (CommonSkeletalMeshesAndAnimationsProperties->Skeleton.IsValid())
	{
		bool bSkeletonCompatible = true;

		//TODO: support skeleton helper in runtime
#if WITH_EDITOR
		bSkeletonCompatible = UE::Interchange::Private::FSkeletonHelper::IsCompatibleSkeleton(CommonSkeletalMeshesAndAnimationsProperties->Skeleton.Get(), RootJointUid, BaseNodeContainer, CommonSkeletalMeshesAndAnimationsProperties->bConvertStaticsWithMorphTargetsToSkeletals || CommonMeshesProperties->ForceAllMeshAsType == EInterchangeForceMeshType::IFMT_SkeletalMesh);
#endif
		if (bSkeletonCompatible)
		{
			FSoftObjectPath SkeletonSoftObjectPath(CommonSkeletalMeshesAndAnimationsProperties->Skeleton.Get());
			AnimSequenceFactoryNode->SetCustomSkeletonSoftObjectPath(SkeletonSoftObjectPath);
		}
		else
		{
			UInterchangeResultDisplay_Generic* Message = AddMessage<UInterchangeResultDisplay_Generic>();
			Message->Text = FText::Format(NSLOCTEXT("UInterchangeGenericAnimationPipeline", "IncompatibleSkeleton", "Incompatible skeleton {0} when importing AnimSequence {1}."),
				FText::FromString(CommonSkeletalMeshesAndAnimationsProperties->Skeleton->GetName()),
				FText::FromString(TrackNode.GetDisplayLabel()));
		}
	}

	{
		TMap<FString, FString> SceneNodeAnimationPayloadKeyUids;
		TMap<FString, uint8> SceneNodeAnimationPayloadKeyTypes;
		TrackNode.GetSceneNodeAnimationPayloadKeys(SceneNodeAnimationPayloadKeyUids, SceneNodeAnimationPayloadKeyTypes);
		AnimSequenceFactoryNode->SetAnimationPayloadKeysForSceneNodeUids(SceneNodeAnimationPayloadKeyUids, SceneNodeAnimationPayloadKeyTypes);
	}

	{
		TMap<FString, FString> MorphTargetNodeAnimationPayloadKeyUids;
		TMap<FString, uint8> MorphTargetNodeAnimationPayloadKeyTypes;
		TrackNode.GetMorphTargetNodeAnimationPayloadKeys(MorphTargetNodeAnimationPayloadKeyUids, MorphTargetNodeAnimationPayloadKeyTypes);
		AnimSequenceFactoryNode->SetAnimationPayloadKeysForMorphTargetNodeUids(MorphTargetNodeAnimationPayloadKeyUids, MorphTargetNodeAnimationPayloadKeyTypes);
	}

	UInterchangeUserDefinedAttributesAPI::DuplicateAllUserDefinedAttribute(&TrackNode, AnimSequenceFactoryNode, false);

	AnimSequenceFactoryNode->AddTargetNodeUid(TrackNode.GetUniqueID());
	TrackNode.AddTargetNodeUid(AnimSequenceFactoryNode->GetUniqueID());

	BaseNodeContainer->AddNode(AnimSequenceFactoryNode);
}