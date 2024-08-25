// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/Effects/Vocoder.h"
#include "HarmonixDsp/Effects/FourBiquads.h"

#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMath.h"

#define FILTER_COEF_RAMP_MS   15.0f


namespace Harmonix::Dsp::Effects
{

void FVocoder::Init()
{
}

FVocoder::FVocoder()
{}

FVocoder::~FVocoder()
{
	DeleteFilters();
	DeleteBandFrequencies();
	DeleteCarrierFilterCoefficients();
	DeleteModulatorFilterCoefficients();
}

void FVocoder::Setup(int32 ChannelCount, float SampleRate, int32 FrameCount, EVocoderBandConfig BandConfig)
{
	TargetSettings.ChannelCount = ChannelCount;
	TargetSettings.SampleRate = SampleRate;
	TargetSettings.FrameCount = FrameCount;
	TargetSettings.BandConfig = BandConfig;
	ResolveSettings(true);
}

void FVocoder::Process(const TAudioBuffer<float>& InCarrier, const TAudioBuffer<float>& InModulator, TAudioBuffer<float>& OutBuffer)
{
	if (!CurrentSettings.IsEnabled)
	{
		OutBuffer.Copy(InCarrier);
		return;
	}

	// Validate channel counts of received buffers...
	checkSlow(CurrentSettings.ChannelCount == InModulator.GetNumValidChannels() &&
		CurrentSettings.ChannelCount == OutBuffer.GetNumValidChannels() &&
		CurrentSettings.ChannelCount == InCarrier.GetNumValidChannels());

	// Validate frame counts of received buffers...
	int32 FrameCount = InCarrier.GetLengthInFrames();
	checkSlow(FrameCount == InModulator.GetLengthInFrames() && FrameCount == OutBuffer.GetLengthInFrames());

	WorkingCarrier.Copy(InCarrier, CurrentSettings.CarrierGain, -1.0f, 1.0f);
	WorkingModulator.Copy(InModulator, CurrentSettings.ModulatorGain, -1.0f, 1.0f);
	OutBuffer.ZeroData();

	ProcessTimeDomain(FrameCount);

	if (OutBuffer.GetIsInterleaved())
	{
		OutBuffer.Interleave(WorkingOutput);
	}
	else
	{
		OutBuffer.Copy(WorkingOutput);
	}
}

void FVocoder::ProcessTimeDomain(int32 FrameCount)
{
	ResolveSettings();

	const int32& BandCount = CurrentSettings.GetBandConfig().BandCount;
	float GeneralMultiplier = CurrentSettings.Wet * CurrentSettings.OutputGain;
#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
	//In-loop variables (declared here to avoid stack allocations in loop)
	VectorRegister4Float GeneralMultiplierVec = VectorSetFloat1(GeneralMultiplier);
	VectorRegister4Float AttackVec = VectorSetFloat1(CurrentSettings.Attack);
	VectorRegister4Float ReleaseVec = VectorSetFloat1(CurrentSettings.Release);
	float FrameOutput;
	for (int32 BandIdx = 0; BandIdx < BandCount; BandIdx++)
	{
		FVocoderBand Band = CurrentSettings.Bands[BandIdx];
		WorkingBandGains[BandIdx] = Band.Gain * ((!CurrentSettings.Soloing || Band.Solo) ? 1.0f : 0.0f);
	}

	for (int32 ChannelIdx = 0; ChannelIdx < CurrentSettings.ChannelCount; ChannelIdx++)
	{
		float* CarrierChannelData = WorkingCarrier.GetValidChannelData(ChannelIdx);
		float* ModulatorChannelData = WorkingModulator.GetValidChannelData(ChannelIdx);
		float* OutputChannelData = WorkingOutput.GetValidChannelData(ChannelIdx);
		LowReductionFilters[ChannelIdx].Process(CarrierChannelData, CarrierChannelData, FrameCount, LowReductionFilterCoefs);
		HighIncreaseFilters[ChannelIdx].Process(CarrierChannelData, CarrierChannelData, FrameCount, HighIncreaseFilterCoefs);

		for (int32 FrameIdx = 0; FrameIdx < FrameCount; FrameIdx++)
		{
			FrameOutput = 0.0f;
			for (int32 BandIdx = 0; BandIdx < BandCount; BandIdx += 4)
			{
				VectorRegister4Float ModulatorFilteredVec = ModulatorFilters[BandIdx / 4][ChannelIdx].ProcessOne(VectorSetFloat1(ModulatorChannelData[FrameIdx]));

				VectorRegister4Float EnvelopeVec = VectorMax(VectorMultiply(ModulatorFilteredVec, VectorSetFloat1(-1.0f)), ModulatorFilteredVec);
				VectorRegister4Float PrevEnvelopeVec = VectorLoadAligned(&EnvelopeSamples[ChannelIdx][BandIdx]);
				VectorRegister4Float AlphaVec = VectorSelect(VectorCompareLT(EnvelopeVec, PrevEnvelopeVec), ReleaseVec, AttackVec);
				EnvelopeVec = VectorAdd(VectorMultiply(PrevEnvelopeVec, VectorSubtract(GlobalVectorConstants::FloatOne, AlphaVec)), VectorMultiply(EnvelopeVec, AlphaVec));
				VectorStoreAligned(EnvelopeVec, &EnvelopeSamples[ChannelIdx][BandIdx]);

				VectorRegister4Float CarrierFilteredVec = CarrierFilters[BandIdx / 4][ChannelIdx].ProcessOne(VectorSetFloat1(CarrierChannelData[FrameIdx]));
				union { VectorRegister4Float OutputVec; float OutBuffer[4]; };
				OutputVec = VectorMultiply(CarrierFilteredVec, EnvelopeVec);
				OutputVec = VectorMultiply(OutputVec, GeneralMultiplierVec);
				OutputVec = VectorMultiply(OutputVec, VectorLoadAligned(&WorkingBandGains[BandIdx]));

				for (int32 LaneIdx = 0; LaneIdx < 4; LaneIdx++)
				{
					FrameOutput += OutBuffer[LaneIdx];
				}
			}
			OutputChannelData[FrameIdx] = FrameOutput;
		}
	}
#else
	float** CarrierData = WorkingCarrier.GetData();

	for (int32 ChannelIdx = 0; ChannelIdx < CurrentSettings.ChannelCount; ChannelIdx++)
	{
		LowReductionFilters[ChannelIdx].Process(CarrierData[ChannelIdx], CarrierData[ChannelIdx], FrameCount, LowReductionFilterCoefs);
		HighIncreaseFilters[ChannelIdx].Process(CarrierData[ChannelIdx], CarrierData[ChannelIdx], FrameCount, HighIncreaseFilterCoefs);
	}

	for (int32 BandIdx = 0; BandIdx < CurrentSettings.GetBandConfig().BandCount; BandIdx++)
	{
		//if(CurrentSettings.SingleBand && Band != CurrentSettings.FocusedBand)
		if (CurrentSettings.Soloing && !CurrentSettings.Bands[BandIdx].Solo)
		{
			continue;
		}

		// Prepare a work buffer for this single Band of the modulator...
		BandWork.Copy(WorkingModulator);

		for (int32 ChannelIdx = 0; ChannelIdx < CurrentSettings.ChannelCount; ChannelIdx++)
		{
			float* BandWorkChannelData = BandWork.GetValidChannelData(ChannelIdx);
			ModulatorFilters[BandIdx][ChannelIdx].Process(BandWorkChannelData, BandWorkChannelData, FrameCount, ModulatorFilterCoefs[Band]);

			// Absolute value is helpful for envelope detection...
			union 
			{ 
				VectorRegister4Float AbsWorkVec; 
				float AbsWork[4]; 
			};
			for (int32 FrameIdx = 0; FrameIdx < FrameCount; FrameIdx += 4)
			{
				AbsWorkVec = MakeVectorRegisterFloat(BandWorkChannelData[FrameIdx], BandWorkChannelData[FrameIdx + 1], BandWorkChannelData[FrameIdx + 2], BandWorkChannelData[FrameIdx + 3]);
				AbsWorkVec = VectorMax(VectorMultiply(AbsWorkVec, VectorSetFloat1(-1.0f)), AbsWorkVec);
				for (int32 LaneIdx = 0; LaneIdx < 4; LaneIdx++)
				{
					BandWorkChannelData[FrameIdx + LaneIdx] = AbsWork[LaneIdx];
				}
			}

			float& EnvelopeSample = EnvelopeSamples[BandIdx][ChannelIdx];
			for (int32 FrameIdx = 0; FrameIdx < FrameCount; FrameIdx++)
			{
				// Choose our alpha, then update our envelope follower for this Band...
				float Alpha = (BandWorkChannelData[FrameIdx] >= EnvelopeSample) ? CurrentSettings.Attack : CurrentSettings.Release;
				EnvelopeSample = (EnvelopeSample * (1.0f - Alpha)) + (BandWorkChannelData[FrameIdx] * Alpha);
				// Now that BandWorkChannelData[FrameIdx] is done being used for envelope detection, we will dump output into it...
				CarrierFilters[BandIdx][ChannelIdx].Process(&CarrierData[ChannelIdx][FrameIdx], &BandWorkChannelData[FrameIdx], 1, CarrierFilterCoefs[BandIdx], GeneralMultiplier * EnvelopeSample * CurrentSettings.Bands[BandIdx].Gain);
			}
		}
		//This Band is done, so we'll accumulate it into our output...
		WorkingOutput.Accumulate(BandWork);
	}
#endif

	// If we want some of the original carrier mixed in, scale it appropriately
	// and accumulate it into our output
	if (CurrentSettings.Wet != 1.0f)
	{
		WorkingCarrier.Scale(1.0f - CurrentSettings.Wet);
		WorkingOutput.Accumulate(WorkingCarrier);
	}
}

void FVocoder::ResolveSettings(bool InForceAll)
{
	// Make sure no one else can mess with settings while we rebuild things...
	FScopeLock Lock(&SettingsLock);

	// Rebuild only what components are necessary based on what has changed...

	bool ChannelCountChanged = TargetSettings.ChannelCount != CurrentSettings.ChannelCount;
	bool SampleRateChanged = TargetSettings.SampleRate != CurrentSettings.SampleRate;
	bool BandConfigChanged = TargetSettings.BandConfig != CurrentSettings.BandConfig;
	bool FrameCountChanged = TargetSettings.FrameCount != CurrentSettings.FrameCount;

	if (ChannelCountChanged || SampleRateChanged || FrameCountChanged || InForceAll)
	{
		InitScratchBuffers();
	}

	if (ChannelCountChanged || BandConfigChanged || InForceAll)
	{
		DeleteFilters();
		CreateFilters();
	}

	if (BandConfigChanged || InForceAll)
	{
		CreateBandFrequencies();
	}

	if (BandConfigChanged || SampleRateChanged || TargetSettings.CarrierThin != CurrentSettings.CarrierThin || InForceAll)
	{
		DeleteCarrierFilterCoefficients();
		CreateCarrierFilterCoefficients();
	}

	if (BandConfigChanged || SampleRateChanged || TargetSettings.ModulatorThin != CurrentSettings.ModulatorThin || InForceAll)
	{
		DeleteModulatorFilterCoefficients();
		CreateModulatorFilterCoefficients();
	}

#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
	if (BandConfigChanged || SampleRateChanged || TargetSettings.CarrierThin != CurrentSettings.CarrierThin || ChannelCountChanged || InForceAll)
	{
		SetCarrierSets();
	}
	if (BandConfigChanged || SampleRateChanged || TargetSettings.ModulatorThin != CurrentSettings.ModulatorThin || ChannelCountChanged || InForceAll)
	{
		SetModulatorSets();
	}
#endif

	if (BandConfigChanged || SampleRateChanged || TargetSettings.HighEmphasis != CurrentSettings.HighEmphasis || InForceAll)
	{
		InitEmphasisFilterCoefficients();
	}

	CurrentSettings = TargetSettings;
}

void FVocoder::InitScratchBuffers()
{
	WorkingCarrier.Configure(TargetSettings.ChannelCount, TargetSettings.FrameCount, EAudioBufferCleanupMode::Delete, TargetSettings.SampleRate, false);
	WorkingCarrier.ZeroData();
	WorkingModulator.Configure(TargetSettings.ChannelCount, TargetSettings.FrameCount, EAudioBufferCleanupMode::Delete, TargetSettings.SampleRate, false);
	WorkingModulator.ZeroData();
	BandWork.Configure(TargetSettings.ChannelCount, TargetSettings.FrameCount, EAudioBufferCleanupMode::Delete, TargetSettings.SampleRate, false);
	BandWork.ZeroData();
	WorkingOutput.Configure(TargetSettings.ChannelCount, TargetSettings.FrameCount, EAudioBufferCleanupMode::Delete, TargetSettings.SampleRate, false);
	WorkingOutput.ZeroData();
}

void FVocoder::CreateFilters()
{
	const int32& BandCount = TargetSettings.GetBandConfig().BandCount;

#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
	CarrierFilters = new FFourBiquads * [BandCount / 4];
	ModulatorFilters = new FFourBiquads * [BandCount / 4];
	EnvelopeSamples = new float* [TargetSettings.ChannelCount];

	for (int32 BandIdx = 0; BandIdx < BandCount / 4; BandIdx++)
	{
		CarrierFilters[BandIdx] = new FFourBiquads[TargetSettings.ChannelCount];
		ModulatorFilters[BandIdx] = new FFourBiquads[TargetSettings.ChannelCount];
	}
	for (int32 ChannelIdx = 0; ChannelIdx < TargetSettings.ChannelCount; ChannelIdx++)
	{
		EnvelopeSamples[ChannelIdx] = (float*)FMemory::Malloc(BandCount * sizeof(float), 16);
		FMemory::Memset(EnvelopeSamples[ChannelIdx], 0, sizeof(float) * BandCount);
	}
#else
	CarrierFilters = new FBiquadFilter * [BandCount];
	ModulatorFilters = new FBiquadFilter * [BandCount];
	EnvelopeSamples = new float* [BandCount];

	for (int32 Banddx = 0; BandIdx < BandCount; BandIdx++)
	{
		CarrierFilters[BandIdx] = new FBiquadFilter[TargetSettings.ChannelCount];
		ModulatorFilters[BandIdx] = new FBiquadFilter[TargetSettings.ChannelCount];
		EnvelopeSamples[BandIdx] = new float[TargetSettings.ChannelCount];
		FMemory::Memset(EnvelopeSamples[BandIdx], 0, sizeof(float) * TargetSettings.ChannelCount);
	}

#endif
	LowReductionFilters = new FBiquadFilter[TargetSettings.ChannelCount];
	HighIncreaseFilters = new FBiquadFilter[TargetSettings.ChannelCount];
}

void FVocoder::CreateBandFrequencies()
{
	const int32& BandCount = TargetSettings.GetBandConfig().BandCount;
	BandFrequencies = new float[BandCount];
	float Ratio = TargetSettings.GetBandConfig().FrequencyRatio;

	// Should yield approximately the same results without w0/sin(w0) and sinh
	float Q = 0.5f / (0.3465736f * (FMath::Log2(Ratio) * 0.5f)); 
	CarrierQ = Q * 3.0f;
	ModulatorQ = Q * 1.5f;

	for (int32 Band = 0; Band < BandCount; Band++)
	{
		if (Band == 0)
		{
			BandFrequencies[Band] = 80.0f;
		}
		else
		{
			BandFrequencies[Band] = BandFrequencies[Band - 1] * Ratio;
		}
	}

#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
	WorkingBandGains = (float*)FMemory::Malloc(BandCount * sizeof(float), 16);
#endif
}

void FVocoder::CreateCarrierFilterCoefficients()
{
	const int32& BandCount = TargetSettings.GetBandConfig().BandCount;

	CarrierFilterCoefs = new FBiquadFilterCoefs[BandCount];

	FBiquadFilterSettings NewSettings;
	NewSettings.IsEnabled = true;
	NewSettings.Type = EBiquadFilterType::BandPass;
	NewSettings.Q = CarrierQ * TargetSettings.CarrierThin;
	for (int32 BandIdx = 0; BandIdx < BandCount; BandIdx++)
	{
		NewSettings.Freq = BandFrequencies[BandIdx];
		CarrierFilterCoefs[BandIdx] = FBiquadFilterCoefs(NewSettings, TargetSettings.SampleRate);
	}
}

void FVocoder::CreateModulatorFilterCoefficients()
{
	const int32& BandCount = TargetSettings.GetBandConfig().BandCount;

	ModulatorFilterCoefs = new FBiquadFilterCoefs[BandCount];

	FBiquadFilterSettings NewSettings;
	NewSettings.IsEnabled = true;
	NewSettings.Type = EBiquadFilterType::BandPass;
	NewSettings.Q = ModulatorQ * TargetSettings.ModulatorThin;
	for (int32 BandIdx = 0; BandIdx < BandCount; BandIdx++)
	{
		NewSettings.Freq = BandFrequencies[BandIdx];
		ModulatorFilterCoefs[BandIdx] = FBiquadFilterCoefs(NewSettings, TargetSettings.SampleRate);
	}
}

#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG

void FVocoder::SetCarrierSets()
{
	for (int32 BandIdx = 0; BandIdx < TargetSettings.GetBandConfig().BandCount; BandIdx++)
	{
		for (int32 ChannelIdx = 0; ChannelIdx < TargetSettings.ChannelCount; ChannelIdx++)
		{
			CarrierFilters[BandIdx / 4][ChannelIdx].SetFilter(BandIdx % 4,
				(float)CarrierFilterCoefs[BandIdx].B0, (float)CarrierFilterCoefs[BandIdx].B1, (float)CarrierFilterCoefs[BandIdx].B2,
				(float)CarrierFilterCoefs[BandIdx].A1, (float)CarrierFilterCoefs[BandIdx].A2);
		}
	}
}

void FVocoder::SetModulatorSets()
{
	for (int32 BandIdx = 0; BandIdx < TargetSettings.GetBandConfig().BandCount; BandIdx++)
	{
		for (int32 ChannelIdx = 0; ChannelIdx < TargetSettings.ChannelCount; ChannelIdx++)
		{
			ModulatorFilters[BandIdx / 4][ChannelIdx].SetFilter(BandIdx % 4,
				(float)ModulatorFilterCoefs[BandIdx].B0, (float)ModulatorFilterCoefs[BandIdx].B1, (float)ModulatorFilterCoefs[BandIdx].B2,
				(float)ModulatorFilterCoefs[BandIdx].A1, (float)ModulatorFilterCoefs[BandIdx].A2);
		}
	}
}

#endif

void FVocoder::InitEmphasisFilterCoefficients()
{
	FBiquadFilterSettings FilterSettings;
	FilterSettings.IsEnabled = true;
	FilterSettings.Type = EBiquadFilterType::HighShelf;
	FilterSettings.Freq = 2000.0f;
	FilterSettings.Q = 1.0f;
	FilterSettings.DesignedDBGain = TargetSettings.HighEmphasis * 18.0f;
	HighIncreaseFilterCoefs = FBiquadFilterCoefs(FilterSettings, TargetSettings.SampleRate);

	FilterSettings.Type = EBiquadFilterType::LowShelf;
	FilterSettings.Freq = 1000.0f;
	FilterSettings.Q = 1.0f;
	FilterSettings.DesignedDBGain = TargetSettings.HighEmphasis * -6.0f;
	LowReductionFilterCoefs = FBiquadFilterCoefs(FilterSettings, TargetSettings.SampleRate);
}

void FVocoder::DeleteFilters()
{
#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
	if (CarrierFilters)
	{
		for (int32 BandIdx = 0; BandIdx < GetBandCount() / 4; BandIdx++)
		{
			delete[] CarrierFilters[BandIdx];
		}
		delete[] CarrierFilters;
		CarrierFilters = nullptr;
	}

	if (ModulatorFilters)
	{
		for (int32 BandIdx = 0; BandIdx < GetBandCount() / 4; BandIdx++)
		{
			delete[] ModulatorFilters[BandIdx];
		}
		delete[] ModulatorFilters;
		ModulatorFilters = nullptr;
	}

	if (EnvelopeSamples)
	{
		for (int32 ChannelIdx = 0; ChannelIdx < CurrentSettings.ChannelCount; ChannelIdx++)
		{
			FMemory::Free(EnvelopeSamples[ChannelIdx]);
		}
		delete[] EnvelopeSamples;
		EnvelopeSamples = nullptr;
	}
#else
	if (CarrierFilters)
	{
		for (int32 Band = 0; Band < GetBandCount(); Band++)
		{
			delete[] CarrierFilters[Band];
		}
		delete[] CarrierFilters;
		CarrierFilters = nullptr;
	}

	if (ModulatorFilters)
	{
		for (int32 Band = 0; Band < GetBandCount(); Band++)
		{
			delete[] ModulatorFilters[Band];
		}
		delete[] ModulatorFilters;
		ModulatorFilters = nullptr;
	}

	if (EnvelopeSamples)
	{
		for (int32 Band = 0; Band < GetBandCount(); Band++)
		{
			delete[] EnvelopeSamples[Band];
		}
		delete[] EnvelopeSamples;
		EnvelopeSamples = nullptr;
	}
#endif

	if (LowReductionFilters)
	{
		delete[] LowReductionFilters;
		LowReductionFilters = nullptr;
	}
	if (HighIncreaseFilters)
	{
		delete[] HighIncreaseFilters;
		HighIncreaseFilters = nullptr;
	}
}

void FVocoder::DeleteBandFrequencies()
{
	if (BandFrequencies)
	{
		delete[] BandFrequencies;
		BandFrequencies = nullptr;
	}

#if !UE_BUILD_DEBUG || SIMD_IN_DEBUG
	FMemory::Free(WorkingBandGains);
	WorkingBandGains = nullptr;
#endif
}

void FVocoder::DeleteCarrierFilterCoefficients()
{
	if (CarrierFilterCoefs)
	{
		delete[] CarrierFilterCoefs;
		CarrierFilterCoefs = nullptr;
	}
}

void FVocoder::DeleteModulatorFilterCoefficients()
{
	if (ModulatorFilterCoefs)
	{
		delete[] ModulatorFilterCoefs;
		ModulatorFilterCoefs = nullptr;
	}
}

};