// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "DSP/SlidingWindow.h"
#include "DSP/DeinterleaveView.h"
#include "DSP/ConstantQ.h"
#include "DSP/FFTAlgorithm.h"
#include "DSP/FloatArrayMath.h"
#include "DSP/AudioFFT.h"
#include "NiagaraDataInterfaceAudio.h"
#include "NiagaraDataInterfaceAudioSpectrum.generated.h"

/** FNiagaraDataInterfaceProxySpectrum
 *  
 *  Proxy class for calculating spectrums on the rendering thread for Niagara.
 */
struct FNiagaraDataInterfaceProxySpectrum : public FNiagaraDataInterfaceProxySubmix
{
	/** Construct an FNiagaraDataInterfaceProxySpectrum
	 *
	 * 	@param InMinimumFrequency - Minimum frequency in spectrum.
	 * 	@param InMaximumFrequency - Maximum frequency in spectrum.
	 * 	@param InNumBands - Number of bands in spectrum.
	 * 	@param InNoiseFloorDb - Decibels level considered as silence. 
	 * 	@param InNumSamplesToBuffer - Number of samples to buffer internally across thread boundaries.
	 */
	FNiagaraDataInterfaceProxySpectrum(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands, float InNoiseFloorDb, int32 InNumSamplesToBuffer);

	virtual ~FNiagaraDataInterfaceProxySpectrum();

	/** Sample value from the spectrum buffer.
	 *  
	 *  @param InNormalizedPositionInSpectrum - Value between 0 and 1 which represents the relative frequency in the spectrum.  
	 *                                          0 corresponds to the MinimumFrequency, 1 corresponds to the MaximumFrequency.
	 *                                          Values between 0 and 1 correspond to logarithmically spaced frequencys in-between
	 *                                          the limits.
	 *  @param InChannelIndex - The index of the audio channel to sample.
	 *
	 *  @return Amplitude at this position. 0.0 corresponds to the nosie floor, while 1.0 corresponds to full volume. Some values
	 *          may exceed 1.0.
	 */
	float GetSpectrumValue(float InNormalizedPositionInSpectrum, int32 InChannelIndex);

	int32 GetNumBands() const;

	/** Updates the minimum and maximum frequency of the CQT on the render thread. */
	void UpdateCQT(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands);

	/** Updates the value considered silence in decibles on the render thread. */
	void UpdateNoiseFloor(float InNoiseFloorDb);


	/** This function enqueues a render thread command to pop audio off of the SubmixListener, transform it into a CQT, and 
	 * post it to the GPUAudioBuffer.
	 */
	void PostDataToGPU();
	FReadBuffer& ComputeAndPostSRV();

	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}

	// Calculates the CQT
	void UpdateSpectrum();

private:
	typedef Audio::TSlidingBuffer<float> FSlidingBuffer;
	typedef Audio::TAutoSlidingWindow<float, Audio::FAudioBufferAlignedAllocator> FSlidingWindow;
	typedef Audio::TAutoDeinterleaveView<float, Audio::FAudioBufferAlignedAllocator> FDeinterleaveView;
	typedef FDeinterleaveView::TChannel<Audio::FAudioBufferAlignedAllocator> FChannel;

	// Updates internal objects for num channels and samplerate
	void SetAudioFormat(int32 InNumChannels, float InSampleRate);

	void SetNoiseFloor(float InNoiseFloorDb);

	// Resize internal abuffers
	void ResizeCQT(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands);
	void ResizeSpectrumBuffer(int32 InNumChannels, int32 InNumBands);
	void ResizeAudioTransform(float InMinimumFrequency, float InMaximumFrequency, float InSampleRate, int32 InNumBands);
	void ResizeWindow(int32 InNumChannels, int32 InFFTSize);


	// Get settings 
	static void Clamp(float& InMinimumFrequency, float& InMaximumFrequency, int32& InNumBand);
	static float GetNumOctaves(float InMinimumFrequency, float InMaximumFrequency);
	static float GetNumBandsPerOctave(int32 InNumBands, float InNumOctaves);
	static float GetBandwidthStretch(float InSampleRate, float InFFTSize, float InNumBandsPerOctave, float InMinimumFrequency);

	static Audio::FPseudoConstantQKernelSettings GetConstantQSettings(float InMinimumFrequency, float InMaximumFrequency, int32 InNumBands, float InNumBandsPerOctave, float InBandwidthStretch);

	static Audio::FFFTSettings GetFFTSettings(float InMinimumFrequency, float InSampleRate, float InNumBandsPerOctave);

	float MinimumFrequency;
	float MaximumFrequency;
	TAtomic<int32> NumBands;
	float NoiseFloorDb;

	int32 NumChannels;
	float SampleRate;
	float FFTScale;

	Audio::FAlignedFloatBuffer PopBuffer;
	TUniquePtr< FSlidingBuffer > SlidingBuffer;

	TArray<Audio::FAlignedFloatBuffer> ChannelSpectrumBuffers;

	TUniquePtr<Audio::FContiguousSparse2DKernelTransform> CQTKernel;
	TUniquePtr<Audio::IFFTAlgorithm> FFTAlgorithm;

	Audio::FAlignedFloatBuffer InterleavedBuffer;
	Audio::FAlignedFloatBuffer DeinterleavedBuffer;
	Audio::FAlignedFloatBuffer FFTInputBuffer;
	Audio::FAlignedFloatBuffer FFTOutputBuffer;
	Audio::FAlignedFloatBuffer PowerSpectrumBuffer;
	Audio::FAlignedFloatBuffer SpectrumBuffer;
	Audio::FAlignedFloatBuffer WindowBuffer;

	// Handle for the SRV used by the generated HLSL.
	FReadBuffer GPUBuffer;

	FCriticalSection BufferLock;
};

/** Data Interface allowing sampling of recent audio spectrum. */
UCLASS(EditInlineNew, Category = "Audio", meta = (DisplayName = "Audio Spectrum"))
class NIAGARA_API UNiagaraDataInterfaceAudioSpectrum : public UNiagaraDataInterfaceAudioSubmix
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(int,				NumChannels)
		SHADER_PARAMETER(int,				Resolution)
		SHADER_PARAMETER_SRV(Buffer<float>, SpectrumBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	// VM function names
	static const FName GetSpectrumFunctionName;
	static const FName GetNumChannelsFunctionName;

	// Global variable prefixes
	static const FString GetSpectrumName;
	static const FString NumChannelsName;
	static const FString ResolutionName;
	
	/** The number of spectrum samples to pass to the GPU */
	UPROPERTY(EditAnywhere, Category = "Spectrum", meta = (ClampMin = "16", ClampMax = "1024") )
	int32 Resolution;

	/** The minimum frequency represented in the spectrum. */
	UPROPERTY(EditAnywhere, Category = "Spectrum", meta = (ClampMin = "20.0", ClampMax = "20000.0"))
	float MinimumFrequency;

	/** The maximum frequency represented in the spectrum. */
	UPROPERTY(EditAnywhere, Category = "Spectrum", meta = (ClampMin = "20.0", ClampMax = "20000.0"))
	float MaximumFrequency;

	/** The decibel level considered as silence. This is used to scale the output of the spectrum. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "Spectrum", meta = (ClampMin = "-120.0", ClampMax = "0.0"))
	float NoiseFloorDb;

	//VM function overrides:
	void GetSpectrumValue(FVectorVMExternalFunctionContext& Context);
	void GetNumChannels(FVectorVMExternalFunctionContext& Context);

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override 
	{
		return true;
	}

#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR


	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
private:

};

