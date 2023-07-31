// Copyright Epic Games, Inc. All Rights Reserved.

#include "FbxAnimation.h"

#include "CoreMinimal.h"
#include "FbxAPI.h"
#include "FbxConvert.h"
#include "FbxHelper.h"
#include "FbxInclude.h"
#include "Fbx/InterchangeFbxMessages.h"
#include "InterchangeCommonAnimationPayload.h"
#include "InterchangeMeshNode.h"
#include "InterchangeResultsContainer.h"
#include "InterchangeSceneNode.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Nodes/InterchangeAnimationAPI.h"
#include "Serialization/LargeMemoryWriter.h"
#include "SkeletalMeshAttributes.h"
#include "StaticMeshAttributes.h"


#define LOCTEXT_NAMESPACE "InterchangeFbxMesh"

namespace UE::Interchange::Private
{
	bool ImportCurve(const FbxAnimCurve* SourceFloatCurves, const float ScaleValue, TArray<FInterchangeCurveKey>& DestinationFloatCurve)
	{
		if (!SourceFloatCurves)
		{
			return true;
		}
		const float DefaultCurveWeight = FbxAnimCurveDef::sDEFAULT_WEIGHT;
		//We use the non const to query the left and right derivative of the key, for whatever reason those FBX API functions are not const
		FbxAnimCurve* NonConstSourceFloatCurves = const_cast<FbxAnimCurve*>(SourceFloatCurves);
		int32 KeyCount = SourceFloatCurves->KeyGetCount();
		for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
		{
			FbxAnimCurveKey Key = SourceFloatCurves->KeyGet(KeyIndex);
			FbxTime KeyTime = Key.GetTime();
			const float KeyTimeValue = static_cast<float>(KeyTime.GetSecondDouble());
			float Value = Key.GetValue() * ScaleValue;
			FInterchangeCurveKey& InterchangeCurveKey = DestinationFloatCurve.AddDefaulted_GetRef();
			InterchangeCurveKey.Time = KeyTimeValue;
			InterchangeCurveKey.Value = Value;

			const bool bIncludeOverrides = true;
			FbxAnimCurveDef::ETangentMode KeyTangentMode = Key.GetTangentMode(bIncludeOverrides);
			FbxAnimCurveDef::EInterpolationType KeyInterpMode = Key.GetInterpolation();
			FbxAnimCurveDef::EWeightedMode KeyTangentWeightMode = Key.GetTangentWeightMode();

			EInterchangeCurveInterpMode NewInterpMode = EInterchangeCurveInterpMode::Linear;
			EInterchangeCurveTangentMode NewTangentMode = EInterchangeCurveTangentMode::Auto;
			EInterchangeCurveTangentWeightMode NewTangentWeightMode = EInterchangeCurveTangentWeightMode::WeightedNone;

			float RightTangent = NonConstSourceFloatCurves->KeyGetRightDerivative(KeyIndex) * ScaleValue;
			float LeftTangent = NonConstSourceFloatCurves->KeyGetLeftDerivative(KeyIndex) * ScaleValue;
			float RightTangentWeight = 0.0f;
			float LeftTangentWeight = 0.0f; //This one is dependent on the previous key.
			bool bLeftWeightActive = false;
			bool bRightWeightActive = false;

			const bool bPreviousKeyValid = KeyIndex > 0;
			const bool bNextKeyValid = KeyIndex < KeyCount - 1;
			float PreviousValue = 0.0f;
			float PreviousKeyTimeValue = 0.0f;
			float NextValue = 0.0f;
			float NextKeyTimeValue = 0.0f;
			if (bPreviousKeyValid)
			{
				FbxAnimCurveKey PreviousKey = SourceFloatCurves->KeyGet(KeyIndex - 1);
				FbxTime PreviousKeyTime = PreviousKey.GetTime();
				PreviousKeyTimeValue = static_cast<float>(PreviousKeyTime.GetSecondDouble());
				PreviousValue = PreviousKey.GetValue() * ScaleValue;
				//The left tangent is driven by the previous key. If the previous key have a the NextLeftweight or both flag weighted mode, it mean the next key is weighted on the left side
				bLeftWeightActive = (PreviousKey.GetTangentWeightMode() & FbxAnimCurveDef::eWeightedNextLeft) > 0;
				if (bLeftWeightActive)
				{
					LeftTangentWeight = PreviousKey.GetDataFloat(FbxAnimCurveDef::eNextLeftWeight);
				}
			}
			if (bNextKeyValid)
			{
				FbxAnimCurveKey NextKey = SourceFloatCurves->KeyGet(KeyIndex + 1);
				FbxTime NextKeyTime = NextKey.GetTime();
				NextKeyTimeValue = static_cast<float>(NextKeyTime.GetSecondDouble());
				NextValue = NextKey.GetValue() * ScaleValue;

				bRightWeightActive = (KeyTangentWeightMode & FbxAnimCurveDef::eWeightedRight) > 0;
				if (bRightWeightActive)
				{
					//The right tangent weight should be use only if we are not the last key since the last key do not have a right tangent.
					//Use the current key to gather the right tangent weight
					RightTangentWeight = Key.GetDataFloat(FbxAnimCurveDef::eRightWeight);
				}
			}

			// When this flag is true, the tangent is flat if the value has the same value as the previous or next key.
			const bool bTangentGenericClamp = (KeyTangentMode & FbxAnimCurveDef::eTangentGenericClamp);

			//Time independent tangent this is consider has a spline tangent key
			const bool bTangentGenericTimeIndependent = (KeyTangentMode & FbxAnimCurveDef::ETangentMode::eTangentGenericTimeIndependent);

			// When this flag is true, the tangent is flat if the value is outside of the [previous key, next key] value range.
			//Clamp progressive is (eTangentGenericClampProgressive |eTangentGenericTimeIndependent)
			const bool bTangentGenericClampProgressive = (KeyTangentMode & FbxAnimCurveDef::ETangentMode::eTangentGenericClampProgressive) == FbxAnimCurveDef::ETangentMode::eTangentGenericClampProgressive;

			if (KeyTangentMode & FbxAnimCurveDef::eTangentGenericBreak)
			{
				NewTangentMode = EInterchangeCurveTangentMode::Break;
			}
			else if (KeyTangentMode & FbxAnimCurveDef::eTangentUser)
			{
				NewTangentMode = EInterchangeCurveTangentMode::User;
			}

			switch (KeyInterpMode)
			{
			case FbxAnimCurveDef::eInterpolationConstant://! Constant value until next key.
				NewInterpMode = EInterchangeCurveInterpMode::Constant;
				break;
			case FbxAnimCurveDef::eInterpolationLinear://! Linear progression to next key.
				NewInterpMode = EInterchangeCurveInterpMode::Linear;
				break;
			case FbxAnimCurveDef::eInterpolationCubic://! Cubic progression to next key.
				NewInterpMode = EInterchangeCurveInterpMode::Cubic;
				// get tangents
				{
					bool bIsFlatTangent = false;
					bool bIsComputedTangent = false;
					if (bTangentGenericClampProgressive)
					{
						if (bPreviousKeyValid && bNextKeyValid)
						{
							const float PreviousNextHalfDelta = (NextValue - PreviousValue) * 0.5f;
							const float PreviousNextAverage = PreviousValue + PreviousNextHalfDelta;
							// If the value is outside of the previous-next value range, the tangent is flat.
							bIsFlatTangent = FMath::Abs(Value - PreviousNextAverage) >= FMath::Abs(PreviousNextHalfDelta);
						}
						else
						{
							//Start/End tangent with the ClampProgressive flag are flat.
							bIsFlatTangent = true;
						}
					}
					else if (bTangentGenericClamp && (bPreviousKeyValid || bNextKeyValid))
					{
						if (bPreviousKeyValid && PreviousValue == Value)
						{
							bIsFlatTangent = true;
						}
						if (bNextKeyValid)
						{
							bIsFlatTangent |= Value == NextValue;
						}
					}
					else if (bTangentGenericTimeIndependent)
					{
						//Spline tangent key, because bTangentGenericClampProgressive include bTangentGenericTimeIndependent, we must treat this case after bTangentGenericClampProgressive
						if (KeyCount == 1)
						{
							bIsFlatTangent = true;
						}
						else
						{
							//Spline tangent key must be User mode since we want to keep the tangents provide by the fbx key left and right derivatives
							NewTangentMode = EInterchangeCurveTangentMode::User;
						}
					}

					if (bIsFlatTangent)
					{
						RightTangent = 0;
						LeftTangent = 0;
						//To force flat tangent we need to set the tangent mode to user
						NewTangentMode = EInterchangeCurveTangentMode::User;
					}

				}
				break;
			}

			//auto with weighted give the wrong result, so when auto is weighted we set user mode and set the Right tangent equal to the left tangent.
			//Auto has only the left tangent set
			if (NewTangentMode == EInterchangeCurveTangentMode::Auto && (bLeftWeightActive || bRightWeightActive))
			{

				NewTangentMode = EInterchangeCurveTangentMode::User;
				RightTangent = LeftTangent;
			}

			if (NewTangentMode != EInterchangeCurveTangentMode::Auto)
			{
				const bool bEqualTangents = FMath::IsNearlyEqual(LeftTangent, RightTangent);
				//If tangents are different then broken.
				if (bEqualTangents)
				{
					NewTangentMode = EInterchangeCurveTangentMode::User;
				}
				else
				{
					NewTangentMode = EInterchangeCurveTangentMode::Break;
				}
			}

			//Only cubic interpolation allow weighted tangents
			if (KeyInterpMode == FbxAnimCurveDef::eInterpolationCubic)
			{
				if (bLeftWeightActive && bRightWeightActive)
				{
					NewTangentWeightMode = EInterchangeCurveTangentWeightMode::WeightedBoth;
				}
				else if (bLeftWeightActive)
				{
					NewTangentWeightMode = EInterchangeCurveTangentWeightMode::WeightedArrive;
					RightTangentWeight = DefaultCurveWeight;
				}
				else if (bRightWeightActive)
				{
					NewTangentWeightMode = EInterchangeCurveTangentWeightMode::WeightedLeave;
					LeftTangentWeight = DefaultCurveWeight;
				}
				else
				{
					NewTangentWeightMode = EInterchangeCurveTangentWeightMode::WeightedNone;
					LeftTangentWeight = DefaultCurveWeight;
					RightTangentWeight = DefaultCurveWeight;
				}

				auto ComputeWeightInternal = [](float TimeA, float TimeB, const float TangentSlope, const float TangentWeight)
				{
					const float X = TimeA - TimeB;
					const float Y = TangentSlope * X;
					return FMath::Sqrt(X * X + Y * Y) * TangentWeight;
				};

				if (!FMath::IsNearlyZero(LeftTangentWeight))
				{
					if (bPreviousKeyValid)
					{
						LeftTangentWeight = ComputeWeightInternal(KeyTimeValue, PreviousKeyTimeValue, LeftTangent, LeftTangentWeight);
					}
					else
					{
						LeftTangentWeight = 0.0f;
					}
				}

				if (!FMath::IsNearlyZero(RightTangentWeight))
				{
					if (bNextKeyValid)
					{
						RightTangentWeight = ComputeWeightInternal(NextKeyTimeValue, KeyTimeValue, RightTangent, RightTangentWeight);
					}
					else
					{
						RightTangentWeight = 0.0f;
					}
				}
			}

			const bool bForceDisableTangentRecompute = false; //No need to recompute all the tangents of the curve every time we change de key.
			InterchangeCurveKey.InterpMode = NewInterpMode;
			InterchangeCurveKey.TangentMode = NewTangentMode;
			InterchangeCurveKey.TangentWeightMode = NewTangentWeightMode;

			InterchangeCurveKey.ArriveTangent = LeftTangent;
			InterchangeCurveKey.LeaveTangent = RightTangent;
			InterchangeCurveKey.ArriveTangentWeight = LeftTangentWeight;
			InterchangeCurveKey.LeaveTangentWeight = RightTangentWeight;
		}
		return true;
	}

	template<typename AttributeType>
	void FillStepCurveAttribute(TArray<float>& OutFrameTimes, TArray<AttributeType>& OutFrameValues, const FbxAnimCurve* FbxCurve, TFunctionRef<AttributeType(const FbxAnimCurveKey*, const FbxTime*)> EvaluationFunction)
	{
		const int32 KeyCount = FbxCurve ? FbxCurve->KeyGetCount() : 0;

		if (KeyCount > 0)
		{
			OutFrameTimes.Reserve(KeyCount);
			OutFrameValues.Reserve(KeyCount);
			const FbxTime StartTime = FbxCurve->KeyGet(0).GetTime();

			for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
			{
				FbxAnimCurveKey Key = FbxCurve->KeyGet(KeyIndex);
				FbxTime KeyTime = Key.GetTime() - StartTime;

				OutFrameTimes.Add(KeyTime.GetSecondDouble());
				OutFrameValues.Add(EvaluationFunction(&Key, &KeyTime));
			}
		}
		else
		{
			OutFrameTimes.Add(0);
			OutFrameValues.Add(EvaluationFunction(nullptr, nullptr));
		}
	}

	void ImportFloatStepCurve(const FbxAnimCurve* SourceCurves, FbxProperty& Property, FInterchangeStepCurve& DestinationCurve)
	{
		TArray<float> StepCurveValues;
		FillStepCurveAttribute<float>(DestinationCurve.KeyTimes, StepCurveValues, SourceCurves, [&Property](const FbxAnimCurveKey* Key, const FbxTime* KeyTime)
			{
				if (Key)
				{
					return Key->GetValue();
				}
				else
				{
					return Property.Get<float>();
				}
			});
		DestinationCurve.FloatKeyValues = StepCurveValues;

	}

	void ImportIntegerStepCurve(const FbxAnimCurve* SourceCurves, FbxProperty& Property, FInterchangeStepCurve& DestinationCurve)
	{
		TArray<int32> StepCurveValues;
		FillStepCurveAttribute<int32>(DestinationCurve.KeyTimes, StepCurveValues, SourceCurves, [&Property](const FbxAnimCurveKey* Key, const FbxTime* KeyTime)
			{
				if (Key)
				{
					return static_cast<int32>(Key->GetValue());
				}
				else
				{
					return static_cast<int32>(Property.Get<int32>());
				}
			});
		DestinationCurve.IntegerKeyValues = StepCurveValues;
	}

	void ImportStringStepCurve(const FbxAnimCurve* SourceCurves, FbxProperty& Property, FInterchangeStepCurve& DestinationCurve)
	{
		TArray<FString> StepCurveValues;
		FillStepCurveAttribute<FString>(DestinationCurve.KeyTimes, StepCurveValues, SourceCurves, [&Property](const FbxAnimCurveKey* Key, const FbxTime* KeyTime)
			{
				if (KeyTime)
				{
					FbxPropertyValue& EvaluatedValue = Property.EvaluateValue(*KeyTime);
					FbxString StringValue;
					EvaluatedValue.Get(&StringValue, EFbxType::eFbxString);
					return FString(UTF8_TO_TCHAR(StringValue));
				}
				else
				{
					return FString(UTF8_TO_TCHAR(Property.Get<FbxString>()));
				}
			});
		DestinationCurve.StringKeyValues = StepCurveValues;
	}

	void ImportEnumStepCurve(const FbxAnimCurve* SourceCurves, FbxProperty& Property, FInterchangeStepCurve& DestinationCurve)
	{
		TArray<FString> StepCurveValues;
		FillStepCurveAttribute<FString>(DestinationCurve.KeyTimes, StepCurveValues, SourceCurves, [&Property](const FbxAnimCurveKey* Key, const FbxTime* KeyTime)
			{
				int32 EnumIndex = -1;

				if (KeyTime)
				{
					FbxPropertyValue& EvaluatedValue = Property.EvaluateValue(*KeyTime);
					EvaluatedValue.Get(&EnumIndex, EFbxType::eFbxEnum);
				}
				else
				{
					EnumIndex = Property.Get<FbxEnum>();
				}

				if (EnumIndex < 0 || EnumIndex >= Property.GetEnumCount())
				{
					return FString();
				}

				const char* EnumValue = Property.GetEnumValue(EnumIndex);
				return FString(UTF8_TO_TCHAR(EnumValue));
			});
		DestinationCurve.StringKeyValues = StepCurveValues;
	}

	bool ImportBakeTransforms(FbxNode* Node, FAnimationBakeTransformPayloadData& AnimationBakeTransformPayloadData)
	{
		if (!ensure(!FMath::IsNearlyZero(AnimationBakeTransformPayloadData.BakeFrequency)))
		{
			return false;
		}
		
		FbxTime StartTime;
		StartTime.SetSecondDouble(AnimationBakeTransformPayloadData.RangeStartTime);
		FbxTime EndTime;
		EndTime.SetSecondDouble(AnimationBakeTransformPayloadData.RangeEndTime);
		if (!ensure(AnimationBakeTransformPayloadData.RangeEndTime > AnimationBakeTransformPayloadData.RangeStartTime))
		{
			return false;
		}

		const double TimeStepSecond = 1.0 / AnimationBakeTransformPayloadData.BakeFrequency;
		FbxTime TimeStep = 0;
		TimeStep.SetSecondDouble(TimeStepSecond);

		const int32 NumFrame = FMath::RoundToInt32((AnimationBakeTransformPayloadData.RangeEndTime - AnimationBakeTransformPayloadData.RangeStartTime) * AnimationBakeTransformPayloadData.BakeFrequency);
		check(NumFrame >= 0);

		//Add a threshold when we compare if we have reach the end of the animation
		const FbxTime TimeComparisonThreshold = (UE_DOUBLE_KINDA_SMALL_NUMBER * static_cast<double>(FBXSDK_TC_SECOND));
		AnimationBakeTransformPayloadData.Transforms.Empty(NumFrame);

		for (FbxTime CurTime = StartTime; CurTime < (EndTime + TimeComparisonThreshold); CurTime += TimeStep)
		{
			FbxAMatrix NodeTransform = Node->EvaluateGlobalTransform(CurTime);
			FbxNode* ParentNode = Node->GetParent();
			if (ParentNode)
			{
				FbxAMatrix ParentTransform = ParentNode->EvaluateGlobalTransform(CurTime);
				NodeTransform = ParentTransform.Inverse() * NodeTransform;
			}
			AnimationBakeTransformPayloadData.Transforms.Add(UE::Interchange::Private::FFbxConvert::ConvertTransform(NodeTransform));
		}

		return true;
	}

	struct FGetFbxTransformCurvesParameters
	{
		FGetFbxTransformCurvesParameters(FbxScene* InSDKScene, FbxNode* InNode)
		{
			SDKScene = InSDKScene;
			check(SDKScene);
			Node = InNode;
			check(Node);
		}

		FbxScene* SDKScene = nullptr;
		FbxNode* Node = nullptr;
		TArray<FbxAnimCurve*> TransformChannelCurves;
		int32 TransformChannelCount = 0;
		bool IsNodeAnimated = false;
		FbxTimeSpan ExportedTimeSpan = FbxTimeSpan(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
		FbxTime StartTime = FBXSDK_TIME_INFINITE;
		FbxTime EndTime = FBXSDK_TIME_MINUS_INFINITE;
		int32 KeyCount = 0;
	};

	void GetFbxTransformCurves(FGetFbxTransformCurvesParameters& Parameters)
	{
		if (!ensure(Parameters.SDKScene) || !ensure(Parameters.Node))
		{
			return;
		}
		//Get the node transform curve keys, the transform components are separate into float curve
		//Translation X
		//Translation Y
		//Translation Z
		//Euler X
		//Euler Y
		//Euler Z
		//Scale X
		//Scale Y
		//Scale Z
		int32 NumAnimations = Parameters.SDKScene->GetSrcObjectCount<FbxAnimStack>();
		//Anim stack should be merge so we expect to have only one stack here
		ensure(NumAnimations <= 1);

		Parameters.IsNodeAnimated = false;

		Parameters.TransformChannelCount = 9;
		Parameters.TransformChannelCurves.Reset(Parameters.TransformChannelCount);
		for (int32 AnimationIndex = 0; AnimationIndex < NumAnimations && !Parameters.IsNodeAnimated; AnimationIndex++)
		{
			FbxAnimStack* AnimStack = (FbxAnimStack*)Parameters.SDKScene->GetSrcObject<FbxAnimStack>(AnimationIndex);
			int32 NumLayers = AnimStack->GetMemberCount();
			for (int LayerIndex = 0; LayerIndex < NumLayers && !Parameters.IsNodeAnimated; LayerIndex++)
			{
				FbxAnimLayer* AnimLayer = (FbxAnimLayer*)AnimStack->GetMember(LayerIndex);
				// Display curves specific to properties
				Parameters.TransformChannelCurves.Add(Parameters.Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false));
				Parameters.TransformChannelCurves.Add(Parameters.Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false));
				Parameters.TransformChannelCurves.Add(Parameters.Node->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false));

				Parameters.TransformChannelCurves.Add(Parameters.Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false));
				Parameters.TransformChannelCurves.Add(Parameters.Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false));
				Parameters.TransformChannelCurves.Add(Parameters.Node->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false));

				Parameters.TransformChannelCurves.Add(Parameters.Node->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X, false));
				Parameters.TransformChannelCurves.Add(Parameters.Node->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y, false));
				Parameters.TransformChannelCurves.Add(Parameters.Node->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z, false));

				if (!Parameters.IsNodeAnimated)
				{
					for (int32 ChannelIndex = 0; ChannelIndex < Parameters.TransformChannelCount; ++ChannelIndex)
					{
						if (Parameters.TransformChannelCurves[ChannelIndex])
						{
							Parameters.IsNodeAnimated = true;
							const int32 LocalKeycount = Parameters.TransformChannelCurves[ChannelIndex]->KeyGetCount();
							if (LocalKeycount > Parameters.KeyCount)
							{
								Parameters.KeyCount = LocalKeycount;
							}
							if (LocalKeycount > 1)
							{
								FbxTimeSpan LocalAnimatedTimeSpan(FBXSDK_TIME_INFINITE, FBXSDK_TIME_MINUS_INFINITE);
								Parameters.TransformChannelCurves[ChannelIndex]->GetTimeInterval(LocalAnimatedTimeSpan);
								if (Parameters.StartTime > LocalAnimatedTimeSpan.GetStart())
								{
									Parameters.StartTime = LocalAnimatedTimeSpan.GetStart();
								}
								if (Parameters.EndTime < LocalAnimatedTimeSpan.GetStop())
								{
									Parameters.EndTime = LocalAnimatedTimeSpan.GetStop();
								}
							}
							else if (Parameters.KeyCount == 1)
							{
								//When there is only one key there is no interval
								FbxAnimCurveKey FbxKey = Parameters.TransformChannelCurves[ChannelIndex]->KeyGet(0);
								FbxTime KeyTime = FbxKey.GetTime();
								if (Parameters.StartTime > KeyTime)
								{
									Parameters.StartTime = KeyTime;
								}
								if (Parameters.EndTime < KeyTime)
								{
									Parameters.EndTime = KeyTime;
								}
							}
						}
					}
				}
			}
		}
	}

	bool FAnimationPayloadContextTransform::FetchPayloadToFile(FFbxParser& Parser, const FString& PayloadFilepath)
	{
		if (AttributeFetchPayloadData.IsSet())
		{
			return InternalFetchCurveNodePayloadToFile(Parser, PayloadFilepath);
		}
		else if (MorphTargetFetchPayloadData.IsSet())
		{
			return InternalFetchMorphTargetCurvePayloadToFile(Parser, PayloadFilepath);
		}
		return false;
	}

	bool FAnimationPayloadContextTransform::FetchAnimationBakeTransformPayloadToFile(FFbxParser& Parser, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& PayloadFilepath)
	{
		if (!ensure(NodeTransformFetchPayloadData.IsSet()))
		{
			UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
			Message->Text = LOCTEXT("NodeTransformFetchPayloadData_NotSet", "Cannot fetch FBX animation transform payload because the FBX FNodeTransformFetchPayloadData is not set.");
			return false;
		}

		FNodeTransformFetchPayloadData& FetchPayloadData = NodeTransformFetchPayloadData.GetValue();
		if (!ensure(FetchPayloadData.Node))
		{
			UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
			Message->InterchangeKey = FFbxHelper::GetFbxNodeHierarchyName(FetchPayloadData.Node);
			Message->Text = LOCTEXT("FBXNodeNull", "Cannot fetch FBX animation transform payload because the FBX node is null.");
			return false;
		}

		bool bBakeTransform = false;
		FAnimationBakeTransformPayloadData AnimationBakeTransformPayloadData;
		AnimationBakeTransformPayloadData.BakeFrequency = BakeFrequency;
		AnimationBakeTransformPayloadData.RangeStartTime = RangeStartTime;
		AnimationBakeTransformPayloadData.RangeEndTime = RangeEndTime;

		ImportBakeTransforms(FetchPayloadData.Node, AnimationBakeTransformPayloadData);
		{
			FLargeMemoryWriter Ar;
			AnimationBakeTransformPayloadData.Serialize(Ar);
			uint8* ArchiveData = Ar.GetData();
			int64 ArchiveSize = Ar.TotalSize();
			TArray64<uint8> Buffer(ArchiveData, ArchiveSize);
			FFileHelper::SaveArrayToFile(Buffer, *PayloadFilepath);
		}
		return true;
	}

	bool FAnimationPayloadContextTransform::InternalFetchCurveNodePayloadToFile(FFbxParser& Parser, const FString& PayloadFilepath)
	{
		check(AttributeFetchPayloadData.IsSet());
		FAttributeFetchPayloadData& FetchPayloadData = AttributeFetchPayloadData.GetValue();
		if (!ensure(FetchPayloadData.Node != nullptr))
		{
			UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
			Message->InterchangeKey = FFbxHelper::GetFbxNodeHierarchyName(FetchPayloadData.Node);
			Message->Text = LOCTEXT("InternalFetchCurveNodePayloadToFile_FBXNodeNull", "Cannot fetch FBX animation curve payload because the FBX node is null.");
			return false;
		}

		if (!ensure(FetchPayloadData.AnimCurves != nullptr))
		{
			UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
			Message->InterchangeKey = FFbxHelper::GetFbxNodeHierarchyName(FetchPayloadData.Node);
			Message->Text = LOCTEXT("InternalFetchCurveNodePayloadToFile_FBXCurveNull", "Cannot fetch FBX user attribute animation curve payload because the FBX anim curve node is null.");
			return false;
		}
		if (FetchPayloadData.bAttributeTypeIsStepCurveAnimation)
		{
			//Fetch TArray<FInterchangeStepCurve> step curve, no interpolation
			TArray<FInterchangeStepCurve> InterchangeStepCurves;
			const uint32 ChannelCount = FetchPayloadData.AnimCurves->GetChannelsCount();
			for (uint32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
			{
				const uint32 ChannelCurveCount = FetchPayloadData.AnimCurves->GetCurveCount(ChannelIndex);
				for (uint32 CurveIndex = 0; CurveIndex < ChannelCurveCount; ++CurveIndex)
				{
					if (FbxAnimCurve* CurrentAnimCurve = FetchPayloadData.AnimCurves->GetCurve(ChannelIndex, CurveIndex))
					{
						switch (FetchPayloadData.PropertyType)
						{
							case EFbxType::eFbxBool:
							case EFbxType::eFbxChar:
							case EFbxType::eFbxUChar:
							case EFbxType::eFbxShort:
							case EFbxType::eFbxUShort:
							case EFbxType::eFbxInt:
							case EFbxType::eFbxUInt:
							case EFbxType::eFbxLongLong:
							case EFbxType::eFbxULongLong:
								ImportIntegerStepCurve(CurrentAnimCurve, FetchPayloadData.Property, InterchangeStepCurves.AddDefaulted_GetRef());
								break;
							case EFbxType::eFbxHalfFloat:
							case EFbxType::eFbxFloat:
							case EFbxType::eFbxDouble:
							case EFbxType::eFbxDouble2:
							case EFbxType::eFbxDouble3:
							case EFbxType::eFbxDouble4:
								check(false); //Float curve payload should be extract as FInterchangeCurve since we can interpolate them
								ImportFloatStepCurve(CurrentAnimCurve, FetchPayloadData.Property, InterchangeStepCurves.AddDefaulted_GetRef());
								break;
							case EFbxType::eFbxEnum:
								ImportEnumStepCurve(CurrentAnimCurve, FetchPayloadData.Property, InterchangeStepCurves.AddDefaulted_GetRef());
								break;
							case EFbxType::eFbxString:
								ImportStringStepCurve(CurrentAnimCurve, FetchPayloadData.Property, InterchangeStepCurves.AddDefaulted_GetRef());
								break;
						}
					}
				}
			}
			{
				FLargeMemoryWriter Ar;
				Ar << InterchangeStepCurves;
				uint8* ArchiveData = Ar.GetData();
				int64 ArchiveSize = Ar.TotalSize();
				TArray64<uint8> Buffer(ArchiveData, ArchiveSize);
				FFileHelper::SaveArrayToFile(Buffer, *PayloadFilepath);
			}
		}
		else
		{
			//Fetch TArray<FInterchangeCurve> which are float curves with interpolation
			TArray<FInterchangeCurve> InterchangeCurves;
			const uint32 ChannelCount = FetchPayloadData.AnimCurves->GetChannelsCount();
			for (uint32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
			{
				const uint32 ChannelCurveCount = FetchPayloadData.AnimCurves->GetCurveCount(ChannelIndex);
				for (uint32 CurveIndex = 0; CurveIndex < ChannelCurveCount; ++CurveIndex)
				{
					if (FbxAnimCurve* CurrentAnimCurve = FetchPayloadData.AnimCurves->GetCurve(ChannelIndex, CurveIndex))
					{
						ImportCurve(CurrentAnimCurve, 1.0f, InterchangeCurves.AddDefaulted_GetRef().Keys);
					}
				}
			}
			{
				FLargeMemoryWriter Ar;
				Ar << InterchangeCurves;
				uint8* ArchiveData = Ar.GetData();
				int64 ArchiveSize = Ar.TotalSize();
				TArray64<uint8> Buffer(ArchiveData, ArchiveSize);
				FFileHelper::SaveArrayToFile(Buffer, *PayloadFilepath);
			}
		}
		return true;
	}

	bool FAnimationPayloadContextTransform::InternalFetchMorphTargetCurvePayloadToFile(FFbxParser& Parser, const FString& PayloadFilepath)
	{
		check(MorphTargetFetchPayloadData.IsSet());
		FMorphTargetFetchPayloadData& FetchPayloadData = MorphTargetFetchPayloadData.GetValue();

		if(!FetchPayloadData.SDKScene)
		{
			UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
			Message->Text = LOCTEXT("InternalFetchMorphTargetCurvePayloadToFile_FBXSDKSceneNull", "InternalFetchMorphTargetCurvePayloadToFile, fbx sdk is nullptr.");
			return false;
		}
		
		int32 GeometryCount = FetchPayloadData.SDKScene->GetGeometryCount();

		FbxGeometry* Geometry = FetchPayloadData.SDKScene->GetGeometry(FetchPayloadData.GeometryIndex);
		if (!Geometry || Geometry->GetAttributeType() != FbxNodeAttribute::eMesh)
		{
			UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
			Message->Text = LOCTEXT("InternalFetchMorphTargetCurvePayloadToFile_FBXGeometryNull", "Cannot fetch FBX geometry from the scene.");
			return false;
		}

		FbxAnimCurve* AnimCurve = nullptr;
		int32 NumAnimations = FetchPayloadData.SDKScene->GetSrcObjectCount<FbxAnimStack>();
		//Anim stack should be merge so we expect to have only one stack here
		ensure(NumAnimations <= 1);
		if (FbxAnimStack* AnimStack = NumAnimations == 1 ? (FbxAnimStack*)FetchPayloadData.SDKScene->GetSrcObject<FbxAnimStack>(0) : nullptr)
		{
			AnimCurve = Geometry->GetShapeChannel(FetchPayloadData.MorphTargetIndex, FetchPayloadData.ChannelIndex, (FbxAnimLayer*)AnimStack->GetMember(0));
		}

		if (!ensure(AnimCurve != nullptr))
		{
			UInterchangeResultError_Generic* Message = Parser.AddMessage<UInterchangeResultError_Generic>();
			if (FbxNode* Node = Geometry->GetNode())
			{
				Message->InterchangeKey = FFbxHelper::GetFbxNodeHierarchyName(Node);
			}
			Message->Text = LOCTEXT("InternalFetchMorphTargetCurvePayloadToFile_FBXCurveNull", "Cannot fetch FBX morph target animation curve payload because the FBX anim curve node is null.");
			return false;
		}
		TArray<FInterchangeCurve> InterchangeCurves;
		//Morph target curve in fbx are between 0 and 100, in Unreal we are between 0 and 1, so we must scale
		//The curve with 0.01
		constexpr float ScaleCurve = 0.01f;
		ImportCurve(AnimCurve, ScaleCurve, InterchangeCurves.AddDefaulted_GetRef().Keys);
		{
			FLargeMemoryWriter Ar;
			Ar << InterchangeCurves;
			uint8* ArchiveData = Ar.GetData();
			int64 ArchiveSize = Ar.TotalSize();
			TArray64<uint8> Buffer(ArchiveData, ArchiveSize);
			FFileHelper::SaveArrayToFile(Buffer, *PayloadFilepath);
		}
		return true;
	}

	void FFbxAnimation::AddNodeTransformAnimation(FbxScene* SDKScene
		, FbxNode* Node
		, UInterchangeSceneNode* SceneNode
		, TMap<FString, TSharedPtr<FPayloadContextBase>>& PayloadContexts)
	{
		int32 NumAnimations = SDKScene->GetSrcObjectCount<FbxAnimStack>();
		//Anim stack should be merge so we expect to have only one stack here
		ensure(NumAnimations <= 1);

		double FrameRate = FbxTime::GetFrameRate(SDKScene->GetGlobalSettings().GetTimeMode());

		FGetFbxTransformCurvesParameters Parameters(SDKScene, Node);
		GetFbxTransformCurves(Parameters);

		if (Parameters.IsNodeAnimated)
		{
			FString PayLoadKey = FFbxHelper::GetFbxNodeHierarchyName(Node) + TEXT("_AnimationPayloadKey");
			if (ensure(!PayloadContexts.Contains(PayLoadKey)))
			{
				TSharedPtr<FAnimationPayloadContextTransform> AnimPayload = MakeShared<FAnimationPayloadContextTransform>();
				FNodeTransformFetchPayloadData FetchPayloadData;
				FetchPayloadData.Node = Node;
				AnimPayload->NodeTransformFetchPayloadData = FetchPayloadData;
				PayloadContexts.Add(PayLoadKey, AnimPayload);
			}
			UInterchangeAnimationAPI::SetCustomNodeTransformPayloadKey(SceneNode, PayLoadKey);
			UInterchangeAnimationAPI::SetCustomIsNodeTransformAnimated(SceneNode, Parameters.IsNodeAnimated);
			UInterchangeAnimationAPI::SetCustomNodeTransformAnimationKeyCount(SceneNode, Parameters.KeyCount);
			UInterchangeAnimationAPI::SetCustomNodeTransformAnimationStartTime(SceneNode, Parameters.StartTime.GetSecondDouble());
			UInterchangeAnimationAPI::SetCustomNodeTransformAnimationEndTime(SceneNode, Parameters.EndTime.GetSecondDouble());
		}
	}

	void FFbxAnimation::AddNodeAttributeCurvesAnimation(FbxNode* Node
		, FbxProperty& Property
		, FbxAnimCurveNode* AnimCurveNode
		, UInterchangeSceneNode* SceneNode
		, TMap<FString, TSharedPtr<FPayloadContextBase>>& PayloadContexts
		, EFbxType PropertyType
		, TOptional<FString>& OutPayloadKey)
	{
		const FString PropertyName = FFbxHelper::GetFbxPropertyName(Property);
		const FString PayLoadKey = FFbxHelper::GetFbxNodeHierarchyName(Node) + PropertyName + TEXT("_AnimationPayloadKey");
		if (ensure(!PayloadContexts.Contains(PayLoadKey)))
		{
			TSharedPtr<FAnimationPayloadContextTransform> AnimPayload = MakeShared<FAnimationPayloadContextTransform>();
			FAttributeFetchPayloadData FetchPayloadData;
			FetchPayloadData.Node = Node;
			FetchPayloadData.AnimCurves = AnimCurveNode;

			//Any property that is not decimal should import with step curve
			FetchPayloadData.bAttributeTypeIsStepCurveAnimation = !IsFbxPropertyTypeDecimal(PropertyType);
			FetchPayloadData.PropertyType = PropertyType;
			FetchPayloadData.Property = Property;
			AnimPayload->AttributeFetchPayloadData = FetchPayloadData;
			PayloadContexts.Add(PayLoadKey, AnimPayload);
			OutPayloadKey = PayLoadKey;
		}
	}

	void FFbxAnimation::AddMorphTargetCurvesAnimation(UInterchangeMeshNode* MorphTargetNode
		, FbxScene* SDKScene
		, int32 GeometryIndex
		, int32 MorphTargetIndex
		, int32 ChannelIndex
		, const FString& MorphTargetPayloadKey
		, TMap<FString, TSharedPtr<FPayloadContextBase>>& PayloadContexts)
	{
		check(MorphTargetNode);
		const FString PayLoadKey = MorphTargetPayloadKey + TEXT("_CurveAnimationPayloadKey");
		if (ensure(!PayloadContexts.Contains(PayLoadKey)))
		{
			TSharedPtr<FAnimationPayloadContextTransform> AnimPayload = MakeShared<FAnimationPayloadContextTransform>();
			FMorphTargetFetchPayloadData FetchPayloadData;
			FetchPayloadData.SDKScene = SDKScene;
			FetchPayloadData.GeometryIndex = GeometryIndex;
			FetchPayloadData.MorphTargetIndex = MorphTargetIndex;
			FetchPayloadData.ChannelIndex = ChannelIndex;
			AnimPayload->MorphTargetFetchPayloadData = FetchPayloadData;

			PayloadContexts.Add(PayLoadKey, AnimPayload);
		}
		MorphTargetNode->SetAnimationCurvePayLoadKey(PayLoadKey);
	}

	bool FFbxAnimation::IsFbxPropertyTypeSupported(EFbxType PropertyType)
	{
		switch (PropertyType)
		{
		case EFbxType::eFbxBool:
		case EFbxType::eFbxChar:		//!< 8 bit signed integer.
		case EFbxType::eFbxUChar:		//!< 8 bit unsigned integer.
		case EFbxType::eFbxShort:		//!< 16 bit signed integer.
		case EFbxType::eFbxUShort:		//!< 16 bit unsigned integer.
		case EFbxType::eFbxInt:			//!< 32 bit signed integer.
		case EFbxType::eFbxUInt:		//!< 32 bit unsigned integer.
		case EFbxType::eFbxLongLong:	//!< 64 bit signed integer.
		case EFbxType::eFbxULongLong:	//!< 64 bit unsigned integer.
		case EFbxType::eFbxHalfFloat:	//!< 16 bit floating point.
		case EFbxType::eFbxFloat:		//!< Floating point value.
		case EFbxType::eFbxDouble:	//!< Double width floating point value.
		case EFbxType::eFbxDouble2:	//!< Vector of two double values.
		case EFbxType::eFbxDouble3:	//!< Vector of three double values.
		case EFbxType::eFbxDouble4:	//!< Vector of four double values.
		case EFbxType::eFbxEnum:		//!< Enumeration.
		case EFbxType::eFbxString:	//!< String.
			return true;
		}

		return false;
	};

	bool FFbxAnimation::IsFbxPropertyTypeDecimal(EFbxType PropertyType)
	{
		switch (PropertyType)
		{
		case EFbxType::eFbxHalfFloat:
		case EFbxType::eFbxFloat:
		case EFbxType::eFbxDouble:
		case EFbxType::eFbxDouble2:
		case EFbxType::eFbxDouble3:
		case EFbxType::eFbxDouble4:
			return true;
		}

		return false;
	}
}//ns UE::Interchange::Private

#undef LOCTEXT_NAMESPACE
