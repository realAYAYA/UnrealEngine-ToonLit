// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceAudioSpectrum.h"
#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraShader.h"
#include "NiagaraShaderParametersBuilder.h"
#include "ShaderParameterUtils.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"

#include "DSP/DeinterleaveView.h"
#include "DSP/SlidingWindow.h"
#include "DSP/ConstantQ.h"
#include "DSP/FFTAlgorithm.h"
#include "DSP/FloatArrayMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceAudioSpectrum)

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceGridAudioSpectrum"

FNiagaraDataInterfaceProxySpectrum::FNiagaraDataInterfaceProxySpectrum(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands, float InNoiseFloorDb, int32 InNumSamplesToBuffer)

	: FNiagaraDataInterfaceProxySubmix(InNumSamplesToBuffer)
	, MinimumFrequency(0.f)
	, MaximumFrequency(0.f)
	, NumBands(0)
	, NoiseFloorDb(0.f)
	, NumChannels(0)
	, SampleRate(0.f)
	, FFTScale(1.f)
{
	int32 InitialNumChannels = 2;
	float InitialSampleRate = 48000.f;

	SetAudioFormat(InitialNumChannels, InitialSampleRate);

	ResizeCQT(InMinimumFrequency, InMaximumFrequency, InNumBands);

	SetNoiseFloor(InNoiseFloorDb);
}

FNiagaraDataInterfaceProxySpectrum::~FNiagaraDataInterfaceProxySpectrum()
{
	check(IsInRenderingThread());
	GPUBuffer.Release();
}

float FNiagaraDataInterfaceProxySpectrum::GetSpectrumValue(float InNormalizedPositionInSpectrum, int32 InChannelIndex)
{
	check(InChannelIndex >= 0);

	// Check if channel index is within channel spectrum buffers
	if (InChannelIndex >= ChannelSpectrumBuffers.Num())
	{
		return 0.f;
	}

	const Audio::FAlignedFloatBuffer& Buffer = ChannelSpectrumBuffers[InChannelIndex];
	const int32 MaxIndex = Buffer.Num() - 1;

	if (MaxIndex < 0)
	{
		return 0.0f;
	}

	InNormalizedPositionInSpectrum = FMath::Clamp(InNormalizedPositionInSpectrum, 0.0f, 1.0f);

	float Index = InNormalizedPositionInSpectrum * Buffer.Num();

	int32 LowerIndex = FMath::FloorToInt(Index);
	LowerIndex = FMath::Clamp(LowerIndex, 0, MaxIndex);

	int32 UpperIndex = FMath::CeilToInt(Index);
	UpperIndex = FMath::Clamp(UpperIndex, 0, MaxIndex);

	float Alpha = Index - LowerIndex;

	return FMath::Lerp<float>(Buffer[LowerIndex], Buffer[UpperIndex], Alpha);
}

int32 FNiagaraDataInterfaceProxySpectrum::GetNumBands() const
{
	check(IsInRenderingThread());
	return NumBands;
}


void FNiagaraDataInterfaceProxySpectrum::UpdateCQT(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands)
{
	ENQUEUE_RENDER_COMMAND(FUpdateSpectrumBuffer)(
		[&, InMinimumFrequency, InMaximumFrequency, InNumBands](FRHICommandListImmediate& RHICmdList)
	{
		ResizeCQT(InMinimumFrequency, InMaximumFrequency, InNumBands);
	});
}

void FNiagaraDataInterfaceProxySpectrum::ResizeCQT(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands)
{
	Clamp(InMinimumFrequency, InMaximumFrequency, InNumBands);

	// Check which values were changed
	bool bUpdatedMinimumFrequency = InMinimumFrequency != MinimumFrequency;
	bool bUpdatedMaximumFrequency = InMaximumFrequency != MaximumFrequency;
	bool bUpdatedNumBands = InNumBands != NumBands;

	bool bUpdatedValues = bUpdatedMinimumFrequency || bUpdatedMaximumFrequency || bUpdatedNumBands;

	if (bUpdatedNumBands)
	{
		ResizeSpectrumBuffer(NumChannels, InNumBands);
	}

	if (bUpdatedValues)
	{
		int32 OriginalFFTSize = FFTAlgorithm.IsValid() ? FFTAlgorithm->Size() : 0;

		ResizeAudioTransform(InMinimumFrequency, InMaximumFrequency, SampleRate, InNumBands);

		int32 NewFFTSize = FFTAlgorithm.IsValid() ? FFTAlgorithm->Size() : 0;

		if (NewFFTSize != OriginalFFTSize)
		{
			ResizeWindow(NumChannels, NewFFTSize);
		}

	}

	// Copy values internally
	MinimumFrequency = InMinimumFrequency;
	MaximumFrequency = InMaximumFrequency;
	NumBands = InNumBands;
}

void FNiagaraDataInterfaceProxySpectrum::UpdateNoiseFloor(float InNoiseFloorDb)
{
	ENQUEUE_RENDER_COMMAND(FUpdateSpectrumBuffer)(
		[&, InNoiseFloorDb](FRHICommandListImmediate& RHICmdList)
	{
		SetNoiseFloor(InNoiseFloorDb);
	});
}


void FNiagaraDataInterfaceProxySpectrum::SetNoiseFloor(float InNoiseFloorDb)
{
	NoiseFloorDb = FMath::Clamp(InNoiseFloorDb, -200.f, 10.f);
}

FReadBuffer& FNiagaraDataInterfaceProxySpectrum::ComputeAndPostSRV()
{
	PostDataToGPU();
	return GPUBuffer;
}


void FNiagaraDataInterfaceProxySpectrum::PostDataToGPU()
{
	ENQUEUE_RENDER_COMMAND(FUpdateSpectrumBuffer)(
		[&](FRHICommandListImmediate& RHICmdList)
	{
		UpdateSpectrum();

		// Resize GPU data if needed
		const int32 NumSamplesInBuffer = NumBands * NumChannels;
		const int32 NumBytesInChannelBuffer = NumBands * sizeof(float);
		const int32 NumBytesInBuffer = NumBytesInChannelBuffer * NumChannels;
		
		if (NumBytesInBuffer != GPUBuffer.NumBytes)
		{
			if (GPUBuffer.NumBytes > 0)
			{
				GPUBuffer.Release();
			}

			if (NumSamplesInBuffer > 0)
			{
				GPUBuffer.Initialize(TEXT("FNiagaraDataInterfaceProxySpectrum_GPUBuffer"), sizeof(float), NumSamplesInBuffer, EPixelFormat::PF_R32_FLOAT, BUF_Static);
			}
		}

		// Copy to GPU data
		if (GPUBuffer.NumBytes > 0)
		{
			float *BufferData = static_cast<float*>(RHILockBuffer(GPUBuffer.Buffer, 0, NumBytesInBuffer, EResourceLockMode::RLM_WriteOnly));

			FScopeLock ScopeLock(&BufferLock);

			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
			{
				
				int32 Pos = ChannelIndex * NumBands;
				FPlatformMemory::Memcpy(&BufferData[Pos], ChannelSpectrumBuffers[ChannelIndex].GetData(), NumBytesInChannelBuffer);
			}

			RHIUnlockBuffer(GPUBuffer.Buffer);
		}
	});
}

void FNiagaraDataInterfaceProxySpectrum::UpdateSpectrum()
{
	FScopeLock ScopeLock(&BufferLock);

	int32 SourceNumChannels = GetNumChannels();
	float SourceSampleRate = GetSampleRate();

	// Perform internal update if new samplerate or channel count encountered.
	if ((SourceNumChannels != NumChannels) || (SourceSampleRate != SampleRate))
	{
		// This will update values of NumChannels and SampleRate member variables.
		SetAudioFormat(SourceNumChannels, SourceSampleRate);
	}

	// Set data to zero if we are skipping update due to bad samplerate or channel count.
	if (NumChannels == 0 || FMath::IsNearlyZero(SampleRate))
	{
		for (int32 ChannelIndex = 0; ChannelIndex < ChannelSpectrumBuffers.Num(); ChannelIndex++)
		{
			Audio::FAlignedFloatBuffer& Buffer = ChannelSpectrumBuffers[ChannelIndex];
			if (Buffer.Num() > 0)
			{
				FMemory::Memset(Buffer.GetData(), 0, sizeof(float) * Buffer.Num());
			}
		}
		return;
	}

	
	// Grab audio from buffer.
	int32 NumSamplesToPop = GetNumSamplesAvailable();
	NumSamplesToPop -= (NumSamplesToPop % NumChannels);
	PopBuffer.Reset();
	
	if (NumSamplesToPop > 0)
	{
		
		PopBuffer.AddUninitialized(NumSamplesToPop);

		bool bUseLatestAudio = false;
		int32 NumPopped = PopAudio(PopBuffer.GetData(), NumSamplesToPop, bUseLatestAudio);
	}
	
	// If we're in a bad internal state, give up here.	
	if (!FFTAlgorithm.IsValid() || !CQTKernel.IsValid())
	{
		return;
	}

	// Run sliding window over available audio.
	FSlidingWindow SlidingWindow(*SlidingBuffer, PopBuffer, InterleavedBuffer);

	int32 NumWindows = 0;
	for (Audio::FAlignedFloatBuffer& InterleavedWindow : SlidingWindow)
	{
		if (0 == NumWindows)
		{
			// Zero output on first window.  We need to wait for at least one window to process
			// because we do not want to zero out data if no windows are available.
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
			{
				FMemory::Memset(ChannelSpectrumBuffers[ChannelIndex].GetData(), 0, sizeof(float) * NumBands);
			}
		}

		NumWindows++;

		// Run deinterleave view over window.
		FDeinterleaveView DeinterleaveView(InterleavedWindow, DeinterleavedBuffer, NumChannels);
		for(FChannel Channel : DeinterleaveView)
		{
			Audio::ArrayMultiplyInPlace(WindowBuffer, Channel.Values);

			// Perform FFT
			FMemory::Memcpy(FFTInputBuffer.GetData(), Channel.Values.GetData(), sizeof(float) * Channel.Values.Num());

			FFTAlgorithm->ForwardRealToComplex(FFTInputBuffer.GetData(), FFTOutputBuffer.GetData());

			// Transflate FFTOutput to power spectrum
			Audio::ArrayComplexToPower(FFTOutputBuffer, PowerSpectrumBuffer);

			// Take CQT of power spectrum
			CQTKernel->TransformArray(PowerSpectrumBuffer, SpectrumBuffer);

			// Accumulate power spectrum CQT output.
			Audio::ArrayAddInPlace(SpectrumBuffer, ChannelSpectrumBuffers[Channel.ChannelIndex]);
		}
	}

	if (NumWindows > 0)
	{
		// Two scalings are applied. One for FFT, another for number of windows calculated.
		float LinearScale = FFTScale / static_cast<float>(NumWindows);

		// This scaling moves output to roughly 0 to 1 range.
		float DbScale = 1.f / FMath::Max(1.f, -NoiseFloorDb);

		// Apply scaling for each channel.
		for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
		{
			Audio::FAlignedFloatBuffer& Buffer = ChannelSpectrumBuffers[ChannelIndex];

			Audio::ArrayMultiplyByConstantInPlace(Buffer, LinearScale);

			Audio::ArrayPowerToDecibelInPlace(Buffer, NoiseFloorDb);

			Audio::ArraySubtractByConstantInPlace(Buffer, NoiseFloorDb);

			Audio::ArrayMultiplyByConstantInPlace(Buffer, DbScale);
		}
	}
}

void FNiagaraDataInterfaceProxySpectrum::SetAudioFormat(int32 InNumChannels, float InSampleRate)
{
	if (InSampleRate != SampleRate)
	{
		ResizeAudioTransform(MinimumFrequency, MaximumFrequency, InSampleRate, NumBands);
	}

	if (InNumChannels != NumChannels)
	{
		ResizeSpectrumBuffer(InNumChannels, NumBands);
	}

	if ((InSampleRate != SampleRate) || (InNumChannels != NumChannels))
	{
		int32 FFTSize = FFTAlgorithm.IsValid() ? FFTAlgorithm->Size() : 0;

		ResizeWindow(InNumChannels, FFTSize);
	}

	NumChannels = InNumChannels;
	SampleRate = InSampleRate;
}

void FNiagaraDataInterfaceProxySpectrum::ResizeSpectrumBuffer(int32 InNumChannels, int32 InNumBands)
{
	for (int32 ChannelIndex = ChannelSpectrumBuffers.Num(); ChannelIndex < InNumChannels; ChannelIndex++)
	{
		ChannelSpectrumBuffers.AddDefaulted();
	}

	for (int32 ChannelIndex = 0; ChannelIndex < InNumChannels; ChannelIndex++)
	{
		Audio::FAlignedFloatBuffer& Buffer = ChannelSpectrumBuffers[ChannelIndex];
		Buffer.Reset();
		if (InNumBands > 0)
		{
			Buffer.AddUninitialized(InNumBands);
		}
	}
}

void FNiagaraDataInterfaceProxySpectrum::ResizeWindow(int32 InNumChannels, int32 InFFTSize)
{
	int32 NumWindowFrames = FMath::Max(1, InFFTSize);

	NumWindowFrames = FMath::Min(NumWindowFrames, 1024);

	WindowBuffer.Reset();
	WindowBuffer.AddUninitialized(NumWindowFrames);

	Audio::GenerateBlackmanWindow(WindowBuffer.GetData(), NumWindowFrames, 1, true);

	int32 NumWindowSamples = NumWindowFrames;

	if (InNumChannels > 0)
	{
		NumWindowSamples *= InNumChannels;
	}

	// 50% overlap
	int32 NumHopSamples = FMath::Max(1, NumWindowSamples / 2);

	SlidingBuffer = MakeUnique<FSlidingBuffer>(NumWindowSamples, NumHopSamples);

}

void FNiagaraDataInterfaceProxySpectrum::ResizeAudioTransform(float InMinimumFrequency, float InMaximumFrequency, float InSampleRate, int32 InNumBands)
{
	FFTAlgorithm.Reset();
	CQTKernel.Reset();
	FFTInputBuffer.Reset();
	FFTOutputBuffer.Reset();
	PowerSpectrumBuffer.Reset();
	SpectrumBuffer.Reset();

	FFTScale = 1.f;

	if (InSampleRate <= 0.f)
	{
		return;
	}

	if (InNumBands < 1)
	{
		return;
	}

	float NumOctaves = GetNumOctaves(InMinimumFrequency, InMaximumFrequency);
	float NumBandsPerOctave = GetNumBandsPerOctave(InNumBands, NumOctaves);

	Audio::FFFTSettings FFTSettings = GetFFTSettings(InMinimumFrequency, InSampleRate, NumBandsPerOctave);
	if (!Audio::FFFTFactory::AreFFTSettingsSupported(FFTSettings))
	{
		UE_LOG(LogNiagara, Warning, TEXT("FFT settings are not supported"));
		return;
	}

	FFTAlgorithm = Audio::FFFTFactory::NewFFTAlgorithm(FFTSettings);

	if (!FFTAlgorithm.IsValid())
	{
		UE_LOG(LogNiagara, Error, TEXT("Failed to create FFT"));
		return;
	}

	FFTInputBuffer.AddZeroed(FFTAlgorithm->NumInputFloats());
	FFTOutputBuffer.AddUninitialized(FFTAlgorithm->NumOutputFloats());
	PowerSpectrumBuffer.AddUninitialized(FFTOutputBuffer.Num() / 2);
	SpectrumBuffer.AddUninitialized(InNumBands);

	float FFTSize = static_cast<float>(FFTAlgorithm->Size());
	check(FFTSize > 0.f);

	// We want to have all FFT implementations
	// return the same scaling so that the energy conservatino property of the fourier transform
	// is supported.  This scaling factor is applied to the power spectrum, so we square the 
	// scaling we would have performed on the magnitude spectrum.
	switch (FFTAlgorithm->ForwardScaling())
	{
		case Audio::EFFTScaling::MultipliedByFFTSize:
			FFTScale = 1.f / (FFTSize * FFTSize);
			break;
			
		case Audio::EFFTScaling::MultipliedBySqrtFFTSize:
			FFTScale = 1.f / FFTSize;
			break;

		case Audio::EFFTScaling::DividedByFFTSize:
			FFTScale = FFTSize * FFTSize;
			break;

		case Audio::EFFTScaling::DividedBySqrtFFTSize:
			FFTScale = FFTSize;
			break;

		default:
			FFTScale = 1.f;
			break;
	}

	float BandwidthStretch = GetBandwidthStretch(InSampleRate, FFTSize, NumBandsPerOctave, InMinimumFrequency);

	Audio::FPseudoConstantQKernelSettings CQTSettings = GetConstantQSettings(InMinimumFrequency, InMaximumFrequency, InNumBands, NumBandsPerOctave, BandwidthStretch);
	CQTKernel = Audio::NewPseudoConstantQKernelTransform(CQTSettings, FFTAlgorithm->Size(), InSampleRate);

	if (!CQTKernel.IsValid())
	{
		UE_LOG(LogNiagara, Error, TEXT("Failed to create CQT kernel."));
		return;
	}
}

void FNiagaraDataInterfaceProxySpectrum::Clamp(float& InMinimumFrequency, float& InMaximumFrequency, int32& InNumBands) 
{
	const int32 MinimumSupportedNumBands = 1;
	const int32 MaximumSupportedNumBands = 16384;
	const float MinimumSupportedFrequency = 20.f;
	const float MaximumSupportedFrequency = 20000.f;

	InMinimumFrequency = FMath::Max(InMinimumFrequency, MinimumSupportedFrequency);
	InMaximumFrequency = FMath::Min(InMaximumFrequency, MaximumSupportedFrequency);
	InMaximumFrequency = FMath::Max(InMinimumFrequency, InMaximumFrequency);

	InNumBands = FMath::Clamp(InNumBands, MinimumSupportedNumBands, MaximumSupportedNumBands);
}

float FNiagaraDataInterfaceProxySpectrum::GetNumOctaves(float InMinimumFrequency, float InMaximumFrequency) 
{
	const float MinimumSupportedNumOctaves = 0.01f;

	float NumOctaves = FMath::Log2(InMaximumFrequency) - FMath::Log2(InMinimumFrequency);
	NumOctaves = FMath::Max(MinimumSupportedNumOctaves, NumOctaves);

	return NumOctaves;
}

float FNiagaraDataInterfaceProxySpectrum::GetNumBandsPerOctave(int32 InNumBands, float InNumOctaves)
{
	InNumOctaves = FMath::Max(0.01f, InNumOctaves);
	const float MinimumSupportedNumBandsPerOctave = 0.01f;

	float NumBandsPerOctave = FMath::Max(MinimumSupportedNumBandsPerOctave, static_cast<float>(InNumBands) / InNumOctaves);

	return NumBandsPerOctave;
}

float FNiagaraDataInterfaceProxySpectrum::GetBandwidthStretch(float InSampleRate, float InFFTSize, float InNumBandsPerOctave, float InMinimumFrequency)
{
	InFFTSize = FMath::Max(1.f, InFFTSize);

	float MinimumFrequencySpacing = (FMath::Pow(2.f, 1.f / InNumBandsPerOctave) - 1.f) * InMinimumFrequency;
	MinimumFrequencySpacing = FMath::Max(0.01f, MinimumFrequencySpacing);

	float FFTBinSpacing = InSampleRate / InFFTSize;
	float BandwidthStretch = FFTBinSpacing / MinimumFrequencySpacing;

	return FMath::Clamp(BandwidthStretch, 0.5f, 2.f);
}

Audio::FPseudoConstantQKernelSettings FNiagaraDataInterfaceProxySpectrum::GetConstantQSettings(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands, float InNumBandsPerOctave, float InBandwidthStretch)
{
	Audio::FPseudoConstantQKernelSettings CQTKernelSettings;

	CQTKernelSettings.NumBands = InNumBands;
	CQTKernelSettings.KernelLowestCenterFreq = InMinimumFrequency; 
	CQTKernelSettings.NumBandsPerOctave = InNumBandsPerOctave;
	CQTKernelSettings.BandWidthStretch = InBandwidthStretch;
	CQTKernelSettings.Normalization = Audio::EPseudoConstantQNormalization::EqualEnergy;

	return CQTKernelSettings;
}

Audio::FFFTSettings FNiagaraDataInterfaceProxySpectrum::GetFFTSettings(float InMinimumFrequency, float InSampleRate, float InNumBandsPerOctave)
{
	const int32 MaximumSupportedLog2FFTSize = 13;
	const int32 MinimumSupportedLog2FFTSize = 8;

	InNumBandsPerOctave = FMath::Max(0.01f, InNumBandsPerOctave);

	float MinimumFrequencySpacing = (FMath::Pow(2.f, 1.f / InNumBandsPerOctave) - 1.f) * InMinimumFrequency;

	MinimumFrequencySpacing = FMath::Max(0.01f, MinimumFrequencySpacing);

	int32 DesiredFFTSize = FMath::CeilToInt(3.f * InSampleRate / MinimumFrequencySpacing);

	int32 Log2FFTSize = MinimumSupportedLog2FFTSize;
	while ((DesiredFFTSize > (1 << Log2FFTSize)) && (Log2FFTSize < MaximumSupportedLog2FFTSize))
	{
		Log2FFTSize++;
	}

	Audio::FFFTSettings FFTSettings;

	FFTSettings.Log2Size = Log2FFTSize;
    FFTSettings.bArrays128BitAligned = true;
    FFTSettings.bEnableHardwareAcceleration = true;

	return FFTSettings;
}


UNiagaraDataInterfaceAudioSpectrum::UNiagaraDataInterfaceAudioSpectrum(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Resolution(512)
	, MinimumFrequency(55.f)
	, MaximumFrequency(10000.f)
	, NoiseFloorDb(-60.f)
	
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Proxy = MakeUnique<FNiagaraDataInterfaceProxySpectrum>(MinimumFrequency, MaximumFrequency, Resolution, NoiseFloorDb, 16384);
	}
}


void UNiagaraDataInterfaceAudioSpectrum::GetSpectrumValue(FVectorVMExternalFunctionContext& Context)
{
	GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateSpectrum();

	VectorVM::FExternalFuncInputHandler<float> InNormalizedPos(Context);
	VectorVM::FExternalFuncInputHandler<int32> InChannel(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		float Position = InNormalizedPos.Get();
		int32 Channel = InChannel.Get();
		*OutValue.GetDest() = GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->GetSpectrumValue(Position, Channel);

		InNormalizedPos.Advance();
		InChannel.Advance();
		OutValue.Advance();
	}
}

void UNiagaraDataInterfaceAudioSpectrum::GetNumChannels(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> OutChannel(Context);

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		*OutChannel.GetDestAndAdvance() = GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->GetNumChannels();
	}
}

void UNiagaraDataInterfaceAudioSpectrum::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	Super::GetFunctions(OutFunctions);

	{
		FNiagaraFunctionSignature GetSpectrumSignature;
		GetSpectrumSignature.Name = GetSpectrumFunctionName;
		GetSpectrumSignature.Inputs.Add(FNiagaraVariable(GetClass(), TEXT("Spectrum")));
		GetSpectrumSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("NormalizedPositionInSpectrum")));
		GetSpectrumSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ChannelIndex")));
		GetSpectrumSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Amplitude")));

		GetSpectrumSignature.bMemberFunction = true;
		GetSpectrumSignature.bRequiresContext = false;
		OutFunctions.Add(GetSpectrumSignature);
	}

	{
		FNiagaraFunctionSignature NumChannelsSignature;
		NumChannelsSignature.Name = GetNumChannelsFunctionName;
		NumChannelsSignature.Inputs.Add(FNiagaraVariable(GetClass(), TEXT("Spectrum")));
		NumChannelsSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NumChannels")));

		NumChannelsSignature.bMemberFunction = true;
		NumChannelsSignature.bRequiresContext = false;
		OutFunctions.Add(NumChannelsSignature);
	}


}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceAudioSpectrum, GetSpectrumValue);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceAudioSpectrum, GetNumChannels);

void UNiagaraDataInterfaceAudioSpectrum::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	if (BindingInfo.Name == GetSpectrumFunctionName)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceAudioSpectrum, GetSpectrumValue)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetNumChannelsFunctionName)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceAudioSpectrum, GetNumChannels)::Bind(this, OutFunc);
	}
	else
	{
		ensureMsgf(false, TEXT("Error! Function defined for this class but not bound."));
	}
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceAudioSpectrum::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	bSuccess &= InVisitor->UpdateShaderParameters<FShaderParameters>();
	return bSuccess;
}

bool UNiagaraDataInterfaceAudioSpectrum::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	bool ParentRet = Super::GetFunctionHLSL(ParamInfo, FunctionInfo, FunctionInstanceIndex, OutHLSL);
	if (ParentRet)
	{
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetSpectrumFunctionName)
	{
		/**
		 * float FrameIndex = NormalizedPositionInBuffer * SpectrumBuffer.Num();
		 * int32 LowerFrameIndex = FMath::FloorToInt(FrameIndex);
		 * float LowerFrameAmplitude = SpectrumBuffer[LowerFrameIndex * NumChannels + ChannelIndex];
		 * int32 HigherFrameIndex = FMath::CeilToInt(FrameIndex);
		 * float HigherFrameAmplitude = SpectrumBuffer[HigherFrameIndex * NumChannels + ChannelIndex];
		 * float Fraction = HigherFrameIndex - FrameIndex;
		 * return FMath::Lerp<float>(LowerFrameAmplitude, HigherFrameAmplitude, Fraction);
		 */
		static const TCHAR *FormatBounds = TEXT(R"(
			void {FunctionName}(float In_NormalizedPosition, int In_ChannelIndex, out float Out_Val)
			{
				float FrameIndex = In_NormalizedPosition * {SpectrumResolution};
				int MaxIndex = {SpectrumResolution} - 1;
				int LowerIndex = floor(FrameIndex);
				LowerIndex = LowerIndex < {SpectrumResolution} ? LowerIndex : MaxIndex;
				LowerIndex = LowerIndex >= 0 ? LowerIndex : 0;
				int UpperIndex =  LowerIndex < MaxIndex ? LowerIndex + 1 : LowerIndex;
				float Fraction = FrameIndex - LowerIndex;
				Fraction = Fraction > 1.0 ? 1.0 : Fraction;
				Fraction = Fraction < 0.0 ? 0.0 : Fraction;
				float LowerValue = {SpectrumBuffer}.Load(In_ChannelIndex * {SpectrumResolution} + LowerIndex);
				float UpperValue = {SpectrumBuffer}.Load(In_ChannelIndex * {SpectrumResolution} + UpperIndex);

				Out_Val = lerp(LowerValue, UpperValue, Fraction);
			}
		)");

		TMap<FString, FStringFormatArg> ArgsBounds = {
			{TEXT("FunctionName"), FStringFormatArg(FunctionInfo.InstanceName)},
			{TEXT("ChannelCount"), FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + NumChannelsName)},
			{TEXT("SpectrumResolution"), FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + ResolutionName)},
			{TEXT("SpectrumBuffer"), FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + GetSpectrumName)},
		};
		OutHLSL += FString::Format(FormatBounds, ArgsBounds);
		return true;
	}
	else if (FunctionInfo.DefinitionName == GetNumChannelsFunctionName)
	{
		// See GetNumChannels()
		static const TCHAR *FormatBounds = TEXT(R"(
			void {FunctionName}(out int Out_Val)
			{
				Out_Val = {ChannelCount};
			}
		)");
		TMap<FString, FStringFormatArg> ArgsBounds = {
			{TEXT("FunctionName"), FStringFormatArg(FunctionInfo.InstanceName)},
			{TEXT("ChannelCount"), FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + NumChannelsName)},
		};
		OutHLSL += FString::Format(FormatBounds, ArgsBounds);
		return true;
	}
	else
	{
		return false;
	}
}

void UNiagaraDataInterfaceAudioSpectrum::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	Super::GetParameterDefinitionHLSL(ParamInfo, OutHLSL);

	static const TCHAR* FormatDeclarations = TEXT(R"(				
		Buffer<float> {GetSpectrumName};
		int {NumChannelsName};
		int {ResolutionName};

	)");
	TMap<FString, FStringFormatArg> ArgsDeclarations = {
		{ TEXT("GetSpectrumName"),    FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + GetSpectrumName) },
		{ TEXT("NumChannelsName"),    FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + NumChannelsName) },
		{ TEXT("ResolutionName"),     FStringFormatArg(ParamInfo.DataInterfaceHLSLSymbol + ResolutionName) },
	};
	OutHLSL += FString::Format(FormatDeclarations, ArgsDeclarations);
}
#endif

void UNiagaraDataInterfaceAudioSpectrum::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UNiagaraDataInterfaceAudioSpectrum::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNiagaraDataInterfaceProxySpectrum& DIProxy = Context.GetProxy<FNiagaraDataInterfaceProxySpectrum>();
	FReadBuffer& SpectrumBuffer = DIProxy.ComputeAndPostSRV();

	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();
	ShaderParameters->NumChannels = DIProxy.GetNumChannels();
	ShaderParameters->Resolution = DIProxy.GetNumBands();
	ShaderParameters->SpectrumBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat(SpectrumBuffer.SRV);
}

bool UNiagaraDataInterfaceAudioSpectrum::Equals(const UNiagaraDataInterface* Other) const
{
	bool bIsEqual = Super::Equals(Other);

	const UNiagaraDataInterfaceAudioSpectrum* OtherSpectrum = CastChecked<const UNiagaraDataInterfaceAudioSpectrum>(Other);

	bIsEqual &= OtherSpectrum->Resolution == Resolution;
	bIsEqual &= OtherSpectrum->MinimumFrequency == MinimumFrequency;
	bIsEqual &= OtherSpectrum->MaximumFrequency == MaximumFrequency;
	bIsEqual &= OtherSpectrum->NoiseFloorDb == NoiseFloorDb;

	return bIsEqual;
}

#if WITH_EDITOR
void UNiagaraDataInterfaceAudioSpectrum::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	static const FName ResolutionFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioSpectrum, Resolution);
	static const FName MinimumFrequencyFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioSpectrum, MinimumFrequency);
	static const FName MaximumFrequencyFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioSpectrum, MaximumFrequency);
	static const FName NoiseFloorFName = GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceAudioSpectrum, NoiseFloorDb);

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (FProperty* PropertyThatChanged = PropertyChangedEvent.Property)
		{
			const FName& Name = PropertyThatChanged->GetFName();
			if (Name == ResolutionFName || Name == MaximumFrequencyFName || Name == MinimumFrequencyFName)
			{
				GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateCQT(MinimumFrequency, MaximumFrequency, Resolution);
			}
			else if (Name == NoiseFloorFName)
			{
				GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateNoiseFloor(NoiseFloorDb);
			}
		}
	}
}
#endif //WITH_EDITOR

void UNiagaraDataInterfaceAudioSpectrum::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
	else
	{
		GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateCQT(MinimumFrequency, MaximumFrequency, Resolution);
		GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateNoiseFloor(NoiseFloorDb);
	}
}

void UNiagaraDataInterfaceAudioSpectrum::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateCQT(MinimumFrequency, MaximumFrequency, Resolution);
		GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateNoiseFloor(NoiseFloorDb);
	}
}

bool UNiagaraDataInterfaceAudioSpectrum::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceAudioSpectrum* CastedDestination = Cast<UNiagaraDataInterfaceAudioSpectrum>(Destination);

	if (CastedDestination)
	{
		CastedDestination->Resolution = Resolution;
		CastedDestination->MinimumFrequency = MinimumFrequency;
		CastedDestination->MaximumFrequency = MaximumFrequency;
		CastedDestination->NoiseFloorDb = NoiseFloorDb;

		if (!CastedDestination->HasAnyFlags(RF_ClassDefaultObject))
		{
			CastedDestination->GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateCQT(MinimumFrequency, MaximumFrequency, Resolution);
			CastedDestination->GetProxyAs<FNiagaraDataInterfaceProxySpectrum>()->UpdateNoiseFloor(NoiseFloorDb);
		}
	}
	
	return true;
}


// Global VM function names, also used by the shaders code generation methods.
const FName UNiagaraDataInterfaceAudioSpectrum::GetSpectrumFunctionName("AudioSpectrum");
const FName UNiagaraDataInterfaceAudioSpectrum::GetNumChannelsFunctionName("GetNumChannels");

// Global variable prefixes, used in HLSL parameter declarations.
const FString UNiagaraDataInterfaceAudioSpectrum::GetSpectrumName(TEXT("_SpectrumBuffer"));
const FString UNiagaraDataInterfaceAudioSpectrum::NumChannelsName(TEXT("_NumChannels"));
const FString UNiagaraDataInterfaceAudioSpectrum::ResolutionName(TEXT("_Resolution"));


#undef LOCTEXT_NAMESPACE

