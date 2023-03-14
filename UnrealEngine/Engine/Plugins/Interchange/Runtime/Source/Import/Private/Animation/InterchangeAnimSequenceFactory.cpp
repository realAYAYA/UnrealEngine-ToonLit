// Copyright Epic Games, Inc. All Rights Reserved. 
#include "Animation/InterchangeAnimSequenceFactory.h"

#include "Animation/AnimSequence.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/InterchangeAnimationPayload.h"
#include "Animation/InterchangeAnimationPayloadInterface.h"
#include "InterchangeCommonPipelineDataFactoryNode.h"
#include "InterchangeAssetImportData.h"
#include "InterchangeCommonAnimationPayload.h"
#include "InterchangeImportCommon.h"
#include "InterchangeImportLog.h"
#include "InterchangeMeshNode.h"
#include "InterchangeSceneNode.h"
#include "InterchangeAnimSequenceFactoryNode.h"
#include "InterchangeSkeletonFactoryNode.h"
#include "InterchangeSourceData.h"
#include "InterchangeTranslatorBase.h"
#include "Nodes/InterchangeAnimationAPI.h"
#include "Nodes/InterchangeBaseNode.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "Nodes/InterchangeSourceNode.h"
#include "Nodes/InterchangeUserDefinedAttribute.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeAnimSequenceFactory)

#if WITH_EDITORONLY_DATA

#include "EditorFramework/AssetImportData.h"

#endif //WITH_EDITORONLY_DATA

#if WITH_EDITOR
namespace UE::Interchange::Private
{
	void GetSkeletonSceneNodeFlatListRecursive(const UInterchangeBaseNodeContainer* NodeContainer, const FString& NodeUid, TArray<FString>& SkeletonSceneNodeUids)
	{
		SkeletonSceneNodeUids.Add(NodeUid);
		TArray<FString> Children = NodeContainer->GetNodeChildrenUids(NodeUid);
		for (const FString& ChildUid : Children)
		{
			GetSkeletonSceneNodeFlatListRecursive(NodeContainer, ChildUid, SkeletonSceneNodeUids);
		}
	}
	
	template<typename ValueType>
	bool AreAllValuesZero(const TArray<ValueType>& Values, TFunctionRef<bool(const ValueType& Value)> CompareValueToZeroFunction)
	{
		const int32 KeyCount = Values.Num();
		for (int32 KeyIndex = 1; KeyIndex < KeyCount; ++KeyIndex)
		{
			//Only add the not equal keys
			if (!CompareValueToZeroFunction(Values[KeyIndex]))
			{
				return false;
			}
		}
		return true;
	}

	bool CreateAttributeStepCurve(UAnimSequence* TargetSequence
		, FAnimationStepCurvePayloadData& StepCurvePayload
		, const FString& CurveName
		, const FString& BoneName
		, const int32 CurveFlags
		, const bool bDoNotImportCurveWithZero)
	{
		//For bone attribute we support only single curve type (structured type like vector are not allowed)
		if (!TargetSequence || StepCurvePayload.StepCurves.Num() != 1 || CurveName.IsEmpty())
		{
			return false;
		}

		if (bDoNotImportCurveWithZero)
		{
			bool bAllCurveValuesZero = true;
			for (const FInterchangeStepCurve& StepCurve : StepCurvePayload.StepCurves)
			{
				if (StepCurve.FloatKeyValues.IsSet())
				{
					if (!AreAllValuesZero<float>(StepCurve.FloatKeyValues.GetValue(), [](const float& Value)
						{
							return FMath::IsNearlyZero(Value);
						}))
					{
						bAllCurveValuesZero = false;
						break;
					}
				}
				else if (StepCurve.IntegerKeyValues.IsSet())
				{
					if (!AreAllValuesZero<int32>(StepCurve.IntegerKeyValues.GetValue(), [](const int32& Value)
						{
							return (Value == 0);
						}))
					{
						bAllCurveValuesZero = false;
						break;
					}
				}
				else if (StepCurve.StringKeyValues.IsSet())
				{
					if (!AreAllValuesZero<FString>(StepCurve.StringKeyValues.GetValue(), [](const FString& Value)
						{
							return Value.IsEmpty();
						}))
					{
						bAllCurveValuesZero = false;
						break;
					}
				}
			}
			if (bAllCurveValuesZero)
			{
				return false;
			}
		}

		bool bResult = false;
		for (const FInterchangeStepCurve& StepCurve : StepCurvePayload.StepCurves)
		{
			if (StepCurve.FloatKeyValues.IsSet())
			{
				bResult |= UE::Anim::AddTypedCustomAttribute<FFloatAnimationAttribute, float>(FName(CurveName)
					, FName(BoneName)
					, TargetSequence
					, MakeArrayView(StepCurve.KeyTimes)
					, MakeArrayView(StepCurve.FloatKeyValues.GetValue()));
			}
			else if (StepCurve.IntegerKeyValues.IsSet())
			{
				bResult |= UE::Anim::AddTypedCustomAttribute<FIntegerAnimationAttribute, int32>(FName(CurveName)
					, FName(BoneName)
					, TargetSequence
					, MakeArrayView(StepCurve.KeyTimes)
					, MakeArrayView(StepCurve.IntegerKeyValues.GetValue()));
			}
			else if (StepCurve.StringKeyValues.IsSet())
			{
				bResult |= UE::Anim::AddTypedCustomAttribute<FStringAnimationAttribute, FString>(FName(CurveName)
					, FName(BoneName)
					, TargetSequence
					, MakeArrayView(StepCurve.KeyTimes)
					, MakeArrayView(StepCurve.StringKeyValues.GetValue()));
			}
		}
		return bResult;
	}

	bool InternalCreateCurve(UAnimSequence* TargetSequence
		, FAnimationCurvePayloadData& CurvePayload
		, const FString& CurveName
		, const int32 CurveFlags
		, const bool bDoNotImportCurveWithZero
		, const bool bMorphTargetCurve
		, const bool bMaterialCurve)
	{
		if (!TargetSequence || CurvePayload.Curves.Num() != 1 || CurveName.IsEmpty())
		{
			return false;
		}

		if (bDoNotImportCurveWithZero)
		{
			bool bAllCurveValueAreZero = true;
			for (const FRichCurve& Curve : CurvePayload.Curves)
			{
				FKeyHandle KeyHandle = Curve.GetFirstKeyHandle();
				while (KeyHandle != FKeyHandle::Invalid())
				{
					if (!FMath::IsNearlyZero(Curve.GetKeyValue(KeyHandle)))
					{
						bAllCurveValueAreZero = false;
						break;
					}
					KeyHandle = Curve.GetNextKey(KeyHandle);
				}
			}
			if (bAllCurveValueAreZero)
			{
				//Avoid importing morph target curve with only zero value
				return false;
			}
		}
		FName Name = *CurveName;
		USkeleton* Skeleton = TargetSequence->GetSkeleton();
		const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);

		// Add or retrieve curve
		if (!NameMapping->Exists(Name))
		{
			// mark skeleton dirty
			Skeleton->Modify();
		}

		FSmartName NewName;
		Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, Name, NewName);

		FAnimationCurveIdentifier FloatCurveId(NewName, ERawCurveTrackTypes::RCT_Float);


		UAnimDataModel* DataModel = TargetSequence->GetDataModel();
		IAnimationDataController& Controller = TargetSequence->GetController();

		const FFloatCurve* TargetCurve = DataModel->FindFloatCurve(FloatCurveId);
		if (TargetCurve == nullptr)
		{
			// Need to add the curve first
			Controller.AddCurve(FloatCurveId, AACF_DefaultCurve | CurveFlags);
			TargetCurve = DataModel->FindFloatCurve(FloatCurveId);
		}
		else
		{
			// Need to update any of the flags
			Controller.SetCurveFlags(FloatCurveId, CurveFlags | TargetCurve->GetCurveTypeFlags());
		}

		// Should be valid at this point
		ensure(TargetCurve);

		Controller.UpdateCurveNamesFromSkeleton(Skeleton, ERawCurveTrackTypes::RCT_Float);

		//MorphTarget curves are shift to 0 second
		const float CurveStartTime = CurvePayload.Curves[0].GetFirstKey().Time;
		if (CurveStartTime > UE_SMALL_NUMBER)
		{
			CurvePayload.Curves[0].ShiftCurve(-CurveStartTime);
		}
		// Set actual keys on curve within the model
		Controller.SetCurveKeys(FloatCurveId, CurvePayload.Curves[0].GetConstRefOfKeys());

		if (bMaterialCurve || bMorphTargetCurve)
		{
			Skeleton->AccumulateCurveMetaData(*CurveName, bMaterialCurve, bMorphTargetCurve);
		}
		return true;
	}

	bool CreateMorphTargetCurve(UAnimSequence* TargetSequence, FAnimationCurvePayloadData& CurvePayload, const FString& CurveName, int32 CurveFlags, bool bDoNotImportCurveWithZero)
	{
		constexpr bool bIsMorphTargetCurve = true;
		constexpr bool bIsMaterialCurve = false;
		return InternalCreateCurve(TargetSequence, CurvePayload, CurveName, CurveFlags, bDoNotImportCurveWithZero, bIsMorphTargetCurve, bIsMaterialCurve);
	}

	bool CreateMaterialCurve(UAnimSequence* TargetSequence, FAnimationCurvePayloadData& CurvePayload, const FString& CurveName, int32 CurveFlags, bool bDoNotImportCurveWithZero)
	{
		constexpr bool bIsMorphTargetCurve = false;
		constexpr bool bIsMaterialCurve = true;
		return InternalCreateCurve(TargetSequence, CurvePayload, CurveName, CurveFlags, bDoNotImportCurveWithZero, bIsMorphTargetCurve, bIsMaterialCurve);
	}

	bool CreateAttributeCurve(UAnimSequence* TargetSequence, FAnimationCurvePayloadData& CurvePayload, const FString& CurveName, int32 CurveFlags, bool bDoNotImportCurveWithZero)
	{
		//This curve don't animate morph target or material parameter.
		constexpr bool bIsMorphTargetCurve = false;
		constexpr bool bIsMaterialCurve = false;
		return InternalCreateCurve(TargetSequence, CurvePayload, CurveName, CurveFlags, bDoNotImportCurveWithZero, bIsMorphTargetCurve, bIsMaterialCurve);
	}

	void RetrieveAnimationPayloads(UAnimSequence* AnimSequence
		, const UInterchangeAnimSequenceFactoryNode* AnimSequenceFactoryNode
		, const UInterchangeBaseNodeContainer* NodeContainer
		, const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode
		, const IInterchangeAnimationPayloadInterface* AnimSequenceTranslatorPayloadInterface
		, const FString& AssetName
		, TArray<FString> OutCurvesNotFound)
	{
		TMap<const UInterchangeSceneNode*, TFuture<TOptional<UE::Interchange::FAnimationBakeTransformPayloadData>>> AnimationPayloads;

		FString SkeletonRootUid;
		if (!SkeletonFactoryNode->GetCustomRootJointUid(SkeletonRootUid))
		{
			//Cannot import animation without a skeleton
			return;
		}
		
		IAnimationDataController& Controller = AnimSequence->GetController();
		USkeleton* Skeleton = AnimSequence->GetSkeleton();
		check(Skeleton);

		TArray<FString> SkeletonNodes;
		GetSkeletonSceneNodeFlatListRecursive(NodeContainer, SkeletonRootUid, SkeletonNodes);

		bool bImportBoneTracks = false;
		AnimSequenceFactoryNode->GetCustomImportBoneTracks(bImportBoneTracks);
		if (bImportBoneTracks)
		{
			//Get the sample rate, default to 30Hz in case the attribute is missing
			double SampleRate = 30.0;
			AnimSequenceFactoryNode->GetCustomImportBoneTracksSampleRate(SampleRate);

			double RangeStart = 0.0;
			AnimSequenceFactoryNode->GetCustomImportBoneTracksRangeStart(RangeStart);

			double RangeEnd = 1.0 / SampleRate; //One frame duration per default
			AnimSequenceFactoryNode->GetCustomImportBoneTracksRangeStop(RangeEnd);

			const double BakeInterval = 1.0 / SampleRate;
			const double SequenceLength = FMath::Max<double>(RangeEnd - RangeStart, MINIMUM_ANIMATION_LENGTH);
			int32 BakeKeyCount = (SequenceLength / BakeInterval) + 1;

			TMap<FString, FString> PayloadKeys;
			AnimSequenceFactoryNode->GetSceneNodeAnimationPayloadKeys(PayloadKeys);

			if (PayloadKeys.Num() == 0)
			{
				for (const FString& NodeUid : SkeletonNodes)
				{
					if (const UInterchangeSceneNode* SkeletonSceneNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(NodeUid)))
					{
						FString PayloadKey;
						if (UInterchangeAnimationAPI::GetCustomNodeTransformPayloadKey(SkeletonSceneNode, PayloadKey))
						{
							AnimationPayloads.Add(SkeletonSceneNode, AnimSequenceTranslatorPayloadInterface->GetAnimationBakeTransformPayloadData(PayloadKey, SampleRate, RangeStart, RangeEnd));
						}
					}
				}
			}
			else
			{
				for (const TTuple<FString, FString>& SceneNodeUidAndPayloadKey : PayloadKeys)
				{
					if (const UInterchangeSceneNode* SkeletonSceneNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(SceneNodeUidAndPayloadKey.Key)))
					{
						AnimationPayloads.Add(SkeletonSceneNode, AnimSequenceTranslatorPayloadInterface->GetAnimationBakeTransformPayloadData(SceneNodeUidAndPayloadKey.Value, SampleRate, RangeStart, RangeEnd));
					}
				}
			}

			//This destroy all previously imported animation raw data
			Controller.RemoveAllBoneTracks();
			Controller.SetPlayLength(FGenericPlatformMath::Max<float>(SequenceLength, MINIMUM_ANIMATION_LENGTH));

			FTransform GlobalOffsetTransform;
			bool bBakeMeshes = false;
			{
				GlobalOffsetTransform = FTransform::Identity;
				if (UInterchangeCommonPipelineDataFactoryNode* CommonPipelineDataFactoryNode = UInterchangeCommonPipelineDataFactoryNode::GetUniqueInstance(NodeContainer))
				{
					CommonPipelineDataFactoryNode->GetCustomGlobalOffsetTransform(GlobalOffsetTransform);
					CommonPipelineDataFactoryNode->GetBakeMeshes(bBakeMeshes);
				}
			}

			for (const TTuple< const UInterchangeSceneNode*, TFuture<TOptional<UE::Interchange::FAnimationBakeTransformPayloadData>>>& AnimationPayload : AnimationPayloads)
			{
				const FName BoneName = FName(*(AnimationPayload.Key->GetDisplayLabel()));
				const int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(BoneName);
				if (BoneIndex == INDEX_NONE)
				{
					//Skip this bone, we did not found it in the skeleton
					continue;
				}
				//If we are getting the root 
				bool bApplyGlobalOffset = AnimationPayload.Key->GetUniqueID().Equals(SkeletonRootUid);
				
				TOptional<UE::Interchange::FAnimationBakeTransformPayloadData> AnimationTransformPayload = AnimationPayload.Value.Get();
				if (!AnimationTransformPayload.IsSet())
				{
					FString PayloadKey = PayloadKeys[AnimationPayload.Key->GetUniqueID()];
					UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid animation transform payload key [%s] AnimSequence asset %s"), *PayloadKey, *AssetName);
					continue;
				}

				if (AnimationTransformPayload->Transforms.Num() == 0)
				{
					//We need at least one transform
					AnimationTransformPayload->Transforms.Add(FTransform::Identity);
				}

				FRawAnimSequenceTrack RawTrack;
				RawTrack.PosKeys.Reserve(BakeKeyCount);
				RawTrack.RotKeys.Reserve(BakeKeyCount);
				RawTrack.ScaleKeys.Reserve(BakeKeyCount);
				TArray<float> TimeKeys;
				TimeKeys.Reserve(BakeKeyCount);

				//Everything should match Key count, sample rate and range
				check(AnimationTransformPayload->Transforms.Num() == BakeKeyCount);
				check(FMath::IsNearlyEqual(AnimationTransformPayload->BakeFrequency, SampleRate, UE_DOUBLE_KINDA_SMALL_NUMBER));
				check(FMath::IsNearlyEqual(AnimationTransformPayload->RangeStartTime, RangeStart, UE_DOUBLE_KINDA_SMALL_NUMBER));
				check(FMath::IsNearlyEqual(AnimationTransformPayload->RangeEndTime, RangeEnd, UE_DOUBLE_KINDA_SMALL_NUMBER));

				int32 TransformIndex = 0;
				for (double CurrentTime = RangeStart; CurrentTime <= RangeEnd + UE_DOUBLE_SMALL_NUMBER; CurrentTime += BakeInterval, ++TransformIndex)
				{
					check(AnimationTransformPayload->Transforms.IsValidIndex(TransformIndex));
					FTransform3f AnimKeyTransform = FTransform3f(AnimationTransformPayload->Transforms[TransformIndex]);
					if (bApplyGlobalOffset)
					{
						if (bBakeMeshes)
						{
							const UInterchangeSceneNode* RootJointNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(SkeletonRootUid));
							if (RootJointNode)
							{
								FString RootJointParentNodeUid = RootJointNode->GetParentUid();

								const UInterchangeSceneNode* RootJointParentNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(RootJointParentNodeUid));
								if (RootJointParentNode)
								{
									FTransform GlobalTransform;
									RootJointParentNode->GetCustomGlobalTransform(NodeContainer, GlobalOffsetTransform, GlobalTransform);
									AnimKeyTransform = AnimKeyTransform * FTransform3f(GlobalTransform);
								}
							}
						}
					}
					//Default value to identity
					FVector3f Position(0.0f);
					FQuat4f Quaternion(0.0f);
					FVector3f Scale(1.0f);

					Position = AnimKeyTransform.GetLocation();
					Quaternion = AnimKeyTransform.GetRotation();
					Scale = AnimKeyTransform.GetScale3D();
					RawTrack.ScaleKeys.Add(Scale);
					RawTrack.PosKeys.Add(Position);
					RawTrack.RotKeys.Add(Quaternion);
					//Animation are always translated to zero
					TimeKeys.Add(CurrentTime - RangeStart);
				}

				//Make sure we create the correct amount of keys
				check(RawTrack.ScaleKeys.Num() == BakeKeyCount
					&& RawTrack.PosKeys.Num() == BakeKeyCount
					&& RawTrack.RotKeys.Num() == BakeKeyCount
					&& TimeKeys.Num() == BakeKeyCount);

				//add new track
				Controller.AddBoneTrack(BoneName);
				Controller.SetBoneTrackKeys(BoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys);
			}
		}

		bool bDeleteExistingMorphTargetCurves = false;
		AnimSequenceFactoryNode->GetCustomDeleteExistingMorphTargetCurves(bDeleteExistingMorphTargetCurves);
		bool bDeleteExistingCustomAttributeCurves = false;
		AnimSequenceFactoryNode->GetCustomDeleteExistingCustomAttributeCurves(bDeleteExistingCustomAttributeCurves);
		bool bDeleteExistingNonCurveCustomAttributes = false;
		AnimSequenceFactoryNode->GetCustomDeleteExistingNonCurveCustomAttributes(bDeleteExistingNonCurveCustomAttributes);
		if (bDeleteExistingMorphTargetCurves || bDeleteExistingCustomAttributeCurves)
		{
			TArray<FSmartName> CurveSmartNamesToRemove;
			for (const FFloatCurve& Curve : AnimSequence->GetDataModel()->GetFloatCurves())
			{
				const FCurveMetaData* MetaData = Skeleton->GetCurveMetaData(Curve.Name);
				if (MetaData)
				{
					bool bDeleteCurve = MetaData->Type.bMorphtarget ? bDeleteExistingMorphTargetCurves : bDeleteExistingCustomAttributeCurves;
					if (bDeleteCurve)
					{
						CurveSmartNamesToRemove.Add(Curve.Name);
					}
				}
			}

			for (const FSmartName& CurveName : CurveSmartNamesToRemove)
			{
				const FAnimationCurveIdentifier CurveId(CurveName, ERawCurveTrackTypes::RCT_Float);
				Controller.RemoveCurve(CurveId);
			}
		}

		if (bDeleteExistingNonCurveCustomAttributes)
		{
			Controller.RemoveAllAttributes();
		}

		
		bool bImportAttributeCurves = false;
		AnimSequenceFactoryNode->GetCustomImportAttributeCurves(bImportAttributeCurves);
		if (bImportAttributeCurves)
		{
			const UAnimDataModel* DataModel = AnimSequence->GetDataModel();
			const int32 NumFloatCurves = DataModel->GetNumberOfFloatCurves();
			const FAnimationCurveData& CurveData = DataModel->GetCurveData();

			OutCurvesNotFound.Reset(NumFloatCurves);

			for (const FFloatCurve& FloatCurve : CurveData.FloatCurves)
			{
				const FCurveMetaData* MetaData = Skeleton->GetCurveMetaData(FloatCurve.Name);

				if (MetaData && !MetaData->Type.bMorphtarget)
				{
					OutCurvesNotFound.Add(FloatCurve.Name.DisplayName.ToString());
				}
			}

			bool bMaterialDriveParameterOnCustomAttribute = false;
			AnimSequenceFactoryNode->GetCustomMaterialDriveParameterOnCustomAttribute(bMaterialDriveParameterOnCustomAttribute);
			TArray<FString> MaterialSuffixes;
			AnimSequenceFactoryNode->GetAnimatedMaterialCurveSuffixes(MaterialSuffixes);
			bool bDoNotImportCurveWithZero = false;
			AnimSequenceFactoryNode->GetCustomDoNotImportCurveWithZero(bDoNotImportCurveWithZero);
			bool bRemoveCurveRedundantKeys = false;
			AnimSequenceFactoryNode->GetCustomRemoveCurveRedundantKeys(bRemoveCurveRedundantKeys);

			auto IsCurveHookToMaterial = [&MaterialSuffixes](const FString& CurveName)
			{
				for (const FString& MaterialSuffixe : MaterialSuffixes)
				{
					if (CurveName.EndsWith(MaterialSuffixe))
					{
						return true;
					}
				}
				return false;
			};

			//Import morph target curves
			{
				TMap<FString, TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>>> AnimationCurvesPayloads;
				TMap<FString, FString> AnimationCurveMorphTargetNodeNames;
				//Import morph target curves (FRichCurve is what anim api )
				TArray<FString> AnimatedMorphTargetUids;
				AnimSequenceFactoryNode->GetAnimatedMorphTargetDependencies(AnimatedMorphTargetUids);
				for (const FString& MorphTargetUid : AnimatedMorphTargetUids)
				{
					if (const UInterchangeMeshNode* MorphTargetNode = Cast<UInterchangeMeshNode>(NodeContainer->GetNode(MorphTargetUid)))
					{
						TOptional<FString> PayloadKey = MorphTargetNode->GetAnimationCurvePayLoadKey();
						if (PayloadKey.IsSet())
						{
							AnimationCurvesPayloads.Add(PayloadKey.GetValue(), AnimSequenceTranslatorPayloadInterface->GetAnimationCurvePayloadData(PayloadKey.GetValue()));
							AnimationCurveMorphTargetNodeNames.Add(PayloadKey.GetValue(), MorphTargetNode->GetDisplayLabel());
						}
					}
				}
				for (TPair<FString, TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>>>& CurveNameAndPayload : AnimationCurvesPayloads)
				{
					TOptional<UE::Interchange::FAnimationCurvePayloadData> AnimationCurvePayload = CurveNameAndPayload.Value.Get();
					if (!AnimationCurvePayload.IsSet())
					{
						UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid animation morph target curve payload key [%s] AnimSequence asset %s"), *CurveNameAndPayload.Key, *AssetName);
						continue;
					}
					FAnimationCurvePayloadData& CurvePayload = AnimationCurvePayload.GetValue();
					if (bRemoveCurveRedundantKeys)
					{
						for (FRichCurve& RichCurve : CurvePayload.Curves)
						{
							RichCurve.RemoveRedundantAutoTangentKeys(SMALL_NUMBER);
						}
					}
					constexpr int32 CurveFlags = 0;
					CreateMorphTargetCurve(AnimSequence, CurvePayload, AnimationCurveMorphTargetNodeNames.FindChecked(CurveNameAndPayload.Key), CurveFlags, bDoNotImportCurveWithZero);
				}
			}

			//Import Attribute curves
			{
				//Utility to make sure the curve is compatible with FRichCurve
				auto IsDecimalType = [](UE::Interchange::EAttributeTypes Type)
				{
					switch (Type)
					{
					case UE::Interchange::EAttributeTypes::Double:
					case UE::Interchange::EAttributeTypes::Float:
					case UE::Interchange::EAttributeTypes::Float16:
					case UE::Interchange::EAttributeTypes::Vector2d:
					case UE::Interchange::EAttributeTypes::Vector2f:
					case UE::Interchange::EAttributeTypes::Vector3d:
					case UE::Interchange::EAttributeTypes::Vector3f:
					case UE::Interchange::EAttributeTypes::Vector4d:
					case UE::Interchange::EAttributeTypes::Vector4f:
						return true;
					}
					return false;
				};
				//Import Attribute curves (FRichCurve)
				{
					TArray<FString> AttributeCurveNames;
					AnimSequenceFactoryNode->GetAnimatedAttributeCurveNames(AttributeCurveNames);
					TMap<FString, TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>>> AnimationCurvesPayloads;
					for (const FString& NodeUid : SkeletonNodes)
					{
						if (const UInterchangeSceneNode* SkeletonSceneNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(NodeUid)))
						{
							//Import material parameter curves (FRichCurve)
							TMap<FString, FString> CurveNamePayloads;
							TArray<FInterchangeUserDefinedAttributeInfo> AttributeInfos = UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttributeInfos(SkeletonSceneNode);
							for (const FInterchangeUserDefinedAttributeInfo& AttributeInfo : AttributeInfos)
							{
								//Material curve must be convertible to float since we need a FRichCurve
								if (AttributeInfo.PayloadKey.IsSet() && IsDecimalType(AttributeInfo.Type) && AttributeCurveNames.Contains(AttributeInfo.Name))
								{
									CurveNamePayloads.Add(AttributeInfo.Name, AttributeInfo.PayloadKey.GetValue());
								}
							}
							for (const TPair<FString, FString>& CurveNamePayload : CurveNamePayloads)
							{
								AnimationCurvesPayloads.Add(CurveNamePayload.Key, AnimSequenceTranslatorPayloadInterface->GetAnimationCurvePayloadData(CurveNamePayload.Value));
							}
						}
					}

					for (TPair<FString, TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>>>& CurveNameAndPayload : AnimationCurvesPayloads)
					{
						const FString& CurveName = CurveNameAndPayload.Key;
						TOptional<UE::Interchange::FAnimationCurvePayloadData> AnimationCurvePayload = CurveNameAndPayload.Value.Get();
						if (!AnimationCurvePayload.IsSet())
						{
							UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid animation material curve payload key [%s] AnimSequence asset %s"), *CurveName, *AssetName);
							continue;
						}

						FAnimationCurvePayloadData& CurvePayload = AnimationCurvePayload.GetValue();
						if (bRemoveCurveRedundantKeys)
						{
							for (FRichCurve& RichCurve : CurvePayload.Curves)
							{
								RichCurve.RemoveRedundantAutoTangentKeys(SMALL_NUMBER);
							}
						}
						OutCurvesNotFound.Remove(CurveName);
						constexpr int32 CurveFlags = 0;
						if (bMaterialDriveParameterOnCustomAttribute || IsCurveHookToMaterial(CurveName))
						{
							CreateMaterialCurve(AnimSequence, CurvePayload, CurveName, CurveFlags, bDoNotImportCurveWithZero);
						}
						else
						{
							CreateAttributeCurve(AnimSequence, CurvePayload, CurveName, CurveFlags, bDoNotImportCurveWithZero);
						}
					}
				}

				//Import attribute step curves (step curves, only time and value array, no FRichCurve. The anim API expect the value array is of the type of the translated attribute)
				{
					TArray<FString> AttributeStepCurveNames;
					AnimSequenceFactoryNode->GetAnimatedAttributeStepCurveNames(AttributeStepCurveNames);
					TMap<FString, TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>>> AnimationCurvesPayloads;
					TMap<FString, TFuture<TOptional<UE::Interchange::FAnimationStepCurvePayloadData>>> AnimationStepCurvesPayloads;
					TMap<FString, FString> AnimationBoneNames;
					for (const FString& NodeUid : SkeletonNodes)
					{
						if (const UInterchangeSceneNode* SkeletonSceneNode = Cast<UInterchangeSceneNode>(NodeContainer->GetNode(NodeUid)))
						{
							FString BoneName = SkeletonSceneNode->GetDisplayLabel();
							//Import material parameter curves (FRichCurve)
							TMap<FString, FString> CurveNameFloatPayloads;
							TMap<FString, FString> CurveNameStepCurvePayloads;
							TArray<FInterchangeUserDefinedAttributeInfo> AttributeInfos = UInterchangeUserDefinedAttributesAPI::GetUserDefinedAttributeInfos(SkeletonSceneNode);
							for (const FInterchangeUserDefinedAttributeInfo& AttributeInfo : AttributeInfos)
							{
								//Material curve must be convertible to float since we need a FRichCurve
								if (AttributeInfo.PayloadKey.IsSet() && AttributeStepCurveNames.Contains(AttributeInfo.Name))
								{
									if (IsDecimalType(AttributeInfo.Type))
									{
										CurveNameFloatPayloads.Add(AttributeInfo.Name, AttributeInfo.PayloadKey.GetValue());
									}
									else
									{
										CurveNameStepCurvePayloads.Add(AttributeInfo.Name, AttributeInfo.PayloadKey.GetValue());
									}
								}
							}
							for (const TPair<FString, FString>& CurveNamePayload : CurveNameFloatPayloads)
							{
								AnimationCurvesPayloads.Add(CurveNamePayload.Key, AnimSequenceTranslatorPayloadInterface->GetAnimationCurvePayloadData(CurveNamePayload.Value));
								AnimationBoneNames.Add(CurveNamePayload.Key, BoneName);
							}
							for (const TPair<FString, FString>& StepCurveNamePayload : CurveNameStepCurvePayloads)
							{
								AnimationStepCurvesPayloads.Add(StepCurveNamePayload.Key, AnimSequenceTranslatorPayloadInterface->GetAnimationStepCurvePayloadData(StepCurveNamePayload.Value));
								AnimationBoneNames.Add(StepCurveNamePayload.Key, BoneName);
							}
						}
					}

					auto AddAttributeCurveToAnimSequence = [bRemoveCurveRedundantKeys, bDoNotImportCurveWithZero, &AnimSequence](FAnimationStepCurvePayloadData& StepCurvePayload, const FString& CurveName, const FString& BoneName)
					{
						if (bRemoveCurveRedundantKeys)
						{
							for (FInterchangeStepCurve& StepCurve : StepCurvePayload.StepCurves)
							{
								StepCurve.RemoveRedundantKeys(SMALL_NUMBER);
							}
						}

						constexpr int32 CurveFlags = 0;
						CreateAttributeStepCurve(AnimSequence, StepCurvePayload, CurveName, BoneName, CurveFlags, bDoNotImportCurveWithZero);
					};

					for (TPair<FString, TFuture<TOptional<UE::Interchange::FAnimationCurvePayloadData>>>& CurveNameAndPayload : AnimationCurvesPayloads)
					{
						TOptional<UE::Interchange::FAnimationCurvePayloadData> AnimationCurvePayload = CurveNameAndPayload.Value.Get();
						if (!AnimationCurvePayload.IsSet())
						{
							UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid animation material curve payload key [%s] AnimSequence asset %s"), *CurveNameAndPayload.Key, *AssetName);
							continue;
						}
						FAnimationCurvePayloadData& CurvePayload = AnimationCurvePayload.GetValue();
						FAnimationStepCurvePayloadData StepCurvePayload = FAnimationStepCurvePayloadData::FromAnimationCurvePayloadData(CurvePayload);
						AddAttributeCurveToAnimSequence(StepCurvePayload, CurveNameAndPayload.Key, AnimationBoneNames.FindChecked(CurveNameAndPayload.Key));
					}

					for (TPair<FString, TFuture<TOptional<UE::Interchange::FAnimationStepCurvePayloadData>>>& StepCurveNameAndPayload : AnimationStepCurvesPayloads)
					{
						TOptional<UE::Interchange::FAnimationStepCurvePayloadData> AnimationStepCurvePayload = StepCurveNameAndPayload.Value.Get();
						if (!AnimationStepCurvePayload.IsSet())
						{
							UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid animation material curve payload key [%s] AnimSequence asset %s"), *StepCurveNameAndPayload.Key, *AssetName);
							continue;
						}

						AddAttributeCurveToAnimSequence(AnimationStepCurvePayload.GetValue(), StepCurveNameAndPayload.Key, AnimationBoneNames.FindChecked(StepCurveNameAndPayload.Key));
					}
				}
			}
		}
		else
		{
			// Store float curve tracks which use to exist on the animation
			for (const FFloatCurve& Curve : AnimSequence->GetDataModel()->GetFloatCurves())
			{
				const FCurveMetaData* MetaData = Skeleton->GetCurveMetaData(Curve.Name);
				if (MetaData && !MetaData->Type.bMorphtarget)
				{
					OutCurvesNotFound.Add(Curve.Name.DisplayName.ToString());
				}
			}
		}
	}
} //namespace UE::Interchange::Private
#endif //WITH_EDITOR

UClass* UInterchangeAnimSequenceFactory::GetFactoryClass() const
{
	return UAnimSequence::StaticClass();
}

UObject* UInterchangeAnimSequenceFactory::CreateEmptyAsset(const FCreateAssetParams& Arguments)
{
#if !WITH_EDITOR || !WITH_EDITORONLY_DATA

	UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import animsequence asset in runtime, this is an editor only feature."));
	return nullptr;
#else
	UAnimSequence* AnimSequence = nullptr;
	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	const UInterchangeAnimSequenceFactoryNode* AnimSequenceFactoryNode = Cast<UInterchangeAnimSequenceFactoryNode>(Arguments.AssetNode);
	if (AnimSequenceFactoryNode == nullptr)
	{
		return nullptr;
	}
	
	//Verify if the bone track animation is valid (sequence length versus framerate ...)
	if(!IsBoneTrackAnimationValid(AnimSequenceFactoryNode, Arguments))
	{
		return nullptr;
	}

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		AnimSequence = NewObject<UAnimSequence>(Arguments.Parent, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(UAnimSequence::StaticClass()))
	{
		//This is a reimport, we are just re-updating the source data
		AnimSequence = Cast<UAnimSequence>(ExistingAsset);
	}

	if (!AnimSequence)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Could not create AnimSequence asset %s"), *Arguments.AssetName);
		return nullptr;
	}

	AnimSequence->PreEditChange(nullptr);

	return AnimSequence;
#endif //else !WITH_EDITOR || !WITH_EDITORONLY_DATA
}

UObject* UInterchangeAnimSequenceFactory::CreateAsset(const FCreateAssetParams& Arguments)
{
#if !WITH_EDITOR || !WITH_EDITORONLY_DATA

	UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import AnimSequence asset in runtime, this is an editor only feature."));
	return nullptr;

#else

	if (!Arguments.AssetNode || !Arguments.AssetNode->GetObjectClass()->IsChildOf(GetFactoryClass()))
	{
		return nullptr;
	}

	UInterchangeAnimSequenceFactoryNode* AnimSequenceFactoryNode = Cast<UInterchangeAnimSequenceFactoryNode>(Arguments.AssetNode);
	if (AnimSequenceFactoryNode == nullptr)
	{
		return nullptr;
	}

	//Verify if the bone track animation is valid (sequence length versus framerate ...)
	if (!IsBoneTrackAnimationValid(AnimSequenceFactoryNode, Arguments))
	{
		return nullptr;
	}

	FString SkeletonUid;
	if (!AnimSequenceFactoryNode->GetCustomSkeletonFactoryNodeUid(SkeletonUid))
	{
		//Do not create a empty anim sequence, we need skeleton that contain animation
		return nullptr;
	}

	const UInterchangeSkeletonFactoryNode* SkeletonFactoryNode = Cast<UInterchangeSkeletonFactoryNode>(Arguments.NodeContainer->GetNode(SkeletonUid));
	if (!SkeletonFactoryNode)
	{
		UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid skeleton factory node, the skeleton factory node is obligatory to import this animsequence [%s]!"), *Arguments.AssetName);
		return nullptr;
	}
	FSoftObjectPath SkeletonFactoryNodeReferenceObject;
	SkeletonFactoryNode->GetCustomReferenceObject(SkeletonFactoryNodeReferenceObject);

	USkeleton* Skeleton = nullptr;

	FSoftObjectPath SpecifiedSkeleton;
	AnimSequenceFactoryNode->GetCustomSkeletonSoftObjectPath(SpecifiedSkeleton);
	if (Skeleton == nullptr)
	{
		UObject* SkeletonObject = nullptr;

		if (SpecifiedSkeleton.IsValid())
		{
			SkeletonObject = SpecifiedSkeleton.TryLoad();
		}
		else if (SkeletonFactoryNodeReferenceObject.IsValid())
		{
			SkeletonObject = SkeletonFactoryNodeReferenceObject.TryLoad();
		}

		if (SkeletonObject)
		{
			Skeleton = Cast<USkeleton>(SkeletonObject);

		}

		if (!ensure(Skeleton))
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Invalid Skeleton when importing animation sequence asset %s"), *Arguments.AssetName);
			return nullptr;
		}
	}

	const IInterchangeAnimationPayloadInterface* AnimSequenceTranslatorPayloadInterface = Cast<IInterchangeAnimationPayloadInterface>(Arguments.Translator);
	if (!AnimSequenceTranslatorPayloadInterface)
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Cannot import AnimSequence, the translator do not implement the IInterchangeAnimationPayloadInterface."));
		return nullptr;
	}

	const UClass* AnimSequenceClass = AnimSequenceFactoryNode->GetObjectClass();
	check(AnimSequenceClass && AnimSequenceClass->IsChildOf(GetFactoryClass()));

	// create an asset if it doesn't exist
	UObject* ExistingAsset = StaticFindObject(nullptr, Arguments.Parent, *Arguments.AssetName);

	UObject* AnimSequenceObject = nullptr;
	// create a new material or overwrite existing asset, if possible
	if (!ExistingAsset)
	{
		//NewObject is not thread safe, the asset registry directory watcher tick on the main thread can trig before we finish initializing the UObject and will crash
		//The UObject should have been create by calling CreateEmptyAsset on the main thread.
		check(IsInGameThread());
		AnimSequenceObject = NewObject<UObject>(Arguments.Parent, AnimSequenceClass, *Arguments.AssetName, RF_Public | RF_Standalone);
	}
	else if (ExistingAsset->GetClass()->IsChildOf(AnimSequenceClass))
	{
		//This is a reimport, we are just re-updating the source data
		AnimSequenceObject = ExistingAsset;
	}

	if (!AnimSequenceObject)
	{
		UE_LOG(LogInterchangeImport, Error, TEXT("Could not create AnimSequence asset %s"), *Arguments.AssetName);
		return nullptr;
	}

	UAnimSequence* AnimSequence = Cast<UAnimSequence>(AnimSequenceObject);

	const bool bIsReImport = (Arguments.ReimportObject != nullptr);

	if (!ensure(AnimSequence))
	{
		if (!bIsReImport)
		{
			UE_LOG(LogInterchangeImport, Error, TEXT("Could not create AnimSequence asset %s"), *Arguments.AssetName);
		}
		else
		{
			UE_LOG(LogInterchangeImport, Error, TEXT("Could not find reimported AnimSequence asset %s"), *Arguments.AssetName);
		}
		return nullptr;
	}

	//Fill the animsequence data, we need to retrieve the skeleton and then ask the payload for every joint
	{
		FFrameRate FrameRate(30, 1);
		double SampleRate = 30.0;

		bool bImportBoneTracks = false;
		if (AnimSequenceFactoryNode->GetCustomImportBoneTracks(bImportBoneTracks) && bImportBoneTracks)
		{
			if (AnimSequenceFactoryNode->GetCustomImportBoneTracksSampleRate(SampleRate))
			{
				FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(SampleRate);
			}
		}
		IAnimationDataController& Controller = AnimSequence->GetController();
		Controller.OpenBracket(NSLOCTEXT("InterchangeAnimSequenceFactory", "ImportAnimationInterchange_Bracket", "Importing Animation (Interchange)"));
		AnimSequence->SetSkeleton(Skeleton);
		AnimSequence->ImportFileFramerate = SampleRate;
		AnimSequence->ImportResampleFramerate = SampleRate;
		Controller.SetFrameRate(FrameRate);

		TArray<FString> CurvesNotFound;
		UE::Interchange::Private::RetrieveAnimationPayloads(AnimSequence
			, AnimSequenceFactoryNode
			, Arguments.NodeContainer
			, SkeletonFactoryNode
			, AnimSequenceTranslatorPayloadInterface
			, Arguments.AssetName
			, CurvesNotFound);

		if (CurvesNotFound.Num())
		{
			for (const FString& CurveName : CurvesNotFound)
			{
				UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
				Message->SourceAssetName = Arguments.SourceData->GetFilename();
				Message->DestinationAssetName = Arguments.AssetName;
				Message->AssetType = UAnimSequence::StaticClass();
				Message->Text = FText::Format(NSLOCTEXT("UInterchangeAnimSequenceFactory", "NonExistingCurve", "Curve ({0}) was not found in the new Animation."), FText::FromString(CurveName));
			}
		}
		Controller.NotifyPopulated();
		Controller.CloseBracket(false);
	}

	if (!bIsReImport)
	{
		/** Apply all AnimSequenceFactoryNode custom attributes to the skeletal mesh asset */
		AnimSequenceFactoryNode->ApplyAllCustomAttributeToObject(AnimSequence);
	}
	else
	{
		//Apply the re import strategy 
		UInterchangeAssetImportData* InterchangeAssetImportData = Cast<UInterchangeAssetImportData>(AnimSequence->AssetImportData);
		UInterchangeFactoryBaseNode* PreviousNode = nullptr;
		if (InterchangeAssetImportData)
		{
			PreviousNode = InterchangeAssetImportData->NodeContainer->GetFactoryNode(InterchangeAssetImportData->NodeUniqueID);
		}
		UInterchangeFactoryBaseNode* CurrentNode = NewObject<UInterchangeAnimSequenceFactoryNode>(GetTransientPackage());
		UInterchangeBaseNode::CopyStorage(AnimSequenceFactoryNode, CurrentNode);
		CurrentNode->FillAllCustomAttributeFromObject(AnimSequence);
		UE::Interchange::FFactoryCommon::ApplyReimportStrategyToAsset(AnimSequence, PreviousNode, CurrentNode, AnimSequenceFactoryNode);
	}

	//Getting the file Hash will cache it into the source data
	Arguments.SourceData->GetFileContentHash();

	//The interchange completion task (call in the GameThread after the factories pass), will call PostEditChange which will trig another asynchronous system that will build all material in parallel
	return AnimSequenceObject;

#endif //else !WITH_EDITOR || !WITH_EDITORONLY_DATA
}

/* This function is call in the completion task on the main thread, use it to call main thread post creation step for your assets*/
void UInterchangeAnimSequenceFactory::PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments)
{
	check(IsInGameThread());
	Super::PreImportPreCompletedCallback(Arguments);

	// TODO: make sure this works at runtime
#if WITH_EDITORONLY_DATA
	if (ensure(Arguments.ImportedObject && Arguments.SourceData))
	{
		// We must call the Update of the asset source file in the main thread because UAssetImportData::Update execute some delegate we do not control
		UAnimSequence* AnimSequence = CastChecked<UAnimSequence>(Arguments.ImportedObject);

		UAssetImportData* ImportDataPtr = AnimSequence->AssetImportData;
		UE::Interchange::FFactoryCommon::FUpdateImportAssetDataParameters UpdateImportAssetDataParameters(AnimSequence, ImportDataPtr, Arguments.SourceData, Arguments.NodeUniqueID, Arguments.NodeContainer, Arguments.OriginalPipelines);
		ImportDataPtr = UE::Interchange::FFactoryCommon::UpdateImportAssetData(UpdateImportAssetDataParameters);
		AnimSequence->AssetImportData = ImportDataPtr;
	}
#endif
}

bool UInterchangeAnimSequenceFactory::GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const
{
#if WITH_EDITORONLY_DATA
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Object))
	{
		return UE::Interchange::FFactoryCommon::GetSourceFilenames(AnimSequence->AssetImportData.Get(), OutSourceFilenames);
	}
#endif

	return false;
}

bool UInterchangeAnimSequenceFactory::SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const
{
#if WITH_EDITORONLY_DATA
	if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(Object))
	{
		return UE::Interchange::FFactoryCommon::SetSourceFilename(AnimSequence->AssetImportData.Get(), SourceFilename, SourceIndex);
	}
#endif

	return false;
}

bool UInterchangeAnimSequenceFactory::IsBoneTrackAnimationValid(const UInterchangeAnimSequenceFactoryNode* AnimSequenceFactoryNode, const FCreateAssetParams& Arguments)
{
	bool bResult = true;
	FFrameRate FrameRate(30, 1);
	double SampleRate = 30.0;

	bool bImportBoneTracks = false;
	if (AnimSequenceFactoryNode->GetCustomImportBoneTracks(bImportBoneTracks) && bImportBoneTracks)
	{
		if (AnimSequenceFactoryNode->GetCustomImportBoneTracksSampleRate(SampleRate))
		{
			FrameRate = UE::Interchange::Animation::ConvertSampleRatetoFrameRate(SampleRate);
		}

		double RangeStart = 0.0;
		AnimSequenceFactoryNode->GetCustomImportBoneTracksRangeStart(RangeStart);

		double RangeEnd = 1.0 / SampleRate; //One frame duration per default
		AnimSequenceFactoryNode->GetCustomImportBoneTracksRangeStop(RangeEnd);

		const double SequenceLength = FMath::Max<double>(RangeEnd - RangeStart, MINIMUM_ANIMATION_LENGTH);

		const float SubFrame = FrameRate.AsFrameTime(SequenceLength).GetSubFrame();

		if (!FMath::IsNearlyZero(SubFrame, KINDA_SMALL_NUMBER) && !FMath::IsNearlyEqual(SubFrame, 1.0f, KINDA_SMALL_NUMBER))
		{
			UInterchangeResultError_Generic* Message = AddMessage<UInterchangeResultError_Generic>();
			Message->SourceAssetName = Arguments.SourceData->GetFilename();
			Message->DestinationAssetName = Arguments.AssetName;
			Message->AssetType = UAnimSequence::StaticClass();
			Message->Text = FText::Format(NSLOCTEXT("UInterchangeAnimSequenceFactory", "WrongSequenceLength", "Animation length {0} is not compatible with import frame-rate {1} (sub frame {2}), animation has to be frame-border aligned."),
				FText::AsNumber(SequenceLength), FrameRate.ToPrettyText(), FText::AsNumber(SubFrame));
			bResult = false;
		}
	}
	return bResult;
}

