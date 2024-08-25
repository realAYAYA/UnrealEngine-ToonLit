// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HarmonixDsp/Effects/Settings/VocoderSettings.h"
#include "HarmonixDsp/Effects/BiquadFilter.h"
#include "HarmonixDsp/AudioBuffer.h"
#include "HAL/CriticalSection.h"

namespace Harmonix::Dsp::Effects
{

	class FFourBiquads;

	class FVocoder
	{
	public:

		FVocoder();
		~FVocoder();

		static void Init();

		void Setup(int32 ChannelCount, float SampleRate, int32 FrameCount, EVocoderBandConfig VocoderBandConfig);
		void Setup(int32 InChannelCount, float InSampleRate, int32 InFrameCount)
		{
			Setup(InChannelCount, InSampleRate, InFrameCount, TargetSettings.BandConfig);
		}

		void Process(const TAudioBuffer<float>& InCarrier, const TAudioBuffer<float>& InModulator, TAudioBuffer<float>& OutBuffer);
		void Process(TAudioBuffer<float>& InOutCarrier, const TAudioBuffer<float>& InModulator) { Process(InOutCarrier, InModulator, InOutCarrier); }

		// Mutators
		void ImportSettings(const FVocoderSettings& InSettings) { TargetSettings = InSettings; }
		void SetChannelCount(int32 ChannelCount) { TargetSettings.ChannelCount = ChannelCount; }
		void SetBandConfig(EVocoderBandConfig BandConfig) { TargetSettings.BandConfig = BandConfig; }
		void SetSoloing(bool Soling) { TargetSettings.Soloing = Soling; }
		void SetCarrierGain(float Gain) { TargetSettings.CarrierGain = Gain; }
		void SetModulatorGain(float Gain) { TargetSettings.ModulatorGain = Gain; }
		void SetCarrierThin(float CarrierThin) { TargetSettings.CarrierThin = CarrierThin; }
		void SetModulatorThin(float ModulatorThin) { TargetSettings.ModulatorThin = ModulatorThin; }
		void SetAttack(float Attack) { TargetSettings.Attack = Attack; }
		void SetRelease(float Release) { TargetSettings.Release = Release; }
		void SetHighEmphasis(float HighEmphasis) { TargetSettings.HighEmphasis = HighEmphasis; }
		void SetWet(float Wet) { TargetSettings.Wet = Wet; }
		void SetOutputGain(float Gain) { TargetSettings.OutputGain = Gain; }
		void SetPerBandSettings(TArray<FVocoderBand> Bands) { TargetSettings.Bands = Bands; }

		// Accessors
		int32 GetChannelCount() const { return CurrentSettings.ChannelCount; }
		EVocoderBandConfig GetBandConfigIndex() const { return CurrentSettings.BandConfig; }
		int32 GetBandCount() const { return CurrentSettings.GetBandConfig().BandCount; }
		float GetCarrierThin() const { return CurrentSettings.CarrierThin; }
		float GetModulatorThin() const { return CurrentSettings.ModulatorThin; }
		float GetAttack() const { return CurrentSettings.Attack; }
		float GetRelease() const { return CurrentSettings.Release; }
		float GetHighEmphasis() const { return CurrentSettings.HighEmphasis; }
		float GetCarrierGain() const { return CurrentSettings.CarrierGain; }
		float GetModulatorGain() const { return CurrentSettings.ModulatorGain; }
		float GetWet() const { return CurrentSettings.Wet; }
		float GetOutputGain() const { return CurrentSettings.OutputGain; }


	private:
		// Internal methods
		void ProcessTimeDomain(int32 FrameCount);
		void ResolveSettings(bool bForceAll = false);
		void InitScratchBuffers();
		void CreateFilters();
		void CreateBandFrequencies();
		void CreateCarrierFilterCoefficients();
		void CreateModulatorFilterCoefficients();
#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
		void SetCarrierSets();
		void SetModulatorSets();
#endif
		void InitEmphasisFilterCoefficients();
		void DeleteFilters();
		void DeleteBandFrequencies();
		void DeleteCarrierFilterCoefficients();
		void DeleteModulatorFilterCoefficients();

		TAudioBuffer<float> WorkingCarrier;
		TAudioBuffer<float> WorkingModulator;
		TAudioBuffer<float> BandWork;
		TAudioBuffer<float> WorkingOutput;
		float* WorkingBandGains = nullptr;

		float				CarrierQ = 0.0f;
		float				ModulatorQ = 0.0f;
		float* BandFrequencies = nullptr;
#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
		FFourBiquads** CarrierFilters = nullptr;
		FFourBiquads** ModulatorFilters = nullptr;
#else
		FBiquadFilter** CarrierFilters = nullptr;
		FBiquadFilter** ModulatorFilters = nullptr;
#endif
		FBiquadFilterCoefs* CarrierFilterCoefs = nullptr;
		FBiquadFilterCoefs* ModulatorFilterCoefs = nullptr;
		FBiquadFilter* HighIncreaseFilters = nullptr;
		FBiquadFilterCoefs  HighIncreaseFilterCoefs;
		FBiquadFilter* LowReductionFilters = nullptr;
		FBiquadFilterCoefs  LowReductionFilterCoefs;
		float** EnvelopeSamples = nullptr;

		FCriticalSection	SettingsLock;
		FVocoderSettings	CurrentSettings;
		FVocoderSettings	TargetSettings;
	};

};