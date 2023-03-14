// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithGLTFAnimationImporter.h"

#include "GLTFAsset.h"

#include "DatasmithSceneFactory.h"
#include "IDatasmithSceneElements.h"
#include "DatasmithUtils.h"

#include "Math/InterpCurve.h"

namespace DatasmithGLTFImporterImpl
{
	EDatasmithTransformType ConvertToTransformType(GLTF::FAnimation::EPath Path)
	{
		switch (Path)
		{
			case GLTF::FAnimation::EPath::Translation:
				return EDatasmithTransformType::Translation;
			case GLTF::FAnimation::EPath::Rotation:
				return EDatasmithTransformType::Rotation;
			case GLTF::FAnimation::EPath::Scale:
				return EDatasmithTransformType::Scale;
			case GLTF::FAnimation::EPath::Weights:
			default:
				check(false);
				return EDatasmithTransformType::Translation;
		}
	}

	FDatasmithTransformFrameInfo CreateFrameInfo(float FrameNumber, const FVector& Vec)
	{
		check(FrameNumber >= 0.f);
		FrameNumber = FMath::RoundFromZero(FrameNumber);
		return FDatasmithTransformFrameInfo(FrameNumber, Vec);
	}

	template<typename TSourceType, typename TConvertToUE>
	void ResampleTrack(
		const FFrameRate& FrameRate,
		const TArray<float>& FrameTimeBuffer,
		const TSourceType* FrameSourceBuffer,
		GLTF::FAnimation::EInterpolation Interpolation,
		TConvertToUE ConvertToUE,
		IDatasmithTransformAnimationElement& AnimationElement,
		const EDatasmithTransformType TransformType)
	{
		FInterpCurve<TSourceType> InterpCurve;
		float MinKey = FLT_MAX;
		float MaxKey = -FLT_MAX;

		for (int FrameIndex = 0; FrameIndex < FrameTimeBuffer.Num(); ++FrameIndex)
		{
			float FrameTime = FrameTimeBuffer[FrameIndex];

			FInterpCurvePoint<TSourceType> CurvePoint;

			if (Interpolation == GLTF::FAnimation::EInterpolation::CubicSpline)
			{
				int32 BufferIndex = FrameIndex * 3 + 1; // spline vertex value located between two tangents

				// "When used with CUBICSPLINE interpolation, tangents (ak, bk) and values (vk) are grouped within keyframes:
				// a1, a2, ...an, v1, v2, ...vn, b1, b2, ...bn"
				// https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#animations
				CurvePoint = FInterpCurvePoint<TSourceType>(FrameTime,
					FrameSourceBuffer[FrameIndex * 3 + 1], // value
					FrameSourceBuffer[FrameIndex * 3 + 0], // in-tangent
					FrameSourceBuffer[FrameIndex * 3 + 2], // out-tangent
					EInterpCurveMode::CIM_CurveUser
					);
			}
			else
			{
				int32 BufferIndex = FrameIndex;
				CurvePoint = FInterpCurvePoint<TSourceType>(FrameTime, FrameSourceBuffer[BufferIndex]);

				switch (Interpolation)
				{
				case GLTF::FAnimation::EInterpolation::Step:
					CurvePoint.InterpMode = EInterpCurveMode::CIM_Constant;
					break;
				case GLTF::FAnimation::EInterpolation::Linear:
					CurvePoint.InterpMode = EInterpCurveMode::CIM_Linear;
					break;
				};
			}

			InterpCurve.Points.Add(CurvePoint);

			MinKey = FMath::Min(MinKey, FrameTime);
			MaxKey = FMath::Max(MaxKey, FrameTime);
		}

		FFrameNumber StartFrame = FrameRate.AsFrameNumber(MinKey);

		// If we use AsFrameNumber it will floor, and we might lose the very end of the animation
		const double TimeAsFrame = (double(MaxKey) * FrameRate.Numerator) / FrameRate.Denominator;
		FFrameNumber EndFrame = FFrameNumber(static_cast<int32>(FMath::CeilToDouble(TimeAsFrame)));

		// We go to EndFrame.Value+1 here so that if its a 2 second animation at 30fps, frame 60 belongs
		// to the actual animation, as opposed to being range [0, 59]. This guarantees that the animation will
		// actually complete within its range, which is necessary in order to play it correctly at runtime
		for (int32 Frame = StartFrame.Value; Frame <= EndFrame.Value + 1; ++Frame)
		{
			float TimeSeconds = FrameRate.AsSeconds(Frame);

			AnimationElement.AddFrame(TransformType, FDatasmithTransformFrameInfo(Frame, (FVector)ConvertToUE(InterpCurve.Eval(TimeSeconds))));
		}
	}

	class FAnimationResampler
	{
	public:

		FAnimationResampler(FFrameRate InFrameRate, const TArray<float>& InFrameTimeBuffer, IDatasmithTransformAnimationElement& InAnimationElement) 
			: FrameRate(InFrameRate)
			, FrameTimeBuffer(InFrameTimeBuffer)
			, AnimationElement(InAnimationElement)
		{}

		void ResampleRotation(
			GLTF::FAnimation::EInterpolation Interpolation,
			const FQuat4f* FrameSourceBuffer)
		{
			ResampleTrack<FQuat4f>(FrameRate, 
				FrameTimeBuffer, FrameSourceBuffer, Interpolation,
				[](const FQuat4f& Quat) { return Quat.Euler(); },
				AnimationElement, EDatasmithTransformType::Rotation);
		}

		void ResampleTranslation(
			GLTF::FAnimation::EInterpolation Interpolation,
			const FVector3f* FrameSourceBuffer,
			float ScaleFactor)
		{
			ResampleTrack<FVector3f>(FrameRate,
				FrameTimeBuffer, FrameSourceBuffer, Interpolation,
				[ScaleFactor](const FVector3f& Vec) { return Vec * ScaleFactor; },
				AnimationElement, EDatasmithTransformType::Translation);
		}

		void ResampleScale(
			GLTF::FAnimation::EInterpolation Interpolation,
			const FVector3f* FrameSourceBuffer)
		{
			ResampleTrack<FVector3f>(FrameRate,
				FrameTimeBuffer, FrameSourceBuffer, Interpolation,
				[](const FVector3f& Vec) { return Vec; },
				AnimationElement, EDatasmithTransformType::Scale);
		}

	private:
		FFrameRate FrameRate;
		const TArray<float>& FrameTimeBuffer;
		IDatasmithTransformAnimationElement& AnimationElement;
	};
}

FDatasmithGLTFAnimationImporter::FDatasmithGLTFAnimationImporter(TArray<GLTF::FLogMessage>& LogMessages)
    : CurrentScene(nullptr)
    , LogMessages(LogMessages)
{
}

void FDatasmithGLTFAnimationImporter::CreateAnimations(const GLTF::FAsset& GLTFAsset, bool bAnimationFPSFromFile)
{
	using namespace DatasmithGLTFImporterImpl;

	check(CurrentScene);
	
	TArray<float> FrameTimes;

	ImportedSequences.Empty();
	for (const GLTF::FAnimation& Animation : GLTFAsset.Animations)
	{
		TSharedRef<IDatasmithLevelSequenceElement> SequenceElement = FDatasmithSceneFactory::CreateLevelSequence(*Animation.Name);
	
		NodeChannelMap.Empty(GLTFAsset.Nodes.Num() / 2);

		float FramesPerSec = SequenceElement->GetFrameRate();

		for (const GLTF::FAnimation::FChannel& Channel : Animation.Channels)
		{
			if (bAnimationFPSFromFile)
			{
				// Compute FPS from glTF data
				const GLTF::FAnimation::FSampler& Sampler = Animation.Samplers[Channel.Sampler];

				FrameTimes.Empty(Sampler.Input.Count);
				Sampler.Input.GetFloatArray(FrameTimes);

				if (FrameTimes.Num() > 1)
				{
					const float StartTime = FrameTimes[0];
					const float EndTime = FrameTimes.Last();

					if (EndTime > StartTime)
					{
						FramesPerSec = FMath::Max(FramesPerSec, static_cast<float>(FrameTimes.Num() - 1) / (EndTime - StartTime));
					}
				}
			}

			if (Channel.Target.Path != GLTF::FAnimation::EPath::Weights)
			{
				TArray<GLTF::FAnimation::FChannel>& NodeChannels = NodeChannelMap.FindOrAdd(&Channel.Target.Node);
				NodeChannels.Add(Channel);
			}
			else
				LogMessages.Emplace(GLTF::EMessageSeverity::Error, TEXT("Morph animations aren't supported: ") + Animation.Name);
		}

		if (bAnimationFPSFromFile)
		{
			SequenceElement->SetFrameRate(FramesPerSec);
		}
	
		FFrameRate FrameRate = FFrameRate(FMath::RoundToInt(SequenceElement->GetFrameRate()), 1);
		for (const auto& NodeChannelPair : NodeChannelMap)
		{
			const GLTF::FNode*                        Node     = NodeChannelPair.Get<0>();
			const TArray<GLTF::FAnimation::FChannel>& Channels = NodeChannelPair.Get<1>();

			TSharedRef<IDatasmithTransformAnimationElement> AnimationElement = FDatasmithSceneFactory::CreateTransformAnimation(*Node->Name);

			ResampleAnimationFrames(Animation, Channels, FrameRate, *AnimationElement);
			SequenceElement->AddAnimation(AnimationElement);
		}

		ImportedSequences.Add(SequenceElement);
		CurrentScene->AddLevelSequence(SequenceElement);
	}
}

uint32 FDatasmithGLTFAnimationImporter::ResampleAnimationFrames(const GLTF::FAnimation& Animation,
	const TArray<GLTF::FAnimation::FChannel>& Channels,
	FFrameRate FrameRate,
	IDatasmithTransformAnimationElement& AnimationElement)
{
	using namespace DatasmithGLTFImporterImpl;

	static_assert((int)GLTF::FAnimation::EPath::Translation == 0, "INVALID_VALUE");
	static_assert((int)GLTF::FAnimation::EPath::Rotation == 1, "INVALID_VALUE");
	static_assert((int)GLTF::FAnimation::EPath::Scale == 2, "INVALID_VALUE");
	static_assert((int)GLTF::FAnimation::EInterpolation::Linear == (int)EDatasmithCurveInterpMode::Linear, "INVALID_ENUM_VALUE");
	static_assert((int)GLTF::FAnimation::EInterpolation::Step == (int)EDatasmithCurveInterpMode::Constant, "INVALID_ENUM_VALUE");
	static_assert((int)GLTF::FAnimation::EInterpolation::CubicSpline == (int)EDatasmithCurveInterpMode::Cubic, "INVALID_ENUM_VALUE");

	uint32 FrameCount = 0;

	EDatasmithTransformChannels ActiveChannels = EDatasmithTransformChannels::None;

	FAnimationResampler Resampler(FrameRate, FrameTimeBuffer, AnimationElement);

	bool bProcessedPath[3] = { false, false, false };
	for (const GLTF::FAnimation::FChannel& Channel : Channels)
	{
		check(bProcessedPath[(int32)Channel.Target.Path] == false);

		const GLTF::FAnimation::FSampler& Sampler = Animation.Samplers[Channel.Sampler];
		Sampler.Input.GetFloatArray(FrameTimeBuffer);

		const EDatasmithTransformType TransformType = ConvertToTransformType(Channel.Target.Path);
		AnimationElement.SetCurveInterpMode(TransformType, (EDatasmithCurveInterpMode)Sampler.Interpolation);

		int32 Index = 0;
		switch (Channel.Target.Path)
		{
		case GLTF::FAnimation::EPath::Rotation:
		{
			FrameDataBuffer.SetNumUninitialized(Sampler.Output.Count * 4);
			// GTFL Accessor public api returns quaternions into FVector4 array
			Sampler.Output.GetQuatArray(reinterpret_cast<FVector4f*>(FrameDataBuffer.GetData()));

			Resampler.ResampleRotation(Sampler.Interpolation, reinterpret_cast<FQuat4f*>(FrameDataBuffer.GetData()));

			ActiveChannels = ActiveChannels | FDatasmithAnimationUtils::SetChannelTypeComponents(ETransformChannelComponents::All, TransformType);
			break;
		}
		case GLTF::FAnimation::EPath::Translation:
		{
			FrameDataBuffer.SetNumUninitialized(Sampler.Output.Count * 3);
			FVector3f* SourceData = reinterpret_cast<FVector3f*>(FrameDataBuffer.GetData());
			Sampler.Output.GetCoordArray(SourceData);

			Resampler.ResampleTranslation(Sampler.Interpolation, SourceData, ScaleFactor);

			ActiveChannels = ActiveChannels | FDatasmithAnimationUtils::SetChannelTypeComponents(ETransformChannelComponents::All, TransformType);
			break;
		}
		case GLTF::FAnimation::EPath::Scale:
		{
			FrameDataBuffer.SetNumUninitialized(Sampler.Output.Count * 3);
			FVector3f* SourceData = reinterpret_cast<FVector3f*>(FrameDataBuffer.GetData());
			Sampler.Output.GetCoordArray(SourceData);

			Resampler.ResampleScale(Sampler.Interpolation, SourceData);

			ActiveChannels = ActiveChannels | FDatasmithAnimationUtils::SetChannelTypeComponents(ETransformChannelComponents::All, TransformType);
			break;
		}
		default:
			ensure(false);
			break;
		}

		FrameCount = FMath::Max(Sampler.Input.Count, FrameCount);
		bProcessedPath[(int32)Channel.Target.Path] = true;
	}

	AnimationElement.SetEnabledTransformChannels(ActiveChannels);

	return FrameCount;
}
