// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"

#include "HarmonixDsp/Streaming/TrackChannelInfo.h"

class IStretcherAndPitchShifter;
struct FTrackChannelInfo;
template<typename Type> 
class TAudioBuffer;
class FGainMatrix;
class FFusionSampler;
class FSoundWaveProxy;

class IAudioDataRenderer : public TSharedFromThis<IAudioDataRenderer>
{
public:

	/** Settings for a AudioDataRenderer. */
	struct FSettings
	{
		TSharedPtr<IStretcherAndPitchShifter, ESPMode::ThreadSafe> Shifter = nullptr;
		const TArray<FTrackChannelInfo>* TrackChannelInfo = nullptr;
		const FFusionSampler* Sampler = nullptr;
	};

	virtual ~IAudioDataRenderer() {};

	virtual void SetAudioData(TSharedRef<FSoundWaveProxy> SoundWaveProxy, const FSettings& InSettings) = 0;

	virtual void Reset() = 0;
	
	virtual const TSharedPtr<FSoundWaveProxy> GetAudioData() const = 0;

	virtual void MigrateToSampler(const FFusionSampler* InSampler) = 0;

	virtual void SetFrame(uint32 InFrameNum) = 0;

	virtual double Render(TAudioBuffer<float>& OutBuffer, double InPos, int32 InMaxFrame, double InResampleInc, double InPitchShift, double InSpeed,
		bool MaintainPitchWhenSpeedChanges, bool InShouldHonorLoopPoints, const FGainMatrix& InGain) = 0;

	virtual double RenderUnshifted(TAudioBuffer<float>& OutBuffer, double InPos, int32 InMaxFrame, double InInc,
		bool InShouldHonorLoopPoints, const FGainMatrix& InGain) = 0;

protected:

	struct FLerpData
	{
		uint32 PosA;
		uint32 PosB;

		// pos a and b Relative to start position, and never loop
		uint32 PosARelative;
		uint32 PosBRelative;

		float WeightA;
		float WeightB;
	};

	double CalculateLerpData(FLerpData* LerpArray, int32 InNumPoints, uint32 InNumOutFrames, double InPos, int32 InMaxFrame, bool InHonorLoopPoints, double Inc) const;
};