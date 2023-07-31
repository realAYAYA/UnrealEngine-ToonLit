// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Gltf/InterchangeGltfPrivate.h"

#include "Animation/InterchangeAnimationPayload.h"
#include "InterchangeImportLog.h"

#include "GLTFAsset.h"

#include "Animation/AnimTypes.h"

namespace UE::Interchange::Gltf::Private
{
	/*
	* According to gltf specification Seconds acquired from Samplers.input:
	* "The values represent time in seconds with time[0] >= 0.0, and strictly increasing values, Index.e., time[n + 1] > time[n]."
	* Meaning the incoming array(Seconds) is sorted.
	*/
	TTuple<int32, int32> GetNeighbourIndices(TArray<float>& Seconds, float SampleTime)
	{
		int32 Lower = 0;
		int32 Higher = 0;

		if (Seconds.Num() == 0)
		{
			return TTuple<int32, int32>(0, 0);
		}
		else if (Seconds.Num() == 1)
		{
			return TTuple<int32, int32>(Lower, Higher);
		}

		if (SampleTime < Seconds[0])
		{
			Lower = Higher = 0;
		}
		else if (SampleTime >= Seconds[Seconds.Num() - 1])
		{
			Lower = Higher = Seconds.Num() - 1;
		}
		else
		{
			for (size_t Index = 0; Index < Seconds.Num() - 1; Index++)
			{
				if ((Seconds[Index] <= SampleTime) && (SampleTime < Seconds[Index + 1]))
				{
					Lower = Index;
					Higher = Index + 1;
					break;
				}
			}
		}

		return TTuple<int32, int32>(Lower, Higher);
	}

	template <typename T>
	T InterpolateValue(TArray<float>& Seconds /*input*/, TArray<T>& SegmentValues /*output*/, int32 kIndex /*lower*/, int32 kp1Index /*higher*/, double CurrentRequestedTimeStamp, GLTF::FAnimation::EInterpolation Interpolation, bool IsRotation)
	{
		if (SegmentValues.Num() <= kIndex || SegmentValues.Num() <= kp1Index || kIndex < 0 || kp1Index < 0)
		{
			UE_LOG(LogInterchangeImport, Warning, TEXT("Interpolation Failed due to invalid kIndices"));
			return T();
		}

		if (kIndex == kp1Index)
		{
			if (Interpolation == GLTF::FAnimation::EInterpolation::CubicSpline)
			{
				return SegmentValues[kIndex * 3 + 1];
			}
			return SegmentValues[kp1Index];
		}

		// To match variables against https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-c-interpolation
		// Time := Time
		// TimeDelta := td
		double TimeDelta = Seconds[kp1Index] - Seconds[kIndex];
		double Time = (CurrentRequestedTimeStamp - Seconds[kIndex]) / TimeDelta;

		T Value;
		switch (Interpolation)
		{
		case GLTF::FAnimation::EInterpolation::Linear:
		{
			if (IsRotation)
			{
				//Spherical Linear Interpolation
				// a=arccos(|vk⋅vk+1|)
				// s=vk⋅vk+1 / |vk⋅vk+1|

				T vk = SegmentValues[kIndex];
				T vkp1 = SegmentValues[kp1Index];

				double DotProduct = FMath::Clamp(double(vk | vkp1), -1.0, 1.0);
				double a = acos(abs(DotProduct));

				if (FMath::IsNearlyZero(a))
				{
					//Implementation Note
					//	When a is close to zero, spherical linear interpolation turns into regular linear interpolation.
					Value = (1.0 - Time) * vk + Time * vkp1;
				}
				else
				{
					double s = DotProduct / abs(DotProduct);

					// vt=(sin(a(1−t))/sin(a))∗vk + s∗(sin(at)/sin(a))∗vk+1

					Value = (sin(a * (1.0 - Time)) / sin(a)) * vk + s * (sin(a * Time) / sin(a)) * vkp1;
				}
			}
			else
			{
				//Normal Linear Interpolation
				// vt=(1−t)∗vk + t∗vk+1

				T vk = SegmentValues[kIndex];
				T vkp1 = SegmentValues[kp1Index];

				Value = (1.0 - Time) * vk + Time * vkp1;
			}
			break;
		}
		case GLTF::FAnimation::EInterpolation::Step:
		{
			//Step Interpolation
			// vt=vk

			T vk = SegmentValues[kIndex];
			T vkp1 = SegmentValues[kp1Index];
			if (Time < 0.5)
			{
				Value = vk;
			}
			else
			{
				Value = vkp1;
			}
			break;
		}
		case GLTF::FAnimation::EInterpolation::CubicSpline:
		{
			//Cubic Spline Interpolation
			// For each timestamp stored in the animation sampler, there are three associated keyframe values: in-tangent, property Value, and out-tangent.
			// ak, vk, and bk be the in-tangent, the property Value, and the out-tangent of the k-th frame respectively.
			// The interpolated sampler Value vt at the timestamp tc is computed as follows.
			// vt = (2t^3−3t^2 + 1)∗vk 
			//		+ td(t^3−2t^2 + t)∗bk 
			//		+ (−2t^3 + 3t^2)∗vk + 1 
			//		+ td(t^3−t^2)∗ak + 1

			T ak = SegmentValues[kIndex * 3 + 0];
			T vk = SegmentValues[kIndex * 3 + 1];
			T bk = SegmentValues[kIndex * 3 + 2];
			T akp1 = SegmentValues[kp1Index * 3 + 0];
			T vkp1 = SegmentValues[kp1Index * 3 + 1];
			T bkp1 = SegmentValues[kp1Index * 3 + 2];

			Value = (2 * Time * Time * Time - 3 * Time * Time + 1) * vk 
				+ TimeDelta * (Time * Time * Time - 2 * Time * Time + Time) * bk 
				+ (-2 * Time * Time * Time + 3 * Time * Time) * vkp1 
				+ TimeDelta * (Time * Time * Time - Time * Time) * akp1;

			break;
		}

		default:
			break;
		}

		return Value;
	}

	bool ParsePayLoadKey(const GLTF::FAsset& GltfAsset, const FString& PayLoadKey, int32& AnimationIndex, TArray<int32>& ChannelIndices)
	{
		TArray<FString> PayLoadValues;

		PayLoadKey.ParseIntoArray(PayLoadValues, TEXT(":"));

		LexFromString(AnimationIndex, *PayLoadValues[0]);

		if (!GltfAsset.Animations.IsValidIndex(AnimationIndex))
		{
			return false;
		}

		for (size_t Index = 1; Index < PayLoadValues.Num(); Index++)
		{
			int32 ChannelIndex;
			LexFromString(ChannelIndex, *PayLoadValues[Index]);

			if (GltfAsset.Animations[AnimationIndex].Channels.IsValidIndex(ChannelIndex))
			{
				ChannelIndices.Add(ChannelIndex);
			}
		}

		return ChannelIndices.Num() != 0;
	}

	bool GetAnimationTransformPayloadData(const FString& PayLoadKey, const GLTF::FAsset& GltfAsset, FAnimationCurvePayloadData& OutPayloadData)
	{
		//translation, rotation, scale time 3 channels:
		OutPayloadData.Curves.SetNum(9);

		TArray<int32> ChannelIndices;
		int32 AnimationIndex;

		if (!ParsePayLoadKey(GltfAsset, PayLoadKey, AnimationIndex, ChannelIndices))
		{
			return false;
		}
		const GLTF::FAnimation& Animation = GltfAsset.Animations[AnimationIndex];
		
		for (const int32 ChannelIndex : ChannelIndices)
		{
			const GLTF::FAnimation::FChannel& Channel = Animation.Channels[ChannelIndex];
			const GLTF::FAnimation::FSampler& Sampler = Animation.Samplers[Channel.Sampler];

			TArray<float> FrameTimeBuffer;
			TArray<float> FrameDataBuffer;

			Sampler.Input.GetFloatArray(FrameTimeBuffer);

			ERichCurveInterpMode InterpolationMode = static_cast<ERichCurveInterpMode>(Sampler.Interpolation);

			TFunction<void(TFunction<FVector3f(const float*)>, int32, int32, bool)> ConvertCurve = [InterpolationMode, &FrameTimeBuffer, &FrameDataBuffer, &OutPayloadData](TFunction<FVector3f(const float*)> ConvertSample, int32 CurvesIndexOffset, int32 SampleSize, bool bUnwindRotation)
			{
				int32 ActualSampleSize = SampleSize;
				if (InterpolationMode == ERichCurveInterpMode::RCIM_Cubic)
				{
					ActualSampleSize *= 3;
				}
				const int32 SampleCount = FrameDataBuffer.Num() / ActualSampleSize;

				FRichCurve& Curve0 = OutPayloadData.Curves[CurvesIndexOffset + 0];
				FRichCurve& Curve1 = OutPayloadData.Curves[CurvesIndexOffset + 1];
				FRichCurve& Curve2 = OutPayloadData.Curves[CurvesIndexOffset + 2];

				for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
				{


					if (InterpolationMode == ERichCurveInterpMode::RCIM_Cubic)
					{
						const float* RawSampleArrive = FrameDataBuffer.GetData() + (SampleIndex * ActualSampleSize) + 0 * SampleSize;
						const float* RawSampleValue = FrameDataBuffer.GetData() + (SampleIndex * ActualSampleSize) + 1 * SampleSize;
						const float* RawSampleLeave = FrameDataBuffer.GetData() + (SampleIndex * ActualSampleSize) + 2 * SampleSize;

						FVector3f SampleArrive = ConvertSample(RawSampleArrive);
						FVector3f SampleValue = ConvertSample(RawSampleValue);
						FVector3f SampleLeave = ConvertSample(RawSampleLeave);

						FKeyHandle Key0 = Curve0.AddKey(FrameTimeBuffer[SampleIndex], SampleValue[0], bUnwindRotation);
						FRichCurveKey& CurveKey0 = Curve0.GetKey(Key0);
						CurveKey0.ArriveTangent = SampleArrive[0];
						CurveKey0.LeaveTangent = SampleLeave[0];

						FKeyHandle Key1 = Curve1.AddKey(FrameTimeBuffer[SampleIndex], SampleValue[1], bUnwindRotation);
						FRichCurveKey& CurveKey1 = Curve1.GetKey(Key1);
						CurveKey1.ArriveTangent = SampleArrive[1];
						CurveKey1.LeaveTangent = SampleLeave[1];

						FKeyHandle Key2 = Curve2.AddKey(FrameTimeBuffer[SampleIndex], SampleValue[2], bUnwindRotation);
						FRichCurveKey& CurveKey2 = Curve2.GetKey(Key2);
						CurveKey2.ArriveTangent = SampleArrive[2];
						CurveKey2.LeaveTangent = SampleLeave[2];

						Curve0.SetKeyInterpMode(Key0, InterpolationMode);
						Curve1.SetKeyInterpMode(Key1, InterpolationMode);
						Curve2.SetKeyInterpMode(Key2, InterpolationMode);
					}
					else
					{
						const float* RawSample = FrameDataBuffer.GetData() + (SampleIndex * ActualSampleSize);
						FVector3f Sample = ConvertSample(RawSample);

						FKeyHandle Key0 = Curve0.AddKey(FrameTimeBuffer[SampleIndex], Sample[0], bUnwindRotation);
						FKeyHandle Key1 = Curve1.AddKey(FrameTimeBuffer[SampleIndex], Sample[1], bUnwindRotation);
						FKeyHandle Key2 = Curve2.AddKey(FrameTimeBuffer[SampleIndex], Sample[2], bUnwindRotation);

						Curve0.SetKeyInterpMode(Key0, InterpolationMode);
						Curve1.SetKeyInterpMode(Key1, InterpolationMode);
						Curve2.SetKeyInterpMode(Key2, InterpolationMode);
					}
				}
			};

			switch (Channel.Target.Path)
			{
			case GLTF::FAnimation::EPath::Translation:
			{
				FrameDataBuffer.SetNumUninitialized(Sampler.Output.Count * 3);
				Sampler.Output.GetCoordArray(reinterpret_cast<FVector3f*>(FrameDataBuffer.GetData()));
				const float Scale = GltfUnitConversionMultiplier; // Convert m to cm
				ConvertCurve([&Scale](const float* VecData)
					{
						return FVector3f(VecData[0] * Scale, VecData[1] * Scale, VecData[2] * Scale);
					}, 0, 3, false);
				break;
			}
			case GLTF::FAnimation::EPath::Rotation:
			{
				FrameDataBuffer.SetNumUninitialized(Sampler.Output.Count * 4);
				Sampler.Output.GetQuatArray(reinterpret_cast<FVector4f*>(FrameDataBuffer.GetData()));

				struct HeadingHelper
				{
					FRotator3f Heading;
					bool bHeadingSet = false;
				};
				HeadingHelper Helper;
				ConvertCurve([&Helper](const float* QuatData)
					{
						//GLTF uses quaternions while Uneal uses Eulers for level animations
						// Quaternions can represent multiple Orientations in Eulers.
						// We cannot tell which precise Orientation version was the author of gltf trying to achieve at the moment of creation.
						// For that reason we compare each consecutive orientation and pick the closest (by ManhattanDistance) from the 2 EquivalentRotators.
						// This means that animations of Rotations can be correctly represented with this system, if
						//    every consecutive frame's intended (absolute) rotations are less than 3x180 degrees (aka less than 180 degrees per axis)

						FQuat4f Quat(QuatData[0], QuatData[1], QuatData[2], QuatData[3]);
						FRotator3f Rotator = Quat.Rotator();
						if (Helper.bHeadingSet)
						{
							FRotator3f OtherChoice = Rotator.GetEquivalentRotator().GetNormalized();
							float FirstDiff = Helper.Heading.GetManhattanDistance(Rotator);
							float SecondDiff = Helper.Heading.GetManhattanDistance(OtherChoice);
							if (SecondDiff < FirstDiff)
							{
								Rotator = OtherChoice;
							}
						}
						else
						{
							Helper.bHeadingSet = true;
						}

						Helper.Heading = Rotator;
						
						return Rotator.Euler();
					}, 3, 4, true);
				break;
			}
			case GLTF::FAnimation::EPath::Scale:
			{
				FrameDataBuffer.SetNumUninitialized(Sampler.Output.Count * 3);
				Sampler.Output.GetCoordArray(reinterpret_cast<FVector3f*>(FrameDataBuffer.GetData()));
				ConvertCurve([](const float* VecData)
					{
						return FVector3f(VecData[0], VecData[1], VecData[2]);
					}, 6, 3, false);
				break;
			}
			case GLTF::FAnimation::EPath::Weights:
			{
				UE_LOG(LogInterchangeImport, Warning, TEXT("Morph Animation type not supported yet."));
				break;
			}
			default:
				UE_LOG(LogInterchangeImport, Warning, TEXT("Animation type not supported"));
				break;
			}
		}
		return true;
	}

	bool GetBakedAnimationTransformPayloadData(const FString& PayLoadKey, const GLTF::FAsset& GltfAsset, FAnimationBakeTransformPayloadData& PayloadData)
	{
		TArray<int32> ChannelIndices;
		int32 AnimationIndex;

		if (!ParsePayLoadKey(GltfAsset, PayLoadKey, AnimationIndex, ChannelIndices))
		{
			return false;
		}
		const GLTF::FAnimation& GltfAnimation = GltfAsset.Animations[AnimationIndex];
		
		const double BakeInterval = 1.0 / PayloadData.BakeFrequency;
		const double SequenceLength = FMath::Max<double>(PayloadData.RangeEndTime - PayloadData.RangeStartTime, MINIMUM_ANIMATION_LENGTH);
		int32 BakeKeyCount = (SequenceLength / BakeInterval) + 1;

		//buffers to use for final Transform:
		TArray<FVector3f> TranslationData;
		TArray<FQuat4f> RotationData;
		TArray<FVector3f> ScaleData;

		//initial transforms:
		//channels are targetting the same node:
		bool BaseTransformsAcquired = false;

		for (const int32& ChannelIndex : ChannelIndices)
		{
			const GLTF::FAnimation::FChannel& Channel = GltfAnimation.Channels[ChannelIndex];
			const GLTF::FAnimation::FSampler& Sampler = GltfAnimation.Samplers[Channel.Sampler];

			if (!BaseTransformsAcquired)
			{
				//on the first valid channel id we set the targeted nodes default transforms
				//the default transforms will be overwritten by the provided animation data, IF they are provided
				BaseTransformsAcquired = true;
				FVector3f JointBaseTranslation(Channel.Target.Node.Transform.GetTranslation());
				FQuat4f JointBaseRotation(Channel.Target.Node.Transform.GetRotation());
				FVector3f JointBaseScale(Channel.Target.Node.Transform.GetScale3D());

				TranslationData.Init(JointBaseTranslation, BakeKeyCount);
				RotationData.Init(JointBaseRotation, BakeKeyCount);
				ScaleData.Init(JointBaseScale, BakeKeyCount);
			}

			TArray<float> Seconds;
			Sampler.Input.GetFloatArray(Seconds);

			if (Seconds.Num() == 0)
			{
				continue;
			}

			switch (Channel.Target.Path)
			{
			case GLTF::FAnimation::EPath::Translation:
			{
				TArray<FVector3f> TranslationBuffer;
				TranslationBuffer.SetNumUninitialized(Sampler.Output.Count);
				Sampler.Output.GetCoordArray(TranslationBuffer.GetData());
				const float Scale = GltfUnitConversionMultiplier; // Convert m to cm
				for (FVector3f& Position : TranslationBuffer)
				{
					Position *= Scale;
				}

				for (size_t Index = 0; Index < BakeKeyCount; Index++)
				{
					double CurrentRequestedTimeStamp = Index * BakeInterval;
					TTuple<int32, int32> LowerHigherIndices = GetNeighbourIndices(Seconds, CurrentRequestedTimeStamp);
					TranslationData[Index] = InterpolateValue(Seconds, TranslationBuffer, LowerHigherIndices.Key, LowerHigherIndices.Value, CurrentRequestedTimeStamp, Sampler.Interpolation, false);
				}
				break;
			}
			case GLTF::FAnimation::EPath::Rotation:
			{
				TArray<FQuat4f> RotationBuffer;
				TArray<FVector4f> RotationBufferTemp;
				RotationBufferTemp.SetNumUninitialized(Sampler.Output.Count);
				RotationBuffer.SetNumUninitialized(Sampler.Output.Count);

				Sampler.Output.GetQuatArray(RotationBufferTemp.GetData());

				for (size_t Index = 0; Index < Sampler.Output.Count; Index++)
				{
					RotationBuffer[Index] = FQuat4f(RotationBufferTemp[Index][0], RotationBufferTemp[Index][1], RotationBufferTemp[Index][2], RotationBufferTemp[Index][3]);
				}

				//When the animation sampler targets a node’s rotation property, the interpolated quaternion MUST be normalized before applying the result to the node’s rotation.
				for (size_t Index = 0; Index < BakeKeyCount; Index++)
				{
					double CurrentRequestedTimeStamp = Index * BakeInterval;
					TTuple<int32, int32> LowerHigherIndices = GetNeighbourIndices(Seconds, CurrentRequestedTimeStamp);
					RotationData[Index] = InterpolateValue(Seconds, RotationBuffer, LowerHigherIndices.Key, LowerHigherIndices.Value, CurrentRequestedTimeStamp, Sampler.Interpolation, true);
					RotationData[Index].Normalize();
				}
				break;
			}
			case GLTF::FAnimation::EPath::Scale:
			{
				TArray<FVector3f> ScaleBuffer;
				ScaleBuffer.SetNumUninitialized(Sampler.Output.Count);
				Sampler.Output.GetCoordArray(ScaleBuffer.GetData());

				for (size_t Index = 0; Index < BakeKeyCount; Index++)
				{
					double CurrentRequestedTimeStamp = Index * BakeInterval;
					TTuple<int32, int32> LowerHigherIndices = GetNeighbourIndices(Seconds, CurrentRequestedTimeStamp);
					ScaleData[Index] = InterpolateValue(Seconds, ScaleBuffer, LowerHigherIndices.Key, LowerHigherIndices.Value, CurrentRequestedTimeStamp, Sampler.Interpolation, false);
				}

				break;
			}
			default:
				UE_LOG(LogInterchangeImport, Warning, TEXT("Animation type not supported"));
				break;
			}
		}

		for (size_t Index = 0; Index < BakeKeyCount; Index++)
		{
			PayloadData.Transforms.Add(FTransform(
				FQuat4d(RotationData[Index]),
				FVector3d(TranslationData[Index]),
				FVector3d(ScaleData[Index])));
		}

		return true;
	}
}